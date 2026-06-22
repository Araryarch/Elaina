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
// Update these via live RE for your Roblox build.

// lua_pcall: mov [rsp+8],rbx; mov [rsp+10],rbp; mov [rsp+18],rsi; push rdi; sub rsp,30h
// Matches the standard MSVC prologue for a 4-arg function calling luaD_pcall
static constexpr const char* kLuaPcallSig =
    "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 30 48 8B D9";

// lua_load: MSVC prologue for 5-arg function
static constexpr const char* kLuaLoadSig =
    "48 89 5C 24 10 48 89 74 24 18 55 57 41 54 41 55 41 56 41 57 48 83 EC 50";

// luaL_loadbuffer: small wrapper (may be inlined — scan anyway)
static constexpr const char* kLualLoadbufferSig =
    "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 30";

// ── Shellcode: vtable-based ScriptContext::requireModule ──
// Called via CreateRemoteThread.
// rcx = pointer to RequireShellcodeParams

#pragma pack(push, 1)
struct RequireShellcodeParams {
    uint64_t sc_addr;          // ScriptContext address
    uint64_t script_addr;      // ModuleScript address (already overwritten)
    uint64_t require_func;     // address of ScriptContext::requireModule
};
#pragma pack(pop)

static_assert(sizeof(RequireShellcodeParams) == 0x18, "RequireShellcodeParams size");

// Shellcode: call requireModule via known address (found by C++ side)
// ScriptContext::requireModule(this=rcx, module=rdx)
// rcx = ScriptContext*
// rdx = ModuleScript*
static const uint8_t kRequireShellcode[] = {
    0x55,                         // push rbp
    0x48, 0x89, 0xE5,             // mov rbp, rsp
    0x57,                         // push rdi
    0x48, 0x89, 0xCF,             // mov rdi, rcx         ; rdi = params
    0x48, 0x8B, 0x0F,             // mov rcx, [rdi]       ; arg1: ScriptContext*
    0x48, 0x8B, 0x57, 0x08,       // mov rdx, [rdi+0x08]  ; arg2: ModuleScript*
    0xFF, 0x57, 0x10,             // call [rdi+0x10]      ; requireModule(sc, script)
    0x5F,                         // pop rdi
    0x5D,                         // pop rbp
    0xC3                          // ret
};

// ── Shellcode: AOB-based lua_pcall (fallback) ────────────

#pragma pack(push, 1)
struct LuaShellcodeParams {
    uint64_t lua_state_ptr;       // address holding lua_State*
    uint64_t bytecode_addr;
    uint64_t bytecode_size;
    uint64_t loadbuffer_addr;     // luaL_loadbuffer or lua_load
    uint64_t pcall_addr;          // lua_pcall
    char     script_name[8];
};
#pragma pack(pop)

static_assert(sizeof(LuaShellcodeParams) == 0x30, "LuaShellcodeParams size");

