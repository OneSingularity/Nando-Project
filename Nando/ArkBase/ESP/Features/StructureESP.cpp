#include "pch.h"
#include "StructureESP.h"
#include "Config/Configs.h"

namespace ESP {
    void DrawStructureESP(SDK::UCanvas* canvas, SDK::AActor* actor, int actorType, SDK::APlayerController* pc, SDK::UFont* espFont, const SDK::FVector& myLoc, float allowedDist) {
        if (!ValidObject(canvas) || !ValidObject(actor) || actor->bHidden) return;

        auto structObj = static_cast<SDK::APrimalStructure*>(actor);
        if (!ValidObject(structObj) || structObj->MaxHealth <= 1.0f) return;

        if (actorType == 3 && !Config::g_Settings.Structure.bShowTurrets) return;
        if (actorType == 4 && !Config::g_Settings.Structure.bShowContainers) return;
        if (actorType == 5 && !Config::g_Settings.Structure.bShowGenerators) return;
        if (actorType == 6 && !Config::g_Settings.Structure.bShowOthers) return;

        SDK::FVector actorLoc = actor->K2_GetActorLocation();
        float dist = static_cast<float>(myLoc.GetDistanceTo(actorLoc));
        if (dist > allowedDist) return;

        int32_t myTeam = 0;
        if (ValidObject(pc) && ValidObject(pc->Pawn)) {
            auto localChar = static_cast<SDK::APrimalCharacter*>(pc->Pawn);
            if (ValidObject(localChar)) myTeam = localChar->TargetingTeam;
        }

        bool isFriendly = actor->TargetingTeam != 0 && actor->TargetingTeam == myTeam;
        bool isEnemy = !isFriendly;

        if (isFriendly && !Config::g_Settings.Structure.bShowFriendly) return;
        if (isEnemy && !Config::g_Settings.Structure.bShowEnemy) return;

        SDK::FVector2D screenPos;
        if (pc->ProjectWorldLocationToScreen(actorLoc, &screenPos, false)) {
            // Smooth FPS screen bounds check
            if (screenPos.X < -100.f || screenPos.Y < -100.f || screenPos.X > canvas->ClipX + 100.f || screenPos.Y > canvas->ClipY + 100.f) return;

            SDK::FLinearColor color;
            if (isFriendly) {
                color = SDK::FLinearColor{ Config::g_Settings.Colors.Friendly[0], Config::g_Settings.Colors.Friendly[1], Config::g_Settings.Colors.Friendly[2], Config::g_Settings.Colors.Friendly[3] };
            } else if (isEnemy) {
                color = SDK::FLinearColor{ Config::g_Settings.Colors.Enemy[0], Config::g_Settings.Colors.Enemy[1], Config::g_Settings.Colors.Enemy[2], Config::g_Settings.Colors.Enemy[3] };
            } else {
                color = SDK::FLinearColor{ Config::g_Settings.Colors.Structure[0], Config::g_Settings.Colors.Structure[1], Config::g_Settings.Colors.Structure[2], Config::g_Settings.Colors.Structure[3] };
            }
            SDK::FLinearColor outlineColor{0.f, 0.f, 0.f, 1.f};

            SDK::FVector2D scale{Config::g_Settings.ESPTextSize, Config::g_Settings.ESPTextSize};
            float textY = screenPos.Y + 12.f;
            float lineSpacing = 26.f * scale.Y;

            Config::RelationSettings& rSettings = isFriendly ? Config::g_Settings.StructureFriendly : Config::g_Settings.StructureEnemy;

            if (rSettings.bNames) {
                std::wstring label = GetDescriptiveActorName(actor, actorType);

                if (rSettings.bShowSlots && (actorType == 4 || actorType == 5)) {
                    auto container = static_cast<SDK::APrimalStructureItemContainer*>(actor);
                    if (ValidObject(container)) {
                        int currentItems = 0;
                        int maxItems = 0;
                        if (ValidObject(container->MyInventoryComponent)) {
                            maxItems = container->MyInventoryComponent->MaxInventoryItems;
                            auto& items = container->MyInventoryComponent->InventoryItems;
                            int itemNum = items.Num();
                            auto itemPtrs = items.GetDataPtr();
                            if (itemPtrs) {
                                for (int idx = 0; idx < itemNum; idx++) {
                                    SDK::UPrimalItem* item = itemPtrs[idx];
                                    if (ValidObject(item)) {
                                        if (item->bIsEngram) continue;
                                        currentItems++;
                                    }
                                }
                            }
                        } else {
                            currentItems = container->CurrentItemCount;
                            maxItems = container->MaxItemCount;
                        }
                        if (maxItems > 0) {
                            wchar_t slotBuf[64];
                            swprintf_s(slotBuf, 64, L" (%d/%d)", currentItems, maxItems);
                            label += slotBuf;
                        }
                    }
                }

                DrawTextOutlined(canvas, espFont, SDK::FString(label.c_str()), SDK::FVector2D{screenPos.X, textY}, scale, color, outlineColor);
                textY += lineSpacing;
            }

            if (rSettings.bShowBullets && actorType == 3) {
                auto turret = static_cast<SDK::APrimalStructureTurret*>(actor);
                if (ValidObject(turret)) {
                    wchar_t ammoBuf[64];
                    swprintf_s(ammoBuf, 64, L"%d", turret->NumBullets);
                    DrawTextOutlined(canvas, espFont, SDK::FString(ammoBuf), SDK::FVector2D{screenPos.X, textY}, scale * 0.9f, color, outlineColor);
                    textY += lineSpacing;
                }
            }

            if (rSettings.bHealth) {
                float hp = structObj->Health;
                float maxHp = structObj->MaxHealth;
                wchar_t hpBuf[64];
                swprintf_s(hpBuf, 64, L"%.0f / %.0f HP", hp, maxHp);
                DrawTextOutlined(canvas, espFont, SDK::FString(hpBuf), SDK::FVector2D{screenPos.X, textY}, scale * 0.9f, color, outlineColor);
                textY += lineSpacing;
            }

            if (rSettings.bDistance) {
                wchar_t distBuf[32];
                swprintf_s(distBuf, 32, L"%dm", static_cast<int>(dist / 100.f));
                DrawTextOutlined(canvas, espFont, SDK::FString(distBuf), SDK::FVector2D{screenPos.X, textY}, scale * 0.9f, color, outlineColor);
            }
        }
    }

