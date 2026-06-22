#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <iomanip>
#include <optional>
#include <thread>
#include <chrono>
#include <set>
#include <Windows.h>
#include "process/scanner.h"
#include "offsets.h"
#include "execution/luau_wrapper.h"
#include "http_server.h"
#include "unc_payload.h"
#include "sunc_payload.h"

class ScriptExecutor {
public:
    enum ErrorCode {
        SUCCESS = 0,
        ERR_NOT_CONNECTED = -1,
        ERR_NO_DATAMODEL = -2,
        ERR_NO_SCRIPT_FOUND = -3,
        ERR_COMPILE_FAILED = -4,
        ERR_ALLOC_FAILED = -5,
        ERR_WRITE_FAILED = -6,
        ERR_OVERWRITE_FAILED = -7,
        ERR_INJECT_FAILED = -8,
        ERR_NOT_INJECTED = -9,
        ERR_TIMEOUT = -10,
    };

private:
    static inline bool sInjected = false;
    static inline DWORD sRobloxPid = 0;

    // Cached navigation
    static inline uintptr_t sCachedPLManager = 0;
    static inline uintptr_t sCachedModules = 0;
    static inline uintptr_t sCachedCoreGui = 0;
    
    static inline std::set<uintptr_t> sUsedModules;

    // Track every module we modify so we can restore originals on leave
    struct ModifiedModule {
        uintptr_t moduleAddr;
        uintptr_t embeddedAddr;        // the ProtectedString object
        uintptr_t originalBytecodePtr; // original value at embedded+0x10
        uint64_t  originalSize;        // original value at embedded+0x20
        uint64_t  originalCapabilities;// original InstanceCapabilities
    };
    static inline std::vector<ModifiedModule> sModifiedModules;
    static inline HANDLE sLastProcessHandle = nullptr;

