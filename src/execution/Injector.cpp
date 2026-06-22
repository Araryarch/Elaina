#include "Injector.h"
#include "core/Log.h"
#include "instance/Offsets.h"
#include "instance/InstanceTree.h"
#include <algorithm>
#include <thread>
#include <chrono>
#include <cstring>
#include <vector>
#include <windows.h>

// ── AOB signatures for Luau C API in RobloxPlayerBeta.exe ──
// NOTE: These change every Roblox update. Update via live RE.
// Pattern format: hex bytes with ?? for wildcards, space-separated.

static constexpr const char* kLuaPcallSig =
    "48 83 EC 38 48 85 D2 0F 84 ?? ?? ?? ?? 45 33 DB";

static constexpr const char* kLuaLoadSig =
    "48 89 5C 24 10 48 89 74 24 18 55 57 41 54 41 55 41 56 41 57 48 83 EC 50";

static constexpr const char* kLualLoadbufferSig =
    "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 30 4D 85 C0";

// ── Shellcode ─────────────────────────────────────────────
// Executed in Roblox via CreateRemoteThread.
// rcx = pointer to ShellcodeParams

#pragma pack(push, 1)
struct ShellcodeParams {
    uint64_t lua_state_ptr;
    uint64_t bytecode_addr;
    uint64_t bytecode_size;
    uint64_t loadbuffer_addr;
    uint64_t pcall_addr;
    char     script_name[8];
};
#pragma pack(pop)

static_assert(sizeof(ShellcodeParams) == 0x30, "ShellcodeParams size");

static const uint8_t kShellcode[] = {
    0x55,                         // push rbp
    0x48, 0x89, 0xE5,             // mov rbp, rsp
    0x57,                         // push rdi
    0x53,                         // push rbx
    0x41, 0x57,                   // push r15
    0x48, 0x89, 0xCF,             // mov rdi, rcx         ; rdi = params
    0x48, 0x8B, 0x07,             // mov rax, [rdi]       ; rax = lua_state_ptr
    0x4C, 0x8B, 0x38,             // mov r15, [rax]       ; r15 = lua_State*
    0x4C, 0x89, 0xF9,             // mov rcx, r15         ; arg1: L
    0x48, 0x8B, 0x57, 0x08,       // mov rdx, [rdi+0x08]  ; arg2: bytecode
    0x4C, 0x8B, 0x47, 0x10,       // mov r8,  [rdi+0x10]  ; arg3: size
    0x4C, 0x8D, 0x4F, 0x28,       // lea r9,  [rdi+0x28]  ; arg4: "Elaina"
    0x48, 0x83, 0xEC, 0x28,       // sub rsp, 40
    0xFF, 0x57, 0x18,             // call [rdi+0x18]      ; luaL_loadbuffer
    0x48, 0x83, 0xC4, 0x28,       // add rsp, 40
    0x85, 0xC0,                   // test eax, eax
    0x75, 0x16,                   // jnz cleanup (22 bytes)
    0x4C, 0x89, 0xF9,             // mov rcx, r15
    0x31, 0xD2,                   // xor edx, edx
    0x45, 0x31, 0xC0,            // xor r8d, r8d
    0x45, 0x31, 0xC9,            // xor r9d, r9d
    0x48, 0x83, 0xEC, 0x28,       // sub rsp, 40
    0xFF, 0x57, 0x20,             // call [rdi+0x20]      ; lua_pcall
    0x48, 0x83, 0xC4, 0x28,       // add rsp, 40
    0x41, 0x5F,                   // pop r15
    0x5B,                         // pop rbx
    0x5F,                         // pop rdi
    0x5D,                         // pop rbp
    0xC3                          // ret
};

// ── Luau VM address discovery via AOB ─────────────────────

static uint64_t FindLuaFunction(Memory& mem, const std::string& sig, const char* name) {
    auto result = mem.ScanSection(".text", sig);
    if (result)
        Log::Info("Found %s at 0x%llx", name, (unsigned long long)result);
    return result;
}

// ── Lua state discovery ───────────────────────────────────
// Returns the absolute ADDRESS that holds the lua_State*.

