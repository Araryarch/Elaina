#include "hooks.h"
#include "../framework/memory.h"
#include "../executor/luavm.h"
#include "../executor/executor.h"
#include <cstring>

namespace hooks
{
    static RobloxHooks g_hooks = {};
    static bool g_installed = false;

    // Detour trampoline storage
    static constexpr size_t MAX_HOOKS = 64;
    static HookInfo g_hook_list[MAX_HOOKS] = {};
    static int g_hook_count = 0;

    static uintptr_t AllocateTrampoline(uintptr_t target, size_t min_size = 14)
    {
        // Allocate executable memory for a trampoline (jump back to original)
        uintptr_t tramp = (uintptr_t)VirtualAlloc(nullptr, min_size + 5,
            MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

        if (!tramp) return 0;

        // Copy original bytes
        memory::ReadRaw(target, (void*)tramp, min_size);

        // Write JMP back to original+offset (overwritten bytes past the hook)
        uintptr_t ret_addr = target + min_size;
        uintptr_t jmp_slot = tramp + min_size;
        memory::Write<uint8_t>(jmp_slot, 0xE9); // JMP
        memory::Write<uint32_t>(jmp_slot + 1, (uint32_t)(ret_addr - (jmp_slot + 5)));

        return tramp;
    }

    bool InstallDetour(uintptr_t target, uintptr_t detour, uintptr_t* original)
    {
        if (!target || !detour) return false;

        size_t hook_size = 14; // jmp [rip+addr] in x64 = 14 bytes

        // Allocate and write trampoline
        uintptr_t tramp = AllocateTrampoline(target, hook_size);
        if (!tramp) return false;

        if (original)
            *original = tramp;

        // Write JMP at target: jmp [rip+0] -> detour
        DWORD old;
        VirtualProtect((LPVOID)target, hook_size, PAGE_EXECUTE_READWRITE, &old);

        memory::Write<uint16_t>(target, 0x25FF); // JMP [rip+...] in x64
        memory::Write<int32_t>(target + 2, 0);    // offset = 0 (next instruction)
        memory::Write<uintptr_t>(target + 6, detour); // absolute address
        memory::Write<uint8_t>(target + 14, 0x90); // NOP padding

        // Fill rest with NOPs up to hook_size
        for (size_t i = 15; i < hook_size; i++)
            memory::Write<uint8_t>(target + i, 0x90);

        VirtualProtect((LPVOID)target, hook_size, old, &old);

        return true;
    }

    bool RemoveDetour(uintptr_t target)
    {
        // Restore original bytes from trampoline
        // (impractical without saving them - in real code, restore from backup)
        return false;
    }

    // --- Hook detours ---

    static int __fastcall DetourLuaLoad(lua_State* L, void* reader, void* data,
        const char* chunkname, int mode)
    {
        auto original = (lua::t_lua_load)g_hooks.original_lua_load;
        if (!original) return -1;

        // Bypass chunkname restrictions
        if (!chunkname)
            chunkname = "=Elaina";

        return original(L, reader, data, chunkname, mode);
    }

    static int __fastcall DetourLuaPcall(lua_State* L, int nargs, int nresults, int msgh)
    {
        auto original = (lua::t_lua_pcall)g_hooks.original_lua_pcall;
        if (!original) return -1;

        // Ensure executor scripts always have high identity
        // (if this call originates from our executor context)
        if (executor::GetConfig().auto_bypass)
            lua::SetIdentity(L, executor::GetConfig().target_identity);

        return original(L, nargs, nresults, msgh);
    }

    bool Install(void* lua_state)
    {
        if (g_installed) return true;

        uintptr_t base = memory::GetRobloxBase();
        if (!base) return false;

        // Save original functions
        g_hooks.original_lua_load = (uintptr_t)executor::LuaLoad();
        g_hooks.original_lua_pcall = (uintptr_t)executor::LuaPcall();

        // Install detours
        if (g_hooks.original_lua_load)
        {
            InstallDetour(g_hooks.original_lua_load,
                (uintptr_t)DetourLuaLoad, nullptr);
        }

        if (g_hooks.original_lua_pcall)
        {
            InstallDetour(g_hooks.original_lua_pcall,
                (uintptr_t)DetourLuaPcall, nullptr);
        }

        g_installed = true;
        return true;
    }

    bool Uninstall()
    {
        if (!g_installed) return true;

        // Restore hooks (in reverse order)
        for (int i = g_hook_count - 1; i >= 0; i--)
        {
            RemoveDetour(g_hook_list[i].target);
        }

        g_installed = false;
        return true;
    }

    RobloxHooks& Get()
    {
        return g_hooks;
    }
}
