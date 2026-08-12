#include "pch.h"
#include <unordered_map>
#include <string>
#include <mutex>
#include "Scanner/OffsetScanner.h"

extern uintptr_t g_GNames;

namespace SDK {

uintptr_t InSDKUtils::GetImageBase() {
    return globals::g_ImageBase;
}

class UClass* BasicFilesImpleUtils::FindClassByName(const std::string& Name, bool bByFullName) {
    return bByFullName ? UObject::FindClass(Name) : UObject::FindClassFast(Name);
}

class UClass* BasicFilesImpleUtils::FindClassByFullName(const std::string& Name) {
    return UObject::FindClass(Name);
}

std::string BasicFilesImpleUtils::GetObjectName(class UClass* Class) {
    return Class ? Class->GetName() : "";
}

int32 BasicFilesImpleUtils::GetObjectIndex(class UClass* Class) {
    return Class ? Class->Index : -1;
}

uint64 BasicFilesImpleUtils::GetObjFNameAsUInt64(class UClass* Class) {
    return Class ? *reinterpret_cast<uint64*>(&Class->Name) : 0;
}

class UObject* BasicFilesImpleUtils::GetObjectByIndex(int32 Index) {
    return UObject::GObjects ? UObject::GObjects->GetByIndex(Index) : nullptr;
}

UFunction* BasicFilesImpleUtils::FindFunctionByFName(const FName* Name) {
    if (!Name || !UObject::GObjects) return nullptr;
    for (int i = 0; i < UObject::GObjects->Num(); ++i) {
        UObject* Object = UObject::GObjects->GetByIndex(i);
        if (Object && Object->Name == *Name) {
            return static_cast<UFunction*>(Object);
        }
    }
    return nullptr;
}

static FName FindNameInPool(const wchar_t* target) {
    if (!target) return FName();

    static std::unordered_map<std::wstring, FName> nameCache;
    static std::mutex nameCacheMutex;

    {
        std::lock_guard<std::mutex> lock(nameCacheMutex);
        auto it = nameCache.find(target);
        if (it != nameCache.end()) {
            return it->second;
        }
    }

    auto gNamesPool = reinterpret_cast<FNamePool*>(globals::g_ImageBase + OffsetScanner::g_Offset_GNames);
    if (!gNamesPool) return FName();

    uint32_t currentBlock = gNamesPool->CurrentBlock;
    uint32_t currentCursor = gNamesPool->CurrentByteCursor;

    std::string targetAnsi;
    targetAnsi.reserve(512);
    for (const wchar_t* p = target; *p; ++p) {
        targetAnsi.push_back(static_cast<char>(*p));
    }

    for (uint32_t blockIdx = 0; blockIdx <= currentBlock; blockIdx++) {
        uint8_t* block = gNamesPool->Blocks[blockIdx];
        if (!block) continue;

        uint32_t maxOffset = (blockIdx == currentBlock) ? currentCursor : (65536 * 2);
        uint32_t offset = 0;

        while (offset < maxOffset) {
            auto entry = reinterpret_cast<FNameEntry*>(block + offset);
            if (!entry) break;

            uint32_t len = entry->Header.Len;
            if (len == 0) {
                offset += 2;
                continue;
            }

            bool isWide = entry->Header.bIsWide;
            bool matched = false;

            if (isWide) {
                if (len == wcslen(target) && _wcsnicmp(entry->Name.WideName, target, len) == 0) {
                    matched = true;
                }
            } else {
                if (len == targetAnsi.length() && _strnicmp(entry->Name.AnsiName, targetAnsi.c_str(), len) == 0) {
                    matched = true;
                }
            }

            if (matched) {
                uint32_t inChunk = offset / 2;
                uint32_t comparisonIndex = (blockIdx << 16) + inChunk;
                FName result(comparisonIndex, 0);

                std::lock_guard<std::mutex> lock(nameCacheMutex);
                nameCache[target] = result;
                return result;
            }

            uint32_t entrySize = 2 + (isWide ? (len * 2) : len);
            entrySize = (entrySize + 1) & ~1;
            offset += entrySize;
        }
    }

    FName result;
    std::lock_guard<std::mutex> lock(nameCacheMutex);
    nameCache[target] = result;
    return result;
}

FName BasicFilesImpleUtils::StringToName(const wchar_t* Name) {
    return FindNameInPool(Name);
}

const FName& GetStaticName(const wchar_t* Name, FName& StaticName) {
    if (StaticName.IsNone()) {
        StaticName = BasicFilesImpleUtils::StringToName(Name);
    }
    return StaticName;
}

class UObject* FWeakObjectPtr::Get() const {
    return UObject::GObjects ? UObject::GObjects->GetByIndex(ObjectIndex) : nullptr;
}

class UObject* FWeakObjectPtr::operator->() const {
    return UObject::GObjects ? UObject::GObjects->GetByIndex(ObjectIndex) : nullptr;
}

bool FWeakObjectPtr::operator==(const FWeakObjectPtr& Other) const {
    return ObjectIndex == Other.ObjectIndex;
}

bool FWeakObjectPtr::operator!=(const FWeakObjectPtr& Other) const {
    return ObjectIndex != Other.ObjectIndex;
}

bool FWeakObjectPtr::operator==(const class UObject* Other) const {
    return Other && ObjectIndex == Other->Index;
}

bool FWeakObjectPtr::operator!=(const class UObject* Other) const {
    return !Other || ObjectIndex != Other->Index;
}

} // namespace SDK

#include "SDK/CoreUObject_functions.cpp"
#include "SDK/Engine_functions.cpp"

namespace SDK {

bool APrimalDinoCharacter::BPIsTamed() const {
    static UFunction* func = nullptr;
    if (!func) {
        func = UObject::FindObject<UFunction>("Function ShooterGame.PrimalDinoCharacter.BPIsTamed");
    }
    if (!func) return false;

    struct {
        bool ReturnValue;
    } params{};

    const_cast<APrimalDinoCharacter*>(this)->ProcessEvent(func, &params);
    return params.ReturnValue;
}

APrimalBuff* APrimalCharacter::GetBuff(TSubclassOf<APrimalBuff> BuffClass) const {
    static UFunction* func = nullptr;
    if (!func) {
        func = UObject::FindObject<UFunction>("Function ShooterGame.PrimalCharacter.GetBuff");
    }
    if (!func) return nullptr;

    struct {
        TSubclassOf<APrimalBuff> BuffClass;
        APrimalBuff* ReturnValue;
    } params{};

    params.BuffClass = BuffClass;
    const_cast<APrimalCharacter*>(this)->ProcessEvent(func, &params);
    return params.ReturnValue;
}

void APrimalCharacter::BPSuicide() {
    static UFunction* func = nullptr;
    if (!func) {
        func = Class ? Class->GetFunction("PrimalCharacter", "BPSuicide") : nullptr;
    }
    if (func) {
        auto flags = func->FunctionFlags;
        func->FunctionFlags |= 0x400;
        ProcessEvent(func, nullptr);
        func->FunctionFlags = flags;
    }
}

} // namespace SDK
