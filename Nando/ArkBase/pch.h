#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winuser.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <atomic>
#include <mutex>

// Include Unreal SDK Header entrypoints
#pragma warning(disable: 4369 4309 4244 4530)
#include "SDK/Basic.hpp"

typedef int parameter_int;
#define int parameter_int

#include "SDK/CoreUObject_classes.hpp"
#include "SDK/Engine_classes.hpp"
#include "SDK/Engine_parameters.hpp"
#include "SDK/Engine_structs.hpp"
#include "SDK/UMG_classes.hpp"
#include "SDK/UMG_parameters.hpp"
#include "SDK/ShooterGame_classes.hpp"

#undef int

// Global Definitions
namespace globals {
    extern uintptr_t g_ImageBase;
    extern uintptr_t g_DllBase;
    extern DWORD g_GameThreadId;
    extern SDK::UWorld* g_ActiveWorld;
    extern SDK::APlayerController* g_ActivePC;
}
