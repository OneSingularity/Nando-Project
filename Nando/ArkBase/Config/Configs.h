#pragma once

namespace Config {
    struct RelationSettings {
        bool bBoxes = false;
        bool bNames = true;
        bool bSkeleton = false; 
        bool bLevel = true;
        bool bTribe = false; 
        bool bDistance = true;
        bool bHealth = true;
        bool bWeapon = false; 
        bool bWeight = false; 
        bool bArmorHUD = false; 
        bool bShowSlots = true; 
        bool bShowBullets = true; // used for turrets
        float MaxDistance = 300.f;
    };

    struct VisualSettings {
        bool bEnabled = true;
        bool bBoxes = true;
        bool bNames = true;
        bool bSkeleton = true; // used only for players
        bool bLevel = true;
        bool bTribe = false;
        bool bDistance = true;
        bool bHealth = true;
        bool bWeapon = false;
        bool bWeight = false;
        bool bArmorHUD = false;
        bool bShowSlots = true; // used for containers/turrets

        // Relationship filters
        bool bShowFriendly = true;
        bool bShowEnemy = true;
        bool bShowWild = true; // used only for dinos
        bool bShowSleeping = true; // players
        bool bShowCorpses = true; // players
        bool bDrawLocal = false; // players
        bool bSelfArmor = false; // players

        // Subtypes filters (for structures)
        bool bShowTurrets = true;
        bool bShowContainers = true;
        bool bShowGenerators = true;
        bool bShowOthers = false;

        float MaxDistance = 300.0f; // meters
        float TextSize = 0.86f;   // scale factor
    };

    struct ColorSettings {
        float Friendly[4] = { 0.2f, 0.6f, 1.0f, 1.0f };   // Blue
        float Enemy[4] = { 0.9f, 0.2f, 0.2f, 1.0f };      // Red
        float Wild[4] = { 0.8f, 0.8f, 0.8f, 1.0f };       // Gray
        float Structure[4] = { 0.67f, 0.44f, 1.0f, 1.0f }; // Purple
        float Sleeping[4] = { 0.75f, 0.85f, 0.2f, 1.0f };  // Yellow-Green
        float Corpses[4] = { 0.4f, 0.4f, 0.4f, 1.0f };     // Gray
    };

    struct CheatSettings {
        bool bMenuOpen = true;
        bool bUnloadRequested = false;
        
        // Aimbot
        bool bAimbot = true;
        float AimbotFov = 90.0f;
        float AimbotSmooth = 5.0f;
        bool bSilentAim = false;

        // Visuals Configurations (Modular)
        VisualSettings Player;
        VisualSettings Dino;
        VisualSettings Structure;
        ColorSettings Colors;

        // Exploits
        bool bNoRecoil = false;
        bool bRapidFire = false;
        bool bInfiniteAmmo = false;
        bool bFlyHack = false;
        bool bAirstuck = false;
        float AirstuckDeltaTime = 0.0f;
        bool bNoTekRifleOverheat = false;
        bool bInfiniteWeight = false;
        bool bExtendedReach = false;
        bool bUnlimitedArms = false;

        // Visuals
        bool bPreviewMode = false;

        // World ESP
        bool bWorldEnabled = true;
        bool bSupplyDrops = true;
        bool bArtifacts = true;
        bool bCaveDrops = true;
        bool bTerminals = true;
        float WorldMaxDistance = 500.0f;
        
        float ColorSupplyDrop[4] = { 0.9f, 0.9f, 0.2f, 1.0f }; // Yellow
        float ColorArtifact[4] = { 0.2f, 0.9f, 0.9f, 1.0f };   // Cyan
        float ColorCaveDrop[4] = { 0.9f, 0.2f, 0.9f, 1.0f };   // Magenta
        float ColorTerminal[4] = { 0.2f, 0.9f, 0.2f, 1.0f };   // Green

        // FOV Changer
        bool bFOVChanger = false;
        float FOVValue = 90.0f;

        // ESP & Feature Keybinds
        int Keybind_PlayerESP = 0;
        int Keybind_DinoESP = 0;
        int Keybind_StructureESP = 0;
        int Keybind_WorldESP = 0;
        int Keybind_Airstuck = 0;
        int Keybind_Suicide = 0;
        int Keybind_GauntFloat = 0;
        bool bGauntFloat = false;
        bool bSuicide = false;
        bool bUnlockExplorerNotes = false;

        // Wild Dino Filter
        float MinWildLevel = 1.0f;

        // Global ESP text scale
        float ESPTextSize = 0.86f;

        // Exploits & Weapon Tweaks
        bool bRunWhileReloading = false;
        bool bRemoveScopeOverlay = false;

        // Custom Crosshair
        bool bCrosshair = false;
        int CrosshairType = 0; // 0 = Plus (+), 1 = Dot (.), 2 = Circle (O)
        float CrosshairSize = 10.0f;
        float CrosshairThickness = 1.5f;
        float CrosshairColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f }; // RGBA

        // Top-Left HUD Info Toggles
        bool bShowServerFPS = true;
        bool bShowUploadTimer = true;
        float ServerFPSColor[4] = { 0.2f, 1.0f, 0.2f, 1.0f };
        float UploadTimerColor[4] = { 1.0f, 0.85f, 0.2f, 1.0f };

        // Font & UI Settings
        int FontIndex = 22;
        bool bTextOutlines = true;
        bool bShowBottomHUD = true;

        // Relationship-specific settings
        RelationSettings PlayerFriendly;
        RelationSettings PlayerEnemy;

        RelationSettings DinoWild;
        RelationSettings DinoFriendly;
        RelationSettings DinoEnemy;

        RelationSettings StructureFriendly;
        RelationSettings StructureEnemy;
    };

    extern CheatSettings g_Settings;
}
