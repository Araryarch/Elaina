#pragma once

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>

// Ensure SSL is disabled — we only use localhost HTTP
#undef CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "execution/luau_wrapper.h"
#include "execution/rsb1_encoder.h"
#include "process/scanner.h"
#include "instance_utils.h"
#include "winhttp_client.h"

#include <iostream>
#include <thread>
#include <atomic>
#include <string>
#include <mutex>
#include <functional>
#include <chrono>

using json = nlohmann::json;

class HttpServer {
public:
    static constexpr int PORT = 9753;
    static constexpr const char* HOST = "127.0.0.1";

    using FindModuleFn = std::function<uintptr_t(HANDLE, std::string&)>;
    using RestoreFn = std::function<void()>;
    // Callback to track a modified module: (moduleAddr, embeddedAddr, origPtr, origSize, origCaps, hProcess)
    using TrackModuleFn = std::function<void(uintptr_t, uintptr_t, uintptr_t, uint64_t, uint64_t, HANDLE)>;

private:
    static inline httplib::Server* sServer = nullptr;
    static inline std::thread sServerThread;
    static inline std::atomic<bool> sRunning{false};
    static inline FindModuleFn sFindModuleCallback = nullptr;
    static inline RestoreFn sRestoreCallback = nullptr;
    static inline TrackModuleFn sTrackModuleCallback = nullptr;
    
    // Roblox process state — set by executor
    static inline HANDLE sProcessHandle = nullptr;
    static inline uintptr_t sDataModel = 0;
    static inline uintptr_t sBase = 0;

    // Pending execute queue
    static inline std::mutex sExecMutex;
    static inline std::string sPendingScript;
    static inline bool sHasPendingScript = false;

    // Heartbeat tracking
    static inline std::atomic<std::chrono::steady_clock::time_point> sLastPollTime{std::chrono::steady_clock::now()};
    static inline std::thread sWatchdogThread;
    static inline std::atomic<bool> sSessionAlive{false};

public:
    // ========================================================================
    // Start the HTTP server in a background thread
    // ========================================================================
    static void Start(FindModuleFn findFn = nullptr, RestoreFn restoreFn = nullptr, TrackModuleFn trackFn = nullptr) {
        if (findFn) sFindModuleCallback = findFn;
        if (restoreFn) sRestoreCallback = restoreFn;
        if (trackFn) sTrackModuleCallback = trackFn;
        if (sRunning) return;
        
        sServer = new httplib::Server();
        
        // --- Health check ---
        sServer->Get("/ping", [](const httplib::Request&, httplib::Response& res) {
            res.set_content("pong", "text/plain");
        });

        // --- Loadstring endpoint ---
        // Lua sends source code, we compile it and write bytecode to the named module
        sServer->Post("/ls", [](const httplib::Request& req, httplib::Response& res) {
            HandleLoadstring(req, res);
        });

        // --- HTTP proxy endpoint ---
        // Lua sends URL/method/headers, we make the HTTP request and return the result
        sServer->Post("/req", [](const httplib::Request& req, httplib::Response& res) {
            HandleHttpRequest(req, res);
        });

        // --- Execute endpoint ---  
        // C++ posts script here, Lua polls for it
        sServer->Post("/exec", [](const httplib::Request& req, httplib::Response& res) {
            std::lock_guard<std::mutex> lock(sExecMutex);
            std::cout << "[HTTP/POLL] Script queued: " << req.body.size() << " bytes\n";
            sPendingScript = req.body;
            sHasPendingScript = true;
            res.set_content("ok", "text/plain");
        });

        // --- Poll endpoint ---
        // Lua polls this to check for pending scripts
        sServer->Get("/poll", [](const httplib::Request&, httplib::Response& res) {
            sLastPollTime.store(std::chrono::steady_clock::now());
            sSessionAlive.store(true);
            std::lock_guard<std::mutex> lock(sExecMutex);
            if (sHasPendingScript) {
                std::cout << "[HTTP/POLL] Dispatching script to game...\n";
                res.set_content(sPendingScript, "text/plain");
                sHasPendingScript = false;
                sPendingScript.clear();
            } else {
                res.status = 204;
            }
        });

        // --- Cleanup endpoint ---
        // Lua sends this before server leave so we can restore modules
        sServer->Post("/cleanup", [](const httplib::Request&, httplib::Response& res) {
            std::cout << "[HTTP] Cleanup signal received from Lua! Restoring modules...\n" << std::flush;
            if (sRestoreCallback) sRestoreCallback();
            sSessionAlive.store(false);
            res.set_content("ok", "text/plain");
        });

        sRunning = true;
        sServerThread = std::thread([]() {
            std::cout << "[HTTP] Server starting on " << HOST << ":" << PORT << "\n";
            sServer->listen(HOST, PORT);
        });
        sServerThread.detach();
        
        // Start the heartbeat watchdog thread
        StartWatchdog();
        
        // Give server time to start
        Sleep(100);
        std::cout << "[HTTP] Server ready\n";
    }

