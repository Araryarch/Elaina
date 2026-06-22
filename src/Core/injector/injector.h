#pragma once
#include <Windows.h>
#include <string>
#include <vector>

namespace injector
{
    enum class Method
    {
        CreateRemoteThread,
        ManualMap,
        ThreadHijack,
        SetWindowsHookEx,
        APCFlushInstructionCache
    };

    struct Config
    {
        Method method = Method::CreateRemoteThread;
        std::wstring target_process = L"RobloxPlayerBeta.exe";
        bool run_as_admin = false;
        bool hide_from_peb = true;
        int timeout_ms = 10000;
    };

    struct ProcessInfo
    {
        DWORD pid;
        HANDLE handle;
        std::wstring name;
    };

    bool FindTarget(const std::wstring& name, ProcessInfo& out);
    bool InjectDLL(const ProcessInfo& proc, const std::string& dll_path, const Config& cfg = Config());
    bool InjectDLL(DWORD pid, const std::string& dll_path, const Config& cfg = Config());
    bool EjectDLL(DWORD pid, const std::string& dll_name);

    namespace detail
    {
        bool InjectCRT(const ProcessInfo& proc, const std::string& dll_path);
        bool InjectManualMap(const ProcessInfo& proc, const std::string& dll_path);
        bool InjectThreadHijack(const ProcessInfo& proc, const std::string& dll_path);
    }
}
