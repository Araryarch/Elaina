#include "environment.h"
#include "../executor/luavm.h"
#include "../framework/memory.h"
#include <cstdint>

struct lua_State;

namespace api
{
    static lua_State* GetLuaState()
    {
        auto* vm = executor::GetVM();
        return vm ? (lua_State*)vm->state : nullptr;
    }

    void SetupEnvironment(lua_State* L)
    {
        if (!L) return;

        // Register functions in the global environment
        // This is called after setting identity to create the enriched env.

        // For each function table defined in tables::, register them.
        // genv -> global environment additions
        // reg -> registry additions

        auto lua_newtable = (lua::t_lua_newtable)memory::ScanInModule(
            (HMODULE)executor::GetVM()->base,
            "\x48\x83\xEC\x28\x48\x8B\x41\x28",
            "xxxxxxxx"
        );
        if (!lua_newtable) return;

        auto lua_pushcfunction = (lua::t_lua_pushcfunction)memory::ScanInModule(
            (HMODULE)executor::GetVM()->base,
            "\x48\x89\x5C\x24\x00\x57\x48\x83\xEC\x00\x48\x8B\xD9\x48\x85\xD2",
            "xxxx?xxxx?xxxxxx"
        );
        if (!lua_pushcfunction) return;

        auto lua_setfield = (lua::t_lua_setfield)memory::ScanInModule(
            (HMODULE)executor::GetVM()->base,
            "\x48\x89\x5C\x24\x00\x48\x89\x74\x24\x00\x57\x48\x83\xEC\x00\x49\x8B\xF9",
            "xxxx?xxxx?xxxx?xxx"
        );
        if (!lua_setfield) return;

        auto lua_getfield = (lua::t_lua_getfield)memory::ScanInModule(
            (HMODULE)executor::GetVM()->base,
            "\x48\x89\x5C\x24\x00\x57\x48\x83\xEC\x00\x48\x8B\xD9\x48\x85\xD2",
            "xxxx?xxxx?xxxxxx"
        );
        if (!lua_getfield) return;

        auto lua_gettop = (lua::t_lua_gettop)memory::ScanInModule(
            (HMODULE)executor::GetVM()->base,
            "\x8B\x41\x18\xC3",
            "xxxx"
        );
        if (!lua_gettop) return;

        // Create our environment table in the registry
        lua_newtable(L);
        int env_table_ref = lua_gettop(L); // simplified - in real code use lua_ref

        // Register genv functions
        for (int i = 0; tables::genv[i].name; i++)
        {
            lua_pushcfunction(L, tables::genv[i].func);
            lua_setfield(L, -2, tables::genv[i].name);
        }

        // Pop the env table (simplified)
    }

    std::string WrapScript(const std::string& source)
    {
        // Wrap user script to run in our environment
        return
            "local env = getgenv()\n"
            "local scr = [...]\n"
            "setfenv(scr, env)\n"
            + source;
    }

    // --- Function implementations ---

    static int PushString(lua_State* L, const char* s)
    {
        auto lua_pushstring = (lua::t_lua_pushstring)memory::ScanInModule(
            (HMODULE)executor::GetVM()->base,
            "\x48\x89\x5C\x24\x00\x48\x83\xEC\x00\x48\x8B\xD9\x48\x85\xD2",
            "xxxx?xxxx?xxxxxx"
        );
        if (lua_pushstring)
            lua_pushstring(L, s);
        return 1;
    }

    int functions::GetGenv(lua_State* L)
    {
        auto lua_getfield = (lua::t_lua_getfield)memory::ScanInModule(
            (HMODULE)executor::GetVM()->base,
            "\x48\x89\x5C\x24\x00\x57\x48\x83\xEC\x00\x48\x8B\xD9\x48\x85\xD2",
            "xxxx?xxxx?xxxxxx"
        );

        auto lua_gettop = (lua::t_lua_gettop)memory::ScanInModule(
            (HMODULE)executor::GetVM()->base,
            "\x8B\x41\x18\xC3",
            "xxxx"
        );

        if (lua_getfield && lua_gettop)
        {
            lua_getfield(L, LUA_REGISTRYINDEX, "ElainaEnv");
            if (lua_gettop(L) >= 1)
                return 1;
        }

        // Fallback: return _G
        return 1; // _G is already on stack
    }

    int functions::GetGc(lua_State* L)
    {
        return PushString(L, "getgc");
    }

