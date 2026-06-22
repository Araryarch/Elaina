#include "Compiler.h"
#include "core/Log.h"
#include "Luau/Compiler.h"
#include "Luau/BytecodeBuilder.h"
#include "Luau/BytecodeUtils.h"
#include <algorithm>

class OpcodeEncoder : public Luau::BytecodeEncoder {
    void encode(uint32_t* data, size_t count) override {
        for (auto i = 0u; i < count;) {
            auto& op = *reinterpret_cast<uint8_t*>(data + i);
            i += Luau::getOpLength(LuauOpcode(op));
            op *= 227;
        }
    }
};

Result<std::string> Compiler::Compile(const std::string& source) {
    OpcodeEncoder enc;
    std::string bc = Luau::compile(source, {}, {}, &enc);

    if (bc.empty()) return { Err::CompileFailed, "Empty bytecode" };

    if (bc[0] == '\0') {
        bc.erase(std::remove(bc.begin(), bc.end(), '\0'), bc.end());
        return { Err::CompileFailed, bc.empty() ? "Unknown compile error" : bc };
    }

    Log::Info("Compiled %zu bytes -> %zu bytes bytecode", source.size(), bc.size());
    return bc;
}