static uint64_t FindLuaState(Memory& mem, InstanceTree& tree) {
    auto dm = tree.FindDataModel();
    if (!dm) { Log::Warn("FindLuaState: no DataModel"); return 0; }

    auto sc = mem.Read<uint64_t>(dm + g_Offsets.DM_ScriptContext);
    if (!sc || !sc.value) { Log::Warn("FindLuaState: no ScriptContext"); return 0; }

    uint64_t scAddr = sc.value;
    uint64_t luaStateFieldAddr = scAddr + g_Offsets.SC_LuaState;

    auto ls = mem.Read<uint64_t>(luaStateFieldAddr);
    if (!ls || ls.value < 0x10000 || ls.value > 0x7FFFFFFFFFFF) {
        Log::Warn("FindLuaState: bad lua_State* 0x%llx at SC+0x%llx",
                  (unsigned long long)ls.value, (unsigned long long)g_Offsets.SC_LuaState);
        return 0;
    }

    Log::Info("Found lua_State at 0x%llx (ptr stored at 0x%llx)",
              (unsigned long long)ls.value, (unsigned long long)luaStateFieldAddr);
    return luaStateFieldAddr;
}

// ── Exported hooks ─────────────────────────────────────────

bool Injector::FindString(Memory& mem, uint64_t start, uint64_t end, const std::string& pattern, uint64_t& result) {
    const size_t scan_size = 4096;
    std::vector<char> buffer(scan_size);

    for (uint64_t addr = start; addr < end; addr += scan_size - pattern.size()) {
        uint64_t to_read = (std::min)((uint64_t)scan_size, end - addr);
        if (!mem.ReadArray(addr, buffer.data(), (uint32_t)to_read))
            continue;

        auto it = std::search(buffer.data(), buffer.data() + to_read,
                              pattern.data(), pattern.data() + pattern.size());
        if (it != buffer.data() + to_read) {
            result = addr + (it - buffer.data());
            return true;
        }
    }
    return false;
}

Result<std::pair<uint64_t, uint64_t>> Injector::FindProtectedString(Memory& mem, uint64_t scriptAddr) {
    auto psAddr = mem.Read<uint64_t>(scriptAddr + g_Offsets.MS_ProtectedString);
    if (!psAddr) return { Err::MemoryReadFailed, "PS pointer failed" };

    uint64_t psAddrVal = psAddr.value;
    if (!psAddrVal) return { Err::MemoryReadFailed, "ProtectedString is null" };

    auto psSize = mem.Read<uint64_t>(psAddrVal + g_Offsets.PS_Size);
    if (!psSize) return { Err::MemoryReadFailed, "PS size failed" };

    Log::Info("ProtectedString at 0x%llx (size %llu)", psAddrVal, psSize.value);
    return std::make_pair(psAddrVal, psSize.value);
}

bool Injector::OverwriteProtectedString(Memory& mem, uint64_t psAddr, uint64_t psSize, const std::vector<uint8_t>& data) {
    if (data.size() <= psSize) {
        return mem.WriteArray(psAddr, data.data(), (uint32_t)data.size());
    }

    auto newBuf = mem.Allocate(data.size());
    if (!newBuf) {
        Log::Error("Failed to allocate larger RSB1 buffer");
        return false;
    }

    mem.WriteArray(newBuf.value, data.data(), (uint32_t)data.size());
    mem.Write<uint64_t>(psAddr + g_Offsets.PS_Data, newBuf.value);
    mem.Write<uint64_t>(psAddr + g_Offsets.PS_Size, data.size());

    Log::Info("Overwrote ProtectedString with %zu bytes RSB1", data.size());
    return true;
}

bool Injector::TriggerRequire(Memory& mem) {
    Log::Info("TriggerRequire stub (100ms sleep)");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return true;
}

// ── Shellcode-based TriggerRequire ────────────────────────

