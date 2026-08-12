#include "pch.h"
#include "Hooks.h"
#include "Scanner/OffsetScanner.h"
#include "Interface/Menu.h"
#include "Pointers.h"

namespace Hooks {
    bool bShuttingDown = false;
    bool bPostRenderHooked = false;

    ProcessEvent_t Orig_ProcessEvent = nullptr;
    PostRender_t Orig_PostRender = nullptr;

    void** g_Shadow_PC = nullptr;
    void** g_Orig_PC = nullptr;
    void** g_Shadow_Viewport = nullptr;
    void** g_Orig_Viewport = nullptr;

    static int GetVTableSize(void** vtable) {
        int size = 0;
        while (vtable[size]) {
            uintptr_t addr = reinterpret_cast<uintptr_t>(vtable[size]);
            if (addr < 0x10000 || addr > 0x7FFFFFFFFFFF) break;
            size++;
        }
        return size;
    }

    /*
     * Custom Virtual Method Table (VTable) Shadow Hooking implementation.
     * Game structures inherit virtual functions, which are tracked via a list of function pointers (the VTable).
     * To redirect function calls (like ProcessEvent or PostRender) to our own custom functions safely:
     * 1. We calculate the size of the original VTable.
     * 2. We allocate a brand new copy of the VTable (shadow table) in our process heap space.
     * 3. We duplicate the original function pointers into our shadow table.
     * 4. We replace the specific virtual index we want to hook (hookIdx) with our custom callback pointer.
     * 5. We overwrite the instance's vtable pointer to point to our shadow table.
     * This avoids writing directly to the executable memory page (avoiding PAGE_EXECUTE_READWRITE changes),
     * rendering the hook much cleaner and safer.
     */
    static void** HookVTable(void* instance, int hookIdx, void* hookFunc, void**& origVtableOut, void*& origFuncOut) {
        if (!instance) return nullptr;
        void*** pVtableAddr = reinterpret_cast<void***>(instance);
        void** currentVtable = *pVtableAddr;
        if (!currentVtable) return nullptr;

        int size = GetVTableSize(currentVtable);
        if (hookIdx >= size) return nullptr;

        void** shadowVtable = reinterpret_cast<void**>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(void*) * size));
        if (!shadowVtable) return nullptr;

        for (int i = 0; i < size; i++) {
            shadowVtable[i] = currentVtable[i];
        }

        origVtableOut = currentVtable;
        origFuncOut = currentVtable[hookIdx];
        shadowVtable[hookIdx] = hookFunc;

        *pVtableAddr = shadowVtable;
        return shadowVtable;
    }

    static void UnhookVTable(void* instance, void** shadowVtable, void** origVtable) {
        if (!instance || !shadowVtable || !origVtable) return;
        void*** pVtableAddr = reinterpret_cast<void***>(instance);
        if (*pVtableAddr == shadowVtable) {
            *pVtableAddr = origVtable;
            
            // Short yield delay to ensure any execution threads exit shadow memory
            Sleep(50);
            
            HeapFree(GetProcessHeap(), 0, shadowVtable);
        }
    }

    bool HookPlayerController(SDK::APlayerController* PC) {
        if (!PC || g_Shadow_PC) return false;
        void* origPE = nullptr;
        g_Shadow_PC = HookVTable(PC, OffsetScanner::g_Index_ProcessEvent, &Hooked_ProcessEvent, g_Orig_PC, origPE);
        if (g_Shadow_PC) {
            Orig_ProcessEvent = reinterpret_cast<ProcessEvent_t>(origPE);
            return true;
        }
        return false;
    }

    bool HookPostRender() {
        if (bPostRenderHooked) return true;
        SDK::UEngine* engine = GetEngineSafe();
        if (!ValidObjectPtr(engine) || !ValidObjectPtr(engine->GameViewport)) return false;

        void* origPR = nullptr;
        g_Shadow_Viewport = HookVTable(engine->GameViewport, OffsetScanner::g_Index_PostRender, &Hooked_PostRender, g_Orig_Viewport, origPR);
        if (g_Shadow_Viewport) {
            Orig_PostRender = reinterpret_cast<PostRender_t>(origPR);
            bPostRenderHooked = true;
            return true;
        }
        return false;
    }

    bool InstallHooks() {
        bShuttingDown = false;
        
        // 1. Hook viewport PostRender
        if (!HookPostRender()) {
            return false;
        }

        // Hook PeekMessageW for inputs
        if (!HookPeekMessage()) {
            return false;
        }

        return true;
    }

    DWORD WINAPI UnloadThread(LPVOID lpParam) {
        // Sleep a short time to allow current frame PostRender execution to exit DLL memory bounds
        Sleep(500);

        HMODULE hMod = reinterpret_cast<HMODULE>(lpParam);
        
        // Remove hooks, restoring original vtables on game objects
        Hooks::RemoveHooks();

        Sleep(200);

        // Safely free DLL memory and exit unloader thread
        FreeLibraryAndExitThread(hMod, 0);
        return 0;
    }

    void RemoveHooks() {
        bShuttingDown = true;
        
        // Restore mouse input and cursor if open
        if (Menu::g_Open) {
            Menu::ToggleMenuState(false);
        }

        // Unhook ReplicateMoveToServer from CharacterMovementComponent
        UnhookReplicateMove();

        // Restore Viewport vtable
        SDK::UEngine* engine = GetEngineSafe();
        if (engine && engine->GameViewport && g_Orig_Viewport) {
            *reinterpret_cast<void***>(engine->GameViewport) = g_Orig_Viewport;
            if (g_Shadow_Viewport) {
                HeapFree(GetProcessHeap(), 0, g_Shadow_Viewport);
            }
            g_Shadow_Viewport = nullptr;
            g_Orig_Viewport = nullptr;
        }

        // Restore PlayerController vtable
        if (globals::g_ActivePC && g_Orig_PC) {
            *reinterpret_cast<void***>(globals::g_ActivePC) = g_Orig_PC;
            if (g_Shadow_PC) {
                HeapFree(GetProcessHeap(), 0, g_Shadow_PC);
            }
            g_Shadow_PC = nullptr;
            g_Orig_PC = nullptr;
        }
        globals::g_ActivePC = nullptr;

        // Restore PeekMessage
        UnhookPeekMessage();
        
        bPostRenderHooked = false;
    }
}
