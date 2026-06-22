#pragma once
#include <Windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include "core/Result.h"
#include "syscall/Syscall.h"

class Memory {
public:
    Memory(HANDLE hProcess, uintptr_t baseAddr = 0);

    void SetBaseAddr(uintptr_t base) { m_base = base; }
    uintptr_t BaseAddr() const { return m_base; }

    template<typename T>
    Result<T> Read(uintptr_t addr) const {
        T val{};
        auto r = ReadRaw(addr, &val, sizeof(T));
        if (!r) return { r.error, r.message };
        return val;
    }

    template<typename T>
    Result<void> Write(uintptr_t addr, const T& val) {
        return WriteRaw(addr, &val, sizeof(T));
    }

    bool ReadArray(uintptr_t addr, void* buf, uint32_t count) const;
    bool WriteArray(uintptr_t addr, const void* buf, uint32_t count);

    Result<void> ReadRaw(uintptr_t addr, void* buf, size_t size) const;
    Result<void> WriteRaw(uintptr_t addr, const void* buf, size_t size);

    Result<std::string> ReadString(uintptr_t addr) const;

    Result<uintptr_t> Allocate(size_t size, uint32_t protect = PAGE_READWRITE);

private:
    HANDLE m_process;
    uintptr_t m_base{0};
};
