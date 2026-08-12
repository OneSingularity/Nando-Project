#include "pch.h"
#include "ClassIndices.h"
#include "Config/Configs.h"

namespace ESP {
    ClassIndices g_ClassIndices;

    void ClassIndices::Initialize() {
        if (bInitialized) return;

        auto shooterClass = SDK::UObject::FindClassFast("ShooterCharacter");
        auto dinoClass = SDK::UObject::FindClassFast("PrimalDinoCharacter");
        auto structClass = SDK::UObject::FindClassFast("PrimalStructure");
        auto turretClass = SDK::UObject::FindClassFast("PrimalStructureTurret");
        auto containerClass = SDK::UObject::FindClassFast("PrimalStructureItemContainer");
        auto eleGenClass = SDK::UObject::FindClassFast("StructureElectricGenerator");
        auto tekGenClass = SDK::UObject::FindClassFast("StructureTekGenerator");
        auto vaultClass = SDK::UObject::FindClassFast("StructureVault");
        auto crateClass = SDK::UObject::FindClassFast("PrimalSupplyCrate");
        auto terminalClass = SDK::UObject::FindClassFast("PrimalTributeTerminal");

        if (shooterClass) ShooterCharacter = shooterClass->Name.ComparisonIndex;
        if (dinoClass) PrimalDinoCharacter = dinoClass->Name.ComparisonIndex;
        if (structClass) PrimalStructure = structClass->Name.ComparisonIndex;
        if (turretClass) PrimalStructureTurret = turretClass->Name.ComparisonIndex;
        if (containerClass) PrimalStructureItemContainer = containerClass->Name.ComparisonIndex;
        if (eleGenClass) StructureElectricGenerator = eleGenClass->Name.ComparisonIndex;
        if (tekGenClass) StructureTekGenerator = tekGenClass->Name.ComparisonIndex;
        if (vaultClass) StructureVault = vaultClass->Name.ComparisonIndex;
        if (crateClass) PrimalSupplyCrate = crateClass->Name.ComparisonIndex;
        if (terminalClass) PrimalTributeTerminal = terminalClass->Name.ComparisonIndex;

        if (ShooterCharacter == 0) ShooterCharacter = SDK::BasicFilesImpleUtils::StringToName(L"ShooterCharacter").ComparisonIndex;
        if (PrimalDinoCharacter == 0) PrimalDinoCharacter = SDK::BasicFilesImpleUtils::StringToName(L"PrimalDinoCharacter").ComparisonIndex;
        if (PrimalStructure == 0) PrimalStructure = SDK::BasicFilesImpleUtils::StringToName(L"PrimalStructure").ComparisonIndex;

        if (ShooterCharacter != 0 || PrimalDinoCharacter != 0 || PrimalStructure != 0) {
            bInitialized = true;
        }
    }

    int ClassifyActorByComparisonIndex(SDK::AActor* actor) {
        if (!ValidObject(actor) || !ValidObject(actor->Class)) return 0;

        g_ClassIndices.Initialize();

        for (SDK::UStruct* curr = actor->Class; curr && ValidObject(curr); curr = curr->SuperStruct) {
            int32_t idx = curr->Name.ComparisonIndex;
            if (g_ClassIndices.ShooterCharacter != 0 && idx == g_ClassIndices.ShooterCharacter) return 1;
            if (g_ClassIndices.PrimalDinoCharacter != 0 && idx == g_ClassIndices.PrimalDinoCharacter) return 2;
            if (g_ClassIndices.PrimalStructureTurret != 0 && idx == g_ClassIndices.PrimalStructureTurret) return 3;
            if ((g_ClassIndices.PrimalSupplyCrate != 0 && idx == g_ClassIndices.PrimalSupplyCrate) ||
                (g_ClassIndices.PrimalTributeTerminal != 0 && idx == g_ClassIndices.PrimalTributeTerminal)) return 7;
            if ((g_ClassIndices.StructureElectricGenerator != 0 && idx == g_ClassIndices.StructureElectricGenerator) ||
                (g_ClassIndices.StructureTekGenerator != 0 && idx == g_ClassIndices.StructureTekGenerator)) return 5;
            if ((g_ClassIndices.PrimalStructureItemContainer != 0 && idx == g_ClassIndices.PrimalStructureItemContainer) ||
                (g_ClassIndices.StructureVault != 0 && idx == g_ClassIndices.StructureVault)) return 4;
            if (g_ClassIndices.PrimalStructure != 0 && idx == g_ClassIndices.PrimalStructure) return 6;
        }

        if (actor->IsA(SDK::AShooterCharacter::StaticClass())) return 1;
        if (actor->IsA(SDK::APrimalDinoCharacter::StaticClass())) return 2;
        if (actor->IsA(SDK::APrimalStructureTurret::StaticClass())) return 3;
        if (actor->IsA(SDK::APrimalStructureItemContainer::StaticClass())) return 4;
        if (actor->IsA(SDK::APrimalStructure::StaticClass())) return 6;

        return 0;
    }

