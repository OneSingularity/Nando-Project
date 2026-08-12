#include "pch.h"
#include "Scanner/OffsetScanner.h"

// Global declaration for SDK redirect
void AppendString_Custom(const void* Entry, void* OutStringPtr);
const void* GetNameEntryFromName_Custom(unsigned int ComparisonIndex);

namespace OffsetScanner {
    int g_Offset_GObjects = SDK::Offsets::GObjects;
    int g_Offset_GNames = SDK::Offsets::GNames;
    int g_Index_ProcessEvent = SDK::Offsets::ProcessEventIdx;
    int g_Index_PostRender = 0x7A;
    bool g_ScanFailed = false;

    int offset_heat_amount = 0x1208;
    int offset_local_heat = 0x1210;
    int offset_repl_heat = 0x1258;
    int offset_last_overheat = 0x1230;
    int offset_last_time_overheat = 0x1278;
    int offset_time_allow_fire = 0x1280;
    int offset_allow_reload_run = 0x0DD7;

    int g_VTableIndex_RM = 0x1AE; // Dynamic ReplicateMoveToServer VTable index scanner default

    static bool s_CMCScanned = false;
    void TryScanLiveCMC(void *cmcObject) {
        if (!cmcObject || s_CMCScanned) return;
        void **vtable = *reinterpret_cast<void ***>(cmcObject);
        if (!vtable || (uintptr_t)vtable < 0x10000 || (uintptr_t)vtable > 0x7FFFFFFFFFFF) return;

        uintptr_t exeStart = globals::g_ImageBase;
        if (!exeStart) return;

        auto dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(exeStart);
        auto ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(exeStart + dosHeader->e_lfanew);
        uintptr_t exeEnd = exeStart + ntHeaders->OptionalHeader.SizeOfImage;

        int bestCandidate = -1;
        for (int idx = 300; idx < 500; idx++) {
            uintptr_t funcAddr = reinterpret_cast<uintptr_t>(vtable[idx]);
            if (funcAddr < exeStart || funcAddr + 0x5000 > exeEnd) continue;

            uint8_t *fb = reinterpret_cast<uint8_t *>(funcAddr);
            if (fb[0] == 0x40 && fb[1] == 0x55 && fb[2] == 0x53 && fb[3] == 0x56 &&
                fb[4] == 0x57 && fb[5] == 0x41 && fb[6] == 0x54) {
                if ((fb[7] == 0x41 && fb[8] == 0x55 && fb[9] == 0x41 && fb[10] == 0x56 &&
                     fb[11] == 0x48 && fb[12] == 0x8D && fb[13] == 0xAC && fb[14] == 0x24) ||
                    (fb[7] == 0x41 && fb[8] == 0x56 && fb[9] == 0x48 &&
                     fb[10] == 0x8D && fb[11] == 0xAC && fb[12] == 0x24)) {
                    bestCandidate = idx;
                    break;
                }
            }
        }

        if (bestCandidate >= 0) {
            g_VTableIndex_RM = bestCandidate;
        }
        s_CMCScanned = true;
    }

    static FNamePool *g_GNamesPool = nullptr;

    inline void InitGNames() {
        if (!g_GNamesPool) {
            uintptr_t imageBase = globals::g_ImageBase;
            int offset = g_Offset_GNames ? g_Offset_GNames : SDK::Offsets::GNames;
            g_GNamesPool = reinterpret_cast<FNamePool *>(imageBase + offset);
        }
    }

    static void* GetExportedFunctionByPrefix(HMODULE hMod, const char* prefix) {
        if (!hMod) return nullptr;
        uintptr_t base = reinterpret_cast<uintptr_t>(hMod);

        auto dos = reinterpret_cast<PIMAGE_DOS_HEADER>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;

        auto nt = reinterpret_cast<PIMAGE_NT_HEADERS>(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) return nullptr;

        auto exportDirEntry = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        if (exportDirEntry.VirtualAddress == 0 || exportDirEntry.Size == 0) return nullptr;

        auto exportDir = reinterpret_cast<PIMAGE_EXPORT_DIRECTORY>(base + exportDirEntry.VirtualAddress);
        auto names = reinterpret_cast<uint32_t *>(base + exportDir->AddressOfNames);
        auto ordinals = reinterpret_cast<uint16_t *>(base + exportDir->AddressOfNameOrdinals);
        auto functions = reinterpret_cast<uint32_t *>(base + exportDir->AddressOfFunctions);

        for (uint32_t i = 0; i < exportDir->NumberOfNames; i++) {
            const char *name = reinterpret_cast<const char *>(base + names[i]);
            if (name && strstr(name, prefix) == name) {
                uint16_t ordinal = ordinals[i];
                uint32_t funcRva = functions[ordinal];
                return reinterpret_cast<void *>(base + funcRva);
            }
        }
        return nullptr;
    }

