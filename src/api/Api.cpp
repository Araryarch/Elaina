#include "Api.h"
#include "core/Log.h"
#include "core/Result.h"
#include "execution/Compiler.h"
#include "execution/Encoder.h"
#include "execution/Injector.h"
#include "execution/Payload.h"
#include "process/Process.h"
#include "process/Memory.h"
#include "network/Server.h"
#include "network/Proxy.h"

static RobloxHandle g_process;
static std::unique_ptr<Memory> g_memory;
static Server g_server;
static std::string g_status = "Idle";

static std::string HandleRequest(const std::string& method, const std::string& path, const std::string& body) {
    if (path == "/compile" && method == "POST") {
        auto compiled = Compiler::Compile(body);
        if (!compiled) return "Error: " + compiled.error();

        auto encoded = Encoder::Encode(compiled.value);
        if (!encoded) return "Error: " + encoded.error();

        return encoded.value;
    }

    if (path == "/inject" && method == "POST") {
        if (!g_memory) return "Error: Not attached";
        std::vector<uint8_t> rsb1(body.begin(), body.end());
        if (Injector::Execute(*g_memory, rsb1))
            return "ok";
        return "Error: Inject failed";
    }

    return "Error: Unknown route";
}

extern "C" {

bool __stdcall ElainaAttach() {
    Log::Info("ElainaAttach called");

    auto processes = RobloxHandle::FindAll();
    if (processes.empty()) {
        g_status = "Roblox not found";
        Log::Error("No Roblox processes found");
        return false;
    }

    g_process = std::move(processes[0]);
    g_memory = std::make_unique<Memory>(g_process.Handle());
    g_status = "Attached to PID " + std::to_string(g_process.Pid());

    Log::Info("Attached to Roblox PID %d", g_process.Pid());
    return true;
}

void __stdcall ElainaDetach() {
    g_server.Stop();
    g_memory.reset();
    g_process = RobloxHandle{};
    g_status = "Detached";
    Log::Info("ElainaDetach done");
}

bool __stdcall ElainaExecute(const char* script) {
    if (!g_memory) {
        g_status = "Not attached";
        Log::Error("Execute called but not attached");
        return false;
    }

    Log::Info("Compiling script (%zu chars)", strlen(script));

    auto compiled = Compiler::Compile(script);
    if (!compiled) {
        g_status = "Compile failed: " + compiled.error();
        Log::Error("Compile failed: %s", compiled.error().c_str());
        return false;
    }

    auto encoded = Encoder::Encode(compiled.value);
    if (!encoded) {
        g_status = "Encode failed: " + encoded.error();
        Log::Error("Encode failed: %s", encoded.error().c_str());
        return false;
    }

    std::vector<uint8_t> rsb1(encoded.value.begin(), encoded.value.end());
    if (!Injector::Execute(*g_memory, rsb1)) {
        g_status = "Inject failed";
        return false;
    }

    g_status = "Executed ok";
    Log::Info("Script executed successfully");
    return true;
}

const char* __stdcall ElainaGetStatus() {
    return g_status.c_str();
}

bool __stdcall ElainaInject() {
    return ElainaExecute(kUncPayload);
}

void __stdcall ElainaStartServer(int port) {
    g_server.OnRequest(HandleRequest);
    g_server.Start(port);
    g_status = "Server running on port " + std::to_string(port);
    Log::Info("ElainaStartServer on port %d", port);
}

void __stdcall ElainaStopServer() {
    g_server.Stop();
    g_status = "Server stopped";
    Log::Info("ElainaStopServer");
}

} // extern "C"

BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        Log::Init("Elaina");
        Log::Info("Elaina DLL loaded");
    }
    return TRUE;
}