    // ========================================================================
    // INIT SCRIPT — Injected via SpoofWith on first execution
    // Stays alive via script.Parent = container
    // ExecutionListener polls /poll for C++ to queue scripts
    // ========================================================================
    static std::string GetInitScript() {
        std::string initPart1 = R"LUA(
local HttpService = game:FindService("HttpService")
local CoreGui = game:GetService("CoreGui")

local SERVER = "http://127.0.0.1:9753"
local EXEC_NAME = "Elaina"

-- Clean up previous session if any
local existing = CoreGui:FindFirstChild(EXEC_NAME)
if existing then existing:Destroy() end
print("[INIT] Starting init script...")

-- Container setup
local container = Instance.new("Folder")
container.Name = EXEC_NAME
container.Parent = CoreGui

-- Anchor this module script to CoreGui to keep context alive
script.Parent = container
script.Name = "_init"

local lsContainer = Instance.new("Folder")
lsContainer.Name = "Loadstring"
lsContainer.Parent = container

task.spawn(function()
    pcall(function()
        game:GetService("StarterGui"):SetCore("SendNotification", {
            Title = "Elaina Executor",
            Text = "Successfully attached to game!",
            Icon = "rbxassetid://118289455769601",
            Duration = 10,
            Button1 = "OK"
        })
    end)
end)

-- HTTP helper using RequestInternal (CoreScript-only API)
local function SendRequest(options, timeout)
    local timeoutTime = timeout or 10
    local startTime = tick()
    local result = nil

    HttpService:RequestInternal(options):Start(function(success, response)
        response.Success = success
        result = response
    end)

    while not result do
        task.wait()
        if tick() - startTime > timeoutTime then
            return nil
        end
    end
    return result
end

-- Environment globals
local genv = {}
local Syntax = {}

function Syntax.loadstring(content, chunkName)
    if type(content) ~= "string" then return nil, "invalid argument #1 to 'loadstring'" end
    if content:sub(1,1) == "\0" or content:sub(1,1) == "\27" then return nil, "bytecode not supported" end
    
    local lsResult = SendRequest({
        Url = SERVER .. "/ls",
        Body = content,
        Method = "POST",
        Headers = {["Content-Type"] = "text/plain"}
    }, 10)

    if not lsResult or not lsResult.Success or lsResult.StatusCode ~= 200 then
        local err = lsResult and lsResult.Body or "Server unreachable"
        warn("[Loadstring] Compilation failed: " .. tostring(err))
        return nil, err
    end

    local data = HttpService:JSONDecode(lsResult.Body)
    local moduleParts = data.path
    
    local function resolve(parts)
        local cur = game
        local startIndex = 2
        if parts[1] ~= "game" and parts[1] ~= "Game" and parts[1] ~= "App" then
            startIndex = 1
        end

        for i = startIndex, #parts do
            local name = parts[i]
            local nextObj = cur:FindFirstChild(name)
            
            if not nextObj and cur == game then
                pcall(function() nextObj = game:GetService(name) end)
            end
            
            if not nextObj then
                for _, v in ipairs(cur:GetChildren()) do
                    if v.Name == name then
                        nextObj = v
                        break
                    end
                end
            end
            
            cur = nextObj
            if not cur then return nil end
        end
        return cur
    end

    local module = resolve(moduleParts)
    if not module or not module:IsA("ModuleScript") then
        return nil, "Failed to resolve loaded module: " .. table.concat(moduleParts, ".")
    end

    local env = genv

    local start = tick()
    while true do
        local ok, result = pcall(function() return require(module) end)
        
        if not ok then
            if tick() - start > 1 then
                warn("[Loadstring] Error executing module: " .. tostring(result))
            end
        end

        if ok and type(result) == "table" and type(result[EXEC_NAME]) == "function" then
            local func = result[EXEC_NAME]
            setfenv(func, env)
            return func
        end
        if tick() - start > 5 then
            return nil, "Timeout waiting for require"
        end
        task.wait(0.1)
    end
end

function Syntax.request(options)
    assert(type(options) == "table", "invalid argument #1 to 'request'")
    assert(type(options.Url) == "string", "invalid option 'Url'")
    options.Method = options.Method or "GET"

    local reqHeaders = options.Headers or {}
    if not reqHeaders["User-Agent"] and not reqHeaders["user-agent"] then
        reqHeaders["User-Agent"] = "Elaina"
    end

    local result = SendRequest({
        Url = SERVER .. "/req",
        Body = HttpService:JSONEncode({
            url = options.Url,
            method = options.Method,
            headers = reqHeaders,
            body = options.Body or ""
        }),
        Method = "POST",
        Headers = {["Content-Type"] = "application/json"}
    }, 30)

    if not result or not result.Success then
        error("HTTP request failed", 2)
    end

    local data = HttpService:JSONDecode(result.Body)
    local status = type(data.StatusCode) == "number" and data.StatusCode or 0
    return {
        Success = status >= 200 and status < 300,
        StatusCode = status,
        StatusMessage = "OK",
        Body = data.Body or data.message or "",
        Headers = data.Headers or {}
    }
end

function Syntax.httpget(url)
    assert(type(url) == "string", "invalid argument #1 to 'HttpGet'")
    local resp = Syntax.request({Url = url, Method = "GET"})
    if not resp.Success then
        warn("[HttpGet] Error fetching " .. url .. " (Status: " .. tostring(resp.StatusCode) .. ")")
        warn("[HttpGet] Body: " .. tostring(resp.Body))
        error("HTTP request failed: " .. tostring(resp.StatusCode), 2)
    end
    return resp.Body
end

function Syntax.getgenv() return genv end
function Syntax.getrenv() return getfenv(0) end
function Syntax.identifyexecutor() return "Elaina", "2.0.0" end
function Syntax.getexecutorname() return "Elaina" end
function Syntax.getidentity() return 8 end
function Syntax.getthreadidentity() return 8 end
function Syntax.setthreadidentity(n) end
function Syntax.getthreadcontext() return 8 end

-- Global gethui container — parented to PlayerGui, NOT CoreGui!
local huiContainer = nil

function Syntax.gethui()
    if not huiContainer or not huiContainer.Parent then
        pcall(function()
            huiContainer = Instance.new("Folder")
            huiContainer.Name = "CoreGui"
            local playerGui = game:GetService("Players").LocalPlayer:WaitForChild("PlayerGui", 5)
            if playerGui then
                huiContainer.Parent = playerGui
            else
                huiContainer.Parent = game:GetService("CoreGui")
            end
        end)
    end
    return huiContainer
end

-- Install into genv
genv.loadstring = Syntax.loadstring
genv.request = Syntax.request
genv.http_request = Syntax.request
genv.http = { request = Syntax.request }
genv.HttpGet = Syntax.httpget
genv.httpget = Syntax.httpget
genv.getgenv = Syntax.getgenv
genv.getrenv = Syntax.getrenv
genv.identifyexecutor = Syntax.identifyexecutor
genv.getexecutorname = Syntax.getexecutorname
genv.getidentity = Syntax.getidentity
genv.getthreadidentity = Syntax.getthreadidentity
genv.setthreadidentity = Syntax.setthreadidentity
genv.getthreadcontext = Syntax.getthreadcontext
genv.gethui = Syntax.gethui

-- Ensure raw metatable access is unlocked for sandbox interactions
pcall(function() setreadonly(getrawmetatable(game), false) end)

-- Game proxy to intercept CoreGui and HTTP calls invisibly
local realGame = game
local realGetService = realGame.GetService
local gameProxy = setmetatable({}, {
    __index = function(_, k)
        if k == "HttpGet" or k == "httpget" then
            return function(_, url) return Syntax.httpget(url) end
        end
        if k == "CoreGui" then return Syntax.gethui() end
        if k == "GetService" or k == "getService" then
            return function(self, service)
                if service == "CoreGui" then return Syntax.gethui() end
                return realGetService(realGame, service)
            end
        end
        local ok, v = pcall(function() return realGame[k] end)
        if not ok then return nil end
        if type(v) == "function" then
            return function(_, ...) 
                local args = {...}
                local success, result = pcall(function() return v(realGame, unpack(args)) end)
                return success and result or nil
            end
        end
        return v
    end,
    __newindex = function(_, k, v) pcall(function() realGame[k] = v end) end,
    __tostring = function() return tostring(realGame) end
})

local basenv = getfenv(print) or getfenv(0)
setmetatable(genv, {
    __index = basenv
})

genv.game = gameProxy
genv.Game = gameProxy
genv.workspace = basenv.workspace
genv.Workspace = basenv.Workspace
genv.script = nil

-- Store in shared
shared._rblx_genv = genv
shared._rblx_Null = Syntax
shared._rblx_http = SendRequest
shared._rblx_game_proxy = gameProxy

-- Cleanup signal: tell C++ to restore modules before server tears down
local function signalCleanup()
    pcall(function()
        SendRequest({Url = SERVER .. "/cleanup", Method = "POST", Body = "leave"}, 2)
    end)
end

-- Fire cleanup on server leave/teleport
pcall(function() game:BindToClose(function() signalCleanup() task.wait(0.5) end) end)
pcall(function() game.Close:Connect(signalCleanup) end)
pcall(function()
    game:GetPropertyChangedSignal("GameId"):Connect(signalCleanup)
end)
)LUA";

