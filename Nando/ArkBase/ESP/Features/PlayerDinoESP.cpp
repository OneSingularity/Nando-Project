#include "pch.h"
#include "PlayerDinoESP.h"
#include "Config/Configs.h"

namespace ESP {
    const char* g_JointNames[J_COUNT] = {
        "Cnt_Head_JNT_SKL",
        "Cnt_Neck_Joint001_JNT_SKL",
        "Cnt_Chest_000_JNT_SKL",
        "cnt_spine_003_jnt_skl",
        "Cnt_Spine_002_JNT_SKL",
        "Cnt_Pelvis_000_JNT_SKL",
        "Lft_Arm_001Tear000_JNT_SKL",
        "Lft_Arm_002Tear000_JNT_SKL",
        "Lft_Arm_002Tear006_JNT_SKL",
        "Rht_Arm_001Tear000_JNT_SKL",
        "Rht_Arm_002Tear000_JNT_SKL",
        "Rht_Arm_002Tear006_JNT_SKL",
        "Lft_Leg_001Tear000_JNT_SKL",
        "Lft_Leg_002Tear000_JNT_SKL",
        "Lft_Leg_002_JNT_SKL",
        "Rht_Leg_001Tear000_JNT_SKL",
        "Rht_Leg_002Tear000_JNT_SKL",
        "Rht_Leg_002_JNT_SKL"
    };

    const int g_BonePairs[][2] = {
        {J_HEAD, J_NECK},
        {J_NECK, J_SPINE3},
        {J_SPINE3, J_SPINE2},
        {J_SPINE2, J_SPINE1},
        {J_SPINE1, J_PELVIS},
        {J_LSHOULDER, J_LELBOW},
        {J_LELBOW, J_LHAND},
        {J_RSHOULDER, J_RELBOW},
        {J_RELBOW, J_RHAND},
        {J_PELVIS, J_LHIP},
        {J_LHIP, J_LKNEE},
        {J_LKNEE, J_LFOOT},
        {J_PELVIS, J_RHIP},
        {J_RHIP, J_RKNEE},
        {J_RKNEE, J_RFOOT},
        {J_NECK, J_LSHOULDER},
        {J_NECK, J_RSHOULDER}
    };

    int32_t g_JointNameIndices[J_COUNT] = {0};
    bool g_JointNameIndicesInitialized = false;

    void InitializeJointNameIndices() {
        if (g_JointNameIndicesInitialized) return;
        g_JointNameIndicesInitialized = true;
        for (int j = 0; j < J_COUNT; j++) {
            std::wstring wname(g_JointNames[j], g_JointNames[j] + strlen(g_JointNames[j]));
            g_JointNameIndices[j] = SDK::BasicFilesImpleUtils::StringToName(wname.c_str()).ComparisonIndex;
        }
    }

    int FindBoneIdx(SDK::USkeletalMeshComponent* Mesh, int32_t targetComparisonIndex) {
        if (!ValidObject(Mesh) || targetComparisonIndex == 0) return -1;
        int numBones = Mesh->GetNumBones();
        for (int i = 0; i < numBones && i < 250; i++) {
            SDK::FName fn = Mesh->GetBoneName(i);
            if (fn.ComparisonIndex == targetComparisonIndex)
                return i;
        }
        return -1;
    }

