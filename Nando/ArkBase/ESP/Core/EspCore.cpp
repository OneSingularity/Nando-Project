#include "pch.h"
#include "EspCore.h"
#include "../Features/PlayerDinoESP.h"
#include "../Features/StructureESP.h"
#include "Config/Configs.h"
#include "Pointers.h"

namespace ESP {
    void DrawESP(SDK::UCanvas* canvas) {
        if (!ValidObject(canvas)) return;

        SDK::UWorld* world = globals::g_ActiveWorld;
        if (!ValidObject(world) || !ValidObject(world->PersistentLevel)) return;

        SDK::APlayerController* pc = nullptr;
        if (ValidObject(world->OwningGameInstance) && world->OwningGameInstance->LocalPlayers.Num() > 0) {
            auto lp = world->OwningGameInstance->LocalPlayers[0];
            if (ValidObject(lp)) {
                pc = lp->PlayerController;
            }
        }
        if (!ValidObject(pc) || !ValidObject(pc->Pawn)) return;

        static SDK::UFont* espFont = nullptr;
        if (!ValidObject(espFont)) {
            espFont = SDK::UObject::FindObject<SDK::UFont>("Font Roboto18.Roboto18");
            if (!ValidObject(espFont)) {
                SDK::UEngine* engine = GetEngineSafe();
                if (ValidObject(engine)) espFont = engine->MediumFont;
            }
        }
        if (!ValidObject(espFont)) return;

        SDK::FVector myLoc = pc->Pawn->K2_GetActorLocation();
        SDK::TArray<SDK::AActor*>& actors = world->PersistentLevel->Actors;
        int numActors = actors.Num();
        SDK::AActor* const* actorData = actors.GetDataPtr();
        if (!actorData) return;

        for (int i = 0; i < numActors; i++) {
            SDK::AActor* actor = actorData[i];
            if (!ValidObject(actor) || actor->bHidden) continue;

            int type = ClassifyActorByComparisonIndex(actor);
            if (type == 0) continue;

            float maxDist = 300.f;
            if (type == 1) {
                if (actor == pc->Pawn && !Config::g_Settings.Player.bDrawLocal) continue;
                if (!Config::g_Settings.Player.bEnabled) continue;
                
                maxDist = Config::g_Settings.PlayerFriendly.MaxDistance;
                auto primalChar = static_cast<SDK::APrimalCharacter*>(actor);
                if (ValidObject(primalChar)) {
                    int myTeam = pc->Pawn ? pc->Pawn->TargetingTeam : 0;
                    bool isEnemy = primalChar->TargetingTeam != myTeam;
                    maxDist = isEnemy ? Config::g_Settings.PlayerEnemy.MaxDistance : Config::g_Settings.PlayerFriendly.MaxDistance;
                }
            } else if (type == 2) {
                if (!Config::g_Settings.Dino.bEnabled) continue;
                
                maxDist = Config::g_Settings.DinoWild.MaxDistance;
                auto dino = static_cast<SDK::APrimalDinoCharacter*>(actor);
                if (ValidObject(dino)) {
                    int myTeam = pc->Pawn ? pc->Pawn->TargetingTeam : 0;
                    bool isTamed = dino->BPIsTamed() || dino->TargetingTeam >= 50000 || dino->bHasRider != 0;
                    bool isFriendly = isTamed && (dino->TargetingTeam == myTeam || dino->TamingTeamID == myTeam);
                    bool isWild = !isTamed;
                    maxDist = isWild ? Config::g_Settings.DinoWild.MaxDistance : (isFriendly ? Config::g_Settings.DinoFriendly.MaxDistance : Config::g_Settings.DinoEnemy.MaxDistance);
                }
            } else if (type >= 3 && type <= 6) {
                if (!Config::g_Settings.Structure.bEnabled) continue;
                
                maxDist = Config::g_Settings.StructureFriendly.MaxDistance;
                auto structObj = static_cast<SDK::APrimalStructure*>(actor);
                if (ValidObject(structObj)) {
                    int myTeam = pc->Pawn ? pc->Pawn->TargetingTeam : 0;
                    bool isEnemyStruct = structObj->TargetingTeam != myTeam;
                    maxDist = isEnemyStruct ? Config::g_Settings.StructureEnemy.MaxDistance : Config::g_Settings.StructureFriendly.MaxDistance;
                }
            } else if (type == 7) {
                if (!Config::g_Settings.bWorldEnabled) continue;
                maxDist = Config::g_Settings.WorldMaxDistance;
            } else {
                continue;
            }

            float allowedDist = maxDist * 100.f;

            if (type == 1 || type == 2) {
                DrawPlayerDinoESP(canvas, actor, type, pc, espFont, myLoc, allowedDist);
            } else if (type >= 3 && type <= 6) {
                DrawStructureESP(canvas, actor, type, pc, espFont, myLoc, allowedDist);
            } else if (type == 7) {
                DrawWorldESP(canvas, actor, type, pc, espFont, myLoc, allowedDist);
            }
        }
    }
}