        std::string initPart2 = R"LUA(
print("[INIT] Environment ready")

-- ExecutionListener: polls C++ HTTP server for scripts to execute
print("[INIT] ExecutionListener started (HTTP polling)...")

while task.wait(0.1) do
    local ok, err = pcall(function()
        local result = SendRequest({
            Url = SERVER .. "/poll",
            Method = "GET"
        }, 2)

        if result and result.Success and result.StatusCode == 200 and result.Body and #result.Body > 0 then
            print("[EXEC] Got script (" .. #result.Body .. " bytes), compiling...")
            local fn, loadErr = Syntax.loadstring(result.Body)
            if fn then
                task.spawn(fn)
            else
                warn("[EXEC] loadstring error: " .. tostring(loadErr))
            end
        end
    end)
    if not ok then
        warn("[EXEC] Listener error: " .. tostring(err))
    end
end
)LUA";
        return initPart1 + std::string(unc_payload) + std::string(sunc_payload) + initPart2;
    }

public:
    static int Execute(HANDLE hProcess, uintptr_t base, uintptr_t dataModel,
                       const std::string& source, std::string& errorOut) {
        if (!hProcess) { errorOut = "Not connected"; return ERR_NOT_CONNECTED; }
        if (!dataModel) { errorOut = "No DataModel"; return ERR_NO_DATAMODEL; }

        // Cache navigation on first call
        if (!sCachedModules) {
            if (!CacheNavigation(hProcess, dataModel, errorOut)) return ERR_INJECT_FAILED;
        }

        // Ensure HTTP server is running and callback is set
        // Ensure HTTP server is running and callbacks are set
        HttpServer::Start(
            FindUnloadedModule,
            []() { RestoreAllModules(); },
            [](uintptr_t moduleAddr, uintptr_t embeddedAddr, uintptr_t origPtr, uint64_t origSize, uint64_t origCaps, HANDLE h) {
                ModifiedModule m;
                m.moduleAddr = moduleAddr;
                m.embeddedAddr = embeddedAddr;
                m.originalBytecodePtr = origPtr;
                m.originalSize = origSize;
                m.originalCapabilities = origCaps;
                sModifiedModules.push_back(m);
                sLastProcessHandle = h;
            }
        );
        HttpServer::SetContext(hProcess, base, dataModel);

        // Phase 1: First execution — inject init script via SpoofWith
        if (!sInjected) {
            // Check for existing session recovery
            if (sCachedCoreGui && rblx::FindChildByName(hProcess, sCachedCoreGui, "Elaina")) {
                std::cout << "[EXEC] Existing Elaina session found in CoreGui — recovering...\n";
                sInjected = true;
            }

            if (!sInjected) {
                std::cout << "[EXEC] Applying RequireBypass to ScriptContext...\n";
                uintptr_t scriptContext = rblx::FindChildByClassName(hProcess, dataModel, "ScriptContext");
                if (scriptContext) {
                    // NOTE: RequireBypass offset (0x920) is outdated — disabled to prevent crash
                    // uint8_t bypass = 1;
                    // ProcessScanner::WriteMemory(hProcess, scriptContext + offsets::ScriptContext::RequireBypass, &bypass, sizeof(bypass));
                    std::cout << "[EXEC] ScriptContext found, RequireBypass skipped (offset outdated).\n";
                } else {
                    std::cout << "[EXEC] ScriptContext not found for RequireBypass.\n";
                }

                std::cout << "[EXEC] First execution — injecting init script...\n";
                std::string initSource = GetInitScript();
                int result = InjectViaSpoof(hProcess, initSource, "init", errorOut);
                if (result != SUCCESS) return result;
                sInjected = true;

                // Wait for init script to set up env and start listener
                std::cout << "[EXEC] Waiting for ExecutionListener to start...\n";
                Sleep(3000);
            }
        }

        // Phase 2: Queue user script for Lua to pick up via HTTP polling
        if (!source.empty()) {
            std::cout << "[EXEC] Queuing user script (" << source.size() << " bytes)...\n";
            HttpServer::QueueScript(source);
            std::cout << "[EXEC] Script queued — listener will pick it up\n";
        }
        return SUCCESS;
    }

