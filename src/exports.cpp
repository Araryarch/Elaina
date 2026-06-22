#include <iostream>
#include <string>
#include <WinSock2.h>
#include "exports.h"
#include "syscall/Syscall.h"
#include "process/scanner.h"
#include "offsets.h"
#include "execution/executor.h"
#include <optional>
#include <memory>
#include <vector>
#include <cstring>
#include <algorithm>
#include <ostream>
#include <iostream>
#include <string>
#include <iomanip>

// Internal state
static std::optional<RobloxProcess> gCore;
static uintptr_t gDataModel = 0;
static std::string gLastError;
void CleanupSession(); // Forward declaration

// Internal helpers
std::string InternalReadString(HANDLE h, uintptr_t address) {
    if (address < 0x10000 || address > 0x7FFFFFFFFFFF) return "";
    struct {
        union { char buffer[16]; char* ptr; };
        size_t length;
        size_t capacity;
    } strData;
    if (!ProcessScanner::ReadMemory(h, address, &strData, sizeof(strData))) return "";
    if (strData.length == 0 || strData.length > 200) return "";
    if (strData.length < 16) return std::string(strData.buffer, strData.length);
    uintptr_t dataAddr = reinterpret_cast<uintptr_t>(strData.ptr);
    std::vector<char> buffer(strData.length + 1, 0);
    if (ProcessScanner::ReadMemory(h, dataAddr, buffer.data(), strData.length)) return std::string(buffer.data(), strData.length);
    return "";
}


extern "C" {

ELAINA_API bool __stdcall Initialize() {
    return Syscall::Initialize();
}

ELAINA_API DWORD __stdcall FindRobloxProcess() {
    auto pids = ProcessScanner::FindRobloxProcesses();
    if (pids.empty()) return 0;
    if (pids.size() == 1) return pids[0];

    // Multiple Roblox processes: pick the game client (most jobs)
    DWORD bestPid = 0;
    int bestJobs = 0;

    for (DWORD pid : pids) {
        auto proc = ProcessScanner::Connect(pid);
        if (!proc) continue;

        HANDLE h = proc->handle;
        uintptr_t base = proc->baseAddress;
        uintptr_t ts = ProcessScanner::Read<uintptr_t>(h, base + offsets::Pointer::TaskScheduler);
        if (!ts) continue;

        uintptr_t jS = ProcessScanner::Read<uintptr_t>(h, ts + offsets::TaskScheduler::JobStart);
        uintptr_t jE = ProcessScanner::Read<uintptr_t>(h, ts + offsets::TaskScheduler::JobEnd);
        int jobs = (int)((jE > jS) ? (jE - jS) / 8 : 0);

        if (jobs > bestJobs) {
            bestJobs = jobs;
            bestPid = pid;
        }
    }

    return bestPid ? bestPid : pids[0];
}

ELAINA_API bool __stdcall Connect(DWORD pid) {
    gCore = ProcessScanner::Connect(pid);
    if (gCore.has_value()) {
        ScriptExecutor::SetRobloxPid(pid);
        // Start HTTP server for Lua<->C++ communication
        if (!HttpServer::IsRunning()) {
            HttpServer::Start();
        }

        // Auto-inject init script so attach UI appears immediately
        if (!gDataModel) GetDataModel();
        if (gDataModel) {
            std::string err;
            ScriptExecutor::Execute(gCore->handle, gCore->baseAddress, gDataModel, "", err);
        }
    }
    return gCore.has_value();
}

ELAINA_API void __stdcall Disconnect() {
    std::cout << "[API] Disconnect called — clearing session\n" << std::flush;
    gCore.reset();
    gDataModel = 0;
    ScriptExecutor::ResetState();
    HttpServer::Stop();
}

} // end extern "C" (temporary close for CleanupSession)

// Internal: Just reset DataModel state when leaving a server, keep the handle
void CleanupSession() {
    std::cout << "[API] Resetting session state (keeping handle)\n" << std::flush;
    gDataModel = 0;
    ScriptExecutor::ResetState();
}

