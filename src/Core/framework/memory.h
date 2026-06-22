#pragma once
#include <Windows.h>
#include <cstdint>
#include <vector>
#include <string>
#include <cstring>

namespace memory
{
    template <typename T>
    inline T Read(uintptr_t addr)
    {
        T val;
        ReadProcessMemory(GetCurrentProcess(), (LPCVOID)addr, &val, sizeof(T), nullptr);
        return val;
    }

    template <typename T>
    inline void Write(uintptr_t addr, T val)
    {
        WriteProcessMemory(GetCurrentProcess(), (LPVOID)addr, &val, sizeof(T), nullptr);
    }

    inline bool ReadRaw(uintptr_t addr, void* buf, size_t size)
    {
        return ReadProcessMemory(GetCurrentProcess(), (LPCVOID)addr, buf, size, nullptr) != 0;
    }

    inline bool WriteRaw(uintptr_t addr, void* buf, size_t size)
    {
        DWORD old;
        VirtualProtect((LPVOID)addr, size, PAGE_EXECUTE_READWRITE, &old);
        bool ret = WriteProcessMemory(GetCurrentProcess(), (LPVOID)addr, buf, size, nullptr) != 0;
        VirtualProtect((LPVOID)addr, size, old, &old);
        return ret;
    }

    inline uintptr_t Scan(const char* pattern, const char* mask)
    {
        SYSTEM_INFO sys;
        GetSystemInfo(&sys);

        uintptr_t start = (uintptr_t)sys.lpMinimumApplicationAddress;
        uintptr_t end = (uintptr_t)sys.lpMaximumApplicationAddress;

        MEMORY_BASIC_INFORMATION mbi;
        uintptr_t curr = start;

        while (curr < end)
        {
            if (VirtualQuery((LPCVOID)curr, &mbi, sizeof(mbi)))
            {
                if (mbi.State == MEM_COMMIT &&
                    (mbi.Protect & (PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)))
                {
                    std::vector<uint8_t> buffer(mbi.RegionSize);
                    if (ReadRaw(curr, buffer.data(), mbi.RegionSize))
                    {
                        for (size_t i = 0; i < mbi.RegionSize; i++)
                        {
                            bool found = true;
                            for (size_t j = 0; mask[j]; j++)
                            {
                                if (mask[j] == 'x' && buffer[i + j] != (uint8_t)pattern[j])
                                {
                                    found = false;
                                    break;
                                }
                            }
                            if (found)
                                return curr + i;
                        }
                    }
                }
                curr += mbi.RegionSize;
            }
            else
                break;
        }

        return 0;
    }

    inline uintptr_t ScanInModule(HMODULE mod, const char* pattern, const char* mask)
    {
        auto dos = (IMAGE_DOS_HEADER*)mod;
        auto nt = (IMAGE_NT_HEADERS*)((uintptr_t)mod + dos->e_lfanew);
        auto sec = IMAGE_FIRST_SECTION(nt);

        for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++)
        {
            if (memcmp(sec[i].Name, ".text", 5) == 0 ||
                memcmp(sec[i].Name, ".rdata", 6) == 0)
            {
                uintptr_t start = (uintptr_t)mod + sec[i].VirtualAddress;
                uintptr_t size = sec[i].SizeOfRawData;
                std::vector<uint8_t> buffer(size);

                if (ReadRaw(start, buffer.data(), size))
                {
                    for (uintptr_t j = 0; j < size; j++)
                    {
                        bool found = true;
                        for (size_t k = 0; mask[k]; k++)
                        {
                            if (mask[k] == 'x' && buffer[j + k] != (uint8_t)pattern[k])
                            {
                                found = false;
                                break;
                            }
                        }
                        if (found)
                            return start + j;
                    }
                }
            }
        }

        return 0;
    }

    inline uintptr_t GetModuleBase(const wchar_t* name)
    {
        return (uintptr_t)GetModuleHandleW(name);
    }

    inline uintptr_t GetRobloxBase()
    {
        return GetModuleBase(L"RobloxPlayerBeta.exe");
    }
}
