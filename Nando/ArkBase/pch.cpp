#include "pch.h"

namespace globals {
    uintptr_t g_ImageBase = 0;
    uintptr_t g_DllBase = 0;
    DWORD g_GameThreadId = 0;
    SDK::UWorld* g_ActiveWorld = nullptr;
    SDK::APlayerController* g_ActivePC = nullptr;
}
