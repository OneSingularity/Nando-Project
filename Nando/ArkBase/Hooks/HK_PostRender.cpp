#include "pch.h"
#include "Hooks.h"
#include "Config/Configs.h"
#include "Interface/Menu.h"
#include "ESP/Core/EspCore.h"
#include "Exploits/Exploits.h"
#include "Pointers.h"

namespace Hooks {
    extern bool g_KeyState[256];
    extern bool g_KeyPendingDown[256];

    struct FEngineShowFlags {
        uint32_t PostProcessing : 1;
        uint32_t Bloom : 1;
        uint32_t LocalExposure : 1;
        uint32_t AntiAliasing : 1;
        uint32_t TemporalAA : 1;
        uint32_t AmbientCubemap : 1;
        uint32_t EyeAdaptation : 1;
        uint32_t GlobalIllumination : 1;
        uint32_t Vignette : 1;
        uint32_t AmbientOcclusion : 1;
        uint32_t Decals : 1;
        uint32_t OnScreenDebug : 1;
        uint32_t VisualizeNanite : 1;
        uint32_t VisualizeLumen : 1;
        uint32_t VisualizeSubstrate : 1;
        uint32_t VisualizeGroom : 1;
        uint32_t VisualizeVirtualShadowMap : 1;
        uint32_t PointLights : 1;
        uint32_t SpotLights : 1;
        uint32_t RectLights : 1;
        uint32_t DepthOfField : 1;
        uint32_t MotionBlur : 1;
        uint32_t CameraInterpolation : 1;
        uint32_t ToneCurve : 1;
        uint32_t SeparateTranslucency : 1;
        uint32_t ScreenPercentage : 1;
        uint32_t ReflectionEnvironment : 1;
        uint32_t Specular : 1;
        uint32_t ScreenSpaceReflections : 1;
        uint32_t LumenReflections : 1;
        uint32_t ContactShadows : 1;
        uint32_t RayTracedDistanceFieldShadows : 1;
        uint32_t CapsuleShadows : 1;
        uint32_t VolumetricLightmap : 1;
        uint32_t IndirectLightingCache : 1;
        uint32_t TexturedLightProfiles : 1;
        uint32_t LightFunctions : 1;
        uint32_t NaniteMeshes : 1;
        uint32_t InstancedStaticMeshes : 1;
        uint32_t InstancedFoliage : 1;
        uint32_t InstancedGrass : 1;
        uint32_t DynamicShadows : 1;
        uint32_t Particles : 1;
        uint32_t SkeletalMeshes : 1;
        uint32_t Translucency : 1;
        uint32_t LOD : 1;
        uint32_t Lighting : 1;
        uint32_t DeferredLighting : 1;
        uint32_t StaticMeshes : 1;
        uint32_t Landscape : 1;
        uint32_t Fog : 1;
        uint32_t Game : 1;
        uint32_t BSP : 1;
        uint32_t LightShafts : 1;
        uint32_t Atmosphere : 1;
        uint32_t TextRender : 1;
        uint32_t Rendering : 1;
        uint32_t HMDDistortion : 1;
        uint32_t StereoRendering : 1;
        uint32_t DistanceCulledPrimitives : 1;
        uint32_t SkyLighting : 1;
        uint32_t Paper2DSprites : 1;
        uint32_t ScreenSpaceAO : 1;
        uint32_t DistanceFieldAO : 1;
        uint32_t LumenGlobalIllumination : 1;
        uint32_t VolumetricFog : 1;
        uint32_t WidgetComponents : 1;
        uint32_t MediaPlanes : 1;
        uint32_t PathTracing : 1;
    };

    static float g_OriginalFOV = 0.0f;
    static bool g_FOVApplied = false;
    static SDK::APlayerCameraManager* g_LastFOVCamMgr = nullptr;