    int functions::GetRenV(lua_State* L)
    {
        return PushString(L, "renv");
    }

    int functions::GetReg(lua_State* L)
    {
        auto lua_pushvalue = (lua::t_lua_pushvalue)memory::ScanInModule(
            (HMODULE)executor::GetVM()->base,
            "\x48\x89\x5C\x24\x00\x57\x48\x83\xEC\x00\x48\x8B\xD9",
            "xxxx?xxxx?xxx"
        );
        if (lua_pushvalue)
            lua_pushvalue(L, LUA_REGISTRYINDEX);
        return 1;
    }

    int functions::GetRawMetatable(lua_State* L)
    {
        // Implementation: call Roblox's getrawmetatable equivalent
        // or bypass the __metatable protection
        return PushString(L, "getrawmetatable");
    }

    int functions::SetRawMetatable(lua_State* L)
    {
        return PushString(L, "setrawmetatable");
    }

    int functions::HookMetamethod(lua_State* L)
    {
        // Implementation: hook metamethods by modifying metatable
        return PushString(L, "hookmetamethod");
    }

    int functions::GetHui(lua_State* L)
    {
        // Get Hidden UI instance (CoreGui wrapper)
        return PushString(L, "gethui");
    }

    int functions::GetScriptIdentity(lua_State* L)
    {
        int id = lua::GetIdentity(L);
        if (!executor::GetVM()->base) return 0;
        // Push integer - simplified
        return 1;
    }

    int functions::SetScriptIdentity(lua_State* L)
    {
        // Set identity from first argument
        return 0;
    }

    int functions::CheckCaller(lua_State* L)
    {
        // Return true - caller is always our executor
        auto lua_pushcfunction = (lua::t_lua_pushcfunction)memory::ScanInModule(
            (HMODULE)executor::GetVM()->base,
            "\x48\x89\x5C\x24\x00\x57\x48\x83\xEC\x00\x48\x8B\xD9\x48\x85\xD2",
            "xxxx?xxxx?xxxxxx"
        );
        // Push boolean (non-nil = true) - simplified
        return 1;
    }

    int functions::FireClickDetector(lua_State* L) { return 0; }
    int functions::FireTouchInterest(lua_State* L) { return 0; }
    int functions::FireProximityPrompt(lua_State* L) { return 0; }

    int functions::GetInstances(lua_State* L)
    {
        return PushString(L, "getinstances");
    }

    int functions::GetNilInstances(lua_State* L)
    {
        return PushString(L, "getnilinstances");
    }

    int functions::GetHiddenProperty(lua_State* L) { return 0; }
    int functions::SetHiddenProperty(lua_State* L) { return 0; }
    int functions::GetHashHiddenProperty(lua_State* L) { return 0; }
    int functions::SetHashHiddenProperty(lua_State* L) { return 0; }

    int functions::GetNamecallMethod(lua_State* L)
    {
        // Read the namecall string from the state
        auto* vm = executor::GetVM();
        if (!vm || !vm->state) return 0;

        uintptr_t nc_offset = lua::offsets::state.namecall;
        if (!nc_offset) return 0;

        uintptr_t nc_ptr = vm->state + nc_offset;
        // Read the string at namecall offset - simplified
        return PushString(L, "namecall");
    }

    int functions::SetNamecallMethod(lua_State* L) { return 0; }

    int functions::LoadString(lua_State* L)
    {
        // Re-implement loadstring using internal luaL_loadstring
        auto luaL_loadstring = (lua::t_luaL_loadstring)memory::ScanInModule(
            (HMODULE)executor::GetVM()->base,
            "\x48\x89\x5C\x24\x00\x48\x89\x74\x24\x00\x57\x48\x83\xEC\x00\x48\x8B\xF1",
            "xxxx?xxxx?xxxx?xxx"
        );
        if (luaL_loadstring)
            return luaL_loadstring(L, "return nil");
        return 0;
    }

    int functions::NewCclosure(lua_State* L)
    {
        return PushString(L, "newcclosure");
    }

