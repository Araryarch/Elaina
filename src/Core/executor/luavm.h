#pragma once
#include <Windows.h>
#include <cstdint>
#include <string>

struct lua_State;
struct Proto;

// Luau registry index constant (not in headers without Luau SDK)
#ifndef LUA_REGISTRYINDEX
#define LUA_REGISTRYINDEX (-1001000)
#endif
#ifndef LUA_GLOBALSINDEX
#define LUA_GLOBALSINDEX (-1001002)
#endif

namespace lua
{
    enum class VMType
    {
        Unknown,
        Luau,
        Lua51
    };

    struct vm
    {
        uintptr_t state;         // lua_State*
        uintptr_t globals;       // global table
        uintptr_t mainthread;    // main thread
        uintptr_t base;          // Roblox base address
        VMType type;
    };

    using t_lua_load = int(*)(lua_State* L, void* reader, void* data, const char* chunkname, int mode);
    using t_lua_pcall = int(*)(lua_State* L, int nargs, int nresults, int msgh);
    using t_lua_toxstring = const char*(*)(lua_State* L, int idx, size_t* len);
    using t_lua_pushstring = void(*)(lua_State* L, const char* s);
    using t_lua_pushlstring = void(*)(lua_State* L, const char* s, size_t len);
    using t_lua_pushcfunction = void(*)(lua_State* L, int(*fn)(lua_State*));
    using t_lua_setfield = void(*)(lua_State* L, int idx, const char* k);
    using t_lua_getfield = int(*)(lua_State* L, int idx, const char* k);
    using t_lua_gettop = int(*)(lua_State* L);
    using t_lua_settop = void(*)(lua_State* L, int idx);
    using t_lua_createtable = void(*)(lua_State* L, int narr, int nrec);
    using t_lua_newtable = void(*)(lua_State* L);
    using t_lua_checkstack = int(*)(lua_State* L, int extra);
    using t_lua_pushvalue = void(*)(lua_State* L, int idx);
    using t_lua_rawseti = void(*)(lua_State* L, int idx, int n);
    using t_lua_rawgeti = int(*)(lua_State* L, int idx, int n);
    using t_lua_setreadonly = void(*)(lua_State* L, int idx, int ro);
    using t_lua_getinfo = int(*)(lua_State* L, const char* what, void* ar);

    using t_luaL_loadstring = int(*)(lua_State* L, const char* s);
    using t_luaL_loadfile = int(*)(lua_State* L, const char* fn);

    bool FindVM(vm* ctx);
    bool Execute(lua_State* L, const std::string& source);
    bool ExecuteBytecode(lua_State* L, const char* bytecode, size_t size);

    int GetIdentity(lua_State* L);
    bool SetIdentity(lua_State* L, int identity);

    namespace offsets
    {
        struct LuaState
        {
            uintptr_t globals;
            uintptr_t namecall;
            uintptr_t identity;
        };

        bool Resolve();
        extern LuaState state;
    }
}