private:
    // ========================================================================
    // CacheNavigation — find PLManager, Modules folder, CoreGui (once)
    // ========================================================================
    static bool CacheNavigation(HANDLE hProcess, uintptr_t dataModel, std::string& errorOut) {
        sCachedCoreGui = rblx::FindChildByClassName(hProcess, dataModel, "CoreGui");
        if (!sCachedCoreGui) { errorOut = "CoreGui not found"; return false; }

        uintptr_t robloxGui = rblx::FindChildByName(hProcess, sCachedCoreGui, "RobloxGui");
        if (!robloxGui) { errorOut = "RobloxGui not found"; return false; }

        sCachedModules = rblx::FindChildByName(hProcess, robloxGui, "Modules");
        if (!sCachedModules) { errorOut = "Modules not found"; return false; }

        uintptr_t playerList = rblx::FindChildByName(hProcess, sCachedModules, "PlayerList");
        if (!playerList) { errorOut = "PlayerList not found"; return false; }

        sCachedPLManager = rblx::FindChildByName(hProcess, playerList, "PlayerListManager");
        if (!sCachedPLManager) { errorOut = "PlayerListManager not found"; return false; }
        std::cout << "[NAV] PlayerListManager @ 0x" << std::hex << sCachedPLManager << std::dec << "\n";

        return true;
    }

