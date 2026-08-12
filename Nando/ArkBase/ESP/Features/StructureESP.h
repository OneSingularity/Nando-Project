#pragma once
#include "pch.h"
#include "../Classes/ClassIndices.h"

namespace ESP {
    void DrawStructureESP(SDK::UCanvas* canvas, SDK::AActor* actor, int actorType, SDK::APlayerController* pc, SDK::UFont* espFont, const SDK::FVector& myLoc, float allowedDist);
    void DrawWorldESP(SDK::UCanvas* canvas, SDK::AActor* actor, int actorType, SDK::APlayerController* pc, SDK::UFont* espFont, const SDK::FVector& myLoc, float allowedDist);
}