    void DrawPlayerSkeleton(SDK::UCanvas* canvas, SDK::AShooterCharacter* playerChar, SDK::APlayerController* pc, SDK::FLinearColor color) {
        if (!ValidObject(canvas) || !ValidObject(playerChar) || !ValidObject(pc)) return;
        SDK::USkeletalMeshComponent* mesh = playerChar->Mesh;
        if (!ValidObject(mesh) || mesh->GetNumBones() <= 0) return;

        InitializeJointNameIndices();

        int boneIdx[J_COUNT];
        for (int j = 0; j < J_COUNT; j++) {
            boneIdx[j] = FindBoneIdx(mesh, g_JointNameIndices[j]);
        }

        SDK::FVector2D jointScreen[J_COUNT];
        bool jointValid[J_COUNT] = {false};

        for (int j = 0; j < J_COUNT; j++) {
            if (boneIdx[j] < 0) continue;
            SDK::FName boneFN = mesh->GetBoneName(boneIdx[j]);
            if (boneFN.ComparisonIndex == 0) continue;

            SDK::FTransform t = mesh->GetBoneTransform(boneFN, SDK::ERelativeTransformSpace::RTS_World);
            SDK::FVector worldPos = t.Translation;
            if (worldPos.X == 0.f && worldPos.Y == 0.f && worldPos.Z == 0.f) continue;

            SDK::FVector2D sx;
            if (pc->ProjectWorldLocationToScreen(worldPos, &sx, false)) {
                jointScreen[j] = sx;
                jointValid[j] = true;
            }
        }

        int numPairs = sizeof(g_BonePairs) / sizeof(g_BonePairs[0]);
        for (int p = 0; p < numPairs; p++) {
            int a = g_BonePairs[p][0];
            int b = g_BonePairs[p][1];
            if (jointValid[a] && jointValid[b]) {
                canvas->K2_DrawLine(jointScreen[a], jointScreen[b], 2.f, color);
            }
        }
    }

    static void DrawCornerBox(SDK::UCanvas* canvas, float x, float y, float w, float h, float thickness, SDK::FLinearColor color) {
        if (!ValidObject(canvas)) return;
        SDK::FLinearColor black{0.f, 0.f, 0.f, 0.85f};
        float lineW = w / 4.f;
        float lineH = h / 4.f;

        auto drawLineWithShadow = [&](SDK::FVector2D p1, SDK::FVector2D p2) {
            canvas->K2_DrawLine(p1, p2, thickness + 1.f, black);
            canvas->K2_DrawLine(p1, p2, thickness, color);
        };

        drawLineWithShadow({x, y}, {x + lineW, y});
        drawLineWithShadow({x, y}, {x, y + lineH});
        drawLineWithShadow({x + w, y}, {x + w - lineW, y});
        drawLineWithShadow({x + w, y}, {x + w, y + lineH});
        drawLineWithShadow({x, y + h}, {x + lineW, y + h});
        drawLineWithShadow({x, y + h}, {x, y + h - lineH});
        drawLineWithShadow({x + w, y + h}, {x + w - lineW, y + h});
        drawLineWithShadow({x + w, y + h}, {x + w, y + h - lineH});
    }

