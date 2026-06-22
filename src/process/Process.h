#pragma once
#include <Windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include "core/Result.h"

struct RobloxHandle {
    DWORD pid = 0;
    uintptr_t base = 0;
    HANDLE handle = nullptr;

    RobloxHandle() = default;
    RobloxHandle(RobloxHandle&& o) noexcept : pid(o.pid), base(o.base), handle(o.handle) { o.handle = nullptr; }
    RobloxHandle& operator=(RobloxHandle&& o) noexcept {
        if (this != &o) { if (handle) Syscall::CloseHandle(handle); pid = o.pid; base = o.base; handle = o.handle; o.handle = nullptr; }
        return *this;
    }
    RobloxHandle(const RobloxHandle&) = delete;
    RobloxHandle& operator=(const RobloxHandle&) = delete;
    ~RobloxHandle() { if (handle) { Syscall::CloseHandle(handle); handle = nullptr; } }

    bool Valid() const { return handle != nullptr && pid != 0; }
};

class Process {
public:
    static std::vector<DWORD> FindAll();

    static Result<RobloxHandle> Attach(DWORD pid);
    static Result<RobloxHandle> AttachFirst();

    static std::string ErrorName(DWORD pid);
    static bool IsRunning(DWORD pid);
};
