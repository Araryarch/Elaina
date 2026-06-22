#pragma once
#include <WinSock2.h>
#include <Windows.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <stdexcept>
#include <filesystem>

#ifndef _NTDEF_
#define _NTDEF_
typedef LONG NTSTATUS;
typedef NTSTATUS *PNTSTATUS;
#endif

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif

typedef struct _MY_CLIENT_ID {
    HANDLE UniqueProcess;
    HANDLE UniqueThread;
} MY_CLIENT_ID, *PMY_CLIENT_ID;

struct SyscallTable {
    uint32_t NtOpenProcess           = 0;
    uint32_t NtReadVirtualMemory     = 0;
    uint32_t NtWriteVirtualMemory    = 0;
    uint32_t NtAllocateVirtualMemory = 0;
    uint32_t NtFreeVirtualMemory     = 0;
    uint32_t NtProtectVirtualMemory  = 0;
    uint32_t NtSuspendProcess        = 0;
    uint32_t NtResumeProcess         = 0;
    uint32_t NtQueryInformationProcess = 0;
    uint32_t NtClose                 = 0;
};

inline SyscallTable g_syscalls{};

class SyscallResolver {
public:
    static bool Initialize() {
        wchar_t sysDir[MAX_PATH];
        GetSystemDirectoryW(sysDir, MAX_PATH);
        std::wstring ntdllPath = std::wstring(sysDir) + L"\\ntdll.dll";

        HANDLE hFile = CreateFileW(ntdllPath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                    nullptr, OPEN_EXISTING, 0, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) return false;

        HANDLE hMapping = CreateFileMappingW(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (!hMapping) { CloseHandle(hFile); return false; }

        LPVOID fileBase = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
        if (!fileBase) { CloseHandle(hMapping); CloseHandle(hFile); return false; }

        bool result = ParseExports(fileBase);

        UnmapViewOfFile(fileBase);
        CloseHandle(hMapping);
        CloseHandle(hFile);

        return result;
    }

private:
    static bool ParseExports(LPVOID fileBase) {
        auto dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(fileBase);
        if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) return false;

        auto ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS64>(
            reinterpret_cast<uint8_t*>(fileBase) + dosHeader->e_lfanew);
        if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) return false;

        auto& exportDir = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        if (exportDir.VirtualAddress == 0) return false;

        auto rvaToOffset = [&](DWORD rva) -> uint8_t* {
            auto sections = IMAGE_FIRST_SECTION(ntHeaders);
            for (WORD i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++) {
                if (rva >= sections[i].VirtualAddress &&
                    rva < sections[i].VirtualAddress + sections[i].SizeOfRawData) {
                    return reinterpret_cast<uint8_t*>(fileBase) +
                           sections[i].PointerToRawData + (rva - sections[i].VirtualAddress);
                }
            }
            return nullptr;
        };

        auto exports = reinterpret_cast<PIMAGE_EXPORT_DIRECTORY>(rvaToOffset(exportDir.VirtualAddress));
        if (!exports) return false;

        auto names    = reinterpret_cast<DWORD*>(rvaToOffset(exports->AddressOfNames));
        auto funcs    = reinterpret_cast<DWORD*>(rvaToOffset(exports->AddressOfFunctions));
        auto ordinals = reinterpret_cast<WORD*>(rvaToOffset(exports->AddressOfNameOrdinals));

        std::unordered_map<std::string, uint32_t*> needed = {
            {"NtOpenProcess",              &g_syscalls.NtOpenProcess},
            {"NtReadVirtualMemory",        &g_syscalls.NtReadVirtualMemory},
            {"NtWriteVirtualMemory",       &g_syscalls.NtWriteVirtualMemory},
            {"NtAllocateVirtualMemory",    &g_syscalls.NtAllocateVirtualMemory},
            {"NtFreeVirtualMemory",        &g_syscalls.NtFreeVirtualMemory},
            {"NtProtectVirtualMemory",     &g_syscalls.NtProtectVirtualMemory},
            {"NtSuspendProcess",           &g_syscalls.NtSuspendProcess},
            {"NtResumeProcess",            &g_syscalls.NtResumeProcess},
            {"NtQueryInformationProcess",  &g_syscalls.NtQueryInformationProcess},
            {"NtClose",                    &g_syscalls.NtClose},
        };

