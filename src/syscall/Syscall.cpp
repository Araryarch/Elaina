#include "Syscall.h"
#include <string>
#include <unordered_map>

struct SyscallTable {
    uint32_t NtOpenProcess = 0, NtReadVirtualMemory = 0, NtWriteVirtualMemory = 0;
    uint32_t NtAllocateVirtualMemory = 0, NtFreeVirtualMemory = 0, NtProtectVirtualMemory = 0;
    uint32_t NtSuspendProcess = 0, NtResumeProcess = 0, NtQueryInformationProcess = 0, NtClose = 0;
};
static SyscallTable g_sc{};

bool Syscall::Initialize() {
    wchar_t sysDir[MAX_PATH];
    GetSystemDirectoryW(sysDir, MAX_PATH);
    std::wstring ntdllPath = std::wstring(sysDir) + L"\\ntdll.dll";

    HANDLE hFile = CreateFileW(ntdllPath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    HANDLE hMapping = CreateFileMappingW(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!hMapping) { CloseHandle(hFile); return false; }

    void* base = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
    if (!base) { CloseHandle(hMapping); CloseHandle(hFile); return false; }

    bool ok = ParseExports(base);

    UnmapViewOfFile(base);
    CloseHandle(hMapping);
    CloseHandle(hFile);

    return ok;
}

bool Syscall::ParseExports(void* fileBase) {
    auto dos = (PIMAGE_DOS_HEADER)fileBase;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;

    auto nt = (PIMAGE_NT_HEADERS64)((uint8_t*)fileBase + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;

    auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (dir.VirtualAddress == 0) return false;

    auto rva = [&](DWORD rva) -> uint8_t* {
        auto sec = IMAGE_FIRST_SECTION(nt);
        for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++) {
            if (rva >= sec[i].VirtualAddress && rva < sec[i].VirtualAddress + sec[i].SizeOfRawData)
                return (uint8_t*)fileBase + sec[i].PointerToRawData + (rva - sec[i].VirtualAddress);
        }
        return nullptr;
    };

    auto exp = (PIMAGE_EXPORT_DIRECTORY)rva(dir.VirtualAddress);
    if (!exp) return false;

    auto names = (DWORD*)rva(exp->AddressOfNames);
    auto funcs = (DWORD*)rva(exp->AddressOfFunctions);
    auto ords = (WORD*)rva(exp->AddressOfNameOrdinals);

    std::unordered_map<std::string, uint32_t*> needed = {
        {"NtOpenProcess", &g_sc.NtOpenProcess},
        {"NtReadVirtualMemory", &g_sc.NtReadVirtualMemory},
        {"NtWriteVirtualMemory", &g_sc.NtWriteVirtualMemory},
        {"NtAllocateVirtualMemory", &g_sc.NtAllocateVirtualMemory},
        {"NtFreeVirtualMemory", &g_sc.NtFreeVirtualMemory},
        {"NtProtectVirtualMemory", &g_sc.NtProtectVirtualMemory},
        {"NtSuspendProcess", &g_sc.NtSuspendProcess},
        {"NtResumeProcess", &g_sc.NtResumeProcess},
        {"NtQueryInformationProcess", &g_sc.NtQueryInformationProcess},
        {"NtClose", &g_sc.NtClose},
    };

    int found = 0;
    for (DWORD i = 0; i < exp->NumberOfNames; i++) {
        auto name = (const char*)rva(names[i]);
        if (!name) continue;
        auto it = needed.find(name);
        if (it == needed.end()) continue;
        auto body = rva(funcs[ords[i]]);
        if (!body) continue;
        if (body[0] == 0x4C && body[1] == 0x8B && body[2] == 0xD1 && body[3] == 0xB8) {
            *(it->second) = *(uint32_t*)(body + 4);
            found++;
        }
        if (found == (int)needed.size()) break;
    }
    return found == (int)needed.size();
}

// ---- Wrappers ----

NTSTATUS Syscall::OpenProcess(PHANDLE h, ACCESS_MASK a, void* oa, void* cid) {
    currentSSN = g_sc.NtOpenProcess;
    return DoSyscall((void*)h, (void*)(uintptr_t)a, oa, cid, nullptr, nullptr);
}

NTSTATUS Syscall::ReadMemory(HANDLE hProc, void* addr, void* buf, size_t size, SIZE_T* read) {
    currentSSN = g_sc.NtReadVirtualMemory;
    return DoSyscall((void*)hProc, addr, buf, (void*)size, (void*)read, nullptr);
}

NTSTATUS Syscall::WriteMemory(HANDLE hProc, void* addr, const void* buf, size_t size, SIZE_T* written) {
    currentSSN = g_sc.NtWriteVirtualMemory;
    return DoSyscall((void*)hProc, addr, const_cast<void*>(buf), (void*)size, (void*)written, nullptr);
}

NTSTATUS Syscall::AllocateMemory(HANDLE hProc, void** addr, SIZE_T size, ULONG type, ULONG protect) {
    currentSSN = g_sc.NtAllocateVirtualMemory;
    return DoSyscall((void*)hProc, (void*)addr, (void*)0, (void*)&size, (void*)(uintptr_t)type, (void*)(uintptr_t)protect);
}

NTSTATUS Syscall::ProtectMemory(HANDLE hProc, void** addr, SIZE_T* size, ULONG newProt, ULONG* oldProt) {
    currentSSN = g_sc.NtProtectVirtualMemory;
    return DoSyscall((void*)hProc, (void*)addr, (void*)size, (void*)(uintptr_t)newProt, (void*)oldProt, nullptr);
}

NTSTATUS Syscall::SuspendProcess(HANDLE hProc) {
    currentSSN = g_sc.NtSuspendProcess;
    return DoSyscall((void*)hProc, nullptr, nullptr, nullptr, nullptr, nullptr);
}

NTSTATUS Syscall::ResumeProcess(HANDLE hProc) {
    currentSSN = g_sc.NtResumeProcess;
    return DoSyscall((void*)hProc, nullptr, nullptr, nullptr, nullptr, nullptr);
}

NTSTATUS Syscall::CloseHandle(HANDLE h) {
    currentSSN = g_sc.NtClose;
    return DoSyscall((void*)h, nullptr, nullptr, nullptr, nullptr, nullptr);
}
