# Elaina Executor

> **EDUCATIONAL PURPOSE ONLY** — This project is a demonstration of various Windows internals concepts (direct syscalls, external memory manipulation, PE parsing, Luau bytecode compilation) and is **NOT** intended for actual use against Roblox or any other game/platform.

**This project is not 100% complete. It provides only a high-level overview / proof of concept of the architecture described below.** Many components are stubbed or incomplete (SSN resolver, trigger require, offset values are outdated, etc.).

---

Roblox external executor — compiles Luau → RSB1 bytecode, overwrites ModuleScript ProtectedString via external memory, triggers require().

## Architecture

```
api/          DLL exports — thin P/Invoke bridge
service/      Session, ExecutionService, NetworkService (orchestration)
execution/    Compiler, Encoder, Injector (RSB1 pipeline)
instance/     Offsets, InstanceTree (Roblox domain model)
network/      HTTP server (cpp-httplib), WinHTTP proxy
process/      Process attach, external memory R/W via direct syscalls
syscall/      SSN resolver + MASM direct syscall stub
core/         Result<T>, Log (cross-cutting)
```

## Build

```
build.bat
```

Or manually:

```powershell
# 1. C++ DLL
cd build
cmake .. -G "NMake Makefiles" -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build . --config Release

# 2. C# UI (single-file EXE)
dotnet publish ui/ElainaUI.csproj -c Release -r win-x64 --self-contained false -p:PublishSingleFile=true
```

Output: `ui/bin/Release/net8.0-windows/win-x64/publish/Elaina-Executor.exe`

## Usage

1. Launch Roblox (RobloxPlayerBeta.exe)
2. Run `Elaina-Executor.exe`
3. Click **Attach** — reads Roblox DataModel via external memory
4. Click **Inject UNC** — injects UNC API into CoreGui
5. Type script → **Execute** or `Ctrl+Enter`

### HTTP API (port 19283)

| Path | Method | Body | Description |
|------|--------|------|-------------|
| `/compile` | POST | raw Lua source | Returns RSB1-encoded bytecode |
| `/inject` | POST | raw RSB1 bytes | Injects into Roblox via ProtectedString overwrite |
| `/health` | GET | — | Returns `ok` |

## Requirements

- Windows 10/11 x64
- [.NET 8 Runtime](https://dotnet.microsoft.com/en-us/download/dotnet/8.0)
- VS BuildTools 2022 (for building from source)
- RobloxPlayerBeta.exe (target process)

## Technique

1. **Direct syscalls** — SSN resolver maps fresh ntdll.dll from disk, parses PE exports; MASM stub sets EAX from global SSN — never touches hooked ntdll in memory
2. **External memory** — NtOpenProcess + NtReadVirtualMemory/NtWriteVirtualMemory via syscalls, no DLL injected into Roblox
3. **ProtectedString overwrite** — find unloaded ModuleScript in CoreGui → overwrite its ProtectedString data/size with RSB1-encoded bytecode → trigger require()
4. **RSB1 encoding** — BLAKE3 sign(seed=42) → ZSTD maxCLevel → XOR with `ROFL` → `RSB1` + uint32 size header

## Offsets

All offsets in `src/instance/Offsets.h` are version-specific (`version-d599f7fc52a8404c`). The `OffsetTable` struct allows runtime override via config.
