#pragma once
#include <cstdint>

enum ElainaState : int {
    ELAINA_IDLE = 0,
    ELAINA_ATTACHED = 1,
    ELAINA_INJECTED = 2,
    ELAINA_ERROR = -1
};

extern "C" {
    __declspec(dllexport) bool __stdcall ElainaAttach();
    __declspec(dllexport) void __stdcall ElainaDetach();
    __declspec(dllexport) bool __stdcall ElainaExecute(const char* script);
    __declspec(dllexport) int  __stdcall ElainaGetState();
    __declspec(dllexport) int  __stdcall ElainaGetPid();
    __declspec(dllexport) bool __stdcall ElainaInject();
    __declspec(dllexport) void __stdcall ElainaStartServer(int port);
    __declspec(dllexport) void __stdcall ElainaStopServer();
    __declspec(dllexport) const char* __stdcall ElainaDiagnose();
}