    bool ScanOffsets() {
        uintptr_t imageBase = globals::g_ImageBase;
        if (!imageBase) return false;

        auto IsValidGObjectsAddr = [](uintptr_t addr) -> bool {
            if (addr < 0x10000 || addr > 0x7FFFFFFFFFFF || (addr & 0x7)) return false;
            struct FUObjectArray_Raw {
                void** Objects;
                uint8_t Pad_8[8];
                int32_t MaxElements;
                int32_t NumElements;
                int32_t MaxChunks;
                int32_t NumChunks;
            };
            auto* rawArray = reinterpret_cast<FUObjectArray_Raw*>(addr);
            if (!rawArray) return false;
            if (rawArray->NumElements < 500 || rawArray->NumElements > 5000000) return false;
            if (rawArray->NumChunks < 1 || rawArray->NumChunks > 2000) return false;
            if (!rawArray->Objects) return false;
            return true;
        };

        auto IsValidGNamesAddr = [](uintptr_t addr) -> bool {
            if (addr < 0x10000 || addr > 0x7FFFFFFFFFFF || (addr & 0x7)) return false;
            struct FNamePool_Raw {
                uint8_t Lock[8];
                uint32_t CurrentBlock;
                uint32_t CurrentByteCursor;
                void* Blocks[8192];
            };
            auto* pool = reinterpret_cast<FNamePool_Raw*>(addr);
            if (!pool) return false;
            if (pool->CurrentBlock > 8192) return false;
            if (!pool->Blocks[0]) return false;
            return true;
        };

        g_Offset_GObjects = SDK::Offsets::GObjects;
        g_Offset_GNames = SDK::Offsets::GNames;

        if (!IsValidGObjectsAddr(imageBase + g_Offset_GObjects) || !IsValidGNamesAddr(imageBase + g_Offset_GNames)) {
            g_ScanFailed = true;
            return false;
        }

        // Initialize SDK Object Array & FName Hooks (Exact 1:1 match with ASAFeng)
        SDK::UObject::GObjects.InitManually(reinterpret_cast<void*>(imageBase + g_Offset_GObjects));
        InitGNames();

        SDK::FName::AppendString = reinterpret_cast<void*>(&AppendString_Custom);
        SDK::FName::GetNameEntryFromName = reinterpret_cast<void*>(&GetNameEntryFromName_Custom);

        // Resolve ProcessEvent Index from Export
        HMODULE hGame = reinterpret_cast<HMODULE>(imageBase);
        void *peExportAddr = GetExportedFunctionByPrefix(hGame, "?ProcessEvent@UObject@@");
        if (peExportAddr && SDK::UObject::GObjects && SDK::UObject::GObjects->Num() > 0) {
            SDK::UObject* firstObj = SDK::UObject::GObjects->GetByIndex(0);
            if (firstObj) {
                void **vtable = *reinterpret_cast<void ***>(firstObj);
                if (vtable) {
                    for (int i = 0; i < 300; i++) {
                        if (vtable[i] == peExportAddr) {
                            g_Index_ProcessEvent = i;
                            break;
                        }
                    }
                }
            }
        }

        g_ScanFailed = false;
        return true;
    }
}

const void* GetNameEntryFromName_Custom(unsigned int ComparisonIndex) {
    OffsetScanner::InitGNames();
    if (!OffsetScanner::g_GNamesPool) return nullptr;
    return OffsetScanner::g_GNamesPool->GetEntryByIndex(ComparisonIndex);
}

void AppendString_Custom(const void *Entry, void *OutStringPtr) {
    if (!Entry || !OutStringPtr) return;
    const auto *entry = reinterpret_cast<const FNameEntry *>(Entry);
    int32_t len = entry->Header.Len;
    if (len <= 0 || len > 1024) return;

    struct FStringDummy {
        wchar_t *Data;
        int32_t NumElements;
        int32_t MaxElements;
    };
    auto *dummy = reinterpret_cast<FStringDummy *>(OutStringPtr);
    wchar_t *dest = dummy->Data;
    if (!dest) return;

    int32_t currentNum = dummy->NumElements;
    int32_t startIdx = (currentNum > 0) ? (currentNum - 1) : 0;
    int32_t maxSpace = dummy->MaxElements;
    int32_t spaceLeft = maxSpace - startIdx - 1;
    if (spaceLeft <= 0) return;

    int32_t copyLen = (len < spaceLeft) ? len : spaceLeft;

    if (entry->IsWide()) {
        const wchar_t *src = entry->Name.WideName;
        for (int32_t i = 0; i < copyLen; i++) {
            dest[startIdx + i] = src[i];
        }
    } else {
        const char *src = entry->Name.AnsiName;
        for (int32_t i = 0; i < copyLen; i++) {
            dest[startIdx + i] = static_cast<wchar_t>(src[i]);
        }
    }

    dest[startIdx + copyLen] = L'\0';
    dummy->NumElements = startIdx + copyLen + 1;
}
