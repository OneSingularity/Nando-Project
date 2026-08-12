#pragma once
#include "pch.h"
#include "../../Pointers.h"

namespace ESP {
    struct ClassIndices {
        int32_t ShooterCharacter = 0;
        int32_t PrimalCharacter = 0;
        int32_t PrimalDinoCharacter = 0;
        int32_t PrimalStructure = 0;
        int32_t PrimalStructureTurret = 0;
        int32_t PrimalStructureItemContainer = 0;
        int32_t StructureElectricGenerator = 0;
        int32_t StructureTekGenerator = 0;
        int32_t StructureVault = 0;
        int32_t PrimalSupplyCrate = 0;
        int32_t PrimalTributeTerminal = 0;

        bool bInitialized = false;
        void Initialize();
    };

    extern ClassIndices g_ClassIndices;

    inline bool ValidObject(SDK::UObject* obj) {
        return ValidObjectPtr(obj);
    }

    int ClassifyActorByComparisonIndex(SDK::AActor* actor);
    std::wstring GetDescriptiveActorName(SDK::AActor* actor, int actorType);
    void DrawTextOutlined(SDK::UCanvas* canvas, SDK::UFont* font, const SDK::FString& text, const SDK::FVector2D& pos, const SDK::FVector2D& scale, const SDK::FLinearColor& color, const SDK::FLinearColor& outlineColor);
}
