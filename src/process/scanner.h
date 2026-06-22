#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <iostream>
#include "syscalls/syscalls.h"

struct RobloxProcess {
    DWORD pid = 0;
    uintptr_t baseAddress = 0;
    HANDLE handle = nullptr;
    int state = 0;

    RobloxProcess() = default;

    RobloxProcess(RobloxProcess&& other) noexcept
        : pid(other.pid), baseAddress(other.baseAddress),
          handle(other.handle), state(other.state) {
        other.handle = nullptr;
    }

    RobloxProcess& operator=(RobloxProcess&& other) noexcept {
        if (this != &other) {
            if (handle) syscall::NtClose(handle);
            pid = other.pid;
            baseAddress = other.baseAddress;
            handle = other.handle;
            state = other.state;
            other.handle = nullptr;
        }
        return *this;
    }

    RobloxProcess(const RobloxProcess&) = delete;
    RobloxProcess& operator=(const RobloxProcess&) = delete;

    ~RobloxProcess() {
        if (handle) {
            syscall::NtClose(handle);
            handle = nullptr;
        }
    }
};

class ProcessScanner {
public:
    static std::vector<DWORD> FindRobloxProcesses() {
        std::vector<DWORD> pids;
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE) return pids;

        PROCESSENTRY32W pe{};
        pe.dwSize = sizeof(pe);

        if (Process32FirstW(snapshot, &pe)) {
            do {
                std::wstring name(pe.szExeFile);
                if (name == L"RobloxPlayerBeta.exe" || name == L"Windows10Universal.exe") {
                    pids.push_back(pe.th32ProcessID);
                }
            } while (Process32NextW(snapshot, &pe));
        }

        CloseHandle(snapshot);
        return pids;
    }

    static uintptr_t GetBaseAddress(DWORD pid) {
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        if (snapshot == INVALID_HANDLE_VALUE) return 0;

        MODULEENTRY32W me{};
        me.dwSize = sizeof(me);

        uintptr_t base = 0;
        if (Module32FirstW(snapshot, &me)) {
            base = reinterpret_cast<uintptr_t>(me.modBaseAddr);
        }

        CloseHandle(snapshot);
        return base;
    }

    static HANDLE OpenProcess(DWORD pid) {
        HANDLE hProcess = nullptr;

        struct NT_OBJECT_ATTRIBUTES {
            ULONG Length;
            HANDLE RootDirectory;
            void* ObjectName;
            ULONG Attributes;
            void* SecurityDescriptor;
            void* SecurityQualityOfService;
        };

        struct NT_CLIENT_ID {
            HANDLE UniqueProcess;
            HANDLE UniqueThread;
        };

        NT_OBJECT_ATTRIBUTES oa{};
        oa.Length = sizeof(NT_OBJECT_ATTRIBUTES);

        NT_CLIENT_ID cid{};
        cid.UniqueProcess = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(pid));
        cid.UniqueThread = nullptr;

        NTSTATUS status = syscall::NtOpenProcess(
            &hProcess,
            PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION,
            &oa, &cid
        );

        if (status != 0) return nullptr;
        return hProcess;
    }

    static bool ReadMemory(HANDLE hProcess, uintptr_t address, void* buffer, size_t size) {
        SIZE_T bytesRead = 0;
        NTSTATUS status = syscall::NtReadVirtualMemory(
            hProcess, reinterpret_cast<PVOID>(address), buffer, size, &bytesRead);
        return status == 0 && bytesRead == size;
    }

    static bool WriteMemory(HANDLE hProcess, uintptr_t address, const void* buffer, size_t size) {
        SIZE_T bytesWritten = 0;
        NTSTATUS status = syscall::NtWriteVirtualMemory(
            hProcess, reinterpret_cast<PVOID>(address),
            const_cast<PVOID>(static_cast<const void*>(buffer)), size, &bytesWritten);
        return status == 0 && bytesWritten == size;
    }

    template<typename T>
    static T Read(HANDLE hProcess, uintptr_t address) {
        T value{};
        ReadMemory(hProcess, address, &value, sizeof(T));
        return value;
    }

    static std::string ReadString(HANDLE hProcess, uintptr_t address) {
        size_t length = Read<size_t>(hProcess, address + 0x10);
        size_t capacity = Read<size_t>(hProcess, address + 0x18);
        if (length == 0 || length > 4096) return "";

        std::string result(length, '\0');

        if (capacity > 15) {
            uintptr_t ptr = Read<uintptr_t>(hProcess, address);
            if (ptr == 0) return "";
            ReadMemory(hProcess, ptr, result.data(), length);
        } else {
            ReadMemory(hProcess, address, result.data(), length);
        }

        return result;
    }

    static std::optional<RobloxProcess> Connect(DWORD pid) {
        RobloxProcess rblx;
        rblx.pid = pid;
        rblx.baseAddress = GetBaseAddress(pid);

        if (rblx.baseAddress == 0) {
            std::cerr << "[-] Failed to get base address for PID " << pid << std::endl;
            return std::nullopt;
        }

        rblx.handle = OpenProcess(pid);
        if (!rblx.handle) return std::nullopt;

        rblx.state = 0;
        return rblx;
    }
};
