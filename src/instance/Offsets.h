#pragma once
// Roblox version-d599f7fc52a8404c — update on every Roblox patch
#include <cstdint>
#include <unordered_map>
#include <string>

// Compile-time defaults — can be overridden at runtime
struct OffsetTable {
    // TaskScheduler
    uintptr_t TaskScheduler_Ptr = 0x801D408;
    uintptr_t TS_JobStart = 0x1D0;
    uintptr_t TS_JobEnd = 0x1D8;
    uintptr_t TS_MaxFPS = 0x1B0;

    // Jobs
    uintptr_t Job_Name = 0x18;
    uintptr_t RenderJob_DataModel = 0x1B0;
    uintptr_t RenderJob_FakeDataModel = 0x38;
    uintptr_t RenderJob_RenderView = 0x218;

    // VisualEngine
    uintptr_t VE_Ptr = 0x7B1F068;
    uintptr_t VE_ToDM1 = 0x840;
    uintptr_t VE_ToDM2 = 0x6F0;
    uintptr_t VE_Dimensions = 0xA70;
    uintptr_t VE_ViewMatrix = 0x140;

    // FakeDataModel
    uintptr_t FDM_Ptr = 0x7F6C228;
    uintptr_t FDM_DataModel = 0x1C0;
    uintptr_t FDM_Deleter = 0x7F6C230;

    // Instance
    uintptr_t Inst_Children = 0x70;
    uintptr_t Inst_ChildrenEnd = 0x8;
    uintptr_t Inst_ClassDesc = 0x18;
    uintptr_t Inst_CD_ClassName = 0x8;
    uintptr_t Inst_Parent = 0x68;
    uintptr_t Inst_Name = 0xB0;
    uintptr_t Inst_Capabilities = 0x208;

    // DataModel
    uintptr_t DM_ScriptContext = 0x3F0;
    uintptr_t DM_Workspace = 0x178;
    uintptr_t DM_PlaceId = 0x198;
    uintptr_t DM_GameId = 0x190;
    uintptr_t DM_JobId = 0x138;
    uintptr_t DM_GameLoaded = 0x638;
    uintptr_t DM_CreatorId = 0x188;

    // Players
    uintptr_t Players_LocalPlayer = 0x130;

    // Player
    uintptr_t Player_UserId = 0x2C8;
    uintptr_t Player_DisplayName = 0x130;
    uintptr_t Player_ModelInstance = 0x380;

    // ModuleScript
    uintptr_t MS_Bytecode = 0x150;
    uintptr_t MS_BytecodePtr = 0x10;
    uintptr_t MS_Hash = 0x160;
    uintptr_t MS_ProtectedString = 0x140;

    // ProtectedString
    uintptr_t PS_Data = 0x0;
    uintptr_t PS_Size = 0x8;
    uintptr_t PS_Capabilities = 0x10;

    // Lua state (from ScriptContext)
    uintptr_t SC_LuaState = 0x130;

    // LocalScript
    uintptr_t LS_Bytecode = 0x1A8;
    uintptr_t LS_BytecodePtr = 0x10;
    uintptr_t LS_RunContext = 0x148;

    // PlayerListManager
    uintptr_t PLM_SpoofTarget = 0x8;

    // ScriptContext
    uintptr_t SC_RequireBypass = 0x920;

    // Workspace
    uintptr_t WS_Camera = 0x460;
    uintptr_t WS_Gravity = 0x978;

    // Camera
    uintptr_t Cam_FOV = 0x160;
    uintptr_t Cam_CFrame = 0xF8;

    // BasePart
    uintptr_t BP_CFrame = 0xC0;
    uintptr_t BP_Position = 0xE4;
    uintptr_t BP_Size = 0x1B0;
    uintptr_t BP_Transparency = 0xF0;
    uintptr_t BP_Anchored = 0x1AE;
    uintptr_t BP_Color = 0x194;

    static OffsetTable Default() { return {}; }
};

// Global offset table — load from config at startup
inline OffsetTable g_Offsets = OffsetTable::Default();

namespace offsets {
    // Convenience wrappers using the global table
    namespace Instance {
        inline constexpr uintptr_t Children = 0x70;
        inline constexpr uintptr_t ChildrenEnd = 0x8;
        inline constexpr uintptr_t ClassDescriptor = 0x18;
        inline constexpr uintptr_t ClassDescriptorToClassName = 0x8;
        inline constexpr uintptr_t Parent = 0x68;
        inline constexpr uintptr_t Name = 0xB0;
        inline constexpr uintptr_t InstanceCapabilities = 0x208;
    }
    namespace ModuleScript {
        inline constexpr uintptr_t ModuleScriptByteCode = 0x150;
        inline constexpr uintptr_t ModuleScriptBytecodePointer = 0x10;
        inline constexpr uintptr_t ModuleScriptHash = 0x160;
        inline constexpr uintptr_t ModuleScriptProtectedString = 0x140;
    }
    namespace ProtectedString {
        inline constexpr uintptr_t PS_Data = 0x0;
        inline constexpr uintptr_t PS_Size = 0x8;
        inline constexpr uintptr_t PS_Capabilities = 0x10;
    }
    namespace RenderJob {
        inline constexpr uintptr_t DataModel = 0x1B0;
    }
    namespace TaskScheduler {
        inline constexpr uintptr_t JobEnd = 0x1D8;
        inline constexpr uintptr_t JobStart = 0x1D0;
    }
    namespace Pointer {
        inline constexpr uintptr_t TaskScheduler = 0x801D408;
        inline constexpr uintptr_t FakeDataModelPointer = 0x7F6C228;
        inline constexpr uintptr_t VisualEnginePointer = 0x7B1F068;
    }
    namespace FakeDataModel {
        inline constexpr uintptr_t DataModel = 0x1C0;
    }
    namespace VisualEngine {
        inline constexpr uintptr_t VisualEngineToDataModel1 = 0x840;
        inline constexpr uintptr_t VisualEngineToDataModel2 = 0x6F0;
    }
    namespace Jobs {
        inline constexpr uintptr_t JobName = 0x18;
    }
    namespace PlayerListManager {
        inline constexpr uintptr_t SpoofTarget = 0x8;
    }
    namespace ScriptContext {
        inline constexpr uintptr_t RequireBypass = 0x920;
        inline constexpr uintptr_t LuaState = 0x130;
    }
}
