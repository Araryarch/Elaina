#pragma once
#include <Windows.h>
#include <cstdint>

namespace hooks
{
    struct HookInfo
    {
        const char* name;
        uintptr_t target;
        uintptr_t detour;
        uintptr_t* original;
        bool enabled;
    };

    bool Install(void* lua_state);
    bool Uninstall();

    // VMT hooking
    bool HookVMT(uintptr_t instance, uintptr_t* vtable_ptr);
    bool UnhookVMT(uintptr_t instance);

    // IAT hooking
    bool HookIAT(const char* module, const char* func, uintptr_t detour, uintptr_t* original);
    bool UnhookIAT(const char* module, const char* func);

    // Detour hooking
    bool InstallDetour(uintptr_t target, uintptr_t detour, uintptr_t* original);
    bool RemoveDetour(uintptr_t target);

    // Roblox-specific hooks
    struct RobloxHooks
    {
        uintptr_t original_lua_load;
        uintptr_t original_lua_pcall;
        uintptr_t original_lua_newthread;
        uintptr_t original_lua_resume;
    };

    RobloxHooks& Get();
}