        int found = 0;
        for (DWORD i = 0; i < exports->NumberOfNames; i++) {
            auto name = reinterpret_cast<const char*>(rvaToOffset(names[i]));
            if (!name) continue;

            auto it = needed.find(name);
            if (it == needed.end()) continue;

            auto funcBody = rvaToOffset(funcs[ordinals[i]]);
            if (!funcBody) continue;

            // mov r10, rcx; mov eax, SSN
            if (funcBody[0] == 0x4C && funcBody[1] == 0x8B && funcBody[2] == 0xD1 &&
                funcBody[3] == 0xB8) {
                uint32_t ssn = *reinterpret_cast<uint32_t*>(funcBody + 4);
                *(it->second) = ssn;
                found++;
            }

            if (found == static_cast<int>(needed.size())) break;
        }

        return found == static_cast<int>(needed.size());
    }
};

extern "C" {
    extern uint32_t currentSSN;
    NTSTATUS DoSyscall(void* a1, void* a2, void* a3, void* a4, void* a5, void* a6);
}

namespace syscall {
    inline NTSTATUS NtOpenProcess(PHANDLE ProcessHandle, ACCESS_MASK DesiredAccess, void* ObjectAttributes, void* ClientId) {
        currentSSN = g_syscalls.NtOpenProcess;
        return DoSyscall((void*)ProcessHandle, (void*)(uintptr_t)DesiredAccess, (void*)ObjectAttributes, (void*)ClientId, nullptr, nullptr);
    }

    inline NTSTATUS NtReadVirtualMemory(HANDLE ProcessHandle, PVOID BaseAddress, PVOID Buffer, SIZE_T NumberOfBytesToRead, PSIZE_T NumberOfBytesRead) {
        currentSSN = g_syscalls.NtReadVirtualMemory;
        return DoSyscall((void*)ProcessHandle, (void*)BaseAddress, (void*)Buffer, (void*)NumberOfBytesToRead, (void*)NumberOfBytesRead, nullptr);
    }

    inline NTSTATUS NtWriteVirtualMemory(HANDLE ProcessHandle, PVOID BaseAddress, PVOID Buffer, SIZE_T NumberOfBytesToWrite, PSIZE_T NumberOfBytesWritten) {
        currentSSN = g_syscalls.NtWriteVirtualMemory;
        return DoSyscall((void*)ProcessHandle, (void*)BaseAddress, (void*)Buffer, (void*)NumberOfBytesToWrite, (void*)NumberOfBytesWritten, nullptr);
    }

    inline NTSTATUS NtAllocateVirtualMemory(HANDLE ProcessHandle, PVOID* BaseAddress, ULONG_PTR ZeroBits, PSIZE_T RegionSize, ULONG AllocationType, ULONG Protect) {
        currentSSN = g_syscalls.NtAllocateVirtualMemory;
        return DoSyscall((void*)ProcessHandle, (void*)BaseAddress, (void*)ZeroBits, (void*)RegionSize, (void*)(uintptr_t)AllocationType, (void*)(uintptr_t)Protect);
    }

    inline NTSTATUS NtProtectVirtualMemory(HANDLE ProcessHandle, PVOID* BaseAddress, PSIZE_T RegionSize, ULONG NewProtect, PULONG OldProtect) {
        currentSSN = g_syscalls.NtProtectVirtualMemory;
        return DoSyscall((void*)ProcessHandle, (void*)BaseAddress, (void*)RegionSize, (void*)(uintptr_t)NewProtect, (void*)OldProtect, nullptr);
    }

    inline NTSTATUS NtSuspendProcess(HANDLE ProcessHandle) {
        currentSSN = g_syscalls.NtSuspendProcess;
        return DoSyscall((void*)ProcessHandle, nullptr, nullptr, nullptr, nullptr, nullptr);
    }

    inline NTSTATUS NtResumeProcess(HANDLE ProcessHandle) {
        currentSSN = g_syscalls.NtResumeProcess;
        return DoSyscall((void*)ProcessHandle, nullptr, nullptr, nullptr, nullptr, nullptr);
    }

    inline NTSTATUS NtClose(HANDLE Handle) {
        currentSSN = g_syscalls.NtClose;
        return DoSyscall((void*)Handle, nullptr, nullptr, nullptr, nullptr, nullptr);
    }
}
