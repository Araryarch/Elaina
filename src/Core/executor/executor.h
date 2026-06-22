#pragma once
#include <string>
#include <functional>
#include <cstdint>
#include "luavm.h"

namespace executor
{
    using ExecuteCallback = std::function<void(bool success, const std::string& result)>;

    struct Config
    {
        int target_identity = 8;   // Script identity level (8 = admin)
        bool auto_bypass = true;   // Auto-bypass protections
        std::string pipe_name = R"(\\.\pipe\ElainaExecutor)";
    };

    bool Initialize(const Config& cfg = Config());
    void SetVM(lua::vm* ctx);
    bool ExecuteScript(const std::string& source);
    bool ExecuteScriptAsync(const std::string& source, ExecuteCallback cb);

    lua::t_lua_load LuaLoad();
    lua::t_lua_pcall LuaPcall();
    lua::vm* GetVM();
    Config& GetConfig();
}
