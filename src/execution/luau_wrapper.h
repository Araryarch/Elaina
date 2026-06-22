#pragma once
#include <string>
#include <iostream>
#include <algorithm>
#include "Luau/Compiler.h"
#include "Luau/BytecodeBuilder.h"
#include "Luau/BytecodeUtils.h"

class BytecodeEncoder : public Luau::BytecodeEncoder {
    inline void encode(uint32_t* data, size_t count) override {
        for (auto i = 0u; i < count;) {
            auto& opcode = *reinterpret_cast<uint8_t*>(data + i);
            i += Luau::getOpLength(LuauOpcode(opcode));
            opcode *= 227;
        }
    }
};

class LuauCompiler {
public:
    static std::pair<bool, std::string> Compile(const std::string& source) {
        BytecodeEncoder encoder{};
        const std::string bytecode = Luau::compile(source, {}, {}, &encoder);

        if (bytecode.empty()) return { false, "Compilation returned empty bytecode" };

        if (bytecode[0] == '\0') {
            std::string error_message = bytecode;
            error_message.erase(
                std::remove(error_message.begin(), error_message.end(), '\0'),
                error_message.end()
            );
            return { false, error_message };
        }

        return { true, bytecode };
    }
};
