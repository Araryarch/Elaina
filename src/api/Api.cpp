#include "Api.h"
#include "service/Session.h"
#include "service/ExecutionService.h"
#include "service/NetworkService.h"
#include "execution/Injector.h"
#include "process/Process.h"
#include "core/Log.h"
#include <cstring>

static Session g_session;
static NetworkService g_network;
static int g_state = ELAINA_IDLE;
static int g_pid = 0;

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
        g_state = ELAINA_ERROR;
        g_pid = 0;
        return false;
    }
    g_pid = g_session.Handle().pid;
    g_state = ELAINA_ATTACHED;
    return true;
}

void __stdcall ElainaDetach() {
    g_network.Stop();
    g_session.Detach();
    g_state = ELAINA_IDLE;
    g_pid = 0;
}

bool __stdcall ElainaExecute(const char* script) {
    if (g_state < ELAINA_ATTACHED) return false;
    if (!ExecutionService::Execute(g_session, script)) return false;
    return true;
}

int __stdcall ElainaGetState() {
    if (g_state >= ELAINA_ATTACHED && g_pid && !Process::IsRunning(g_pid)) {
        g_session.Detach();
        g_state = ELAINA_IDLE;
        g_pid = 0;
    }
    return g_state;
}

int __stdcall ElainaGetPid() {
    return g_pid;
}

bool __stdcall ElainaInject() {
    if (g_state != ELAINA_ATTACHED) return false;
    if (!ExecutionService::InjectUnc(g_session)) {
        g_state = ELAINA_ERROR;
        return false;
    }
    g_state = ELAINA_INJECTED;
    return true;
}

void __stdcall ElainaStartServer(int port) {
    g_network.SetHandler(HandleRequest);
    g_network.Start(port);
}

void __stdcall ElainaStopServer() {
    g_network.Stop();
}

} // extern "C"

BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        Log::Info("Elaina DLL loaded");
    }
    return TRUE;
}
