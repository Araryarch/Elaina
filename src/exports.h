#pragma once
#include <windows.h>
#include <string>

#ifdef SYNTAX_EXPORT
#define SYNTAX_API __declspec(dllexport)
#else
#define SYNTAX_API __declspec(dllimport)
#endif

extern "C" {
    // Basic lifecycle
    SYNTAX_API bool __stdcall Initialize();
    SYNTAX_API DWORD __stdcall FindRobloxProcess();
    SYNTAX_API bool __stdcall Connect(DWORD pid);
    SYNTAX_API void __stdcall Disconnect();
    SYNTAX_API DWORD __stdcall GetRobloxPid();
    SYNTAX_API void __stdcall RedirConsole();

    // DataModel discovery
    SYNTAX_API uintptr_t __stdcall GetDataModel();
    SYNTAX_API int __stdcall GetJobCount();

    // Script execution
    SYNTAX_API int __stdcall ExecuteScript(const char* source, int sourceLen);
    SYNTAX_API int __stdcall GetLastExecError(char* buffer, int bufLen);

    // Memory operations (wrapped for C# ease)
    SYNTAX_API bool __stdcall ReadMemory(uintptr_t address, void* buffer, size_t size);
    SYNTAX_API bool __stdcall WriteMemory(uintptr_t address, const void* buffer, size_t size);

    // Dynamic data
    SYNTAX_API bool __stdcall GetClientInfo(char* buffer, int maxSize);
}