    static bool ProjectWorldToScreenManual(SDK::APlayerController* pc, SDK::UCanvas* canvas, const SDK::FVector& worldPos, SDK::FVector2D& screenPos) {
        if (!ValidObject(pc) || !ValidObject(canvas)) return false;
        SDK::APlayerCameraManager* cameraManager = pc->PlayerCameraManager;
        if (!ValidObject(cameraManager)) return false;

        SDK::FVector camLoc = cameraManager->GetCameraLocation();
        SDK::FRotator camRot = cameraManager->GetCameraRotation();
        float fov = cameraManager->GetFOVAngle();

        SDK::FVector temp = worldPos - camLoc;
        SDK::FVector axisX, axisY, axisZ;
        SDK::UKismetMathLibrary::GetAxes(camRot, &axisX, &axisY, &axisZ);

        float transformedX = temp.X * axisY.X + temp.Y * axisY.Y + temp.Z * axisY.Z;
        float transformedY = temp.X * axisZ.X + temp.Y * axisZ.Y + temp.Z * axisZ.Z;
        float transformedZ = temp.X * axisX.X + temp.Y * axisX.Y + temp.Z * axisX.Z;

        if (transformedZ > 0.01f) {
            float fovRad = fov * (3.1415926535f / 360.f);
            float screenCenterX = static_cast<float>(canvas->SizeX) / 2.f;
            float screenCenterY = static_cast<float>(canvas->SizeY) / 2.f;

            screenPos.X = screenCenterX + transformedX * (screenCenterX / tanf(fovRad)) / transformedZ;
            screenPos.Y = screenCenterY - transformedY * (screenCenterX / tanf(fovRad)) / transformedZ;
            return true;
        }
        return false;
    }

    void DrawWorldESP(SDK::UCanvas* canvas, SDK::AActor* actor, int actorType, SDK::APlayerController* pc, SDK::UFont* espFont, const SDK::FVector& myLoc, float allowedDist) {
        if (!ValidObject(canvas) || !ValidObject(actor) || actor->bHidden) return;

        SDK::FVector actorLoc = actor->K2_GetActorLocation();
        float dist = static_cast<float>(myLoc.GetDistanceTo(actorLoc));
        if (dist > allowedDist) return;

        SDK::FVector2D screenPos;
        if (ProjectWorldToScreenManual(pc, canvas, actorLoc, screenPos)) {
            if (screenPos.X < -100.f || screenPos.Y < -100.f || screenPos.X > canvas->ClipX + 100.f || screenPos.Y > canvas->ClipY + 100.f) return;

            SDK::FLinearColor color{ 1.f, 1.f, 1.f, 1.f };
            SDK::FLinearColor outlineColor{ 0.f, 0.f, 0.f, 1.f };
            SDK::FVector2D scale{ Config::g_Settings.ESPTextSize, Config::g_Settings.ESPTextSize };
            float textY = screenPos.Y;
            float lineSpacing = 22.f * scale.Y;

            std::wstring worldItemLabel = GetDescriptiveActorName(actor, actorType);
            DrawTextOutlined(canvas, espFont, SDK::FString(worldItemLabel.c_str()), SDK::FVector2D{screenPos.X, textY}, scale, color, outlineColor);
            textY += lineSpacing;

            wchar_t distBuf[32];
            swprintf_s(distBuf, 32, L"%dm", static_cast<int>(dist / 100.f));
            DrawTextOutlined(canvas, espFont, SDK::FString(distBuf), SDK::FVector2D{screenPos.X, textY}, scale * 0.9f, color, outlineColor);
        }
    }
}
