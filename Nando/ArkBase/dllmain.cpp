#include "pch.h"
#include "Scanner/OffsetScanner.h"
#include "Hooks/Hooks.h"
#include "ESP/Classes/ClassIndices.h"
#include "Pointers.h"

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        globals::g_DllBase = reinterpret_cast<uintptr_t>(hModule);
        globals::g_ImageBase = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));

        if (OffsetScanner::ScanOffsets()) {
            ESP::g_ClassIndices.Initialize();
            Hooks::InstallHooks();
        }
        break;

    case DLL_PROCESS_DETACH:
        Hooks::RemoveHooks();
        break;
    }
    return TRUE;
}
