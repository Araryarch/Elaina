#pragma once
#include <Windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include <utility>
#include "core/Result.h"
#include "syscall/Syscall.h"

struct SectionInfo {
    uintptr_t start = 0;
    size_t size = 0;
};

struct PatternByte {
    uint8_t data;
    bool wildcard;
};

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

    // PE / section helpers
    SectionInfo GetSection(const char* name) const;
    uintptr_t GetModuleBase() const { return m_base; }

    // Pattern scanning
    static std::vector<PatternByte> ParsePattern(const std::string& pattern);
    uintptr_t ScanPattern(uintptr_t start, size_t size, const std::vector<PatternByte>& pattern) const;
    uintptr_t ScanSection(const char* section, const std::string& patternStr) const;

    // Handle access
    HANDLE GetProcessHandle() const { return m_process; }

    // Remote thread creation helper
    Result<HANDLE> CreateRemoteThread(void* startAddr, void* param) const;

private:
    HANDLE m_process;
    uintptr_t m_base{0};
};
