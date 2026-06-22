#pragma once
#include <cstdint>
#include <vector>
#include <string>

namespace bytecode
{
    enum class Format
    {
        Luau,
        Lua51
    };

    struct Header
    {
        uint8_t signature[4];
        uint8_t format_version;
        uint8_t luau_version;
        uint8_t flags;
        uint8_t reserved;
    };

    bool Compile(const std::string& source, std::vector<uint8_t>& out, Format fmt = Format::Luau);
    bool Decompile(const uint8_t* bytecode, size_t size, std::string& out);
    bool IsValid(const uint8_t* data, size_t size);

    // Luau bytecode header constants
    namespace luau
    {
        constexpr uint8_t kSignature[] = { 0x1B, 0x4C, 0x75, 0x61 }; // ESC + "Lua"
        constexpr uint8_t kLuauVersion = 0x01;
        constexpr uint8_t kFormatVersion = 0x06;
    }
}
