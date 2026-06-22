#include "Process.h"
#include "core/Log.h"
#include "syscall/Syscall.h"
#include <TlHelp32.h>

std::vector<DWORD> Process::FindAll() {
    std::vector<DWORD> pids;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return pids;

    PROCESSENTRY32W pe{ .dwSize = sizeof(pe) };
    if (Process32FirstW(snap, &pe)) {
        do {
            std::wstring name(pe.szExeFile);
            if (name == L"RobloxPlayerBeta.exe" || name == L"Windows10Universal.exe")
                pids.push_back(pe.th32ProcessID);
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    Log::Info("Found %zu Roblox process(es)", pids.size());
    return pids;
}

Result<RobloxHandle> Process::Attach(DWORD pid) {
    RobloxHandle rbx;

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE)
        return { Err::ProcessNotFound, "Cannot snapshot modules" };
    MODULEENTRY32W me{ .dwSize = sizeof(me) };
    if (Module32FirstW(snap, &me)) rbx.base = (uintptr_t)me.modBaseAddr;
    CloseHandle(snap);

    if (!rbx.base)
        return { Err::ProcessNotFound, "Cannot get base address" };

    struct { ULONG Length; HANDLE Root; void* Name; ULONG Attr; void* SD; void* QOS; } oa{};
    oa.Length = sizeof(oa);
    struct { HANDLE P; HANDLE T; } cid{};
    cid.P = (HANDLE)(uintptr_t)pid;

    NTSTATUS st = Syscall::OpenProcess(&rbx.handle,
        PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION,
        &oa, &cid);

    if (st != 0 || !rbx.handle)
        return { Err::ProcessOpenFailed, "NtOpenProcess returned " + std::to_string(st) };

    rbx.pid = pid;
    Log::Info("Attached to PID %u, base=0x%llx", pid, (unsigned long long)rbx.base);
    return rbx;
}

Result<RobloxHandle> Process::AttachFirst() {
    auto pids = FindAll();
    if (pids.empty()) return { Err::ProcessNotFound, "No Roblox process running" };
    return Attach(pids[0]);
}

bool Process::IsRunning(DWORD pid) {
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!h) return false;
    DWORD exit = 0;
    GetExitCodeProcess(h, &exit);
    CloseHandle(h);
    return exit == STILL_ACTIVE;
}