extern "C" {

ELAINA_API DWORD __stdcall GetRobloxPid() {
    return (gCore && gCore->pid) ? gCore->pid : 0;
}

ELAINA_API void __stdcall RedirConsole() {
    // Attempt to sync with whatever console is available
    FreeConsole();
    if (AttachConsole(ATTACH_PARENT_PROCESS) || AllocConsole()) {
        FILE* f;
        freopen_s(&f, "CONOUT$", "w", stdout);
        freopen_s(&f, "CONOUT$", "w", stderr);

        std::ios::sync_with_stdio(true);
        std::cout.clear();
        std::cerr.clear();
        std::cout.setstate(std::ios::goodbit);

        std::cout << "[DLL] Native console redirection successful\n" << std::flush;
    }
}

ELAINA_API uintptr_t __stdcall GetDataModel() {
    if (!gCore || !gCore->handle) { std::cout << "[DM] No core/handle\n"; return 0; }
    if (gDataModel) return gDataModel;

    HANDLE h = gCore->handle;
    uintptr_t base = gCore->baseAddress;
    std::cout << "[DM] Base: 0x" << std::hex << base << std::dec << "\n";

    // Try FakeDataModel pointer approach (proven working)
    uintptr_t fakeDM = ProcessScanner::Read<uintptr_t>(h, base + offsets::Pointer::FakeDataModelPointer);
    if (fakeDM) {
        uintptr_t dmCandidate = ProcessScanner::Read<uintptr_t>(h, fakeDM + 0x1C0);
        if (dmCandidate) {
            uintptr_t cd = ProcessScanner::Read<uintptr_t>(h, dmCandidate + 0x18);
            if (cd > 0x10000 && cd < 0x7FFFFFFFFFFF) {
                uintptr_t namePtr = ProcessScanner::Read<uintptr_t>(h, cd + 0x8);
                std::string cn = InternalReadString(h, namePtr);
                if (cn == "DataModel") {
                    std::cout << "[DM] FOUND via FakeDataModel!\n";
                    gDataModel = dmCandidate;
                    return gDataModel;
                }
            }
        }
    }

    // Fallback: TaskScheduler job scan
    uintptr_t ts = ProcessScanner::Read<uintptr_t>(h, base + offsets::Pointer::TaskScheduler);
    if (!ts) return 0;

    uintptr_t jS = ProcessScanner::Read<uintptr_t>(h, ts + offsets::TaskScheduler::JobStart);
    uintptr_t jE = ProcessScanner::Read<uintptr_t>(h, ts + offsets::TaskScheduler::JobEnd);
    size_t count = (jE > jS) ? (jE - jS) / 8 : 0;

    for (size_t i = 0; i < count; i++) {
        uintptr_t job = ProcessScanner::Read<uintptr_t>(h, jS + i * 8);
        if (!job) continue;

        std::vector<uintptr_t> jobMem(0x80, 0);
        ProcessScanner::ReadMemory(h, job, jobMem.data(), jobMem.size() * 8);

        for (size_t k = 0; k < jobMem.size(); k++) {
            uintptr_t ptr = jobMem[k];
            if (ptr < 0x10000 || ptr > 0x7FFFFFFFFFFF || (ptr % 8 != 0)) continue;

            uintptr_t cd = ProcessScanner::Read<uintptr_t>(h, ptr + 0x18);
            if (cd < 0x10000 || cd > 0x7FFFFFFFFFFF || (cd % 8 != 0)) continue;

            uintptr_t namePtr = ProcessScanner::Read<uintptr_t>(h, cd + 0x8);
            if (namePtr < 0x10000 || namePtr > 0x7FFFFFFFFFFF) continue;
            std::string className = InternalReadString(h, namePtr);
            if (className == "DataModel") {
                gDataModel = ptr;
                std::cout << "[DM] FOUND via job scan!\n";
                return gDataModel;
            }
        }
    }
    return 0;
}

ELAINA_API int __stdcall GetJobCount() {
    if (!gCore || !gCore->handle) return -1;
    HANDLE h = gCore->handle;
    uintptr_t base = gCore->baseAddress;
    uintptr_t ts = ProcessScanner::Read<uintptr_t>(h, base + offsets::Pointer::TaskScheduler);
    if (!ts) return -1;
    uintptr_t jS = ProcessScanner::Read<uintptr_t>(h, ts + offsets::TaskScheduler::JobStart);
    uintptr_t jE = ProcessScanner::Read<uintptr_t>(h, ts + offsets::TaskScheduler::JobEnd);
    return (int)((jE - jS) / 8);
}

ELAINA_API int __stdcall ExecuteScript(const char* source, int sourceLen) {
    if (!gCore || !gCore->handle) {
        gLastError = "Not connected to Roblox";
        return ScriptExecutor::ERR_NOT_CONNECTED;
    }

    // Ensure DataModel is found and still valid
    if (gDataModel) {
        bool valid = false;
        try {
            std::string name = rblx::ReadInstanceName(gCore->handle, gDataModel);
            uintptr_t parent = ProcessScanner::Read<uintptr_t>(gCore->handle, gDataModel + offsets::Instance::Parent);
            if ((name == "Game" || name == "App") && parent == 0) {
                valid = true;
            }
        } catch (...) { valid = false; }

        if (!valid) {
            std::cout << "[API] DataModel invalid (Teleport/Leave detected) — resetting session\n";
            CleanupSession();
        }
    }

    if (!gDataModel) {
        GetDataModel();
        if (!gDataModel) {
            std::cout << "[API] ERROR: Could not find DataModel\n";
            gLastError = "DataModel not found";
            return ScriptExecutor::ERR_NO_DATAMODEL;
        }
    }

    std::string src(source, sourceLen > 0 ? sourceLen : strlen(source));
    std::cout << "[API] ExecuteScript: " << src.size() << " bytes\n";

    return ScriptExecutor::Execute(gCore->handle, gCore->baseAddress, gDataModel, src, gLastError);
}

ELAINA_API int __stdcall GetLastExecError(char* buffer, int bufLen) {
    if (!buffer || bufLen <= 0) return 0;
    int copyLen = (int)std::min((size_t)bufLen - 1, gLastError.size());
    memcpy(buffer, gLastError.c_str(), copyLen);
    buffer[copyLen] = '\0';
    return copyLen;
}

ELAINA_API bool __stdcall ReadMemory(uintptr_t address, void* buffer, size_t size) {
    if (!gCore || !gCore->handle) return false;
    return ProcessScanner::ReadMemory(gCore->handle, address, buffer, size);
}

ELAINA_API bool __stdcall WriteMemory(uintptr_t address, const void* buffer, size_t size) {
    if (!gCore || !gCore->handle) return false;
    return ProcessScanner::WriteMemory(gCore->handle, address, buffer, size);
}

ELAINA_API bool __stdcall GetClientInfo(char* buffer, int maxSize) {
    if (!gCore || !gCore->handle || !gDataModel) return false;
    HANDLE h = gCore->handle;

    std::string jobId = "Active";
    std::string name = "Unknown";
    std::string userId = "1";

    // Attempt to get JobId
    std::string jid = rblx::ReadRobloxString(h, gDataModel + offsets::DataModel::JobId);
    if (!jid.empty()) jobId = jid;

    // Get Players Service
    uintptr_t players = rblx::FindChildByClassName(h, gDataModel, "Players");
    if (players) {
        // Get LocalPlayer
        uintptr_t lp = ProcessScanner::Read<uintptr_t>(h, players + offsets::Players::LocalPlayer);
        if (lp) {
            std::string lpName = rblx::ReadInstanceName(h, lp);
            if (!lpName.empty()) name = lpName;

            uint64_t uid = ProcessScanner::Read<uint64_t>(h, lp + offsets::Player::UserId);
            if (uid > 0) userId = std::to_string(uid);
        }
    }

    std::string placeId = "0";
    uint64_t pid = ProcessScanner::Read<uint64_t>(h, gDataModel + offsets::DataModel::PlaceId);
    if (pid > 0) placeId = std::to_string(pid);

    std::string result = name + "|" + userId + "|" + jobId + "|" + placeId;
    if (result.length() >= (size_t)maxSize) return false;

    strcpy_s(buffer, maxSize, result.c_str());
    return true;
}

}
