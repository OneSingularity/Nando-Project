#pragma once

namespace Hooks {
    extern bool bShuttingDown;
    extern bool bPostRenderHooked;
    extern bool g_KeyState[256];
    extern bool g_KeyPendingDown[256];

    // Hook entry points
    bool InstallHooks();
    void RemoveHooks();
    bool HookPlayerController(SDK::APlayerController* PC);
    bool HookPeekMessage();
    void UnhookPeekMessage();

    // Hook function typedefs
    typedef void(__fastcall* ProcessEvent_t)(SDK::UObject*, SDK::UFunction*, void*);
    typedef void(__fastcall* PostRender_t)(void*, SDK::UCanvas*);
    
    // Original function pointers
    extern ProcessEvent_t Orig_ProcessEvent;
    extern PostRender_t Orig_PostRender;

    // Shadow VTables
    extern void** g_Shadow_PC;
    extern void** g_Orig_PC;
    extern void** g_Shadow_Viewport;
    extern void** g_Orig_Viewport;

    // Process Hook callbacks
    void __fastcall Hooked_ProcessEvent(SDK::UObject* Object, SDK::UFunction* Function, void* Parms);
    void __fastcall Hooked_PostRender(void* ViewportClient, SDK::UCanvas* Canvas);
    bool HookReplicateMove(void* cmcObj);
    void UnhookReplicateMove();
    DWORD WINAPI UnloadThread(LPVOID lpParam);
}
