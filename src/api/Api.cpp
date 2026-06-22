#include "Api.h"
#include "service/Session.h"
#include "service/ExecutionService.h"
#include "service/NetworkService.h"
#include "execution/Injector.h"
#include "core/Log.h"
#include <cstring>

static Session g_session;
static NetworkService g_network;
static std::string g_status = "Idle";

static std::string HandleRequest(const std::string& method, const std::string& path, const std::string& body) {
    if (path == "/compile" && method == "POST") {
        auto result = ExecutionService::CompileAndEncode(body);
        if (!result) return "Error: " + result.ErrorMsg();
        return std::string(result.value.begin(), result.value.end());
    }

    if (path == "/inject" && method == "POST") {
        if (!g_session.IsAttached()) return "Error: Not attached";
        std::vector<uint8_t> rsb1(body.begin(), body.end());
        if (Injector::Execute(g_session.Mem(), g_session.Tree(), rsb1))
            return "ok";
        return "Error: Inject failed";
    }

    return "Error: Unknown route";
}

extern "C" {

bool __stdcall ElainaAttach() {
    if (!g_session.Attach()) {
        g_status = "Failed to attach";
        return false;
    }
    g_status = "Attached to PID " + std::to_string(g_session.Handle().pid);
    return true;
}

void __stdcall ElainaDetach() {
    g_network.Stop();
    g_session.Detach();
    g_status = "Detached";
}

bool __stdcall ElainaExecute(const char* script) {
    if (!g_session.IsAttached()) {
        g_status = "Not attached";
        return false;
    }
    if (!ExecutionService::Execute(g_session, script)) {
        g_status = "Execution failed";
        return false;
    }
    g_status = "Executed OK";
    return true;
}

const char* __stdcall ElainaGetStatus() {
    return g_status.c_str();
}

bool __stdcall ElainaInject() {
    if (!ExecutionService::InjectUnc(g_session)) {
        g_status = "UNC inject failed";
        return false;
    }
    g_status = "UNC injected";
    return true;
}

void __stdcall ElainaStartServer(int port) {
    g_network.SetHandler(HandleRequest);
    g_network.Start(port);
    g_status = "Server running on port " + std::to_string(port);
}

void __stdcall ElainaStopServer() {
    g_network.Stop();
    g_status = "Server stopped";
}

} // extern "C"

BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        Log::Info("Elaina DLL loaded");
    }
    return TRUE;
}