bool Injector::TriggerRequireShellcode(Memory& mem, InstanceTree& tree,
                                       uint64_t scriptAddr, const std::vector<uint8_t>& rsb1Data) {
    Log::Info("TriggerRequireShellcode: launching remote thread...");

    // 1. Scan for Lua VM addresses
    auto loadAddr = FindLuaFunction(mem, kLualLoadbufferSig, "luaL_loadbuffer");
    auto pcallAddr = FindLuaFunction(mem, kLuaPcallSig, "lua_pcall");

    if (!loadAddr) {
        Log::Warn("luaL_loadbuffer not found, trying lua_load");
        loadAddr = FindLuaFunction(mem, kLuaLoadSig, "lua_load");
    }

    if (!loadAddr || !pcallAddr) {
        Log::Error("Luau function scan failed. Update AOB patterns in Injector.cpp.");
        Log::Error("  load=0x%llx  pcall=0x%llx",
                   (unsigned long long)loadAddr, (unsigned long long)pcallAddr);
        return false;
    }

    // 2. Find lua_State*
    auto luaStateFieldAddr = FindLuaState(mem, tree);
    if (!luaStateFieldAddr) {
        Log::Error("lua_State not found. Check DM_ScriptContext / SC_LuaState offsets.");
        return false;
    }

    // 3. Allocate one RWX page for shellcode + params + bytecode
    const size_t pageSize = 0x1000;
    auto remotePage = mem.Allocate(pageSize, PAGE_EXECUTE_READWRITE);
    if (!remotePage) {
        Log::Error("Failed to allocate RWX page");
        return false;
    }
    uint64_t base = remotePage.value;

    const uint64_t kParamsOff = 0x100;
    const uint64_t kBytecodeOff = 0x140;

    // 4. Build params
    ShellcodeParams params{};
    params.lua_state_ptr = luaStateFieldAddr;
    params.bytecode_addr = base + kBytecodeOff;
    params.bytecode_size = rsb1Data.size();
    params.loadbuffer_addr = loadAddr;
    params.pcall_addr = pcallAddr;
    memcpy(params.script_name, "Elaina", 7);

    // 5. Write to remote page
    if (!mem.WriteArray(base, kShellcode, sizeof(kShellcode)))
        { Log::Error("write shellcode failed"); goto fail; }
    if (!mem.WriteArray(base + kParamsOff, &params, sizeof(params)))
        { Log::Error("write params failed"); goto fail; }
    if (!mem.WriteArray(base + kBytecodeOff, rsb1Data.data(), (uint32_t)rsb1Data.size()))
        { Log::Error("write bytecode failed"); goto fail; }

    Log::Info("Remote page 0x%llx: shellcode @0x0, params @0x%llx, bc @0x%llx",
              (unsigned long long)base,
              (unsigned long long)(base + kParamsOff),
              (unsigned long long)(base + kBytecodeOff));

    // 6. Create remote thread
    {
        HANDLE hThread = nullptr;
        {
            auto hResult = mem.CreateRemoteThread((void*)base, (void*)(base + kParamsOff));
            if (!hResult) {
                Log::Error("CreateRemoteThread: %s", hResult.ErrorMsg().c_str());
                goto fail;
            }
            hThread = hResult.value;
        }

        // 7. Wait briefly for completion
        Syscall::WaitForSingleObject(hThread, 5000);
        Syscall::CloseHandle(hThread);
    }

    Log::Info("Shellcode thread completed successfully");
    return true;

fail:
    void* freeAddr = (void*)base;
    SIZE_T freeSize = 0;
    Syscall::FreeMemory(mem.GetProcessHandle(), &freeAddr, &freeSize, MEM_RELEASE);
    return false;
}

// ── Main Execute entry point ──────────────────────────────

bool Injector::Execute(Memory& mem, InstanceTree& tree, const std::vector<uint8_t>& rsb1Data) {
    Log::Info("Execute: searching for any ModuleScript...");

    // Try FindAnyModuleScript — recursive search through entire DataModel
    auto scriptAddr = tree.FindAnyModuleScript();
    if (!scriptAddr) {
        Log::Error("No ModuleScript found in DataModel");
        return false;
    }
    Log::Info("Found ModuleScript at 0x%llx", (unsigned long long)scriptAddr);

    auto psResult = FindProtectedString(mem, scriptAddr);
    if (!psResult) {
        Log::Error("Execute: %s", psResult.ErrorMsg().c_str());
        return false;
    }

    auto [psAddr, psSize] = psResult.value;
    if (!OverwriteProtectedString(mem, psAddr, psSize, rsb1Data))
        return false;

    // Try shellcode-based trigger first; fall back to stub sleep
    if (TriggerRequireShellcode(mem, tree, scriptAddr, rsb1Data))
        return true;

    Log::Warn("Shellcode trigger failed, falling back to stub TriggerRequire");
    return TriggerRequire(mem);
}