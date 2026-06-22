#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "core/Result.h"

class Encoder {
public:
    static Result<std::string> Encode(const std::string& bytecode);
    static std::vector<uint8_t> EncodeVec(const std::vector<uint8_t>& bytecode);
};
