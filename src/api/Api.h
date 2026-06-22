#pragma once
#include <cstdint>

extern "C" {
    __declspec(dllexport) bool __stdcall ElainaAttach();
    __declspec(dllexport) void __stdcall ElainaDetach();
    __declspec(dllexport) bool __stdcall ElainaExecute(const char* script);
    __declspec(dllexport) const char* __stdcall ElainaGetStatus();
    __declspec(dllexport) bool __stdcall ElainaInject();
    __declspec(dllexport) void __stdcall ElainaStartServer(int port);
    __declspec(dllexport) void __stdcall ElainaStopServer();
}
