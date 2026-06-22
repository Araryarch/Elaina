#include "Injector.h"
#include "core/Log.h"
#include "instance/Offsets.h"
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
    auto psAddr = mem.Read<uint64_t>(scriptAddr + g_Offsets.ModuleScript_ProtectedString);
    if (!psAddr) return { Err::ProcessRead, "Failed to read ProtectedString pointer" };

    auto psSize = mem.Read<uint64_t>(*psAddr + g_Offsets.ProtectedString_Size);
    if (!psSize) return { Err::ProcessRead, "Failed to read ProtectedString size" };

    Log::Info("Found ProtectedString at 0x%llx (size %llu)", *psAddr, *psSize);
    return std::make_pair(*psAddr, *psSize);
}

bool Injector::OverwriteProtectedString(Memory& mem, uint64_t psAddr, uint64_t psSize, const std::vector<uint8_t>& data) {
    if (data.size() <= psSize) {
        auto sizeDelta = psSize - data.size();
        auto written = mem.WriteArray(psAddr, data.data(), (uint32_t)data.size());
        if (!written) {
            Log::Error("Failed to write RSB1 data to ProtectedString buffer");
            return false;
        }
        if (sizeDelta > 0) {
            std::vector<uint8_t> zeros(sizeDelta, 0);
            mem.WriteArray(psAddr + data.size(), zeros.data(), (uint32_t)sizeDelta);
        }
    } else {
        if (psAddr) {
            auto newBuf = mem.Allocate(data.size());
            if (!newBuf) {
                Log::Error("Failed to allocate larger RSB1 buffer in Roblox");
                return false;
            }
            mem.WriteArray(*newBuf, data.data(), (uint32_t)data.size());

            mem.Write<uint64_t>(psAddr + g_Offsets.ProtectedString_Data, *newBuf);
            mem.Write<uint64_t>(psAddr + g_Offsets.ProtectedString_Size, data.size());
        } else {
            Log::Error("Cannot resize ProtectedString (null buffer)");
            return false;
        }
    }

    Log::Info("Overwrote ProtectedString with %zu bytes of RSB1 data", data.size());
    return true;
}

bool Injector::TriggerRequire(Memory& mem) {
    Log::Info("Triggering require() on injected script...");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    return true;
}

bool Injector::Execute(Memory& mem, const std::vector<uint8_t>& rsb1Data) {
    auto findResult = mem.FindModuleScript("UnnamedScript");
    if (!findResult) {
        Log::Warn("No UnnamedScript found, trying generic script search");
        return false;
    }

    auto psResult = FindProtectedString(mem, *findResult);
    if (!psResult) {
        Log::Error("Injector: %s", psResult.error().c_str());
        return false;
    }

    auto [psAddr, psSize] = *psResult;
    if (!OverwriteProtectedString(mem, psAddr, psSize, rsb1Data))
        return false;

    if (!TriggerRequire(mem))
        return false;

    Log::Info("Injection successful!");
    return true;
}