    static void Stop() {
        if (sServer && sRunning) {
            sServer->stop();
            sRunning = false;
        }
    }

    static bool IsRunning() { return sRunning; }

    // Queue a script for the Lua polling loop to pick up
    static void QueueScript(const std::string& source) {
        std::lock_guard<std::mutex> lock(sExecMutex);
        sPendingScript = source;
        sHasPendingScript = true;
    }

    // Set Roblox process context (called from executor)
    static void SetContext(HANDLE hProcess, uintptr_t base, uintptr_t dataModel) {
        sProcessHandle = hProcess;
        sBase = base;
        sDataModel = dataModel;
        sLastPollTime.store(std::chrono::steady_clock::now());
    }

private:
    // Heartbeat watchdog — detects dead Lua session and restores modules
    static void StartWatchdog() {
        sWatchdogThread = std::thread([]() {
            while (sRunning) {
                Sleep(1000);
                if (!sSessionAlive.load()) continue;
                
                auto now = std::chrono::steady_clock::now();
                auto lastPoll = sLastPollTime.load();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastPoll).count();
                
                if (elapsed > 3000) {
                    std::cout << "[WATCHDOG] No heartbeat for " << elapsed << "ms — Lua session dead. Restoring modules...\n" << std::flush;
                    if (sRestoreCallback) sRestoreCallback();
                    sSessionAlive.store(false);
                }
            }
        });
        sWatchdogThread.detach();
    }
    // ========================================================================
    // /ls — Loadstring handler
    // Receives Lua source in POST body
    // Returns JSON: {"path": "game.CoreGui...", "name": "..."}
    // ========================================================================
    static void HandleLoadstring(const httplib::Request& req, httplib::Response& res) {
        std::string source = req.body;
        // Allows empty source (e.g. loadstring(""))

        if (!sProcessHandle || !sFindModuleCallback) {
            std::cout << "[HTTP/LS] ERROR: Process not connected or module finder not set\n";
            res.status = 500;
            res.set_content("No process or module finder", "text/plain");
            return;
        }

        std::string moduleName;
        uintptr_t module = sFindModuleCallback(sProcessHandle, moduleName);

        if (!module) {
            std::cout << "[HTTP/LS] ERROR: No available unloaded modules\n";
            res.status = 500;
            res.set_content("No available modules", "text/plain");
            return;
        }

        std::vector<std::string> fullPath = rblx::GetFullInstancePath(sProcessHandle, module);
        std::cout << "[HTTP/LS] Compiling for module '" << moduleName << "' @ 0x" << std::hex << module << std::dec << " (" << source.size() << " bytes)...\n";

        // Wrap as module function matching spec format:
        // local module = {}; module["Syntax"] = function() <source> end; return module
        std::string wrappedSource = 
            "local module = {}\n"
            "module[\"Syntax\"] = function()\n"
            + source + "\n"
            "end\n"
            "return module";

        // Compile
        auto [ok, bytecodeOrErr] = LuauCompiler::Compile(wrappedSource);
        if (!ok) {
            std::cout << "[HTTP/LS] Compile error: " << bytecodeOrErr << "\n";
            res.status = 400;
            res.set_content(bytecodeOrErr, "text/plain");
            return;
        }

        // BLAKE3 sign + RSB1 encode
        std::string rsb1 = RSB1Encoder::Encode(bytecodeOrErr);
        if (rsb1.empty()) {
            res.status = 500;
            res.set_content("RSB1 encoding failed", "text/plain");
            return;
        }

        // Write bytecode to the module
        if (!SetBytecode(sProcessHandle, module, rsb1)) {
            std::cout << "[HTTP/LS] ERROR: SetBytecode failed\n";
            res.status = 500;
            res.set_content("Failed to write bytecode", "text/plain");
            return;
        }

        std::cout << "[HTTP/LS] Wrote " << rsb1.size() << " bytes bytecode\n";
        json resp;
        resp["path"] = fullPath;
        resp["name"] = moduleName;
        res.set_content(resp.dump(), "application/json");
    }

    // ========================================================================
    // /req — HTTP proxy handler
    // Lua sends JSON: {url, method, headers, body}
    // We make the request and return the response
    // ========================================================================
    static void HandleHttpRequest(const httplib::Request& req, httplib::Response& res) {
        try {
            json requestData = json::parse(req.body);
            
            std::string url = requestData.value("url", "");
            std::string method = requestData.value("method", "GET");
            
            if (url.empty()) {
                res.status = 400;
                res.set_content("{\"success\":false,\"message\":\"Missing URL\"}", "application/json");
                return;
            }

            std::cout << "[HTTP/REQ] " << method << " " << url << "\n";

            // Parse URL into host + path
            auto schemeEnd = url.find("://");
            if (schemeEnd == std::string::npos) {
                res.status = 400;
                res.set_content("{\"success\":false,\"message\":\"Invalid URL\"}", "application/json");
                return;
            }

            auto pathStart = url.find('/', schemeEnd + 3);
            std::string host = (pathStart == std::string::npos) ? url : url.substr(0, pathStart);
            std::string path = (pathStart == std::string::npos) ? "/" : url.substr(pathStart);

            std::map<std::string, std::string> headers;
            if (requestData.contains("headers") && requestData["headers"].is_object()) {
                for (auto& [key, val] : requestData["headers"].items()) {
                    headers[key] = val.get<std::string>();
                }
            }

            std::string body = requestData.value("body", "");

            auto result = WinHttpClient::MakeRequest(method, url, headers, body);

            if (result.status_code == 0) {
                res.status = 502; // Bad Gateway
                json resp;
                resp["success"] = false;
                resp["message"] = "HTTP proxy request failed completely";
                res.set_content(resp.dump(), "application/json");
                return;
            }

            json resp;
            resp["success"] = true;
            resp["StatusCode"] = result.status_code;
            resp["Body"] = result.body;
            json respHeaders;
            for (auto& [k, v] : result.headers) {
                respHeaders[k] = v;
            }
            resp["Headers"] = respHeaders;
            res.set_content(resp.dump(), "application/json");

        } catch (const std::exception& e) {
            json resp;
            resp["success"] = false;
            resp["message"] = std::string("Exception: ") + e.what();
            res.set_content(resp.dump(), "application/json");
        }
    }

    static bool SetBytecode(HANDLE h, uintptr_t moduleScript, const std::string& rsb1Data) {
        uintptr_t embedded = ProcessScanner::Read<uintptr_t>(h, moduleScript + 0x150);
        if (embedded < 0x10000 || embedded > 0x7FFFFFFFFFFF) return false;

        // Save originals before overwriting (use tracking callback)
        if (sTrackModuleCallback) {
            uintptr_t origPtr = ProcessScanner::Read<uintptr_t>(h, embedded + 0x10);
            uint64_t origSize = ProcessScanner::Read<uint64_t>(h, embedded + 0x20);
            uint64_t origCaps = ProcessScanner::Read<uint64_t>(h, moduleScript + offsets::Instance::InstanceCapabilities);
            sTrackModuleCallback(moduleScript, embedded, origPtr, origSize, origCaps, h);
        }

        void* addr = nullptr;
        SIZE_T rs = rsb1Data.size();
        if (Syscall::AllocateMemory(h, &addr, rs, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE) != 0)
            return false;
        uintptr_t remoteData = reinterpret_cast<uintptr_t>(addr);

        if (!ProcessScanner::WriteMemory(h, remoteData, rsb1Data.data(), rsb1Data.size()))
            return false;

        ProcessScanner::WriteMemory(h, embedded + 0x10, &remoteData, sizeof(uintptr_t));
        uint64_t newSize = rsb1Data.size();
        ProcessScanner::WriteMemory(h, embedded + 0x20, &newSize, sizeof(uint64_t));

        // Patch SecurityCapabilities to maximum (Spoofing identity/permissions)
        uint64_t maxCapabilities = 0x3FFFFFFF;
        ProcessScanner::WriteMemory(h, moduleScript + offsets::Instance::InstanceCapabilities, &maxCapabilities, sizeof(maxCapabilities));

        return true;
    }
};
