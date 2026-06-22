#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "core/Result.h"
#include "process/Memory.h"

class InstanceTree;

class Injector {
public:
    static bool FindString(Memory& mem, uint64_t start, uint64_t end, const std::string& pattern, uint64_t& result);
    static Result<std::pair<uint64_t, uint64_t>> FindProtectedString(Memory& mem, uint64_t scriptAddr);
    static bool OverwriteProtectedString(Memory& mem, uint64_t psAddr, uint64_t psSize, const std::vector<uint8_t>& data);
    static bool TriggerRequire(Memory& mem);
    static bool TriggerRequireShellcode(Memory& mem, InstanceTree& tree,
                                        uint64_t scriptAddr, const std::vector<uint8_t>& rsb1Data);
    static bool Execute(Memory& mem, InstanceTree& tree, const std::vector<uint8_t>& rsb1Data);
};
