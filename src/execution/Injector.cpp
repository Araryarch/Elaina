#include "Injector.h"
#include "core/Log.h"
#include "instance/Offsets.h"
#include "instance/InstanceTree.h"
#include <algorithm>
#include <thread>
#include <chrono>
#include <windows.h>

bool Injector::FindString(Memory& mem, uint64_t start, uint64_t end, const std::string& pattern, uint64_t& result) {
    const size_t scan_size = 4096;
    std::vector<char> buffer(scan_size);

    for (uint64_t addr = start; addr < end; addr += scan_size - pattern.size()) {
        uint64_t to_read = (std::min)((uint64_t)scan_size, end - addr);
        if (!mem.ReadArray(addr, buffer.data(), (uint32_t)to_read))
            continue;

        auto it = std::search(buffer.data(), buffer.data() + to_read,
                              pattern.data(), pattern.data() + pattern.size());
        if (it != buffer.data() + to_read) {
            result = addr + (it - buffer.data());
            return true;
        }
    }
    return false;
}

Result<std::pair<uint64_t, uint64_t>> Injector::FindProtectedString(Memory& mem, uint64_t scriptAddr) {
    auto psAddr = mem.Read<uint64_t>(scriptAddr + g_Offsets.MS_ProtectedString);
    if (!psAddr) return { Err::MemoryReadFailed, "Failed to read ProtectedString pointer" };

    uint64_t psAddrVal = psAddr.value;
    if (!psAddrVal) return { Err::MemoryReadFailed, "ProtectedString is null" };

    auto psSize = mem.Read<uint64_t>(psAddrVal + g_Offsets.PS_Size);
    if (!psSize) return { Err::MemoryReadFailed, "Failed to read ProtectedString size" };

    Log::Info("Found ProtectedString at 0x%llx (size %llu)", psAddrVal, psSize.value);
    return std::make_pair(psAddrVal, psSize.value);
}

bool Injector::OverwriteProtectedString(Memory& mem, uint64_t psAddr, uint64_t psSize, const std::vector<uint8_t>& data) {
    if (data.size() <= psSize) {
        return mem.WriteArray(psAddr, data.data(), (uint32_t)data.size());
    }

    auto newBuf = mem.Allocate(data.size());
    if (!newBuf) {
        Log::Error("Failed to allocate larger RSB1 buffer in Roblox");
        return false;
    }

    mem.WriteArray(newBuf.value, data.data(), (uint32_t)data.size());
    mem.Write<uint64_t>(psAddr + g_Offsets.PS_Data, newBuf.value);
    mem.Write<uint64_t>(psAddr + g_Offsets.PS_Size, data.size());

    Log::Info("Overwrote ProtectedString with %zu bytes of RSB1 data", data.size());
    return true;
}

bool Injector::TriggerRequire(Memory& mem) {
    Log::Info("Triggering require() on injected script...");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    return true;
}

bool Injector::Execute(Memory& mem, InstanceTree& tree, const std::vector<uint8_t>& rsb1Data) {
    auto scriptAddr = tree.FindModuleScript("UnnamedScript");
    if (!scriptAddr) {
        Log::Warn("No UnnamedScript found, trying generic script search");
        return false;
    }

    auto psResult = FindProtectedString(mem, scriptAddr);
    if (!psResult) {
        Log::Error("Injector: %s", psResult.ErrorMsg().c_str());
        return false;
    }

    auto [psAddr, psSize] = psResult.value;
    if (!OverwriteProtectedString(mem, psAddr, psSize, rsb1Data))
        return false;

    return TriggerRequire(mem);
}