    std::wstring GetDescriptiveActorName(SDK::AActor* actor, int actorType) {
        if (!ValidObject(actor)) return L"Unknown";

        if (actorType == 1) {
            auto shooterChar = static_cast<SDK::AShooterCharacter*>(actor);
            if (ValidObject(shooterChar)) {
                if (shooterChar->PlayerName.GetDataPtr()) {
                    std::string pName = shooterChar->PlayerName.ToString();
                    if (!pName.empty() && pName != "None") {
                        return std::wstring(pName.begin(), pName.end());
                    }
                }
            }
            auto primalChar = static_cast<SDK::APrimalCharacter*>(actor);
            if (ValidObject(primalChar)) {
                if (primalChar->DescriptiveName.GetDataPtr()) {
                    std::string s = primalChar->DescriptiveName.ToString();
                    if (!s.empty() && s != "None") return std::wstring(s.begin(), s.end());
                }
            }
            return L"Player";
        }

        if (actorType == 2) {
            auto dino = static_cast<SDK::APrimalDinoCharacter*>(actor);
            if (ValidObject(dino)) {
                if (dino->TamedName.GetDataPtr()) {
                    std::string tName = dino->TamedName.ToString();
                    if (!tName.empty() && tName != "None") {
                        return std::wstring(tName.begin(), tName.end());
                    }
                }
            }
            if (ValidObject(actor->Class)) {
                std::string cName = actor->Class->GetName();
                size_t bpIdx = cName.find("_Character_BP");
                if (bpIdx != std::string::npos) cName = cName.substr(0, bpIdx);
                size_t cIdx = cName.find("_C");
                if (cIdx != std::string::npos && cIdx == cName.length() - 2) cName = cName.substr(0, cIdx);
                for (auto& ch : cName) if (ch == '_') ch = ' ';
                if (!cName.empty()) return std::wstring(cName.begin(), cName.end());
            }
            return L"Dino";
        }

        if (actorType >= 3 && actorType <= 6) {
            auto structObj = static_cast<SDK::APrimalStructure*>(actor);
            if (ValidObject(structObj)) {
                if (structObj->DescriptiveName.GetDataPtr()) {
                    std::string sName = structObj->DescriptiveName.ToString();
                    if (!sName.empty() && sName != "None") {
                        return std::wstring(sName.begin(), sName.end());
                    }
                }
            }
            if (ValidObject(actor->Class)) {
                std::string cName = actor->Class->GetName();
                if (cName.rfind("Structure", 0) == 0) cName = cName.substr(9);
                size_t cIdx = cName.find("_C");
                if (cIdx != std::string::npos && cIdx == cName.length() - 2) cName = cName.substr(0, cIdx);
                for (auto& ch : cName) if (ch == '_') ch = ' ';
                if (!cName.empty()) return std::wstring(cName.begin(), cName.end());
            }
            if (actorType == 3) return L"Turret";
            if (actorType == 4) return L"Container";
            if (actorType == 5) return L"Generator";
            return L"Structure";
        }

        if (actorType == 7) {
            if (ValidObject(actor->Class)) {
                std::string cName = actor->Class->GetName();
                size_t cIdx = cName.find("_C");
                if (cIdx != std::string::npos && cIdx == cName.length() - 2) cName = cName.substr(0, cIdx);
                for (auto& ch : cName) if (ch == '_') ch = ' ';
                if (!cName.empty()) return std::wstring(cName.begin(), cName.end());
            }
            return L"World Item";
        }

        return L"Actor";
    }

    void DrawTextOutlined(SDK::UCanvas* canvas, SDK::UFont* font, const SDK::FString& text, const SDK::FVector2D& pos, const SDK::FVector2D& scale, const SDK::FLinearColor& color, const SDK::FLinearColor& outlineColor) {
        if (!ValidObject(canvas) || !ValidObject(font)) return;

        if (pos.X < -200.f || pos.Y < -200.f || pos.X > canvas->ClipX + 200.f || pos.Y > canvas->ClipY + 200.f) return;

        bool bOutlined = Config::g_Settings.bTextOutlines;
        canvas->K2_DrawText(font, text, pos, scale, color, 0.f, outlineColor, SDK::FVector2D{0.5f, 0.5f}, true, true, bOutlined, outlineColor);
    }
}