public:
    // ========================================================================
    static inline int s_ModuleIndex = 0;

    // CRITICAL: Only accept modules containing jest/test/spec/story in their name.
    // Active UI modules with different naming would crash if hijacked → Error 268.
    static bool IsSafeModuleName(const std::string& name) {
        std::string lower;
        lower.resize(name.size());
        std::transform(name.begin(), name.end(), lower.begin(), ::tolower);
        return lower.find("jest") != std::string::npos ||
               lower.find("test") != std::string::npos ||
               lower.find("spec") != std::string::npos ||
               lower.find("story") != std::string::npos;
    }

    static uintptr_t FindUnloadedModule(HANDLE hProcess, std::string& outName) {
        if (!sCachedModules) return 0;
        
        std::vector<uintptr_t> validModules;

        auto trySearchFolder = [&](uintptr_t folder) {
            if (!folder) return;
            for (uintptr_t child : rblx::GetChildren(hProcess, folder)) {
                if (rblx::ReadClassName(hProcess, child) != "ModuleScript") continue;
                
                // Name filter: only jest/test/spec/story modules
                std::string childName = rblx::ReadInstanceName(hProcess, child);
                if (!IsSafeModuleName(childName)) continue;
                
                uint8_t loadedStatus = ProcessScanner::Read<uint8_t>(hProcess, child + 0x188);
                if (loadedStatus == 0x00) {
                    uintptr_t parent = ProcessScanner::Read<uintptr_t>(hProcess, child + 0x68);
                    if (parent && rblx::IsValidInstance(hProcess, parent)) {
                        uintptr_t parentOfParent = ProcessScanner::Read<uintptr_t>(hProcess, parent + 0x68);
                        if (parentOfParent && rblx::IsValidInstance(hProcess, parentOfParent)) {
                            validModules.push_back(child);
                        }
                    }
                }
            }
        };

        // Scan all folders in Modules
        for (uintptr_t folder : rblx::GetChildren(hProcess, sCachedModules)) {
            trySearchFolder(folder);
            
            // Deep scan for nested folders (e.g., Common/Util)
            if (rblx::ReadClassName(hProcess, folder) == "Folder") {
                for (uintptr_t subfolder : rblx::GetChildren(hProcess, folder)) {
                    trySearchFolder(subfolder);
                }
            }
        }
        
        if (validModules.empty()) {
            std::cout << "[WARN] No more unloaded modules found in CoreGui!\n";
            return 0;
        }

        uintptr_t selected = validModules[s_ModuleIndex % validModules.size()];
        s_ModuleIndex++;
        
        std::vector<std::string> pathParts;
        uintptr_t current = selected;
        while (current) {
            std::string name = rblx::ReadInstanceName(hProcess, current);
            pathParts.push_back(name);
            if (name == "CoreGui") break;
            current = ProcessScanner::Read<uintptr_t>(hProcess, current + offsets::Instance::Parent);
        }
        std::string fullPath;
        for (auto it = pathParts.rbegin(); it != pathParts.rend(); ++it) {
            fullPath += *it;
            if (it + 1 != pathParts.rend()) fullPath += ".";
        }
        outName = fullPath;
        
        sUsedModules.insert(selected);
        return selected;
    }


