#include <Windows.h>
#include "communication/pipe_server.h"
#include "executor/executor.h"
#include "executor/luavm.h"
#include "hooks/hooks.h"
#include "framework/threading.h"

HINSTANCE g_instance = nullptr;

static bool Initialize()
{
    pipe_server::Start();

    lua::vm ctx;
    if (!lua::FindVM(&ctx))
        return false;

    executor::SetVM(&ctx);
    hooks::Install(&ctx);

    return true;
}

static void Cleanup()
{
    hooks::Uninstall();
    pipe_server::Stop();
}

BOOL APIENTRY DllMain(HINSTANCE inst, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_instance = inst;
        DisableThreadLibraryCalls(inst);

        threading::CreateDetachedThread(Initialize);
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        Cleanup();
    }

    return TRUE;
}
