#pragma once
#include <Windows.h>
#include <cstdint>

#ifndef _NTDEF_
#define _NTDEF_
typedef LONG NTSTATUS;
#endif
#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif

typedef struct _MY_CLIENT_ID {
    HANDLE UniqueProcess;
    HANDLE UniqueThread;
} MY_CLIENT_ID;

class Syscall {
public:
    static bool Initialize();

    static NTSTATUS OpenProcess(PHANDLE h, ACCESS_MASK access, void* oa, void* cid);
    static NTSTATUS ReadMemory(HANDLE hProc, void* addr, void* buf, size_t size, SIZE_T* read);
    static NTSTATUS WriteMemory(HANDLE hProc, void* addr, const void* buf, size_t size, SIZE_T* written);
    static NTSTATUS AllocateMemory(HANDLE hProc, void** addr, SIZE_T size, ULONG type, ULONG protect);
    static NTSTATUS ProtectMemory(HANDLE hProc, void** addr, SIZE_T* size, ULONG newProt, ULONG* oldProt);
    static NTSTATUS SuspendProcess(HANDLE hProc);
    static NTSTATUS ResumeProcess(HANDLE hProc);
    static NTSTATUS CloseHandle(HANDLE h);
    static NTSTATUS CreateThreadEx(PHANDLE hThread, ACCESS_MASK access, PVOID objAttr, HANDLE hProc, PVOID start, PVOID param);
    static NTSTATUS FreeMemory(HANDLE hProc, void** addr, SIZE_T* size, ULONG type);
    static NTSTATUS GetContextThread(HANDLE hThread, CONTEXT* ctx);
    static NTSTATUS SetContextThread(HANDLE hThread, CONTEXT* ctx);
    static NTSTATUS ResumeThread(HANDLE hThread);
    static NTSTATUS WaitForSingleObject(HANDLE h, ULONG timeout);

private:
    static bool ParseExports(void* fileBase);
};

extern "C" {
    extern uint32_t currentSSN;
    NTSTATUS DoSyscall(void* a1, void* a2, void* a3, void* a4, void* a5, void* a6);
}
