#pragma once
#include "pch.h"
#include "../Classes/ClassIndices.h"

namespace ESP {
    enum EJoints {
        J_HEAD,
        J_NECK,
        J_SPINE3, // chest
        J_SPINE2,
        J_SPINE1,
        J_PELVIS,
        J_LSHOULDER,
        J_LELBOW,
        J_LHAND,
        J_RSHOULDER,
        J_RELBOW,
        J_RHAND,
        J_LHIP,
        J_LKNEE,
        J_LFOOT,
        J_RHIP,
        J_RKNEE,
        J_RFOOT,
        J_COUNT
    };

    extern const char* g_JointNames[J_COUNT];
    extern const int g_BonePairs[][2];
    extern int32_t g_JointNameIndices[J_COUNT];
    extern bool g_JointNameIndicesInitialized;

    void InitializeJointNameIndices();
    int FindBoneIdx(SDK::USkeletalMeshComponent* Mesh, int32_t targetComparisonIndex);
    void DrawPlayerSkeleton(SDK::UCanvas* canvas, SDK::AShooterCharacter* playerChar, SDK::APlayerController* pc, SDK::FLinearColor color);
    void DrawPlayerDinoESP(SDK::UCanvas* canvas, SDK::AActor* actor, int actorType, SDK::APlayerController* pc, SDK::UFont* espFont, const SDK::FVector& myLoc, float allowedDist);
}
