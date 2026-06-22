#include "executor.h"
#include "../framework/memory.h"
#include "../api/environment.h"
#include <thread>
#include <mutex>

struct lua_State;

namespace executor
{
    static Config g_config;
    static lua::vm g_vm = {};
    static bool g_initialized = false;
    static std::mutex g_exec_mutex;

    bool Initialize(const Config& cfg)
    {
        g_config = cfg;

        if (!lua::FindVM(&g_vm))
            return false;

        if (cfg.auto_bypass)
        {
            lua::SetIdentity((lua_State*)g_vm.state, cfg.target_identity);
        }

        api::SetupEnvironment((lua_State*)g_vm.state);

        g_initialized = true;
        return true;
    }

    void SetVM(lua::vm* ctx)
    {
        g_vm = *ctx;
    }

    bool ExecuteScript(const std::string& source)
    {
        if (!g_initialized || !g_vm.state)
            return false;

        std::lock_guard<std::mutex> lock(g_exec_mutex);

        if (g_config.auto_bypass)
            lua::SetIdentity((lua_State*)g_vm.state, g_config.target_identity);

        std::string wrapped = api::WrapScript(source);
        return lua::Execute((lua_State*)g_vm.state, wrapped);
    }

    void ExecuteScriptAsync(const std::string& source, ExecuteCallback cb)
    {
        std::thread([source, cb]()
        {
            bool success = ExecuteScript(source);
            if (cb)
                cb(success, success ? "Executed successfully" : "Execution failed");
        }).detach();
    }

    lua::t_lua_load LuaLoad()
    {
        uintptr_t base = memory::GetRobloxBase();
        if (!base) return nullptr;

        return (lua::t_lua_load)memory::ScanInModule(
            (HMODULE)base,
            "\x48\x89\x5C\x24\x00\x48\x89\x74\x24\x00\x57",
            "xxxx?xxxx?x"
        );
    }

    lua::t_lua_pcall LuaPcall()
    {
        uintptr_t base = memory::GetRobloxBase();
        if (!base) return nullptr;

        return (lua::t_lua_pcall)memory::ScanInModule(
            (HMODULE)base,
            "\x48\x89\x5C\x24\x00\x57\x48\x83\xEC\x00\x48\x8B\xD9\x48\x85\xD2",
            "xxxx?xxxx?xxxxxx"
        );
    }

    lua::vm* GetVM()
    {
        return &g_vm;
    }

    Config& GetConfig()
    {
        return g_config;
    }
}
