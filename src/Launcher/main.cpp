#include <Windows.h>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <tlhelp32.h>
#include "../../src/Core/injector/injector.h"

void PrintBanner()
{
    std::cout << R"(
  _____ _       _       _
 | ____| | __ _(_)_ __ | |_ __ _ _ __   __ _
 |  _| | |/ _` | | '_ \| __/ _` | '_ \ / _` |
 | |___| | (_| | | | | | || (_| | | | | (_| |
 |_____|_|\__,_|_|_| |_|\__\__,_|_| |_|\__,_|

 Roblox Executor - Elaina v0.1
)" << std::endl;
}

void PrintUsage()
{
    std::cout << "Usage: ElainaLauncher [options]\n"
        << "Options:\n"
        << "  --auto       Auto-inject into RobloxPlayerBeta.exe\n"
        << "  --pid <n>    Target specific process ID\n"
        << "  --manual     Manual map injection (stealth)\n"
        << "  --cr         CreateRemoteThread injection (default)\n"
        << "  --wait       Wait for Roblox process\n"
        << "  --help       Show this help\n"
        << std::endl;
}

int main(int argc, char* argv[])
{
    PrintBanner();

    injector::Config cfg;
    bool auto_mode = false;
    bool wait_mode = false;
    DWORD target_pid = 0;

    // Parse args
    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg == "--auto" || arg == "-a")
            auto_mode = true;
        else if (arg == "--pid" && i + 1 < argc)
            target_pid = atoi(argv[++i]);
        else if (arg == "--manual" || arg == "-m")
            cfg.method = injector::Method::ManualMap;
        else if (arg == "--cr")
            cfg.method = injector::Method::CreateRemoteThread;
        else if (arg == "--wait" || arg == "-w")
            wait_mode = true;
        else if (arg == "--help" || arg == "-h")
        {
            PrintUsage();
            return 0;
        }
    }

    if (wait_mode)
    {
        std::cout << "[*] Waiting for RobloxPlayerBeta.exe..." << std::endl;
        injector::ProcessInfo proc;
        while (!injector::FindTarget(L"RobloxPlayerBeta.exe", proc))
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        target_pid = proc.pid;
        std::cout << "[+] Found Roblox (PID: " << target_pid << ")" << std::endl;
    }

    injector::ProcessInfo target;
    if (target_pid)
    {
        target.pid = target_pid;
        target.handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, target_pid);
        target.name = L"RobloxPlayerBeta.exe";
    }
    else if (auto_mode || wait_mode)
    {
        if (!injector::FindTarget(L"RobloxPlayerBeta.exe", target))
        {
            std::cerr << "[-] RobloxPlayerBeta.exe not found." << std::endl;
            return 1;
        }
    }
    else
    {
        PrintUsage();
        return 1;
    }

    if (!target.handle)
    {
        std::cerr << "[-] Failed to open target process (run as admin?)" << std::endl;
        return 1;
    }

    // Path to our DLL
    char dll_path[MAX_PATH];
    GetModuleFileNameA(nullptr, dll_path, MAX_PATH);
    std::string exe_path = dll_path;
    size_t pos = exe_path.find_last_of("\\/");
    std::string dll_dir = exe_path.substr(0, pos);
    std::string dll_full = dll_dir + "\\ElainaCore.dll";

    std::cout << "[*] Target PID: " << target.pid << std::endl;
    std::cout << "[*] DLL: " << dll_full << std::endl;
    std::cout << "[*] Method: "
        << (cfg.method == injector::Method::ManualMap ? "Manual Map" : "CreateRemoteThread")
        << std::endl;

    // Inject
    std::cout << "[*] Injecting..." << std::endl;

    if (injector::InjectDLL(target, dll_full, cfg))
    {
        std::cout << "[+] Injection successful!" << std::endl;
        std::cout << "[*] Elaina is now running in Roblox." << std::endl;
    }
    else
    {
        std::cerr << "[-] Injection failed (error: "
            << GetLastError() << ")" << std::endl;
        CloseHandle(target.handle);
        return 1;
    }

    CloseHandle(target.handle);

    if (wait_mode)
    {
        std::cout << "[*] Press Ctrl+C to detach..." << std::endl;
        while (true)
            std::this_thread::sleep_for(std::chrono::seconds(10));
    }

    return 0;
}
