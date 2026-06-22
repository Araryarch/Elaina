#include "luavm.h"
#include "../framework/memory.h"
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>

namespace lua
{
    namespace offsets
    {
        LuaState state = {};

        bool Resolve()
        {
            uintptr_t base = memory::GetRobloxBase();
            if (!base) return false;

            // --- Luau 2025+ struct offsets ---
            // These change EVERY Roblox update. You must RE them live.
            // The values below are placeholders for the pattern search approach.

            // lua_State->gl_offs[offset]:
            // pattern: "48 8D 05 ?? ?? ?? ?? 48 89 03" lea rax, [lua_State.globals]
            uintptr_t gl_ref = memory::ScanInModule(
                (HMODULE)base,
                "\x48\x8D\x05\x00\x00\x00\x00\x48\x89\x03",
                "xxx????xxx"
            );
            if (gl_ref)
            {
                int32_t disp = memory::Read<int32_t>(gl_ref + 3);
                state.globals = (gl_ref + 7 + disp) - base;
            }

            // namecall offset (in TValue or LClosure):
            // pattern: "89 15 ?? ?? ?? ?? 48 8B 5C 24"
            uintptr_t nc_ref = memory::ScanInModule(
                (HMODULE)base,
                "\x89\x15\x00\x00\x00\x00\x48\x8B\x5C\x24",
                "xx????xxxx"
            );
            if (nc_ref)
            {
                int32_t disp = memory::Read<int32_t>(nc_ref + 2);
                state.namecall = (nc_ref + 6 + disp) - base;
            }

            // identity offset:
            // pattern: "8B 40 ?? 83 F8 01" mov eax, [rax+XX]; cmp eax, 1
            uintptr_t id_ref = memory::ScanInModule(
                (HMODULE)base,
                "\x8B\x40\x00\x83\xF8\x01",
                "xx?xxx"
            );
            if (id_ref)
                state.identity = memory::Read<uint8_t>(id_ref + 2);

            return (state.globals != 0);
        }
    }

    static uintptr_t FindLuaLoad()
    {
        uintptr_t base = memory::GetRobloxBase();
        if (!base) return 0;

        // Pattern for lua_load / luaL_loadbuffer:
        // "48 89 5C 24 ?? 48 89 74 24 ?? 57 48 83 EC ?? 48 8B FA 48 8B D9 48 85 D2"
        // This changes per update — needs live RE.
        return memory::ScanInModule(
            (HMODULE)base,
            "\x48\x89\x5C\x24\x00\x48\x89\x74\x24\x00\x57",
            "xxxx?xxxx?x"
        );
    }

    static uintptr_t FindPcall()
    {
        uintptr_t base = memory::GetRobloxBase();
        if (!base) return 0;

        return memory::ScanInModule(
            (HMODULE)base,
            "\x48\x89\x5C\x24\x00\x57\x48\x83\xEC\x00\x48\x8B\xD9\x48\x85\xD2",
            "xxxx?xxxx?xxxxxx"
        );
    }

    bool FindVM(vm* ctx)
    {
        ctx->base = memory::GetRobloxBase();
        if (!ctx->base) return false;

        // Find lua_State by scanning for the gl_ref pattern
        if (!offsets::Resolve())
            return false;

        // Find main thread: scan for references to lua_newthread return
        // For now: find a likely lua_State by scanning for a pointer chain
        // that leads to the global table.

        uintptr_t state_ref = memory::ScanInModule(
            (HMODULE)base,
            "\x48\x8D\x0D\x00\x00\x00\x00\xE8",
            "xxx????x"
        );

        if (state_ref)
        {
            int32_t disp = memory::Read<int32_t>(state_ref + 3);
            ctx->state = state_ref + 7 + disp;
        }
        else
        {
            ctx->state = memory::ScanInModule(
                (HMODULE)base,
                "\x48\x8B\x0D\x00\x00\x00\x00\x48\x8B\x01",
                "xxx????xxx"
            );
            if (!ctx->state) return false;
            int32_t disp = memory::Read<int32_t>(ctx->state + 3);
            ctx->state = ctx->state + 7 + disp;
            ctx->state = memory::Read<uintptr_t>(ctx->state);
        }

        ctx->mainthread = ctx->state;
        ctx->type = VMType::Luau;

        return ctx->state != 0;
    }

    bool Execute(lua_State* L, const std::string& source)
    {
        auto luaL_loadstring = (t_luaL_loadstring)FindLuaLoad();
        auto lua_pcall = (t_lua_pcall)FindPcall();

        if (!luaL_loadstring || !lua_pcall)
            return false;

        if (luaL_loadstring(L, source.c_str()) != 0)
            return false;

        return lua_pcall(L, 0, 0, 0) == 0;
    }

    bool ExecuteBytecode(lua_State* L, const char* bytecode, size_t size)
    {
        // For Luau: use the internal compiler or load pre-compiled bytecode
        // This needs access to the Luau compiler (lua_load with binary reader)
        // or directly write Proto* into the state.
        //
        // Simplified: use loadstring on the source instead.
        // True bytecode execution requires lua_load with a binary chunk reader.

        return false; // Stub - implement with real Luau compiler
    }

    int GetIdentity(lua_State* L)
    {
        if (!offsets::state.identity) return 0;
        return memory::Read<int>((uintptr_t)L + offsets::state.identity);
    }

    bool SetIdentity(lua_State* L, int identity)
    {
        if (!offsets::state.identity) return false;
        memory::Write<int>((uintptr_t)L + offsets::state.identity, identity);
        return true;
    }
}
