#pragma once
#include <string>

struct lua_State;

namespace api
{
    struct FunctionDef
    {
        const char* name;
        int(*func)(lua_State*);
    };

    void SetupEnvironment(lua_State* L);
    std::string WrapScript(const std::string& source);

    namespace functions
    {
        int GetGenv(lua_State* L);
        int GetGc(lua_State* L);
        int GetRenV(lua_State* L);
        int GetReg(lua_State* L);
        int GetRawMetatable(lua_State* L);
        int SetRawMetatable(lua_State* L);
        int HookMetamethod(lua_State* L);
        int GetHui(lua_State* L);
        int GetScriptIdentity(lua_State* L);
        int SetScriptIdentity(lua_State* L);
        int CheckCaller(lua_State* L);
        int FireClickDetector(lua_State* L);
        int FireTouchInterest(lua_State* L);
        int FireProximityPrompt(lua_State* L);
        int GetInstances(lua_State* L);
        int GetNilInstances(lua_State* L);
        int GetHiddenProperty(lua_State* L);
        int SetHiddenProperty(lua_State* L);
        int GetHashHiddenProperty(lua_State* L);
        int SetHashHiddenProperty(lua_State* L);
        int GetNamecallMethod(lua_State* L);
        int SetNamecallMethod(lua_State* L);
        int LoadString(lua_State* L);
        int NewCclosure(lua_State* L);
        int NewLclosure(lua_State* L);
        int CloneFunction(lua_State* L);
        int Decompile(lua_State* L);
        int IsLclosure(lua_State* L);
        int IsCclosure(lua_State* L);
        int IsExecutor(lua_State* L);
        int DumpString(lua_State* L);
        int ProtectString(lua_State* L);
        int Crypt(lua_State* L);
        int HttpGet(lua_State* L);
        int HttpPost(lua_State* L);
        int Request(lua_State* L);
        int SetClipboard(lua_State* L);
        int Messagebox(lua_State* L);
        int TakeScreenshot(lua_State* L);
        int GetFpsCap(lua_State* L);
        int SetFpsCap(lua_State* L);
        int Rconsole(lua_State* L);
        int RconsolePrint(lua_State* L);
        int RconsoleInput(lua_State* L);
        int RconsoleClear(lua_State* L);
        int RconsoleSetTitle(lua_State* L);
        int RconsoleShow(lua_State* L);
        int RconsoleHide(lua_State* L);
        int GetThreadIdentity(lua_State* L);
        int SetThreadIdentity(lua_State* L);
        int GetContext(lua_State* L);
        int SetContext(lua_State* L);
    }

    namespace tables
    {
        extern const FunctionDef genv[];
        extern const FunctionDef reg[];
        extern const FunctionDef renv[];
    }
}
