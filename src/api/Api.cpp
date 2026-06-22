#include "Api.h"
#include "core/Log.h"
#include "syscall/Syscall.h"
#include "process/scanner.h"
#include "offsets.h"
#include "execution/executor.h"
#include "instance_utils.h"
#include <optional>
#include <cstring>
#include <stdexcept>

static std::optional<RobloxProcess> gCore;
static uintptr_t gDataModel = 0;
static int g_state = ELAINA_IDLE;
static std::string g_lastError;

static std::string InternalReadString(HANDLE h, uintptr_t address) {
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
    if (ProcessScanner::ReadMemory(h, dataAddr, buffer.data(), strData.length))
        return std::string(buffer.data(), strData.length);
    return "";
}

static uintptr_t FindDataModel(HANDLE h, uintptr_t base) {
    uintptr_t fakeDM = ProcessScanner::Read<uintptr_t>(h, base + offsets::Pointer::FakeDataModelPointer);
    if (fakeDM) {
        uintptr_t dmCandidate = ProcessScanner::Read<uintptr_t>(h, fakeDM + offsets::FakeDataModel::DataModel);
        if (dmCandidate) {
            uintptr_t cd = ProcessScanner::Read<uintptr_t>(h, dmCandidate + offsets::Instance::ClassDescriptor);
            if (cd > 0x10000 && cd < 0x7FFFFFFFFFFF) {
                uintptr_t namePtr = ProcessScanner::Read<uintptr_t>(h, cd + offsets::Instance::ClassDescriptorToClassName);
                std::string cn = InternalReadString(h, namePtr);
                if (cn == "DataModel") return dmCandidate;
            }
        }
    }

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
            uintptr_t cd = ProcessScanner::Read<uintptr_t>(h, ptr + offsets::Instance::ClassDescriptor);
            if (cd < 0x10000 || cd > 0x7FFFFFFFFFFF || (cd % 8 != 0)) continue;
            uintptr_t namePtr = ProcessScanner::Read<uintptr_t>(h, cd + offsets::Instance::ClassDescriptorToClassName);
            if (namePtr < 0x10000 || namePtr > 0x7FFFFFFFFFFF) continue;
            std::string className = InternalReadString(h, namePtr);
            if (className == "DataModel") return ptr;
        }
    }
    return 0;
}

