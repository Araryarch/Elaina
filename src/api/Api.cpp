#include "Api.h"
#include "service/Session.h"
#include "service/ExecutionService.h"
#include "service/NetworkService.h"
#include "execution/Injector.h"
#include "process/Process.h"
#include "core/Log.h"
#include <cstring>
#include <stdexcept>

static Session g_session;
static NetworkService g_network;
static int g_state = ELAINA_IDLE;
static int g_pid = 0;
static std::string g_lastError;
static std::string g_lastDiag;

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
    try {
        if (!g_session.Attach()) {
            g_lastError = "Attach failed: no Roblox process found";
            g_state = ELAINA_ERROR;
            g_pid = 0;
            return false;
        }
        g_pid = g_session.Handle().pid;
        g_state = ELAINA_ATTACHED;
        g_lastError.clear();
        return true;
    } catch (const std::exception& e) {
        g_lastError = std::string("Attach exception: ") + e.what();
        Log::Error("ElainaAttach exception: %s", e.what());
        g_state = ELAINA_ERROR;
        g_pid = 0;
        return false;
    }
}

void __stdcall ElainaDetach() {
    try {
        g_network.Stop();
        g_session.Detach();
    } catch (const std::exception& e) {
        Log::Error("ElainaDetach exception: %s", e.what());
    }
    g_state = ELAINA_IDLE;
    g_pid = 0;
}

bool __stdcall ElainaExecute(const char* script) {
    try {
        if (g_state < ELAINA_ATTACHED) {
            g_lastError = "Not attached";
            return false;
        }
        if (!ExecutionService::Execute(g_session, script)) {
            g_lastError = "ExecutionService::Execute failed";
            return false;
        }
        g_lastError.clear();
        return true;
    } catch (const std::exception& e) {
        g_lastError = std::string("Execute exception: ") + e.what();
        Log::Error("ElainaExecute exception: %s", e.what());
        g_state = ELAINA_ERROR;
        return false;
    }
}

int __stdcall ElainaGetState() {
    try {
        if (g_state >= ELAINA_ATTACHED && g_pid && !Process::IsRunning(g_pid)) {
            g_session.Detach();
            g_state = ELAINA_IDLE;
            g_pid = 0;
        }
    } catch (const std::exception& e) {
        Log::Error("ElainaGetState exception: %s", e.what());
        g_state = ELAINA_ERROR;
    }
    return g_state;
}

int __stdcall ElainaGetPid() {
    return g_pid;
}

bool __stdcall ElainaInject() {
    try {
        if (g_state != ELAINA_ATTACHED) {
            g_lastError = "Not in ATTACHED state";
            return false;
        }
        if (!ExecutionService::InjectUnc(g_session)) {
            g_lastError = "Injection failed — see diagnose output";
            g_state = ELAINA_ERROR;
            return false;
        }
        g_state = ELAINA_INJECTED;
        g_lastError.clear();
        return true;
    } catch (const std::exception& e) {
        g_lastError = std::string("Inject exception: ") + e.what();
        Log::Error("ElainaInject exception: %s", e.what());
        g_state = ELAINA_ERROR;
        return false;
    }
}

void __stdcall ElainaStartServer(int port) {
    try {
        g_network.SetHandler(HandleRequest);
        g_network.Start(port);
    } catch (const std::exception& e) {
        Log::Error("ElainaStartServer exception: %s", e.what());
    }
}

void __stdcall ElainaStopServer() {
    try {
        g_network.Stop();
    } catch (const std::exception& e) {
        Log::Error("ElainaStopServer exception: %s", e.what());
    }
}

const char* __stdcall ElainaDiagnose() {
    try {
        if (!g_session.IsAttached()) {
            g_lastDiag = "Not attached. Call ElainaAttach() first.";
            return g_lastDiag.c_str();
        }
        g_lastDiag = Injector::Diagnose(g_session.Mem(), g_session.Tree());
        return g_lastDiag.c_str();
    } catch (const std::exception& e) {
        Log::Error("ElainaDiagnose exception: %s", e.what());
        g_lastDiag = std::string("Exception: ") + e.what();
        return g_lastDiag.c_str();
    }
}

const char* __stdcall ElainaGetLastError() {
    return g_lastError.c_str();
}

} // extern "C"

BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        Log::Info("Elaina DLL loaded");
    }
    return TRUE;
}
