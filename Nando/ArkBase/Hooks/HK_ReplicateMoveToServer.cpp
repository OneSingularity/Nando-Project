#include "pch.h"
#include "Hooks.h"
#include "Config/Configs.h"
#include "Scanner/OffsetScanner.h"
#include "Pointers.h"
#include <unordered_map>
#include <mutex>

namespace Hooks {

    typedef void(__fastcall* ReplicateMoveToServer_t)(void* rcx, float DeltaTime, void* newVelocity);
    static ReplicateMoveToServer_t Orig_ReplicateMoveToServer = nullptr;
    static bool bReplicateMoveHooked = false;

    // Shadow VTable state
    static void** g_ShadowVTable_CMC = nullptr;
    static void** g_OrigVTable_CMC = nullptr;
    static void*  g_LastHookedCMC = nullptr;

    static void __fastcall HK_ReplicateMoveToServer(void* rcx, float DeltaTime, void* newVelocity) {
        if (!rcx || bShuttingDown) {
            if (Orig_ReplicateMoveToServer) {
                Orig_ReplicateMoveToServer(rcx, DeltaTime, newVelocity);
            }
            return;
        }

        // Apply Airstuck modifications if enabled in settings
        if (Config::g_Settings.bAirstuck) {
            float t = Config::g_Settings.AirstuckDeltaTime;
            if (t < 0.f) t = 0.f;
            if (t > 1.f) t = 1.f;
            float divisor = 500.f - t * 490.f;
            constexpr float kMinDelta = 0.000001f;
            float step = DeltaTime / divisor;
            if (step < kMinDelta) step = kMinDelta;
            DeltaTime = step;
        }

        if (Orig_ReplicateMoveToServer) {
            Orig_ReplicateMoveToServer(rcx, DeltaTime, newVelocity);
        }
    }

    bool ApplyShadowVTableCMC(void* cmcObject) {
        if (!cmcObject) return false;
        
        OffsetScanner::TryScanLiveCMC(cmcObject);
        int targetIdx = OffsetScanner::g_VTableIndex_RM;
        if (targetIdx < 0) return false;

        void** currentVTable = *reinterpret_cast<void***>(cmcObject);
        if (!currentVTable) return false;

        if (currentVTable == g_ShadowVTable_CMC && g_ShadowVTable_CMC != nullptr) {
            g_LastHookedCMC = cmcObject;
            return true;
        }

        g_OrigVTable_CMC = currentVTable;

        constexpr int vtableSize = 1024;
        g_ShadowVTable_CMC = reinterpret_cast<void**>(
            HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(void*) * vtableSize)
        );
        if (!g_ShadowVTable_CMC) return false;

        for (int i = 0; i < vtableSize; i++) {
            g_ShadowVTable_CMC[i] = currentVTable[i];
        }

        void* entryAtRM = g_ShadowVTable_CMC[targetIdx];
        if (entryAtRM && entryAtRM != reinterpret_cast<void*>(&HK_ReplicateMoveToServer)) {
            Orig_ReplicateMoveToServer = reinterpret_cast<ReplicateMoveToServer_t>(entryAtRM);
        }

        g_ShadowVTable_CMC[targetIdx] = reinterpret_cast<void*>(&HK_ReplicateMoveToServer);
        *reinterpret_cast<void***>(cmcObject) = g_ShadowVTable_CMC;

        g_LastHookedCMC = cmcObject;
        return true;
    }

    bool HookReplicateMove(void* cmcObj) {
        if (!cmcObj) return false;

        if (cmcObj == g_LastHookedCMC) {
            void** current = *reinterpret_cast<void***>(cmcObj);
            if (current != g_ShadowVTable_CMC && g_ShadowVTable_CMC != nullptr) {
                *reinterpret_cast<void***>(cmcObj) = g_ShadowVTable_CMC;
            }
            return true;
        }
        
        if (!ApplyShadowVTableCMC(cmcObj)) return false;

        bReplicateMoveHooked = true;
        return true;
    }

    void UnhookReplicateMove() {
        if (g_LastHookedCMC && g_OrigVTable_CMC) {
            if (ValidPtr(g_LastHookedCMC)) {
                void*** pVTableAddr = reinterpret_cast<void***>(g_LastHookedCMC);
                if (ValidPtr(pVTableAddr) && *pVTableAddr == g_ShadowVTable_CMC) {
                    *pVTableAddr = g_OrigVTable_CMC;
                }
            }
        }

        if (g_ShadowVTable_CMC) {
            Sleep(50);
            HeapFree(GetProcessHeap(), 0, g_ShadowVTable_CMC);
            g_ShadowVTable_CMC = nullptr;
        }

        Orig_ReplicateMoveToServer = nullptr;
        g_OrigVTable_CMC = nullptr;
        g_LastHookedCMC = nullptr;
        bReplicateMoveHooked = false;
    }
}