private:

    // ========================================================================
    // InjectViaSpoof — SpoofWith technique (only for init script)
    // ========================================================================
    static int InjectViaSpoof(HANDLE hProcess, const std::string& luaSource,
                              const std::string& label, std::string& errorOut) {
        std::string targetName;
        uintptr_t targetModule = FindUnloadedModule(hProcess, targetName);
        if (!targetModule) { errorOut = "No init module found"; return ERR_INJECT_FAILED; }
        std::cout << "[INJECT-" << label << "] Target: " << targetName
                  << " @ 0x" << std::hex << targetModule << std::dec << "\n";

        sUsedModules.insert(targetModule);

        // Wrap: task.spawn the code, return dummy table for require
        std::string wrapped =
            "task.spawn(function() " + luaSource + "\nend);"
            "return setmetatable({}, {__index = function() return function() end end})";

        auto [ok, bytecodeOrErr] = LuauCompiler::Compile(wrapped);
        if (!ok) { errorOut = label + " compile error: " + bytecodeOrErr; return ERR_COMPILE_FAILED; }

        std::string rsb1 = RSB1Encoder::Encode(bytecodeOrErr);
        if (rsb1.empty()) { errorOut = "RSB1 encoding failed"; return ERR_COMPILE_FAILED; }

        std::cout << "[INJECT-" << label << "] Compiled: " << rsb1.size() << " bytes RSB1\n";

        if (!SetBytecode(hProcess, targetModule, rsb1)) {
            errorOut = "Failed to write bytecode";
            return ERR_WRITE_FAILED;
        }

        // --- Save-and-Revert Spoof Strategy ---
        // 1. Save the original pointer
        uintptr_t originalTarget = 0;
        ProcessScanner::ReadMemory(hProcess, sCachedPLManager + offsets::PlayerListManager::SpoofTarget, &originalTarget, sizeof(uintptr_t));

        // 2. Apply the spoof
        ProcessScanner::WriteMemory(hProcess, sCachedPLManager + offsets::PlayerListManager::SpoofTarget, &targetModule, sizeof(uintptr_t));
        std::cout << "[INJECT-" << label << "] Spoof applied\n";

        // 3. Trigger the script via menu simulation
        SimulateEscKey();  // Open menu — triggers require of our module
        
        std::cout << "[INJECT-" << label << "] Waiting for init signal (Elaina folder in CoreGui)...\n";
        bool signaled = false;
        for (int i = 0; i < 40; i++) { // Max 4 seconds (40 * 100ms)
            if (sCachedCoreGui && rblx::FindChildByName(hProcess, sCachedCoreGui, "Elaina")) {
                signaled = true;
                break;
            }
            Sleep(100);
        }

        if (signaled) {
            std::cout << "[INJECT-" << label << "] Signal received! Restoring memory headers.\n";
        } else {
            std::cout << "[INJECT-" << label << "] WARNING: Init signal timeout. Restoring memory anyway.\n";
        }

        SimulateEscKey();  // Close menu
        Sleep(200);

        // 4. IMMEDIATELY restore the original pointer
        ProcessScanner::WriteMemory(hProcess, sCachedPLManager + offsets::PlayerListManager::SpoofTarget, &originalTarget, sizeof(uintptr_t));
        std::cout << "[INJECT-" << label << "] Spoof reverted (Restore 0x" << std::hex << originalTarget << std::dec << ")\n";

        std::cout << "[INJECT-" << label << "] Complete!\n";
        return SUCCESS;
    }

    // ========================================================================
    // SetBytecode — Write RSB1 data to ModuleScript's ProtectedString
    // ========================================================================
    static bool SetBytecode(HANDLE hProcess, uintptr_t moduleScript, const std::string& rsb1Data) {
        uintptr_t embedded = ProcessScanner::Read<uintptr_t>(hProcess, moduleScript + offsets::ModuleScript::ModuleScriptByteCode);
        if (embedded < 0x10000 || embedded > 0x7FFFFFFFFFFF) {
            std::cout << "[BC] ERROR: Invalid embedded bytecode pointer 0x" << std::hex << embedded << std::dec << "\n";
            return false;
        }

        // Save originals BEFORE overwriting so we can restore on leave
        ModifiedModule saved;
        saved.moduleAddr = moduleScript;
        saved.embeddedAddr = embedded;
        saved.originalBytecodePtr = ProcessScanner::Read<uintptr_t>(hProcess, embedded + 0x10);
        saved.originalSize = ProcessScanner::Read<uint64_t>(hProcess, embedded + 0x20);
        saved.originalCapabilities = ProcessScanner::Read<uint64_t>(hProcess, moduleScript + offsets::Instance::InstanceCapabilities);
        sModifiedModules.push_back(saved);
        sLastProcessHandle = hProcess;

        uintptr_t remoteData = AllocateRemote(hProcess, rsb1Data.size());
        if (!remoteData) {
            std::cout << "[BC] ERROR: Failed to allocate remote memory\n";
            sModifiedModules.pop_back(); // undo tracking
            return false;
        }

        if (!ProcessScanner::WriteMemory(hProcess, remoteData, rsb1Data.data(), rsb1Data.size())) {
            std::cout << "[BC] ERROR: Failed to write remote memory\n";
            sModifiedModules.pop_back();
            return false;
        }

        std::cout << "[BC] Redirecting BytecodePointer (0x" << std::hex << embedded + 0x10 << ") -> 0x" << remoteData << std::dec << "\n";
        ProcessScanner::WriteMemory(hProcess, embedded + 0x10, &remoteData, sizeof(uintptr_t));
        uint64_t newSize = rsb1Data.size();
        ProcessScanner::WriteMemory(hProcess, embedded + 0x20, &newSize, sizeof(uint64_t));
        
        // Patch SecurityCapabilities to maximum (Spoofing identity/permissions)
        uint64_t maxCapabilities = 0x3FFFFFFF;
        ProcessScanner::WriteMemory(hProcess, moduleScript + offsets::Instance::InstanceCapabilities, &maxCapabilities, sizeof(maxCapabilities));

        return true;
    }

    // ========================================================================
    // SimulateEscKey
    // ========================================================================
    static void SimulateEscKey() {
        HWND hwnd = NULL;
        EnumWindows([](HWND h, LPARAM lParam) -> BOOL {
            DWORD wPid = 0;
            GetWindowThreadProcessId(h, &wPid);
            if (wPid == sRobloxPid && IsWindowVisible(h)) {
                char title[256];
                GetWindowTextA(h, title, sizeof(title));
                if (strlen(title) > 0) {
                    *reinterpret_cast<HWND*>(lParam) = h;
                    return FALSE;
                }
            }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&hwnd));

        if (!hwnd) hwnd = FindWindowA(NULL, "Roblox");
        if (!hwnd) { std::cout << "[ESC] Window not found\n"; return; }

        SetForegroundWindow(hwnd);
        int tries = 0;
        while (GetForegroundWindow() != hwnd && tries++ < 20) {
            SetForegroundWindow(hwnd);
            Sleep(50);
        }

        INPUT inputs[2] = {};
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = VK_ESCAPE;
        inputs[0].ki.wScan = (WORD)MapVirtualKey(VK_ESCAPE, 0);
        inputs[1].type = INPUT_KEYBOARD;
        inputs[1].ki.wVk = VK_ESCAPE;
        inputs[1].ki.wScan = (WORD)MapVirtualKey(VK_ESCAPE, 0);
        inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(ARRAYSIZE(inputs), inputs, sizeof(INPUT));
        
        // Supplement with PostMessage in case SendInput is unredirected
        PostMessage(hwnd, WM_KEYDOWN, VK_ESCAPE, 0);
        PostMessage(hwnd, WM_KEYUP, VK_ESCAPE, 0);
        
        std::cout << "[ESC] Key sent (SendInput + PostMessage)\n";
    }

    static uintptr_t AllocateRemote(HANDLE h, size_t size) {
        void* addr = nullptr;
        SIZE_T rs = size;
        if (Syscall::AllocateMemory(h, &addr, rs, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE) != 0)
            return 0;
        return reinterpret_cast<uintptr_t>(addr);
    }