extern "C" {

bool __stdcall ElainaAttach() {
    try {
        if (!Syscall::Initialize()) {
            g_lastError = "Syscall init failed";
            g_state = ELAINA_ERROR;
            return false;
        }

        auto pids = ProcessScanner::FindRobloxProcesses();
        if (pids.empty()) {
            g_lastError = "No Roblox process found";
            g_state = ELAINA_ERROR;
            return false;
        }

        DWORD pid = pids[0];
        if (pids.size() > 1) {
            DWORD bestPid = 0;
            int bestJobs = 0;
            for (DWORD p : pids) {
                auto proc = ProcessScanner::Connect(p);
                if (!proc) continue;
                uintptr_t ts = ProcessScanner::Read<uintptr_t>(proc->handle, proc->baseAddress + offsets::Pointer::TaskScheduler);
                if (!ts) continue;
                uintptr_t jS = ProcessScanner::Read<uintptr_t>(proc->handle, ts + offsets::TaskScheduler::JobStart);
                uintptr_t jE = ProcessScanner::Read<uintptr_t>(proc->handle, ts + offsets::TaskScheduler::JobEnd);
                int jobs = (int)((jE > jS) ? (jE - jS) / 8 : 0);
                if (jobs > bestJobs) { bestJobs = jobs; bestPid = p; }
            }
            if (bestPid) pid = bestPid;
        }

        gCore = ProcessScanner::Connect(pid);
        if (!gCore.has_value()) {
            g_lastError = "Failed to connect to process";
            g_state = ELAINA_ERROR;
            return false;
        }

        ScriptExecutor::SetRobloxPid(pid);
        if (!HttpServer::IsRunning()) HttpServer::Start();

        gDataModel = FindDataModel(gCore->handle, gCore->baseAddress);
        if (gDataModel) {
            std::string err;
            ScriptExecutor::Execute(gCore->handle, gCore->baseAddress, gDataModel, "", err);
        }

        g_state = ELAINA_ATTACHED;
        g_lastError.clear();
        Log::Info("Elaina attached to PID %u", pid);
        return true;
    } catch (const std::exception& e) {
        g_lastError = std::string("Attach exception: ") + e.what();
        Log::Error("ElainaAttach exception: %s", e.what());
        g_state = ELAINA_ERROR;
        return false;
    }
}

void __stdcall ElainaDetach() {
    try {
        gCore.reset();
        gDataModel = 0;
        ScriptExecutor::ResetState();
        HttpServer::Stop();
    } catch (const std::exception& e) {
        Log::Error("ElainaDetach exception: %s", e.what());
    }
    g_state = ELAINA_IDLE;
}

bool __stdcall ElainaExecute(const char* script) {
    try {
        if (!gCore || !gCore->handle) {
            g_lastError = "Not connected";
            return false;
        }

        if (gDataModel) {
            bool valid = false;
            try {
                std::string name = rblx::ReadInstanceName(gCore->handle, gDataModel);
                uintptr_t parent = ProcessScanner::Read<uintptr_t>(gCore->handle, gDataModel + offsets::Instance::Parent);
                if ((name == "Game" || name == "App") && parent == 0) valid = true;
            } catch (...) { valid = false; }
            if (!valid) {
                gDataModel = 0;
                ScriptExecutor::ResetState();
            }
        }

        if (!gDataModel) {
            gDataModel = FindDataModel(gCore->handle, gCore->baseAddress);
            if (!gDataModel) {
                g_lastError = "DataModel not found";
                return false;
            }
        }

        std::string src(script);
        int result = ScriptExecutor::Execute(gCore->handle, gCore->baseAddress, gDataModel, src, g_lastError);
        return result == ScriptExecutor::SUCCESS;
    } catch (const std::exception& e) {
        g_lastError = std::string("Execute exception: ") + e.what();
        Log::Error("ElainaExecute exception: %s", e.what());
        return false;
    }
}

int __stdcall ElainaGetState() {
    try {
        if (g_state >= ELAINA_ATTACHED && gCore && gCore->pid) {
            HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, gCore->pid);
            if (h) {
                DWORD exitCode = 0;
                if (!GetExitCodeProcess(h, &exitCode) || exitCode != STILL_ACTIVE) {
                    CloseHandle(h);
                    gCore.reset();
                    gDataModel = 0;
                    g_state = ELAINA_IDLE;
                    g_lastError = "Target process exited";
                    return g_state;
                }
                CloseHandle(h);
            }
        }
    } catch (...) {
        g_state = ELAINA_ERROR;
    }
    return g_state;
}

int __stdcall ElainaGetPid() {
    return (gCore && gCore->pid) ? gCore->pid : 0;
}

bool __stdcall ElainaInject() {
    if (!gCore || !gCore->handle) {
        g_lastError = "Not connected";
        return false;
    }
    g_state = ELAINA_INJECTED;
    g_lastError.clear();
    return true;
}

void __stdcall ElainaStartServer(int port) {}

void __stdcall ElainaStopServer() {
    try { HttpServer::Stop(); } catch (const std::exception& e) {
        Log::Error("ElainaStopServer exception: %s", e.what());
    }
}

const char* __stdcall ElainaDiagnose() {
    try {
        if (!gCore || !gCore->handle) {
            g_lastError = "Not attached. Call ElainaAttach() first.";
            return g_lastError.c_str();
        }
        std::string diag;
        diag += "PID: " + std::to_string(gCore->pid) + "\n";
        diag += "Base: 0x" + std::to_string(gCore->baseAddress) + "\n";
        diag += "DataModel: " + (gDataModel ? "0x" + std::to_string(gDataModel) : "NOT FOUND") + "\n";
        diag += "Server: " + std::string(HttpServer::IsRunning() ? "running" : "stopped") + "\n";

        std::string moduleName;
        uintptr_t module = ScriptExecutor::FindUnloadedModule(gCore->handle, moduleName);
        diag += "Next module: " + moduleName + "\n";

        g_lastError = diag;
        return g_lastError.c_str();
    } catch (const std::exception& e) {
        g_lastError = std::string("Exception: ") + e.what();
        return g_lastError.c_str();
    }
}

const char* __stdcall ElainaGetLastError() {
    return g_lastError.c_str();
}

} // extern "C"

BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        Log::Info("Elaina DLL loaded");
    }
    return TRUE;
}