    void DrawPlayerDinoESP(SDK::UCanvas* canvas, SDK::AActor* actor, int actorType, SDK::APlayerController* pc, SDK::UFont* espFont, const SDK::FVector& myLoc, float allowedDist) {
        if (!ValidObject(canvas) || !ValidObject(actor) || actor->bHidden) return;

        auto primalChar = static_cast<SDK::APrimalCharacter*>(actor);
        if (!ValidObject(primalChar)) return;

        bool isDead = false;
        if (ValidObject(primalChar->MyCharacterStatusComponent)) {
            float hp = primalChar->MyCharacterStatusComponent->CurrentStatusValues[0];
            if (hp <= 0.f) {
                isDead = true;
            }
        }

        int32_t myTeam = 0;
        if (ValidObject(pc) && ValidObject(pc->Pawn)) {
            auto localChar = static_cast<SDK::APrimalCharacter*>(pc->Pawn);
            if (ValidObject(localChar)) myTeam = localChar->TargetingTeam;
        }

        SDK::FLinearColor color{1.f, 1.f, 1.f, 1.f};
        SDK::FLinearColor outlineColor{0.f, 0.f, 0.f, 1.f};
        bool drawBoxes = false;
        bool drawNames = false;
        bool drawLevel = false;
        bool drawDistance = false;
        bool drawHealth = false;
        bool drawTribe = false;
        bool drawWeight = false;
        float textSize = 1.0f;

        bool isEnemy = primalChar->TargetingTeam != myTeam;

        if (actorType == 1) { // Player
            Config::RelationSettings& rSettings = isEnemy ? Config::g_Settings.PlayerEnemy : Config::g_Settings.PlayerFriendly;
            if (isDead) {
                if (!Config::g_Settings.Player.bShowCorpses) return;
                color = SDK::FLinearColor{ Config::g_Settings.Colors.Corpses[0], Config::g_Settings.Colors.Corpses[1], Config::g_Settings.Colors.Corpses[2], Config::g_Settings.Colors.Corpses[3] };
            } else if (primalChar->bIsSleeping) {
                if (!Config::g_Settings.Player.bShowSleeping) return;
                color = SDK::FLinearColor{ Config::g_Settings.Colors.Sleeping[0], Config::g_Settings.Colors.Sleeping[1], Config::g_Settings.Colors.Sleeping[2], Config::g_Settings.Colors.Sleeping[3] };
            } else {
                if (isEnemy && !Config::g_Settings.Player.bShowEnemy) return;
                if (!isEnemy && !Config::g_Settings.Player.bShowFriendly) return;

                color = isEnemy ? 
                    SDK::FLinearColor{ Config::g_Settings.Colors.Enemy[0], Config::g_Settings.Colors.Enemy[1], Config::g_Settings.Colors.Enemy[2], Config::g_Settings.Colors.Enemy[3] } :
                    SDK::FLinearColor{ Config::g_Settings.Colors.Friendly[0], Config::g_Settings.Colors.Friendly[1], Config::g_Settings.Colors.Friendly[2], Config::g_Settings.Colors.Friendly[3] };
            }

            drawBoxes = rSettings.bBoxes;
            drawNames = rSettings.bNames;
            drawLevel = rSettings.bLevel;
            drawDistance = rSettings.bDistance;
            drawHealth = rSettings.bHealth;
            drawTribe = rSettings.bTribe;
            drawWeight = rSettings.bWeight;
            textSize = Config::g_Settings.ESPTextSize;
        } else if (actorType == 2) { // Dino
            if (isDead) return;
            
            auto dino = static_cast<SDK::APrimalDinoCharacter*>(actor);
            bool isTamed = dino && (dino->BPIsTamed() || dino->TargetingTeam >= 50000 || dino->bHasRider != 0);
            bool isFriendly = isTamed && (dino->TargetingTeam == myTeam || dino->TamingTeamID == myTeam);
            bool isWild = !isTamed;
            bool isEnemyDino = isTamed && !isFriendly;

            if (isWild && !Config::g_Settings.Dino.bShowWild) return;
            if (isFriendly && !Config::g_Settings.Dino.bShowFriendly) return;
            if (isEnemyDino && !Config::g_Settings.Dino.bShowEnemy) return;

            if (isWild && ValidObject(primalChar->MyCharacterStatusComponent)) {
                int lvl = primalChar->MyCharacterStatusComponent->BaseCharacterLevel;
                if (static_cast<float>(lvl) < Config::g_Settings.MinWildLevel) return;
            }

            Config::RelationSettings& rSettings = isWild ? Config::g_Settings.DinoWild : 
                                                 (isFriendly ? Config::g_Settings.DinoFriendly : Config::g_Settings.DinoEnemy);

            if (isFriendly) {
                color = SDK::FLinearColor{ Config::g_Settings.Colors.Friendly[0], Config::g_Settings.Colors.Friendly[1], Config::g_Settings.Colors.Friendly[2], Config::g_Settings.Colors.Friendly[3] };
            } else if (isEnemyDino) {
                color = SDK::FLinearColor{ Config::g_Settings.Colors.Enemy[0], Config::g_Settings.Colors.Enemy[1], Config::g_Settings.Colors.Enemy[2], Config::g_Settings.Colors.Enemy[3] };
            } else {
                color = SDK::FLinearColor{ Config::g_Settings.Colors.Wild[0], Config::g_Settings.Colors.Wild[1], Config::g_Settings.Colors.Wild[2], Config::g_Settings.Colors.Wild[3] };
            }

            drawBoxes = false;
            drawNames = rSettings.bNames;
            drawLevel = rSettings.bLevel;
            drawDistance = rSettings.bDistance;
            drawHealth = rSettings.bHealth;
            textSize = Config::g_Settings.ESPTextSize;
        } else {
            return;
        }

        SDK::FVector actorLoc = actor->K2_GetActorLocation();
        float dist = static_cast<float>(myLoc.GetDistanceTo(actorLoc));
        if (dist > allowedDist) return;

        SDK::FVector2D screenPos;
        if (pc->ProjectWorldLocationToScreen(actorLoc, &screenPos, false)) {
            // Smooth FPS screen bounds check
            if (screenPos.X < -100.f || screenPos.Y < -100.f || screenPos.X > canvas->ClipX + 100.f || screenPos.Y > canvas->ClipY + 100.f) return;

            if (drawBoxes) {
                SDK::FVector headWorld = actorLoc;
                headWorld.Z += 85.f;
                SDK::FVector2D headScreen;
                if (pc->ProjectWorldLocationToScreen(headWorld, &headScreen, false)) {
                    float boxH = screenPos.Y - headScreen.Y;
                    float boxW = actorType == 1 ? boxH * 0.55f : boxH * 0.9f;
                    float boxX = headScreen.X - (boxW / 2.f);
                    DrawCornerBox(canvas, boxX, headScreen.Y, boxW, boxH, 1.5f, color);
                }
            }

            if (actorType == 1) {
                auto shooterChar = static_cast<SDK::AShooterCharacter*>(actor);
                bool isEnemyPlayer = primalChar->TargetingTeam != myTeam;
                Config::RelationSettings& rSettings = isEnemyPlayer ? Config::g_Settings.PlayerEnemy : Config::g_Settings.PlayerFriendly;
                if (rSettings.bSkeleton) {
                    DrawPlayerSkeleton(canvas, shooterChar, pc, color);
                }
            }

            SDK::FVector2D scale{textSize, textSize};
            float textY = screenPos.Y + 8.f;
            float lineSpacing = 22.f * scale.Y;

            if (drawNames || drawLevel) {
                std::wstring label = GetDescriptiveActorName(actor, actorType);
                if (drawLevel && ValidObject(primalChar->MyCharacterStatusComponent)) {
                    int lvl = primalChar->MyCharacterStatusComponent->BaseCharacterLevel;
                    if (lvl > 0) {
                        wchar_t lvlBuf[32];
                        swprintf_s(lvlBuf, 32, L" Lvl %d", lvl);
                        label += lvlBuf;
                    }
                }
                DrawTextOutlined(canvas, espFont, SDK::FString(label.c_str()), SDK::FVector2D{screenPos.X, textY}, scale, color, outlineColor);
                textY += lineSpacing;
            }

            if (drawHealth && ValidObject(primalChar->MyCharacterStatusComponent)) {
                float hp = primalChar->MyCharacterStatusComponent->CurrentStatusValues[0];
                float maxHp = primalChar->MyCharacterStatusComponent->MaxStatusValues[0];
                wchar_t hpBuf[64];
                swprintf_s(hpBuf, 64, L"%.0f / %.0f HP", hp, maxHp);
                DrawTextOutlined(canvas, espFont, SDK::FString(hpBuf), SDK::FVector2D{screenPos.X, textY}, scale * 0.9f, color, outlineColor);
                textY += lineSpacing;
            }

            if (drawDistance) {
                wchar_t distBuf[32];
                swprintf_s(distBuf, 32, L"%dm", static_cast<int>(dist / 100.f));
                DrawTextOutlined(canvas, espFont, SDK::FString(distBuf), SDK::FVector2D{screenPos.X, textY}, scale * 0.9f, color, outlineColor);
            }
        }
    }
}