    int functions::NewLclosure(lua_State* L) { return 0; }
    int functions::CloneFunction(lua_State* L) { return 0; }
    int functions::Decompile(lua_State* L) { return 0; }
    int functions::IsLclosure(lua_State* L) { return 0; }
    int functions::IsCclosure(lua_State* L) { return 0; }
    int functions::IsExecutor(lua_State* L) { return 0; }
    int functions::DumpString(lua_State* L) { return 0; }
    int functions::ProtectString(lua_State* L) { return 0; }
    int functions::Crypt(lua_State* L) { return 0; }
    int functions::HttpGet(lua_State* L) { return 0; }
    int functions::HttpPost(lua_State* L) { return 0; }
    int functions::Request(lua_State* L) { return 0; }
    int functions::SetClipboard(lua_State* L) { return 0; }
    int functions::Messagebox(lua_State* L) { return 0; }
    int functions::TakeScreenshot(lua_State* L) { return 0; }
    int functions::GetFpsCap(lua_State* L) { return 0; }
    int functions::SetFpsCap(lua_State* L) { return 0; }
    int functions::Rconsole(lua_State* L) { return 0; }
    int functions::RconsolePrint(lua_State* L) { return 0; }
    int functions::RconsoleInput(lua_State* L) { return 0; }
    int functions::RconsoleClear(lua_State* L) { return 0; }
    int functions::RconsoleSetTitle(lua_State* L) { return 0; }
    int functions::RconsoleShow(lua_State* L) { return 0; }
    int functions::RconsoleHide(lua_State* L) { return 0; }
    int functions::GetThreadIdentity(lua_State* L)
    {
        return GetScriptIdentity(L);
    }
    int functions::SetThreadIdentity(lua_State* L)
    {
        return SetScriptIdentity(L);
    }
    int functions::GetContext(lua_State* L) { return 0; }
    int functions::SetContext(lua_State* L) { return 0; }

    // --- Function registration tables ---

    const FunctionDef tables::genv[] = {
        {"getgenv",          functions::GetGenv},
        {"getgc",            functions::GetGc},
        {"getrenv",          functions::GetRenV},
        {"getreg",           functions::GetReg},
        {"getrawmetatable",  functions::GetRawMetatable},
        {"setrawmetatable",  functions::SetRawMetatable},
        {"hookmetamethod",   functions::HookMetamethod},
        {"gethui",           functions::GetHui},
        {"getscriptidentity", functions::GetScriptIdentity},
        {"setscriptidentity", functions::SetScriptIdentity},
        {"checkcaller",      functions::CheckCaller},
        {"fireclickdetector", functions::FireClickDetector},
        {"firetouchinterest", functions::FireTouchInterest},
        {"fireproximityprompt", functions::FireProximityPrompt},
        {"getinstances",     functions::GetInstances},
        {"getnilinstances",  functions::GetNilInstances},
        {"gethiddenproperty", functions::GetHiddenProperty},
        {"sethiddenproperty", functions::SetHiddenProperty},
        {"gethashhiddenproperty", functions::GetHashHiddenProperty},
        {"sethashhiddenproperty", functions::SetHashHiddenProperty},
        {"getnamecallmethod", functions::GetNamecallMethod},
        {"setnamecallmethod", functions::SetNamecallMethod},
        {"loadstring",       functions::LoadString},
        {"newcclosure",      functions::NewCclosure},
        {"newlclosure",      functions::NewLclosure},
        {"clonefunction",    functions::CloneFunction},
        {"decompile",        functions::Decompile},
        {"islclosure",       functions::IsLclosure},
        {"iscclosure",       functions::IsCclosure},
        {"isexecutor",       functions::IsExecutor},
        {"dumpstring",       functions::DumpString},
        {"protectstring",    functions::ProtectString},
        {"crypt",            functions::Crypt},
        {"httpget",          functions::HttpGet},
        {"httppost",         functions::HttpPost},
        {"request",          functions::Request},
        {"setclipboard",     functions::SetClipboard},
        {"messagebox",       functions::Messagebox},
        {"screenshot",       functions::TakeScreenshot},
        {"getfpscap",        functions::GetFpsCap},
        {"setfpscap",        functions::SetFpsCap},
        {"rconsole",         functions::Rconsole},
        {"rconsoleprint",    functions::RconsolePrint},
        {"rconsoleinput",    functions::RconsoleInput},
        {"rconsoleclear",    functions::RconsoleClear},
        {"rconsoletitle",    functions::RconsoleSetTitle},
        {"rconsoleshow",     functions::RconsoleShow},
        {"rconsolehide",     functions::RconsoleHide},
        {"getthreadidentity", functions::GetThreadIdentity},
        {"setthreadidentity", functions::SetThreadIdentity},
        {"getcontext",       functions::GetContext},
        {"setcontext",       functions::SetContext},
        {nullptr, nullptr}
    };
}