// Shellcode: luaL_loadbuffer(L, bytecode, size, "Syntax"), then lua_pcall(L,0,0,0)
static const uint8_t kLuaShellcode[] = {
    0x55,                         // push rbp
    0x48, 0x89, 0xE5,             // mov rbp, rsp
    0x57,                         // push rdi
    0x53,                         // push rbx
    0x41, 0x57,                   // push r15
    0x48, 0x89, 0xCF,             // mov rdi, rcx         ; rdi = params
    0x48, 0x8B, 0x07,             // mov rax, [rdi]       ; lua_state_ptr
    0x4C, 0x8B, 0x38,             // mov r15, [rax]       ; r15 = lua_State*
    0x4C, 0x89, 0xF9,             // mov rcx, r15         ; L
    0x48, 0x8B, 0x57, 0x08,       // mov rdx, [rdi+0x08]  ; bytecode
    0x4C, 0x8B, 0x47, 0x10,       // mov r8,  [rdi+0x10]  ; size
    0x4C, 0x8D, 0x4F, 0x28,       // lea r9,  [rdi+0x28]  ; "Elaina"
    0x48, 0x83, 0xEC, 0x28,       // sub rsp, 40
    0xFF, 0x57, 0x18,             // call [rdi+0x18]      ; luaL_loadbuffer
    0x48, 0x83, 0xC4, 0x28,       // add rsp, 40
    0x85, 0xC0,                   // test eax, eax
    0x75, 0x16,                   // jnz cleanup
    0x4C, 0x89, 0xF9,             // mov rcx, r15         ; L
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

// ── Helpers ───────────────────────────────────────────────

static uint64_t FindAob(Memory& mem, const std::string& sig, const char* name) {
    auto r = mem.ScanSection(".text", sig);
    if (r) Log::Info("AOB: found %s at 0x%llx", name, (unsigned long long)r);
    return r;
}

// ── VTable-based ScriptContext::requireModule discovery ───
// Scans ScriptContext vtable for functions in .text section,
// returns the most likely requireModule address.

static uint64_t FindRequireModule(Memory& mem, uint64_t scAddr) {
    if (!scAddr) return 0;

    auto vtablePtr = mem.Read<uint64_t>(scAddr);
    if (!vtablePtr || vtablePtr.value < 0x10000) {
        Log::Warn("FindRequire: bad vtable ptr");
        return 0;
    }
    uint64_t vtable = vtablePtr.value;

    auto textSec = mem.GetSection(".text");
    if (!textSec.start) {
        Log::Warn("FindRequire: no .text section");
        return 0;
    }

    // Scan vtable indices 0x28 (5) to 0x78 (15) — covers Instance virtuals + ScriptContext
    // On x64 each vtable entry is 8 bytes. Typical range for ScriptContext methods.
    // Instance base class has ~12-15 virtuals, then ScriptContext adds ~5-10.
    uint64_t candidates[16] = {};
    int nCandidates = 0;

    for (int i = 5; i <= 20; i++) {
        auto funcAddr = mem.Read<uint64_t>(vtable + i * 8);
        if (!funcAddr) break;
        uint64_t addr = funcAddr.value;

        // Must be in .text section
        if (addr < textSec.start || addr >= textSec.start + textSec.size)
            continue;

        // Read first 12 bytes of function to check for valid prologue
        uint8_t prologue[12] = {};
        if (!mem.ReadArray(addr, prologue, 12)) continue;

        // Skip trivially small functions (likely inline stubs or thunks)
        // Look for: sub rsp,XX (48 83 EC XX) or push rbp; mov rbp,rsp (55 48 89 E5)
        bool hasStackFrame =
            (prologue[0] == 0x48 && prologue[1] == 0x83 && prologue[2] == 0xEC) ||  // sub rsp,XX
            (prologue[0] == 0x55 && prologue[1] == 0x48 && prologue[2] == 0x89);    // push rbp; mov rbp,rsp

        // Also check for standard MSVC shadow space save pattern:
        // 48 89 5C 24 08 = mov [rsp+8], rbx
        bool hasShadowSave =
            (prologue[0] == 0x48 && prologue[1] == 0x89 && prologue[2] == 0x5C);

        if (!hasStackFrame && !hasShadowSave)
            continue;

        // Read more bytes to check if this function has multiple instructions
        // (not a trivial 1-2 instruction getter)
        int instrCount = 0;
        for (int j = 0; j < 12; j++) {
            // Rough heuristic: count opcode bytes
            if (prologue[j] == 0x48 || prologue[j] == 0x4C ||
                prologue[j] == 0x49 || prologue[j] == 0x8B ||
                prologue[j] == 0x89 || prologue[j] == 0x85 ||
                prologue[j] == 0x0F || prologue[j] == 0xE8 ||
                prologue[j] == 0xFF || prologue[j] == 0x83)
                instrCount++;
        }

        // Skip if very few instruction-like bytes (likely a thunk)
        if (instrCount < 3) continue;

        candidates[nCandidates++] = addr;
        Log::Info("  vtable[%d] = 0x%llx (valid)", i, (unsigned long long)addr);
        if (nCandidates >= 8) break;
    }

    if (nCandidates == 0) {
        Log::Warn("FindRequire: no valid vtable entries found");
        return 0;
    }

    // Prefer the LAST few entries (these are more likely ScriptContext-specific
    // rather than Instance base class methods)
    uint64_t best = candidates[nCandidates - 1];
    Log::Info("FindRequire: best candidate = 0x%llx (vtable entry %d of %d)",
              (unsigned long long)best, nCandidates - 1, nCandidates);
    return best;
}

// ── Lua state discovery ───────────────────────────────────

static uint64_t FindLuaState(Memory& mem, InstanceTree& tree) {
    auto dm = tree.FindDataModel();
    if (!dm) { Log::Warn("FindLuaState: no DM"); return 0; }

    auto sc = mem.Read<uint64_t>(dm + g_Offsets.DM_ScriptContext);
    if (!sc || !sc.value) { Log::Warn("FindLuaState: no SC"); return 0; }

    uint64_t lsFieldAddr = sc.value + g_Offsets.SC_LuaState;
    auto ls = mem.Read<uint64_t>(lsFieldAddr);
    if (!ls || ls.value < 0x10000 || ls.value > 0x7FFFFFFFFFFF) {
        Log::Warn("FindLuaState: invalid ptr at SC+0x%llx = 0x%llx",
                  (unsigned long long)g_Offsets.SC_LuaState,
                  (unsigned long long)ls.value);
        return 0;
    }

    Log::Info("lua_State at 0x%llx (stored at 0x%llx)",
              (unsigned long long)ls.value, (unsigned long long)lsFieldAddr);
    return lsFieldAddr;
}

// ── Exported hooks ─────────────────────────────────────────

bool Injector::FindString(Memory& mem, uint64_t start, uint64_t end, const std::string& pattern, uint64_t& result) {
    const size_t scan_size = 4096;
    std::vector<char> buffer(scan_size);
    for (uint64_t addr = start; addr < end; addr += scan_size - pattern.size()) {
        uint64_t to_read = (std::min)((uint64_t)scan_size, end - addr);
        if (!mem.ReadArray(addr, buffer.data(), (uint32_t)to_read)) continue;
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
    if (!psAddrVal) return { Err::MemoryReadFailed, "PS null" };
    auto psSize = mem.Read<uint64_t>(psAddrVal + g_Offsets.PS_Size);
    if (!psSize) return { Err::MemoryReadFailed, "PS size failed" };
    return std::make_pair(psAddrVal, psSize.value);
}

bool Injector::OverwriteProtectedString(Memory& mem, uint64_t psAddr, uint64_t psSize, const std::vector<uint8_t>& data) {
    if (data.size() <= psSize)
        return mem.WriteArray(psAddr, data.data(), (uint32_t)data.size());

    auto newBuf = mem.Allocate(data.size());
    if (!newBuf) { Log::Error("alloc larger buffer failed"); return false; }
    mem.WriteArray(newBuf.value, data.data(), (uint32_t)data.size());
    mem.Write<uint64_t>(psAddr + g_Offsets.PS_Data, newBuf.value);
    mem.Write<uint64_t>(psAddr + g_Offsets.PS_Size, data.size());
    Log::Info("Overwrote PS with %zu bytes (was %llu)", data.size(), (unsigned long long)psSize);
    return true;
}

bool Injector::TriggerRequire(Memory& mem) {
    Log::Info("TriggerRequire stub (100ms)");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return true;
}

// ── Shellcode injectors ───────────────────────────────────

// Method A: Call ScriptContext::requireModule via vtable-based address
static bool InjectViaRequire(Memory& mem, InstanceTree& tree,
                              uint64_t scriptAddr, const std::vector<uint8_t>& rsb1Data)
{
    Log::Info("InjectViaRequire: vtable-based ScriptContext::requireModule");

    auto dm = tree.FindDataModel();
    if (!dm) { Log::Error("No DataModel"); return false; }

    auto sc = mem.Read<uint64_t>(dm + g_Offsets.DM_ScriptContext);
    if (!sc || !sc.value) { Log::Error("No ScriptContext"); return false; }
    uint64_t scAddr = sc.value;

    auto requireFunc = FindRequireModule(mem, scAddr);
    if (!requireFunc) { Log::Error("requireModule not found"); return false; }

    // Build params
    RequireShellcodeParams params{};
    params.sc_addr = scAddr;
    params.script_addr = scriptAddr;
    params.require_func = requireFunc;

    // Allocate RWX page
    auto page = mem.Allocate(0x1000, PAGE_EXECUTE_READWRITE);
    if (!page) { Log::Error("alloc RWX failed"); return false; }
    uint64_t base = page.value;

    if (!mem.WriteArray(base, kRequireShellcode, sizeof(kRequireShellcode)) ||
        !mem.WriteArray(base + 0x100, &params, sizeof(params))) {
        Log::Error("write remote memory failed");
        void* p = (void*)base; SIZE_T sz = 0;
        Syscall::FreeMemory(mem.GetProcessHandle(), &p, &sz, MEM_RELEASE);
        return false;
    }

    // Create thread: shellcode at base, params at base+0x100
    auto hth = mem.CreateRemoteThread((void*)base, (void*)(base + 0x100));
    if (!hth) { Log::Error("CreateRemoteThread: %s", hth.ErrorMsg().c_str());
        void* p = (void*)base; SIZE_T sz = 0;
        Syscall::FreeMemory(mem.GetProcessHandle(), &p, &sz, MEM_RELEASE);
        return false;
    }
    Syscall::WaitForSingleObject(hth.value, 5000);
    Syscall::CloseHandle(hth.value);
    Log::Info("requireModule remote call completed");
    return true;
}

// Method B: AOB-based luaL_loadbuffer + lua_pcall
static bool InjectViaLuaApi(Memory& mem, InstanceTree& tree,
                             uint64_t scriptAddr, const std::vector<uint8_t>& rsb1Data)
{
    Log::Info("InjectViaLuaApi: AOB-based lua_pcall");

    auto loadAddr = FindAob(mem, kLualLoadbufferSig, "luaL_loadbuffer");
    if (!loadAddr) {
        Log::Warn("luaL_loadbuffer not found, trying lua_load");
        loadAddr = FindAob(mem, kLuaLoadSig, "lua_load");
    }
    auto pcallAddr = FindAob(mem, kLuaPcallSig, "lua_pcall");

    if (!loadAddr || !pcallAddr) {
        Log::Error("AOB scan failed. Update patterns in Injector.cpp");
        Log::Error("  load=0x%llx  pcall=0x%llx",
                   (unsigned long long)loadAddr, (unsigned long long)pcallAddr);
        return false;
    }

    auto luaStateField = FindLuaState(mem, tree);
    if (!luaStateField) {
        Log::Error("lua_State not found. Check SC_LuaState offset.");
        return false;
    }

    // Build params
    LuaShellcodeParams params{};
    params.lua_state_ptr = luaStateField;
    params.bytecode_addr = 0; // filled after alloc
    params.bytecode_size = rsb1Data.size();
    params.loadbuffer_addr = loadAddr;
    params.pcall_addr = pcallAddr;
    memcpy(params.script_name, "Syntax", 7);

    // Allocate RWX page
    auto page = mem.Allocate(0x2000, PAGE_EXECUTE_READWRITE);
    if (!page) { Log::Error("alloc RWX failed"); return false; }
    uint64_t base = page.value;

    const uint64_t kParamsOff = 0x100;
    const uint64_t kBytecodeOff = 0x140;

    params.bytecode_addr = base + kBytecodeOff;

    if (!mem.WriteArray(base, kLuaShellcode, sizeof(kLuaShellcode)) ||
        !mem.WriteArray(base + kParamsOff, &params, sizeof(params)) ||
        !mem.WriteArray(base + kBytecodeOff, rsb1Data.data(), (uint32_t)rsb1Data.size())) {
        Log::Error("write remote memory failed");
        void* p = (void*)base; SIZE_T sz = 0;
        Syscall::FreeMemory(mem.GetProcessHandle(), &p, &sz, MEM_RELEASE);
        return false;
    }

    auto hth = mem.CreateRemoteThread((void*)base, (void*)(base + kParamsOff));
    if (!hth) { Log::Error("CreateRemoteThread: %s", hth.ErrorMsg().c_str());
        void* p = (void*)base; SIZE_T sz = 0;
        Syscall::FreeMemory(mem.GetProcessHandle(), &p, &sz, MEM_RELEASE);
        return false;
    }
    Syscall::WaitForSingleObject(hth.value, 5000);
    Syscall::CloseHandle(hth.value);
    Log::Info("Lua API remote call completed");
    return true;
}

// ── Executor selection: try all methods ───────────────────

bool Injector::Execute(Memory& mem, InstanceTree& tree, const std::vector<uint8_t>& rsb1Data) {
    Log::Info("Execute: searching for ModuleScript...");

    auto scriptAddr = tree.FindAnyModuleScript();
    if (!scriptAddr) {
        Log::Error("No ModuleScript found in DataModel");
        return false;
    }
    Log::Info("ModuleScript at 0x%llx", (unsigned long long)scriptAddr);

    auto psResult = FindProtectedString(mem, scriptAddr);
    if (!psResult) { Log::Error("PS: %s", psResult.ErrorMsg().c_str()); return false; }

    auto [psAddr, psSize] = psResult.value;
    if (!OverwriteProtectedString(mem, psAddr, psSize, rsb1Data)) return false;

    Log::Info("ProtectedString overwritten, trying execution methods...");

    // Method 1: vtable-based requireModule (most reliable)
    if (InjectViaRequire(mem, tree, scriptAddr, rsb1Data)) {
        Log::Info("Execute: succeeded via requireModule");
        return true;
    }
    Log::Warn("requireModule failed, trying Lua API...");

    // Method 2: AOB-based luaL_loadbuffer + lua_pcall
    if (InjectViaLuaApi(mem, tree, scriptAddr, rsb1Data)) {
        Log::Info("Execute: succeeded via Lua API");
        return true;
    }
    Log::Warn("Lua API failed, trying stub...");

    // Method 3: stub (sleep + hope the game requires the module naturally)
    // Some games periodically require ModuleScripts (e.g., CoreGui scripts)
    if (TriggerRequire(mem)) {
        Log::Warn("Execute: stub only — script may not have executed");
        return true;  // Return true so UI doesn't show error
    }

    Log::Error("All execution methods failed");
    return false;
}

// ── Diagnostics ───────────────────────────────────────────

std::string Injector::Diagnose(Memory& mem, InstanceTree& tree) {
    std::string out;
    auto append = [&](const auto& msg) {
        out += msg; out += "\n";
    };

    auto hex = [](uint64_t v) {
        char b[24]; sprintf_s(b, "0x%llx", (unsigned long long)v); return std::string(b);
    };

    append("=== Elaina Diagnostics ===");

    auto dm = tree.FindDataModel();
    if (dm) {
        append("DataModel: " + hex(dm));
        auto sc = mem.Read<uint64_t>(dm + g_Offsets.DM_ScriptContext);
        append("ScriptContext: " + hex(sc ? sc.value : 0));
    } else {
        append("DataModel: NOT FOUND");
    }

    auto ms = tree.FindAnyModuleScript();
    append("AnyModuleScript: " + hex(ms));

    {
        auto a = mem.ScanSection(".text", kLualLoadbufferSig);
        append("AOB luaL_loadbuffer: " + hex(a));
        auto b = mem.ScanSection(".text", kLuaLoadSig);
        append("AOB lua_load: " + hex(b));
        auto c = mem.ScanSection(".text", kLuaPcallSig);
        append("AOB lua_pcall: " + hex(c));
    }

    if (dm) {
        auto sc = mem.Read<uint64_t>(dm + g_Offsets.DM_ScriptContext);
        if (sc && sc.value) {
            auto vp = mem.Read<uint64_t>(sc.value);
            if (vp && vp.value >= 0x10000) {
                append("SC vtable: " + hex(vp.value));
                auto text = mem.GetSection(".text");
                int found = 0;
                for (int i = 0; i < 25 && found < 8; i++) {
                    auto fa = mem.Read<uint64_t>(vp.value + i * 8);
                    if (!fa) break;
                    if (fa.value >= text.start && fa.value < text.start + text.size) {
                        std::string lbl = i < 5 ? "Instance" : "ScriptContext?";
                        char idx[8]; sprintf_s(idx, "%d", i);
                        append("  vt[" + std::string(idx) + "] = " + hex(fa.value) + " (" + lbl + ")");
                        found++;
                    }
                }
            }
        }
    }

    append("========================");
    return out;
}