    void ProcessFOVChanger(SDK::APlayerController* pc) {
        if (!pc) return;

        SDK::APlayerCameraManager* CamMgr = pc->PlayerCameraManager;
        if (!CamMgr) return;

        static float s_lastAppliedFOV = 0.0f;
        if (CamMgr != g_LastFOVCamMgr) {
            g_LastFOVCamMgr = CamMgr;
            g_FOVApplied = false;
            g_OriginalFOV = 0.0f;
            s_lastAppliedFOV = 0.0f;
        }

        uintptr_t base = reinterpret_cast<uintptr_t>(CamMgr);
        float* pNormalFOV = reinterpret_cast<float*>(base + 0x3680);

        if (Config::g_Settings.bFOVChanger) {
            if (!g_FOVApplied) {
                g_OriginalFOV = *pNormalFOV;
                g_FOVApplied = true;
            }
            if (s_lastAppliedFOV != Config::g_Settings.FOVValue || *pNormalFOV != Config::g_Settings.FOVValue) {
                *pNormalFOV = Config::g_Settings.FOVValue;
                s_lastAppliedFOV = Config::g_Settings.FOVValue;
            }
        } else {
            if (g_FOVApplied) {
                *pNormalFOV = g_OriginalFOV;
                g_FOVApplied = false;
                s_lastAppliedFOV = 0.0f;
            }
        }
    }

