#pragma once
#include "pch.h"
#include "Config/Configs.h"

// Internal Engine Object Flags & Validation Helpers
namespace EngineFlags {
    constexpr int32_t RF_NoFlags          = 0x00000000;
    constexpr int32_t RF_Public           = 0x00000001;
    constexpr int32_t RF_Standalone       = 0x00000002;
    constexpr int32_t RF_MarkAsNative     = 0x00000004;
    constexpr int32_t RF_Transactional    = 0x00000008;
    constexpr int32_t RF_ClassDefaultObject= 0x00000010;
    constexpr int32_t RF_ArchetypeObject  = 0x00000020;
    constexpr int32_t RF_Transient        = 0x00000040;
    constexpr int32_t RF_MarkAsRootSet    = 0x00000080;
    constexpr int32_t RF_TagGarbage       = 0x00000100;
    constexpr int32_t RF_PendingKill      = 0x00020000;
    constexpr int32_t RF_Unreachable      = 0x00200000;

    constexpr int32_t DeadObjectFlags = RF_PendingKill | RF_Unreachable | RF_TagGarbage;
}

template<typename T>
inline bool ValidPtr(T* ptr) {
    if (!ptr) return false;
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    return (addr >= 0x10000 && addr <= 0x7FFFFFFFFFFF && (addr & 0x7) == 0);
}

inline bool ValidObjectPtr(SDK::UObject* obj) {
    if (!ValidPtr(obj)) return false;
    void* vtbl = *reinterpret_cast<void**>(obj);
    if (!ValidPtr(vtbl)) return false;

    static uintptr_t imageBase = 0;
    static uintptr_t imageEnd = 0;
    if (imageEnd == 0) {
        imageBase = globals::g_ImageBase;
        if (imageBase) {
            auto dos = reinterpret_cast<PIMAGE_DOS_HEADER>(imageBase);
            if (dos && dos->e_lfanew) {
                auto nt = reinterpret_cast<PIMAGE_NT_HEADERS>(imageBase + dos->e_lfanew);
                if (nt && nt->OptionalHeader.SizeOfImage)
                    imageEnd = imageBase + nt->OptionalHeader.SizeOfImage;
            }
        }
    }
    uintptr_t vtblAddr = reinterpret_cast<uintptr_t>(vtbl);
    if (imageEnd > 0 && (vtblAddr < imageBase || vtblAddr >= imageEnd)) return false;

    return true;
}

inline SDK::UEngine* GetEngineSafe() {
    static SDK::UEngine* s_Engine = nullptr;
    if (ValidObjectPtr(s_Engine)) return s_Engine;

    auto fastEngine = SDK::UObject::FindObject<SDK::UEngine>("GameEngine Transient.GameEngine");
    if (ValidObjectPtr(fastEngine)) {
        s_Engine = fastEngine;
        return s_Engine;
    }

    auto defaultEngine = SDK::UEngine::GetEngine();
    if (ValidObjectPtr(defaultEngine)) {
        s_Engine = defaultEngine;
        return s_Engine;
    }

    return nullptr;
}


