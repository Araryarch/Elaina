#pragma once
#include <windows.h>
#include <string>

#ifdef ELAINA_EXPORT
#define ELAINA_API __declspec(dllexport)
#else
#define ELAINA_API __declspec(dllimport)
#endif

extern "C" {
    // Basic lifecycle
    ELAINA_API bool __stdcall Initialize();
    ELAINA_API DWORD __stdcall FindRobloxProcess();
    ELAINA_API bool __stdcall Connect(DWORD pid);
    ELAINA_API void __stdcall Disconnect();
    ELAINA_API DWORD __stdcall GetRobloxPid();
    ELAINA_API void __stdcall RedirConsole();

    // DataModel discovery
    ELAINA_API uintptr_t __stdcall GetDataModel();
    ELAINA_API int __stdcall GetJobCount();

    // Script execution
    ELAINA_API int __stdcall ExecuteScript(const char* source, int sourceLen);
    ELAINA_API int __stdcall GetLastExecError(char* buffer, int bufLen);

    // Memory operations (wrapped for C# ease)
    ELAINA_API bool __stdcall ReadMemory(uintptr_t address, void* buffer, size_t size);
    ELAINA_API bool __stdcall WriteMemory(uintptr_t address, const void* buffer, size_t size);

    // Dynamic data
    ELAINA_API bool __stdcall GetClientInfo(char* buffer, int maxSize);
}
