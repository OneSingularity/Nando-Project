#include "pch.h"
#include "Hooks.h"
#include "Config/Configs.h"

namespace Hooks {
    void __fastcall Hooked_ProcessEvent(SDK::UObject* Object, SDK::UFunction* Function, void* Parms) {
        if (!Object || !Function || bShuttingDown) {
            if (Orig_ProcessEvent) Orig_ProcessEvent(Object, Function, Parms);
            return;
        }

        // Capture local thread ID if not set
        if (globals::g_GameThreadId == 0) {
            globals::g_GameThreadId = GetCurrentThreadId();
        }

        // Example: Intercept commands or perform silent actions
        // Let's call the original ProcessEvent to continue engine execution
        if (Orig_ProcessEvent) {
            Orig_ProcessEvent(Object, Function, Parms);
        }
    }
}
