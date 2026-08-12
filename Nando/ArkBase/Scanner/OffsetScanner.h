#pragma once
#include "pch.h"

struct FNameEntryHeader final {
    unsigned short bIsWide : 1;
    unsigned short BitPad_0_1 : 5;
    unsigned short Len : 10;
};

union FStringData final {
    char AnsiName[0x400];
    wchar_t WideName[0x400];
};

struct FNameEntry final {
    struct FNameEntryHeader Header;
    union FStringData Name;
    bool IsWide() const { return Header.bIsWide; }
};

class FNamePool final {
public:
    static constexpr unsigned int FNameEntryStride = 0x0002;
    static constexpr unsigned int FNameBlockOffsetBits = 0x0010;
    static constexpr unsigned int FNameBlockOffsets = 1 << FNameBlockOffsetBits;

    unsigned char Pad_0[0x8];
    unsigned int CurrentBlock;
    unsigned int CurrentByteCursor;
    unsigned char *Blocks[0x2000];

    inline FNameEntry *GetEntryByIndex(unsigned int index) const {
        unsigned int block = index >> FNameBlockOffsetBits;
        unsigned int offset = index & (FNameBlockOffsets - 1);

        if (block >= 0x2000) return nullptr;
        unsigned char *blockPtr = Blocks[block];
        if (!blockPtr) return nullptr;

        return reinterpret_cast<FNameEntry *>(blockPtr + FNameEntryStride * offset);
    }
};

namespace OffsetScanner {
    extern int g_Offset_GObjects;
    extern int g_Offset_GNames;
    extern int g_Index_ProcessEvent;
    extern int g_Index_PostRender;
    extern bool g_ScanFailed;

    extern int offset_heat_amount;
    extern int offset_local_heat;
    extern int offset_repl_heat;
    extern int offset_last_overheat;
    extern int offset_last_time_overheat;
    extern int offset_time_allow_fire;
    extern int offset_allow_reload_run;
    extern int g_VTableIndex_RM;

    bool ScanOffsets();
    void InitGNames();
    void TryScanLiveCMC(void *cmcObject);
}

extern void AppendString_Custom(const void* Entry, void* OutStringPtr);
extern const void* GetNameEntryFromName_Custom(unsigned int ComparisonIndex);