    /*
     * ViewportClient's PostRender hook.
     * The game viewport client invokes this callback at the end of each rendered frame before presenting it on screen.
     * We use this hook to perform rendering operations (ESP overlays and Cheat Menu UI) using the engine Canvas.
     */
    void __fastcall Hooked_PostRender(void* ViewportClient, SDK::UCanvas* Canvas) {
        if (!ViewportClient || !Canvas || bShuttingDown) {
            if (Orig_PostRender) Orig_PostRender(ViewportClient, Canvas);
            return;
        }

        SDK::UWorld* world = SDK::UWorld::GetWorld();
        globals::g_ActiveWorld = world;

        /*
         * Track the local player controller.
         * The game instance holds a list of LocalPlayers (which is usually size 1 for client builds).
         * We retrieve the player controller of the first local player to query local pawn states.
         */
        if (world && world->OwningGameInstance && world->OwningGameInstance->LocalPlayers.Num() > 0) {
            SDK::APlayerController* pc = world->OwningGameInstance->LocalPlayers[0]->PlayerController;
            if (pc && pc != globals::g_ActivePC) {
                globals::g_ActivePC = pc;
            }
        }

        if (globals::g_ActivePC) {
            ProcessFOVChanger(globals::g_ActivePC);

            auto shooterPC = static_cast<SDK::AShooterPlayerController*>(globals::g_ActivePC);
            if (shooterPC) {
                auto shooterChar = static_cast<SDK::AShooterCharacter*>(shooterPC->Pawn);
                Exploits::ProcessNoWeaponOverheat(shooterPC, shooterChar);
                Exploits::ProcessInfiniteWeight(shooterPC);
                Exploits::ProcessLongArms(shooterPC);
                Exploits::ProcessSuicide(shooterPC);
                Exploits::ProcessGauntFloat(shooterPC);
                Exploits::ProcessUnlockExplorerNotes(shooterPC, shooterChar);
                Exploits::ProcessRunWhileReloading(shooterPC, shooterChar);
                Exploits::ProcessRemoveScopeOverlay(shooterPC, shooterChar);
            }

            if (globals::g_ActivePC->Pawn) {
                auto charPawn = static_cast<SDK::ACharacter*>(globals::g_ActivePC->Pawn);
                if (charPawn && charPawn->CharacterMovement) {
                    HookReplicateMove(charPawn->CharacterMovement);
                }
            }
        }

        if (Menu::m_BindingKey && (GetTickCount64() - Menu::g_BindingStartTime > 150)) {
            for (int k = 1; k < 256; k++) {
                if (k == VK_LBUTTON) continue; // Skip left-click so we don't capture the UI click itself
                if (g_KeyPendingDown[k]) {
                    g_KeyPendingDown[k] = false;
                    if (k == VK_ESCAPE) {
                        *Menu::m_BindingKey = 0; // Clear bind
                    } else {
                        *Menu::m_BindingKey = k;
                    }
                    Menu::m_BindingKey = nullptr;
                    break;
                }
            }
        } else {
            // Process hotkey toggle switches for each ESP category
            auto checkToggle = [](int key, bool& state) {
                if (key > 0 && g_KeyPendingDown[key]) {
                    g_KeyPendingDown[key] = false;
                    state = !state;
                }
            };
            checkToggle(Config::g_Settings.Keybind_PlayerESP, Config::g_Settings.Player.bEnabled);
            checkToggle(Config::g_Settings.Keybind_DinoESP, Config::g_Settings.Dino.bEnabled);
            checkToggle(Config::g_Settings.Keybind_StructureESP, Config::g_Settings.Structure.bEnabled);
            checkToggle(Config::g_Settings.Keybind_WorldESP, Config::g_Settings.bWorldEnabled);
            checkToggle(Config::g_Settings.Keybind_Airstuck, Config::g_Settings.bAirstuck);
        }

        // Toggle UI menu state on F8 keypress
        if (g_KeyPendingDown[VK_F8]) {
            g_KeyPendingDown[VK_F8] = false;
            Menu::ToggleMenuState(!Menu::g_Open);
        }

        // Handle DLL unload cleanup and thread termination on unload UI request
        if (Config::g_Settings.bUnloadRequested) {
            Menu::ToggleMenuState(false);
            bShuttingDown = true;
            
            HMODULE hMod = nullptr;
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCSTR>(globals::g_DllBase), &hMod)) {
                CreateThread(nullptr, 0, Hooks::UnloadThread, hMod, 0, nullptr);
            }
            return;
        }

        // 2. Apply Preview Mode / Rendering Tweaks (INI bypass flags)
        uintptr_t vc = reinterpret_cast<uintptr_t>(ViewportClient);
        FEngineShowFlags* flags = reinterpret_cast<FEngineShowFlags*>(vc + 0xB8);
        if (Config::g_Settings.bPreviewMode) {
            *(int*)(vc + 0xB0) = 2; // Set ViewMode to Unlit
            flags->Fog = 0;
            flags->VolumetricFog = 0;
            flags->Atmosphere = 0;
            flags->Bloom = 0;
            flags->DynamicShadows = 0;
        } else {
            if (*(int*)(vc + 0xB0) == 2) {
                *(int*)(vc + 0xB0) = 3; // Restore ViewMode to Lit
            }
            flags->Fog = 1;
            flags->VolumetricFog = 1;
            flags->Atmosphere = 1;
            flags->Bloom = 1;
            flags->DynamicShadows = 1;
        }

        // Call original viewport drawing
        if (Orig_PostRender) {
            Orig_PostRender(ViewportClient, Canvas);
        }

        static SDK::UFont* hudFont = nullptr;
        if (!ValidObjectPtr(hudFont)) {
            hudFont = SDK::UObject::FindObject<SDK::UFont>("Font Roboto18.Roboto18");
            if (!ValidObjectPtr(hudFont)) {
                SDK::UEngine* engine = GetEngineSafe();
                if (engine) hudFont = engine->MediumFont;
            }
        }

        if (ValidObjectPtr(hudFont) && Canvas) {
            SDK::FLinearColor whiteCol{1.f, 1.f, 1.f, 1.f};
            SDK::FLinearColor neonPurple{0.67f, 0.44f, 1.f, 1.f};
            SDK::FLinearColor shadowCol{0.f, 0.f, 0.f, 0.95f};
            SDK::FLinearColor greenCol{0.2f, 0.9f, 0.2f, 1.f};
            SDK::FVector2D textScale{1.0f, 1.0f};

            // A. Top-Left Watermark HUD
            if (world && world->GameState) {
                auto gameState = static_cast<SDK::AShooterGameState*>(world->GameState);
                if (gameState) {
                    float textX = 20.f;
                    float textY = 20.f;
                    float spacing = 26.f;

                    // Watermark
                    Canvas->K2_DrawText(hudFont, SDK::FString(L"Nando Client"), SDK::FVector2D{textX, textY}, textScale, neonPurple, 0.f, shadowCol, SDK::FVector2D{0.f, 0.f}, false, false, Config::g_Settings.bTextOutlines, shadowCol);
                    textY += spacing;

                    // Server Name
                    std::wstring serverName = L"Server: Private";
                    if (gameState->ServerSessionName.GetDataPtr()) {
                        std::string sessionStr = gameState->ServerSessionName.ToString();
                        if (!sessionStr.empty() && sessionStr != "None") {
                            std::wstring sessionWStr(sessionStr.begin(), sessionStr.end());
                            serverName = L"Server: " + sessionWStr;
                        }
                    }
                    Canvas->K2_DrawText(hudFont, SDK::FString(serverName.c_str()), SDK::FVector2D{textX, textY}, textScale, whiteCol, 0.f, shadowCol, SDK::FVector2D{0.f, 0.f}, false, false, Config::g_Settings.bTextOutlines, shadowCol);
                    textY += spacing;

                    // Connected Players Count
                    wchar_t playersBuf[64];
                    swprintf_s(playersBuf, 64, L"Players: %d", gameState->NumPlayerConnected);
                    Canvas->K2_DrawText(hudFont, SDK::FString(playersBuf), SDK::FVector2D{textX, textY}, textScale, whiteCol, 0.f, shadowCol, SDK::FVector2D{0.f, 0.f}, false, false, Config::g_Settings.bTextOutlines, shadowCol);
                    textY += spacing;

                    // Tamed Dinos Count
                    wchar_t dinosBuf[64];
                    swprintf_s(dinosBuf, 64, L"Tamed Dinos: %d", gameState->NumTamedDinos);
                    Canvas->K2_DrawText(hudFont, SDK::FString(dinosBuf), SDK::FVector2D{textX, textY}, textScale, whiteCol, 0.f, shadowCol, SDK::FVector2D{0.f, 0.f}, false, false, Config::g_Settings.bTextOutlines, shadowCol);
                    textY += spacing;

                    // Server FPS
                    if (Config::g_Settings.bShowServerFPS) {
                        SDK::FLinearColor sfpsCol{ Config::g_Settings.ServerFPSColor[0], Config::g_Settings.ServerFPSColor[1], Config::g_Settings.ServerFPSColor[2], Config::g_Settings.ServerFPSColor[3] };
                        wchar_t fpsBuf[64];
                        swprintf_s(fpsBuf, 64, L"Server FPS: %.0f", gameState->ServerFramerate);
                        Canvas->K2_DrawText(hudFont, SDK::FString(fpsBuf), SDK::FVector2D{textX, textY}, textScale, sfpsCol, 0.f, shadowCol, SDK::FVector2D{0.f, 0.f}, false, false, Config::g_Settings.bTextOutlines, shadowCol);
                        textY += spacing;
                    }

                    // Upload Timer
                    if (Config::g_Settings.bShowUploadTimer) {
                        SDK::FLinearColor utCol{ Config::g_Settings.UploadTimerColor[0], Config::g_Settings.UploadTimerColor[1], Config::g_Settings.UploadTimerColor[2], Config::g_Settings.UploadTimerColor[3] };
                        float timeRemaining = gameState->ServerSaveInterval - (gameState->PrivateNetworkTime - gameState->LastServerSaveTime);
                        if (timeRemaining < 0.0f) timeRemaining = 0.0f;
                        int totalSecs = static_cast<int>(timeRemaining);
                        int mins = totalSecs / 60;
                        int secs = totalSecs % 60;
                        wchar_t timerBuf[64];
                        swprintf_s(timerBuf, 64, L"Timer: %02d:%02d", mins, secs);
                        Canvas->K2_DrawText(hudFont, SDK::FString(timerBuf), SDK::FVector2D{textX, textY}, textScale, utCol, 0.f, shadowCol, SDK::FVector2D{0.f, 0.f}, false, false, Config::g_Settings.bTextOutlines, shadowCol);
                        textY += spacing;
                    }
                }
            }

            // Custom Crosshair Rendering
            if (Config::g_Settings.bCrosshair) {
                float centerX = Canvas->SizeX / 2.0f;
                float centerY = Canvas->SizeY / 2.0f;
                float s = Config::g_Settings.CrosshairSize;
                float t = Config::g_Settings.CrosshairThickness;
                SDK::FLinearColor chCol{ Config::g_Settings.CrosshairColor[0], Config::g_Settings.CrosshairColor[1], Config::g_Settings.CrosshairColor[2], Config::g_Settings.CrosshairColor[3] };

                if (Config::g_Settings.CrosshairType == 0) { // Plus (+)
                    Canvas->K2_DrawLine(SDK::FVector2D{ centerX - s, centerY }, SDK::FVector2D{ centerX + s, centerY }, t, chCol);
                    Canvas->K2_DrawLine(SDK::FVector2D{ centerX, centerY - s }, SDK::FVector2D{ centerX, centerY + s }, t, chCol);
                } else if (Config::g_Settings.CrosshairType == 1) { // Dot (.)
                    Canvas->K2_DrawLine(SDK::FVector2D{ centerX - t / 2.f, centerY }, SDK::FVector2D{ centerX + t / 2.f, centerY }, t, chCol);
                } else if (Config::g_Settings.CrosshairType == 2) { // Circle (O)
                    constexpr int segments = 24;
                    constexpr float angleStep = 6.2831853f / static_cast<float>(segments);
                    for (int i = 0; i < segments; i++) {
                        float a1 = angleStep * i;
                        float a2 = angleStep * (i + 1);
                        float x1 = centerX + cosf(a1) * s;
                        float y1 = centerY + sinf(a1) * s;
                        float x2 = centerX + cosf(a2) * s;
                        float y2 = centerY + sinf(a2) * s;
                        Canvas->K2_DrawLine(SDK::FVector2D{ x1, y1 }, SDK::FVector2D{ x2, y2 }, t, chCol);
                    }
                }
            }

            // B. Bottom-Left FPS & Actor Count HUD
            if (Config::g_Settings.bShowBottomHUD) {
                static float s_FPS = 0.f;
                static int s_FrameCount = 0;
                static uint64_t s_LastFPSTime = GetTickCount64();

                s_FrameCount++;
                uint64_t now = GetTickCount64();
                if (now - s_LastFPSTime >= 500) {
                    s_FPS = (s_FrameCount * 1000.f) / static_cast<float>(now - s_LastFPSTime);
                    s_FrameCount = 0;
                    s_LastFPSTime = now;
                }

                int actorCount = 0;
                if (world && world->PersistentLevel) {
                    actorCount = world->PersistentLevel->Actors.Num();
                }

                float btmX = 20.f;
                float btmY = Canvas->ClipY - 60.f;

                wchar_t bottomHudBuf[128];
                swprintf_s(bottomHudBuf, 128, L"FPS: %.1f | Actors: %d", s_FPS, actorCount);
                Canvas->K2_DrawText(hudFont, SDK::FString(bottomHudBuf), SDK::FVector2D{btmX, btmY}, textScale, greenCol, 0.f, shadowCol, SDK::FVector2D{0.f, 0.f}, false, false, Config::g_Settings.bTextOutlines, shadowCol);
            }
        }

        // 4. Draw ESP overlays
        ESP::DrawESP(Canvas);

        // 5. Draw UI Menu overlays
        Menu::RenderMenu(Canvas);
    }
}
