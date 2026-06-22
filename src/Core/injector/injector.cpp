#include "injector.h"
#include "../framework/memory.h"
#include <tlhelp32.h>
#include <string>
#include <cstring>

namespace injector
{
    bool FindTarget(const std::wstring& name, ProcessInfo& out)
    {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) return false;

        PROCESSENTRY32W pe = { sizeof(PROCESSENTRY32W) };
        bool found = false;

        if (Process32FirstW(snap, &pe))
        {
            do
            {
                if (_wcsicmp(pe.szExeFile, name.c_str()) == 0)
                {
                    out.pid = pe.th32ProcessID;
                    out.name = pe.szExeFile;
                    out.handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, out.pid);
                    found = true;
                    break;
                }
            } while (Process32NextW(snap, &pe));
        }

        CloseHandle(snap);
        return found;
    }

    bool InjectDLL(const ProcessInfo& proc, const std::string& dll_path, const Config& cfg)
    {
        if (!proc.handle) return false;

        switch (cfg.method)
        {
        case Method::CreateRemoteThread:
            return detail::InjectCRT(proc, dll_path);
        case Method::ManualMap:
            return detail::InjectManualMap(proc, dll_path);
        case Method::ThreadHijack:
            return detail::InjectThreadHijack(proc, dll_path);
        default:
            return detail::InjectCRT(proc, dll_path);
        }
    }

    bool InjectDLL(DWORD pid, const std::string& dll_path, const Config& cfg)
    {
        ProcessInfo proc;
        HANDLE h = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
        if (!h) return false;

        proc.pid = pid;
        proc.handle = h;
        return InjectDLL(proc, dll_path, cfg);
    }

    bool EjectDLL(DWORD pid, const std::string& dll_name)
    {
        HANDLE h = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
        if (!h) return false;

        // Find the module base address
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
        if (snap == INVALID_HANDLE_VALUE) return false;

        MODULEENTRY32W me = { sizeof(MODULEENTRY32W) };
        uintptr_t mod_base = 0;

        if (Module32FirstW(snap, &me))
        {
            std::wstring wname(dll_name.begin(), dll_name.end());
            do
            {
                if (_wcsicmp(me.szModule, wname.c_str()) == 0)
                {
                    mod_base = (uintptr_t)me.modBaseAddr;
                    break;
                }
            } while (Module32NextW(snap, &me));
        }

        CloseHandle(snap);
        if (!mod_base) return false;

        // Call FreeLibrary in remote process
        auto free_lib = (LPTHREAD_START_ROUTINE)GetProcAddress(
            GetModuleHandleA("kernel32.dll"), "FreeLibrary");

        HANDLE thread = CreateRemoteThread(h, nullptr, 0,
            free_lib, (LPVOID)mod_base, 0, nullptr);

        if (!thread) return false;

        WaitForSingleObject(thread, 5000);
        CloseHandle(thread);
        CloseHandle(h);
        return true;
    }

    namespace detail
    {
        bool InjectCRT(const ProcessInfo& proc, const std::string& dll_path)
        {
            size_t path_size = (dll_path.size() + 1) * sizeof(char);

            // Allocate memory in target process
            LPVOID remote_path = VirtualAllocEx(proc.handle, nullptr,
                path_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            if (!remote_path) return false;

            // Write DLL path
            if (!WriteProcessMemory(proc.handle, remote_path,
                dll_path.c_str(), path_size, nullptr))
            {
                VirtualFreeEx(proc.handle, remote_path, 0, MEM_RELEASE);
                return false;
            }

            // Get LoadLibraryA address
            auto load_lib = (LPTHREAD_START_ROUTINE)GetProcAddress(
                GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
            if (!load_lib) return false;

            // Create remote thread
            HANDLE thread = CreateRemoteThread(proc.handle, nullptr, 0,
                load_lib, remote_path, 0, nullptr);

            if (!thread) return false;

            WaitForSingleObject(thread, 5000);
            CloseHandle(thread);

            VirtualFreeEx(proc.handle, remote_path, 0, MEM_RELEASE);
            return true;
        }

        bool InjectManualMap(const ProcessInfo& proc, const std::string& dll_path)
        {
            // Manual mapping - more advanced, hides from PEB
            // Requires parsing PE headers, relocating, resolving imports
            // This is a complex implementation - stub here
            return InjectCRT(proc, dll_path); // fallback
        }

        bool InjectThreadHijack(const ProcessInfo& proc, const std::string& dll_path)
        {
            // Hijack an existing thread instead of creating a new one
            // More stealthy than CreateRemoteThread

            // Find first thread in target process
            HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
            if (snap == INVALID_HANDLE_VALUE) return false;

            THREADENTRY32 te = { sizeof(THREADENTRY32) };
            DWORD thread_id = 0;

            if (Thread32First(snap, &te))
            {
                do
                {
                    if (te.th32OwnerProcessID == proc.pid)
                    {
                        thread_id = te.th32ThreadID;
                        break;
                    }
                } while (Thread32Next(snap, &te));
            }

            CloseHandle(snap);
            if (!thread_id) return false;

            // Allocate + write path in target (as CRT method)
            size_t path_size = (dll_path.size() + 1) * sizeof(char);
            LPVOID remote_path = VirtualAllocEx(proc.handle, nullptr,
                path_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            if (!remote_path) return false;

            WriteProcessMemory(proc.handle, remote_path,
                dll_path.c_str(), path_size, nullptr);

            // Open thread, suspend, set context to call LoadLibraryA, resume
            HANDLE thread = OpenThread(THREAD_ALL_ACCESS, FALSE, thread_id);
            if (!thread) return false;

            SuspendThread(thread);

            CONTEXT ctx;
            ctx.ContextFlags = CONTEXT_CONTROL;
            GetThreadContext(thread, &ctx);

            auto load_lib = (uintptr_t)GetProcAddress(
                GetModuleHandleA("kernel32.dll"), "LoadLibraryA");

            // Set up remote call via ROP-style execution
            // x64: set RSP, push return, set RIP
            ctx.Rsp -= 8;
            uintptr_t zero = 0;
            memory::WriteRaw(ctx.Rsp, &zero, sizeof(zero)); // return address (crash)
            ctx.Rip = load_lib;
            ctx.Rcx = (uintptr_t)remote_path; // first arg

            SetThreadContext(thread, &ctx);
            ResumeThread(thread);

            CloseHandle(thread);
            return true;
        }
    }
}