public:
    static void SetRobloxPid(DWORD pid) { sRobloxPid = pid; }

    // Restore ALL modified modules back to their original bytecode + capabilities
    static void RestoreAllModules() {
        if (sModifiedModules.empty() || !sLastProcessHandle) return;
        std::cout << "[RESTORE] Restoring " << sModifiedModules.size() << " modified modules...\n" << std::flush;
        for (auto& m : sModifiedModules) {
            if (rblx::IsValidInstance(sLastProcessHandle, m.moduleAddr)) {
                // Restore original bytecode pointer
                ProcessScanner::WriteMemory(sLastProcessHandle, m.embeddedAddr + 0x10, &m.originalBytecodePtr, sizeof(uintptr_t));
                // Restore original bytecode size
                ProcessScanner::WriteMemory(sLastProcessHandle, m.embeddedAddr + 0x20, &m.originalSize, sizeof(uint64_t));
                // Restore original capabilities
                ProcessScanner::WriteMemory(sLastProcessHandle, m.moduleAddr + offsets::Instance::InstanceCapabilities, &m.originalCapabilities, sizeof(uint64_t));
                std::cout << "[RESTORE] Module @ 0x" << std::hex << m.moduleAddr << " restored\n" << std::dec << std::flush;
            } else {
                std::cout << "[RESTORE] Module @ 0x" << std::hex << m.moduleAddr << " skipped (invalid process or memory)\n" << std::dec << std::flush;
            }
        }
        sModifiedModules.clear();
        std::cout << "[RESTORE] All modules restored successfully\n" << std::flush;
    }

    static void ResetState() {
        RestoreAllModules();
        sInjected = false; sCachedPLManager = 0; 
        sCachedModules = 0; sCachedCoreGui = 0; sUsedModules.clear();
        sLastProcessHandle = nullptr;
    }
};
