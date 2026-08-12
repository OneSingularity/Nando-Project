#include "pch.h"
#include "Menu.h"
#include "Config/Configs.h"
#include "Pointers.h"
#include <cmath>

namespace Hooks {
    extern bool g_KeyState[256];
    extern bool g_KeyPendingDown[256];
    extern POINT g_MousePos;
}

namespace Menu {
    bool g_Open = false;
    int* m_BindingKey = nullptr;
    ULONGLONG g_BindingStartTime = 0;
    
    static float m_X = 200.f;
    static float m_Y = 150.f;
    static float m_Width = 1100.f;
    static float m_Height = 640.f;

    static bool m_Dragging = false;
    static float m_DragOffsetX = 0.f;
    static float m_DragOffsetY = 0.f;

    static int m_ActiveSidebar = 1; 
    static int m_VisualsTab = 0; 

    // Fonts caching
    static SDK::UFont* g_MenuFont = nullptr;
    static bool g_FontInitialized = false;

    static void InitializeFont() {
        if (g_FontInitialized && g_MenuFont) return;
        
        SDK::UFont* distFont = SDK::UObject::FindObject<SDK::UFont>("Font Roboto18.Roboto18");
        if (distFont) {
            g_MenuFont = distFont;
            g_FontInitialized = true;
        } else {
            SDK::UEngine* engine = GetEngineSafe();
            if (engine) {
                g_MenuFont = engine->MediumFont;
                g_FontInitialized = true;
            }
        }
    }

    static void SetInputMode_GameAndUIEx_Safe(SDK::APlayerController* pc) {
        if (!pc) return;
        SDK::UClass* widgetBPClass = SDK::UWidgetBlueprintLibrary::StaticClass();
        if (!widgetBPClass || !widgetBPClass->ClassDefaultObject) return;
        
        SDK::UFunction* func = widgetBPClass->GetFunction("WidgetBlueprintLibrary", "SetInputMode_GameAndUIEx");
        if (!func) return;

        struct {
            SDK::APlayerController* PlayerController;
            SDK::UWidget* InWidgetToFocus;
            SDK::EMouseLockMode InMouseLockMode;
            bool bHideCursorDuringCapture;
            bool bFlushInput;
        } params{
            pc,
            nullptr,
            SDK::EMouseLockMode::DoNotLock,
            false,
            false
        };

        widgetBPClass->ClassDefaultObject->ProcessEvent(func, &params);
    }

    static void SetInputMode_GameOnly_Safe(SDK::APlayerController* pc) {
        if (!pc) return;
        SDK::UClass* widgetBPClass = SDK::UWidgetBlueprintLibrary::StaticClass();
        if (!widgetBPClass || !widgetBPClass->ClassDefaultObject) return;
        
        SDK::UFunction* func = widgetBPClass->GetFunction("WidgetBlueprintLibrary", "SetInputMode_GameOnly");
        if (!func) return;

        struct {
            SDK::APlayerController* PlayerController;
            bool bFlushInput;
        } params{
            pc,
            false
        };

        widgetBPClass->ClassDefaultObject->ProcessEvent(func, &params);
    }

    void ToggleMenuState(bool state) {
        g_Open = state;
        
        SDK::UWorld* world = SDK::UWorld::GetWorld();
        if (world && world->OwningGameInstance && world->OwningGameInstance->LocalPlayers.Num() > 0) {
            SDK::APlayerController* pc = world->OwningGameInstance->LocalPlayers[0]->PlayerController;
            if (pc) {
                pc->bShowMouseCursor = g_Open ? true : false;
                if (g_Open) {
                    SetInputMode_GameAndUIEx_Safe(pc);
                    pc->SetIgnoreMoveInput(true);
                    pc->SetIgnoreLookInput(true);
                } else {
                    pc->SetIgnoreMoveInput(false);
                    pc->SetIgnoreLookInput(false);
                    SetInputMode_GameOnly_Safe(pc);
                }
            }
        }
    }

    static void DrawFilledRect(SDK::UCanvas* canvas, float x, float y, float w, float h, SDK::FLinearColor color) {
        if (canvas && canvas->DefaultTexture) {
            canvas->K2_DrawTexture(canvas->DefaultTexture, SDK::FVector2D{x, y}, SDK::FVector2D{w, h}, 
                                   SDK::FVector2D{0, 0}, SDK::FVector2D{0, 0}, color, 
                                   SDK::EBlendMode::BLEND_Translucent, 0, SDK::FVector2D{0, 0});
        } else {
            canvas->K2_DrawPolygon(nullptr, SDK::FVector2D{x, y}, SDK::FVector2D{w, h}, 4, color);
        }
    }

    static void DrawOutlinedRect(SDK::UCanvas* canvas, float x, float y, float w, float h, float thickness, SDK::FLinearColor color) {
        canvas->K2_DrawLine(SDK::FVector2D{x, y}, SDK::FVector2D{x + w, y}, thickness, color);
        canvas->K2_DrawLine(SDK::FVector2D{x + w, y}, SDK::FVector2D{x + w, y + h}, thickness, color);
        canvas->K2_DrawLine(SDK::FVector2D{x + w, y + h}, SDK::FVector2D{x, y + h}, thickness, color);
        canvas->K2_DrawLine(SDK::FVector2D{x, y + h}, SDK::FVector2D{x, y}, thickness, color);
    }

    static void DrawFilledRoundedRect(SDK::UCanvas* canvas, float x, float y, float w, float h, float r, SDK::FLinearColor color) {
        if (r <= 0.0f) {
            DrawFilledRect(canvas, x, y, w, h, color);
            return;
        }
        if (r > w / 2.0f) r = w / 2.0f;
        if (r > h / 2.0f) r = h / 2.0f;

        DrawFilledRect(canvas, x, y + r, w, h - 2.0f * r, color);

        for (int i = 0; i < (int)r; ++i) {
            float dY = (float)i;
            float distY = r - dY;
            float distX = sqrtf(r * r - distY * distY);
            float drawX = x + r - distX;
            float drawW = w - 2.0f * r + 2.0f * distX;
            DrawFilledRect(canvas, drawX, y + dY, drawW, 1.f, color);
            DrawFilledRect(canvas, drawX, y + h - dY - 1.f, drawW, 1.f, color);
        }
    }

    static void DrawRoundedRectOutline(SDK::UCanvas* canvas, float x, float y, float w, float h, float r, float thickness, SDK::FLinearColor color) {
        if (r <= 0.0f) {
            DrawOutlinedRect(canvas, x, y, w, h, thickness, color);
            return;
        }
        if (r > w / 2.0f) r = w / 2.0f;
        if (r > h / 2.0f) r = h / 2.0f;

        canvas->K2_DrawLine(SDK::FVector2D{x + r, y}, SDK::FVector2D{x + w - r, y}, thickness, color);
        canvas->K2_DrawLine(SDK::FVector2D{x + r, y + h}, SDK::FVector2D{x + w - r, y + h}, thickness, color);
        canvas->K2_DrawLine(SDK::FVector2D{x, y + r}, SDK::FVector2D{x, y + h - r}, thickness, color);
        canvas->K2_DrawLine(SDK::FVector2D{x + w, y + r}, SDK::FVector2D{x + w, y + h - r}, thickness, color);

        auto drawCornerArc = [&](float cx, float cy, float startAngle, float endAngle) {
            float steps = 8.0f;
            float angleStep = (endAngle - startAngle) / steps;
            float prevX = cx + r * cosf(startAngle);
            float prevY = cy + r * sinf(startAngle);
            for (int i = 1; i <= steps; ++i) {
                float a = startAngle + i * angleStep;
                float currX = cx + r * cosf(a);
                float currY = cy + r * sinf(a);
                canvas->K2_DrawLine(SDK::FVector2D{prevX, prevY}, SDK::FVector2D{currX, currY}, thickness, color);
                prevX = currX;
                prevY = currY;
            }
        };

        const float PI = 3.14159265f;
        drawCornerArc(x + r, y + r, PI, 1.5f * PI);
        drawCornerArc(x + w - r, y + r, 1.5f * PI, 2.0f * PI);
        drawCornerArc(x + w - r, y + h - r, 0.0f, 0.5f * PI);
        drawCornerArc(x + r, y + h - r, 0.5f * PI, PI);
    }

    static bool MouseInRect(float x, float y, float w, float h) {
        return (Hooks::g_MousePos.x >= x && Hooks::g_MousePos.x <= x + w &&
                Hooks::g_MousePos.y >= y && Hooks::g_MousePos.y <= y + h);
    }

    static void DrawTextClean(SDK::UCanvas* canvas, const wchar_t* text, float x, float y, SDK::FLinearColor color, float fontScale = 1.0f) {
        InitializeFont();
        SDK::FLinearColor shadow{0.f, 0.f, 0.f, 0.9f};
        SDK::FVector2D scale{fontScale, fontScale};
        canvas->K2_DrawText(g_MenuFont, SDK::FString(text), SDK::FVector2D{x, y}, scale, color, 0.f, shadow, SDK::FVector2D{0.f, 0.f}, false, false, Config::g_Settings.bTextOutlines, shadow);
    }

    static void DrawFilledCircle(SDK::UCanvas* canvas, float cx, float cy, float r, SDK::FLinearColor color) {
        for (int y = -static_cast<int>(r); y <= static_cast<int>(r); y++) {
            float width = sqrtf(r * r - y * y);
            canvas->K2_DrawLine(SDK::FVector2D{cx - width, cy + y}, SDK::FVector2D{cx + width, cy + y}, 1.f, color);
        }
    }

    // Floating Popover picker states
    static float* m_ActiveColorTarget = nullptr;
    static float m_PickerX = 0.f;
    static float m_PickerY = 0.f;

    static void DrawColorCircle(SDK::UCanvas* canvas, float cx, float cy, float* colorArray) {
        float swW = 22.f;
        float swH = 14.f;
        float x = cx - swW / 2.f;
        float y = cy - swH / 2.f;

        SDK::FLinearColor col{ colorArray[0], colorArray[1], colorArray[2], 1.f };
        bool isActive = (m_ActiveColorTarget == colorArray);
        bool hovered = MouseInRect(x - 2.f, y - 2.f, swW + 4.f, swH + 4.f);

        SDK::FLinearColor borderCol = isActive ? SDK::FLinearColor{ 0.f, 0.85f, 1.f, 1.f } : (hovered ? SDK::FLinearColor{ 0.4f, 0.5f, 0.65f, 1.f } : SDK::FLinearColor{ 0.2f, 0.25f, 0.35f, 0.8f });

        // Sleek rounded swatch card
        DrawFilledRoundedRect(canvas, x, y, swW, swH, 4.f, col);
        DrawRoundedRectOutline(canvas, x, y, swW, swH, 4.f, 1.2f, borderCol);

        if (Hooks::g_KeyPendingDown[VK_LBUTTON] && hovered) {
            Hooks::g_KeyPendingDown[VK_LBUTTON] = false;
            m_ActiveColorTarget = colorArray;
            m_PickerX = cx;
            m_PickerY = cy;
        }
    }

    static void DrawPickerSlider(SDK::UCanvas* canvas, float x, float y, float w, float& val) {
        float sliderH = 4.f;
        float handleR = 6.f;
        SDK::FLinearColor sliderBG{0.12f, 0.15f, 0.2f, 1.f};
        SDK::FLinearColor handleCol{0.95f, 0.95f, 0.95f, 1.f};
        SDK::FLinearColor activeCol{0.f, 0.85f, 1.f, 1.f};

        canvas->K2_DrawLine(SDK::FVector2D{x, y}, SDK::FVector2D{x + w, y}, 2.f, sliderBG);

        float percent = val;
        if (percent < 0.f) percent = 0.f;
        if (percent > 1.f) percent = 1.f;

        canvas->K2_DrawLine(SDK::FVector2D{x, y}, SDK::FVector2D{x + w * percent, y}, 2.f, activeCol);

        float handleX = x + (w * percent);
        float handleY = y;

        if (Hooks::g_KeyState[VK_LBUTTON] && MouseInRect(x - 5.f, y - 8.f, w + 10.f, 20.f)) {
            float mouseRelX = static_cast<float>(Hooks::g_MousePos.x) - x;
            float newPct = mouseRelX / w;
            if (newPct < 0.f) newPct = 0.f;
            if (newPct > 1.f) newPct = 1.f;
            val = newPct;
        }

        DrawFilledCircle(canvas, handleX, handleY, handleR, handleCol);
    }

    static void RenderColorPickerPopover(SDK::UCanvas* canvas) {
        if (!m_ActiveColorTarget) return;

        float popW = 260.f;
        float popH = 160.f;
        float x = m_PickerX + 15.f;
        float y = m_PickerY - popH / 2.f;

        if (x + popW > m_X + m_Width) {
            x = m_PickerX - popW - 15.f;
        }
        if (y + popH > m_Y + m_Height) {
            y = m_Y + m_Height - popH - 10.f;
        }
        if (y < m_Y) {
            y = m_Y + 10.f;
        }

        SDK::FLinearColor bgCol{0.06f, 0.08f, 0.12f, 0.98f};
        SDK::FLinearColor border{0.18f, 0.24f, 0.32f, 1.f};
        SDK::FLinearColor white{0.95f, 0.95f, 0.95f, 1.f};
        SDK::FLinearColor graySub{0.55f, 0.6f, 0.68f, 1.f};
        
        DrawFilledRoundedRect(canvas, x, y, popW, popH, 8.f, bgCol);
        DrawRoundedRectOutline(canvas, x, y, popW, popH, 8.f, 1.2f, border);

        SDK::FLinearColor curCol{ m_ActiveColorTarget[0], m_ActiveColorTarget[1], m_ActiveColorTarget[2], 1.f };
        DrawFilledRoundedRect(canvas, x + 15.f, y + 15.f, 32.f, 32.f, 4.f, curCol);
        DrawRoundedRectOutline(canvas, x + 15.f, y + 15.f, 32.f, 32.f, 4.f, 1.f, border);

        wchar_t valText[128];
        swprintf_s(valText, 128, L"R: %d  G: %d  B: %d", 
            static_cast<int>(curCol.R * 255.f), 
            static_cast<int>(curCol.G * 255.f), 
            static_cast<int>(curCol.B * 255.f));
        DrawTextClean(canvas, valText, x + 60.f, y + 22.f, white, 0.72f);

        float startX = x + 15.f;
        float sliderW = 230.f;

        DrawTextClean(canvas, L"R", startX, y + 62.f, graySub, 0.65f);
        DrawPickerSlider(canvas, startX + 20.f, y + 68.f, sliderW - 20.f, m_ActiveColorTarget[0]);

        DrawTextClean(canvas, L"G", startX, y + 90.f, graySub, 0.65f);
        DrawPickerSlider(canvas, startX + 20.f, y + 96.f, sliderW - 20.f, m_ActiveColorTarget[1]);

        DrawTextClean(canvas, L"B", startX, y + 118.f, graySub, 0.65f);
        DrawPickerSlider(canvas, startX + 20.f, y + 124.f, sliderW - 20.f, m_ActiveColorTarget[2]);

        float btnX = x + 205.f;
        float btnY = y + 15.f;
        float btnW = 40.f;
        float btnH = 22.f;
        DrawFilledRoundedRect(canvas, btnX, btnY, btnW, btnH, 4.f, SDK::FLinearColor{0.12f, 0.16f, 0.22f, 1.f});
        DrawRoundedRectOutline(canvas, btnX, btnY, btnW, btnH, 4.f, 1.f, border);
        DrawTextClean(canvas, L"OK", btnX + 11.f, btnY + 3.f, white, 0.7f);

        if (Hooks::g_KeyPendingDown[VK_LBUTTON] && MouseInRect(btnX, btnY, btnW, btnH)) {
            Hooks::g_KeyPendingDown[VK_LBUTTON] = false;
            m_ActiveColorTarget = nullptr;
            return;
        }

        if (Hooks::g_KeyPendingDown[VK_LBUTTON] && !MouseInRect(x, y, popW, popH)) {
            m_ActiveColorTarget = nullptr;
        }
    }

    // Vector Icon Drawing Routines
    static void DrawAimbotIcon(SDK::UCanvas* canvas, float x, float y, SDK::FLinearColor color) {
        float r = 5.f;
        canvas->K2_DrawLine(SDK::FVector2D{x - 8.f, y}, SDK::FVector2D{x + 8.f, y}, 1.2f, color);
        canvas->K2_DrawLine(SDK::FVector2D{x, y - 8.f}, SDK::FVector2D{x, y + 8.f}, 1.2f, color);
        DrawFilledCircle(canvas, x, y, 2.f, color);
    }

    static void DrawVisualsIcon(SDK::UCanvas* canvas, float x, float y, SDK::FLinearColor color) {
        DrawOutlinedRect(canvas, x - 6.f, y - 6.f, 12.f, 12.f, 1.2f, color);
        DrawFilledCircle(canvas, x, y, 2.f, color);
    }

    static void DrawAutomationIcon(SDK::UCanvas* canvas, float x, float y, SDK::FLinearColor color) {
        float r = 5.f;
        for (int i = 0; i < 4; i++) {
            float a = i * 1.57079f;
            canvas->K2_DrawLine(SDK::FVector2D{x, y}, SDK::FVector2D{x + r * cosf(a), y + r * sinf(a)}, 1.2f, color);
        }
    }

    static void DrawExploitsIcon(SDK::UCanvas* canvas, float x, float y, SDK::FLinearColor color) {
        canvas->K2_DrawLine(SDK::FVector2D{x - 6.f, y + 5.f}, SDK::FVector2D{x, y - 6.f}, 1.5f, color);
        canvas->K2_DrawLine(SDK::FVector2D{x, y - 6.f}, SDK::FVector2D{x + 6.f, y + 5.f}, 1.5f, color);
        canvas->K2_DrawLine(SDK::FVector2D{x - 4.f, y + 1.f}, SDK::FVector2D{x + 4.f, y + 1.f}, 1.2f, color);
    }

    static void DrawMiscIcon(SDK::UCanvas* canvas, float x, float y, SDK::FLinearColor color) {
        DrawFilledRect(canvas, x - 5.f, y - 5.f, 4.f, 4.f, color);
        DrawFilledRect(canvas, x + 1.f, y - 5.f, 4.f, 4.f, color);
        DrawFilledRect(canvas, x - 5.f, y + 1.f, 4.f, 4.f, color);
        DrawFilledRect(canvas, x + 1.f, y + 1.f, 4.f, 4.f, color);
    }

    static void DrawSaveIcon(SDK::UCanvas* canvas, float x, float y, SDK::FLinearColor color) {
        DrawOutlinedRect(canvas, x - 5.f, y - 5.f, 10.f, 10.f, 1.2f, color);
        canvas->K2_DrawLine(SDK::FVector2D{x - 2.f, y + 1.f}, SDK::FVector2D{x + 2.f, y + 1.f}, 1.2f, color);
    }

    static bool DrawSidebarButton(SDK::UCanvas* canvas, const wchar_t* label, float x, float y, float w, float h, bool active, int iconType) {
        SDK::FLinearColor textCol = active ? SDK::FLinearColor{1.f, 1.f, 1.f, 1.f} : SDK::FLinearColor{0.55f, 0.6f, 0.68f, 1.f};
        SDK::FLinearColor iconCol = active ? SDK::FLinearColor{0.f, 0.85f, 1.f, 1.f} : SDK::FLinearColor{0.45f, 0.5f, 0.58f, 1.f};

        if (active) {
            DrawFilledRoundedRect(canvas, x + 14.f, y, w - 28.f, h, 6.f, SDK::FLinearColor{0.08f, 0.14f, 0.22f, 0.9f});
            DrawRoundedRectOutline(canvas, x + 14.f, y, w - 28.f, h, 6.f, 1.f, SDK::FLinearColor{0.14f, 0.28f, 0.42f, 0.8f});
            DrawFilledRoundedRect(canvas, x + 14.f, y + 6.f, 3.f, h - 12.f, 1.5f, SDK::FLinearColor{0.f, 0.85f, 1.f, 1.f});
        } else {
            if (MouseInRect(x + 14.f, y, w - 28.f, h)) {
                DrawFilledRoundedRect(canvas, x + 14.f, y, w - 28.f, h, 6.f, SDK::FLinearColor{0.07f, 0.1f, 0.15f, 0.5f});
                textCol = SDK::FLinearColor{0.85f, 0.9f, 0.95f, 1.f};
                iconCol = SDK::FLinearColor{0.7f, 0.75f, 0.85f, 1.f};
            }
        }

        float iconX = x + 34.f;
        float iconY = y + (h / 2.f);

        if (iconType == 0) DrawAimbotIcon(canvas, iconX, iconY, iconCol);
        else if (iconType == 1) DrawVisualsIcon(canvas, iconX, iconY, iconCol);
        else if (iconType == 2) DrawAutomationIcon(canvas, iconX, iconY, iconCol);
        else if (iconType == 3) DrawExploitsIcon(canvas, iconX, iconY, iconCol);
        else if (iconType == 4) DrawMiscIcon(canvas, iconX, iconY, iconCol);
        else if (iconType == 6) DrawSaveIcon(canvas, iconX, iconY, iconCol);

        DrawTextClean(canvas, label, x + 52.f, y + (h / 2.f) - 9.f, textCol, 0.88f);

        if (Hooks::g_KeyPendingDown[VK_LBUTTON] && MouseInRect(x + 14.f, y, w - 28.f, h)) {
            return true;
        }
        return false;
    }

    static bool DrawTopTabButton(SDK::UCanvas* canvas, const wchar_t* label, float x, float y, float w, float h, bool active) {
        SDK::FLinearColor textCol = active ? SDK::FLinearColor{1.f, 1.f, 1.f, 1.f} : SDK::FLinearColor{0.5f, 0.55f, 0.65f, 1.f};
        SDK::FLinearColor bgCol = active ? SDK::FLinearColor{0.08f, 0.16f, 0.26f, 0.8f} : SDK::FLinearColor{0.04f, 0.06f, 0.09f, 0.6f};
        SDK::FLinearColor borderCol = active ? SDK::FLinearColor{0.14f, 0.3f, 0.45f, 0.9f} : SDK::FLinearColor{0.1f, 0.12f, 0.18f, 0.6f};

        DrawFilledRoundedRect(canvas, x, y, w, h, 6.f, bgCol);
        DrawRoundedRectOutline(canvas, x, y, w, h, 6.f, 1.f, borderCol);

        if (active) {
            DrawFilledRoundedRect(canvas, x + 10.f, y + h - 2.f, w - 20.f, 2.f, 1.f, SDK::FLinearColor{0.f, 0.85f, 1.f, 1.f});
        } else if (MouseInRect(x, y, w, h)) {
            textCol = SDK::FLinearColor{0.85f, 0.9f, 0.95f, 1.f};
        }

        InitializeFont();
        SDK::FLinearColor shadow{0.f, 0.f, 0.f, 0.9f};
        SDK::FVector2D scale{0.8f, 0.8f};
        canvas->K2_DrawText(g_MenuFont, SDK::FString(label), SDK::FVector2D{x + w / 2.f, y + (h / 2.f) - 8.f}, scale, textCol, 0.f, shadow, SDK::FVector2D{0.f, 0.f}, true, false, false, shadow);

        if (Hooks::g_KeyPendingDown[VK_LBUTTON] && MouseInRect(x, y, w, h)) {
            return true;
        }
        return false;
    }

    static void DrawKeybind(SDK::UCanvas* canvas, float x, float y, float w, float h, int& keyVal) {
        float btnW = 85.f;
        float btnH = 20.f;
        float btnX = x + w - btnW;
        float btnY = y + (h - btnH) / 2.f;

        bool hovered = MouseInRect(btnX, btnY, btnW, btnH);
        bool clicked = Hooks::g_KeyPendingDown[VK_LBUTTON] && hovered;

        if (clicked && !m_BindingKey) {
            Hooks::g_KeyPendingDown[VK_LBUTTON] = false;
            for (int k = 0; k < 256; ++k) {
                Hooks::g_KeyPendingDown[k] = false;
            }
            m_BindingKey = &keyVal;
            g_BindingStartTime = GetTickCount64();
        }

        bool bindingThis = (m_BindingKey == &keyVal);

        SDK::FLinearColor bgCol = bindingThis ? SDK::FLinearColor{0.14f, 0.22f, 0.32f, 1.f} : (hovered ? SDK::FLinearColor{0.1f, 0.14f, 0.2f, 1.f} : SDK::FLinearColor{0.06f, 0.08f, 0.12f, 1.f});
        SDK::FLinearColor border = bindingThis ? SDK::FLinearColor{0.f, 0.85f, 1.f, 1.f} : SDK::FLinearColor{0.16f, 0.22f, 0.3f, 1.f};
        SDK::FLinearColor textCol = bindingThis ? SDK::FLinearColor{0.f, 0.85f, 1.f, 1.f} : SDK::FLinearColor{0.7f, 0.75f, 0.82f, 1.f};

        DrawFilledRoundedRect(canvas, btnX, btnY, btnW, btnH, 4.f, bgCol);
        DrawRoundedRectOutline(canvas, btnX, btnY, btnW, btnH, 4.f, 1.f, border);

        std::wstring keyName = L"NONE";
        if (bindingThis) {
            keyName = L"PRESS KEY";
        } else if (keyVal > 0) {
            wchar_t nameBuf[32];
            if (keyVal == VK_LBUTTON) wcscpy_s(nameBuf, 32, L"LCLICK");
            else if (keyVal == VK_RBUTTON) wcscpy_s(nameBuf, 32, L"RCLICK");
            else if (keyVal == VK_MBUTTON) wcscpy_s(nameBuf, 32, L"M3");
            else if (keyVal == VK_XBUTTON1) wcscpy_s(nameBuf, 32, L"M4");
            else if (keyVal == VK_XBUTTON2) wcscpy_s(nameBuf, 32, L"M5");
            else if (keyVal == VK_SHIFT) wcscpy_s(nameBuf, 32, L"SHIFT");
            else if (keyVal == VK_CONTROL) wcscpy_s(nameBuf, 32, L"CTRL");
            else if (keyVal == VK_MENU) wcscpy_s(nameBuf, 32, L"ALT");
            else if (keyVal == VK_CAPITAL) wcscpy_s(nameBuf, 32, L"CAPS");
            else if (keyVal == VK_ESCAPE) wcscpy_s(nameBuf, 32, L"ESC");
            else if (keyVal == VK_SPACE) wcscpy_s(nameBuf, 32, L"SPACE");
            else if (keyVal == VK_TAB) wcscpy_s(nameBuf, 32, L"TAB");
            else if (keyVal >= 'A' && keyVal <= 'Z') swprintf_s(nameBuf, 32, L"%c", (char)keyVal);
            else if (keyVal >= '0' && keyVal <= '9') swprintf_s(nameBuf, 32, L"%c", (char)keyVal);
            else if (keyVal >= VK_F1 && keyVal <= VK_F12) swprintf_s(nameBuf, 32, L"F%d", keyVal - VK_F1 + 1);
            else swprintf_s(nameBuf, 32, L"KEY %d", keyVal);
            keyName = nameBuf;
            textCol = SDK::FLinearColor{1.f, 1.f, 1.f, 1.f};
        }

        SDK::FLinearColor shadow{0.f, 0.f, 0.f, 0.8f};
        SDK::FVector2D scale{0.7f, 0.7f};
        canvas->K2_DrawText(g_MenuFont, SDK::FString(keyName.c_str()), SDK::FVector2D{btnX + btnW / 2.f, btnY + btnH / 2.f}, scale, textCol, 0.f, shadow, SDK::FVector2D{0.f, 0.f}, true, true, false, shadow);
    }

    static bool DrawButton(SDK::UCanvas* canvas, const wchar_t* label, float x, float y, float w, float h) {
        bool hovered = MouseInRect(x, y, w, h);
        bool clicked = Hooks::g_KeyPendingDown[VK_LBUTTON] && hovered;
        
        SDK::FLinearColor bgCol = clicked ? SDK::FLinearColor{0.12f, 0.22f, 0.32f, 1.f} : (hovered ? SDK::FLinearColor{0.08f, 0.16f, 0.24f, 1.f} : SDK::FLinearColor{0.06f, 0.1f, 0.15f, 1.f});
        SDK::FLinearColor border = clicked ? SDK::FLinearColor{0.f, 0.85f, 1.f, 1.f} : SDK::FLinearColor{0.16f, 0.24f, 0.34f, 1.f};
        SDK::FLinearColor white{0.95f, 0.95f, 0.95f, 1.f};
        
        DrawFilledRoundedRect(canvas, x, y, w, h, 5.f, bgCol);
        DrawRoundedRectOutline(canvas, x, y, w, h, 5.f, 1.f, border);
        
        InitializeFont();
        SDK::FLinearColor shadow{0.f, 0.f, 0.f, 0.8f};
        SDK::FVector2D scale{0.8f, 0.8f};
        canvas->K2_DrawText(g_MenuFont, SDK::FString(label), SDK::FVector2D{x + w / 2.f, y + h / 2.f}, scale, white, 0.f, shadow, SDK::FVector2D{0.f, 0.f}, true, true, false, shadow);

        if (clicked) {
            Hooks::g_KeyPendingDown[VK_LBUTTON] = false;
            return true;
        }
        return false;
    }

    static void DrawCheckbox(SDK::UCanvas* canvas, const wchar_t* label, float x, float y, bool& state, float* colorArray = nullptr) {
        float switchW = 32.f;
        float switchH = 16.f;
        float knobR = 6.f;

        bool hovered = MouseInRect(x, y - 2.f, switchW + 180.f, switchH + 4.f);

        if (Hooks::g_KeyPendingDown[VK_LBUTTON] && hovered) {
            Hooks::g_KeyPendingDown[VK_LBUTTON] = false;
            state = !state;
        }

        // Modern Pill Switch Track
        SDK::FLinearColor trackBG = state ? SDK::FLinearColor{0.f, 0.65f, 0.95f, 0.9f} : (hovered ? SDK::FLinearColor{0.12f, 0.16f, 0.24f, 1.f} : SDK::FLinearColor{0.07f, 0.09f, 0.14f, 1.f});
        SDK::FLinearColor trackBorder = state ? SDK::FLinearColor{0.3f, 0.85f, 1.f, 1.f} : (hovered ? SDK::FLinearColor{0.25f, 0.32f, 0.42f, 1.f} : SDK::FLinearColor{0.15f, 0.2f, 0.28f, 1.f});
        
        DrawFilledRoundedRect(canvas, x, y, switchW, switchH, 8.f, trackBG);
        DrawRoundedRectOutline(canvas, x, y, switchW, switchH, 8.f, 1.f, trackBorder);

        // Smooth Knob handle positioning
        float knobX = state ? (x + switchW - knobR - 3.f) : (x + knobR + 3.f);
        float knobY = y + (switchH / 2.f);
        SDK::FLinearColor knobCol = state ? SDK::FLinearColor{1.f, 1.f, 1.f, 1.f} : SDK::FLinearColor{0.55f, 0.62f, 0.72f, 1.f};

        DrawFilledCircle(canvas, knobX, knobY, knobR, knobCol);

        // Text Label
        SDK::FLinearColor textCol = state ? SDK::FLinearColor{0.98f, 0.98f, 0.98f, 1.f} : (hovered ? SDK::FLinearColor{0.8f, 0.85f, 0.92f, 1.f} : SDK::FLinearColor{0.62f, 0.68f, 0.76f, 1.f});
        DrawTextClean(canvas, label, x + switchW + 12.f, y - 2.f, textCol, 0.82f);

        // Color Swatch Badge
        if (colorArray) {
            DrawColorCircle(canvas, x + 245.f, y + 8.f, colorArray);
        }
    }

    static void DrawSlider(SDK::UCanvas* canvas, const wchar_t* label, float x, float y, float w, float minVal, float maxVal, float& val, const wchar_t* suffix = L"") {
        float sliderH = 4.f;
        float handleR = 6.f;
        
        DrawFilledRoundedRect(canvas, x, y + 16.f, w, sliderH, 2.f, SDK::FLinearColor{0.06f, 0.08f, 0.12f, 1.f});
        DrawRoundedRectOutline(canvas, x, y + 16.f, w, sliderH, 2.f, 1.f, SDK::FLinearColor{0.16f, 0.22f, 0.3f, 1.f});

        float percent = (val - minVal) / (maxVal - minVal);
        if (percent < 0.f) percent = 0.f;
        if (percent > 1.f) percent = 1.f;
        DrawFilledRoundedRect(canvas, x, y + 16.f, w * percent, sliderH, 2.f, SDK::FLinearColor{0.f, 0.85f, 1.f, 1.f});

        float handleX = x + (w * percent);
        float handleY = y + 16.f + (sliderH / 2.f);
        
        if (Hooks::g_KeyState[VK_LBUTTON] && MouseInRect(x - 5.f, y + 8.f, w + 10.f, 20.f)) {
            float mouseRelX = static_cast<float>(Hooks::g_MousePos.x) - x;
            float newPct = mouseRelX / w;
            if (newPct < 0.f) newPct = 0.f;
            if (newPct > 1.f) newPct = 1.f;
            val = minVal + newPct * (maxVal - minVal);
        }

        DrawFilledCircle(canvas, handleX, handleY, handleR, SDK::FLinearColor{0.95f, 0.95f, 0.95f, 1.f});
        
        DrawTextClean(canvas, label, x, y - 4.f, SDK::FLinearColor{0.65f, 0.7f, 0.78f, 1.f}, 0.8f);
        
        wchar_t valBuf[32];
        if (maxVal <= 10.f) {
            swprintf_s(valBuf, 32, L"%.2f%s", val, suffix);
        } else {
            swprintf_s(valBuf, 32, L"%.0f%s", val, suffix);
        }
        DrawTextClean(canvas, valBuf, x + w - 45.f, y - 4.f, SDK::FLinearColor{0.f, 0.85f, 1.f, 1.f}, 0.8f);
    }

    static void DrawRelationTabs2(SDK::UCanvas* canvas, float x, float y, float w, float h, const wchar_t* opt1, const wchar_t* opt2, int& activeIdx) {
        float btnW = w / 2.f;
        DrawFilledRoundedRect(canvas, x, y, w, h, 4.f, SDK::FLinearColor{ 0.04f, 0.06f, 0.09f, 1.0f });
        DrawRoundedRectOutline(canvas, x, y, w, h, 4.f, 1.f, SDK::FLinearColor{ 0.14f, 0.2f, 0.28f, 1.0f });
        
        float hx = (activeIdx == 0) ? x : x + btnW;
        DrawFilledRoundedRect(canvas, hx, y, btnW, h, 4.f, SDK::FLinearColor{ 0.08f, 0.18f, 0.28f, 1.0f });
        DrawRoundedRectOutline(canvas, hx, y, btnW, h, 4.f, 1.f, SDK::FLinearColor{ 0.f, 0.85f, 1.0f, 0.9f });
        
        SDK::FLinearColor white{0.95f, 0.95f, 0.95f, 1.f};
        SDK::FLinearColor graySub{0.48f, 0.52f, 0.6f, 1.f};
        
        DrawTextClean(canvas, opt1, x + (btnW / 2.f) - 20.f, y + (h / 2.f) - 8.f, (activeIdx == 0) ? white : graySub, 0.75f);
        DrawTextClean(canvas, opt2, x + btnW + (btnW / 2.f) - 20.f, y + (h / 2.f) - 8.f, (activeIdx == 1) ? white : graySub, 0.75f);
        
        if (Hooks::g_KeyPendingDown[VK_LBUTTON] && MouseInRect(x, y, w, h)) {
            Hooks::g_KeyPendingDown[VK_LBUTTON] = false;
            activeIdx = (Hooks::g_MousePos.x < x + btnW) ? 0 : 1;
        }
    }

    static void DrawRelationTabs3(SDK::UCanvas* canvas, float x, float y, float w, float h, const wchar_t* opt1, const wchar_t* opt2, const wchar_t* opt3, int& activeIdx) {
        float btnW = w / 3.f;
        DrawFilledRoundedRect(canvas, x, y, w, h, 4.f, SDK::FLinearColor{ 0.04f, 0.06f, 0.09f, 1.0f });
        DrawRoundedRectOutline(canvas, x, y, w, h, 4.f, 1.f, SDK::FLinearColor{ 0.14f, 0.2f, 0.28f, 1.0f });
        
        float hx = x + activeIdx * btnW;
        DrawFilledRoundedRect(canvas, hx, y, btnW, h, 4.f, SDK::FLinearColor{ 0.08f, 0.18f, 0.28f, 1.0f });
        DrawRoundedRectOutline(canvas, hx, y, btnW, h, 4.f, 1.f, SDK::FLinearColor{ 0.f, 0.85f, 1.0f, 0.9f });
        
        SDK::FLinearColor white{0.95f, 0.95f, 0.95f, 1.f};
        SDK::FLinearColor graySub{0.48f, 0.52f, 0.6f, 1.f};
        
        DrawTextClean(canvas, opt1, x + (btnW / 2.f) - 15.f, y + (h / 2.f) - 8.f, (activeIdx == 0) ? white : graySub, 0.75f);
        DrawTextClean(canvas, opt2, x + btnW + (btnW / 2.f) - 25.f, y + (h / 2.f) - 8.f, (activeIdx == 1) ? white : graySub, 0.75f);
        DrawTextClean(canvas, opt3, x + btnW * 2.f + (btnW / 2.f) - 15.f, y + (h / 2.f) - 8.f, (activeIdx == 2) ? white : graySub, 0.75f);
        
        if (Hooks::g_KeyPendingDown[VK_LBUTTON] && MouseInRect(x, y, w, h)) {
            Hooks::g_KeyPendingDown[VK_LBUTTON] = false;
            float mx = static_cast<float>(Hooks::g_MousePos.x);
            if (mx < x + btnW) activeIdx = 0;
            else if (mx < x + btnW * 2.f) activeIdx = 1;
            else activeIdx = 2;
        }
    }

    static void DrawInactiveModuleCard(SDK::UCanvas* canvas, float x, float y, float w, float h, const wchar_t* title) {
        SDK::FLinearColor panelBG{0.04f, 0.06f, 0.1f, 0.6f};
        SDK::FLinearColor borderCard{0.12f, 0.18f, 0.26f, 0.8f};
        SDK::FLinearColor white{0.95f, 0.95f, 0.95f, 1.f};
        SDK::FLinearColor cyan{0.f, 0.85f, 1.f, 1.f};

        DrawFilledRoundedRect(canvas, x, y, w, h, 8.f, panelBG);
        DrawRoundedRectOutline(canvas, x, y, w, h, 8.f, 1.f, borderCard);

        float itemX = x + 25.f;
        float itemY = y + 25.f;

        DrawFilledRoundedRect(canvas, itemX, itemY + 2.f, 3.f, 16.f, 1.5f, cyan);
        DrawTextClean(canvas, title, itemX + 12.f, itemY, white, 0.88f);
        itemY += 35.f;

        DrawFilledRect(canvas, x + 15.f, itemY, w - 30.f, 1.f, borderCard);
    }

    void RenderMenu(SDK::UCanvas* canvas) {
        if (!g_Open) return;

        m_Width = 1100.f;
        m_Height = 640.f;
        float sidebarW = 220.f;
        float dragH = 45.f;

        // 1. Draggable panel logic
        if (Hooks::g_KeyState[VK_LBUTTON]) {
            if (!m_Dragging && MouseInRect(m_X, m_Y, m_Width, dragH)) {
                m_Dragging = true;
                m_DragOffsetX = static_cast<float>(Hooks::g_MousePos.x) - m_X;
                m_DragOffsetY = static_cast<float>(Hooks::g_MousePos.y) - m_Y;
            }
            if (m_Dragging) {
                m_X = static_cast<float>(Hooks::g_MousePos.x) - m_DragOffsetX;
                m_Y = static_cast<float>(Hooks::g_MousePos.y) - m_DragOffsetY;
            }
        } else {
            m_Dragging = false;
        }

        // Modern Cyber Dark Palette
        SDK::FLinearColor mainBG{0.03f, 0.04f, 0.06f, 0.92f};      
        SDK::FLinearColor borderMain{0.12f, 0.18f, 0.26f, 0.9f};    
        SDK::FLinearColor panelBG{0.05f, 0.07f, 0.11f, 0.6f};       
        SDK::FLinearColor borderCard{0.12f, 0.16f, 0.24f, 0.9f};     
        SDK::FLinearColor white{0.95f, 0.95f, 0.95f, 1.f};
        SDK::FLinearColor graySub{0.48f, 0.52f, 0.6f, 1.f};
        SDK::FLinearColor cyan{0.f, 0.85f, 1.f, 1.f};

        // Render main container backdrop & frame
        DrawFilledRoundedRect(canvas, m_X, m_Y, m_Width, m_Height, 10.f, mainBG);
        DrawRoundedRectOutline(canvas, m_X, m_Y, m_Width, m_Height, 10.f, 1.2f, borderMain);
        
        // Header Branding Bar
        DrawFilledRoundedRect(canvas, m_X, m_Y, m_Width, dragH, 10.f, SDK::FLinearColor{0.05f, 0.07f, 0.11f, 0.9f});
        DrawFilledRect(canvas, m_X, m_Y + dragH - 1.f, m_Width, 1.f, borderCard);
        
        DrawFilledCircle(canvas, m_X + 25.f, m_Y + (dragH / 2.f), 4.f, cyan);
        DrawTextClean(canvas, L"Nando", m_X + 38.f, m_Y + 12.f, white, 0.92f);

        // 2. Render Sidebar Navigation
        float sidebarY = m_Y + dragH + 15.f;
        float buttonH = 36.f;

        DrawTextClean(canvas, L"MAIN", m_X + 22.f, sidebarY, graySub, 0.68f);
        sidebarY += 18.f;
        if (DrawSidebarButton(canvas, L"Aimbot", m_X, sidebarY, sidebarW, buttonH, m_ActiveSidebar == 0, 0)) {
            Hooks::g_KeyPendingDown[VK_LBUTTON] = false;
            m_ActiveSidebar = 0;
        }
        sidebarY += buttonH + 4.f;
        if (DrawSidebarButton(canvas, L"Visuals", m_X, sidebarY, sidebarW, buttonH, m_ActiveSidebar == 1, 1)) {
            Hooks::g_KeyPendingDown[VK_LBUTTON] = false;
            m_ActiveSidebar = 1;
        }

        sidebarY += buttonH + 20.f;
        DrawTextClean(canvas, L"MODULES", m_X + 22.f, sidebarY, graySub, 0.68f);
        sidebarY += 18.f;
        if (DrawSidebarButton(canvas, L"Automation", m_X, sidebarY, sidebarW, buttonH, m_ActiveSidebar == 2, 2)) {
            Hooks::g_KeyPendingDown[VK_LBUTTON] = false;
            m_ActiveSidebar = 2;
        }
        sidebarY += buttonH + 4.f;
        if (DrawSidebarButton(canvas, L"Exploits", m_X, sidebarY, sidebarW, buttonH, m_ActiveSidebar == 3, 3)) {
            Hooks::g_KeyPendingDown[VK_LBUTTON] = false;
            m_ActiveSidebar = 3;
        }
        sidebarY += buttonH + 4.f;
        if (DrawSidebarButton(canvas, L"Miscellaneous", m_X, sidebarY, sidebarW, buttonH, m_ActiveSidebar == 4, 4)) {
            Hooks::g_KeyPendingDown[VK_LBUTTON] = false;
            m_ActiveSidebar = 4;
        }

        sidebarY += buttonH + 20.f;
        DrawTextClean(canvas, L"SYSTEM", m_X + 22.f, sidebarY, graySub, 0.68f);
        sidebarY += 18.f;
        if (DrawSidebarButton(canvas, L"Settings", m_X, sidebarY, sidebarW, buttonH, m_ActiveSidebar == 5, 6)) {
            Hooks::g_KeyPendingDown[VK_LBUTTON] = false;
            m_ActiveSidebar = 5;
        }

        // Sidebar Divider Line
        DrawFilledRect(canvas, m_X + sidebarW, m_Y + dragH, 1.f, m_Height - dragH, borderCard);

        // 3. Render Right Content Panel
        float mainPanelX = m_X + sidebarW + 1.f;
        float maxW = m_Width - sidebarW - 1.f;
        float activeY = (m_ActiveSidebar == 1) ? m_Y + dragH + 55.f : m_Y + dragH + 15.f;

        // Subtabs for Visuals
        if (m_ActiveSidebar == 1) {
            float tabW = 115.f;
            float tabH = 34.f;
            float topTabX = mainPanelX + 20.f;
            float topTabY = m_Y + dragH + 10.f;

            if (DrawTopTabButton(canvas, L"Players", topTabX, topTabY, tabW, tabH, m_VisualsTab == 0)) {
                Hooks::g_KeyPendingDown[VK_LBUTTON] = false;
                m_VisualsTab = 0;
            }
            if (DrawTopTabButton(canvas, L"Dinos", topTabX + tabW + 8.f, topTabY, tabW, tabH, m_VisualsTab == 1)) {
                Hooks::g_KeyPendingDown[VK_LBUTTON] = false;
                m_VisualsTab = 1;
            }
            if (DrawTopTabButton(canvas, L"Containers", topTabX + (tabW + 8.f) * 2, topTabY, tabW, tabH, m_VisualsTab == 2)) {
                Hooks::g_KeyPendingDown[VK_LBUTTON] = false;
                m_VisualsTab = 2;
            }
            if (DrawTopTabButton(canvas, L"World", topTabX + (tabW + 8.f) * 3, topTabY, tabW, tabH, m_VisualsTab == 3)) {
                Hooks::g_KeyPendingDown[VK_LBUTTON] = false;
                m_VisualsTab = 3;
            }

            DrawFilledRect(canvas, mainPanelX + 15.f, m_Y + dragH + 50.f, maxW - 30.f, 1.f, borderCard);
        }

        if (m_ActiveSidebar == 0) { // Aimbot Tab
            DrawInactiveModuleCard(canvas, mainPanelX + 20.f, activeY, maxW - 40.f, m_Height - dragH - 30.f, L"Aimbot System");
        }
        else if (m_ActiveSidebar == 1) { // Visuals / ESP Config Panel
            float colW = (maxW - 50.f) / 2.f;
            float colH = m_Height - dragH - 75.f;

            if (m_VisualsTab == 0) { // Players Tab
                // Left Panel: Master Control & Target Filters
                float leftCardX = mainPanelX + 20.f;
                DrawFilledRoundedRect(canvas, leftCardX, activeY, colW, colH, 8.f, panelBG);
                DrawRoundedRectOutline(canvas, leftCardX, activeY, colW, colH, 8.f, 1.f, borderCard);

                float leftX = leftCardX + 20.f;
                float leftY = activeY + 18.f;

                DrawFilledRoundedRect(canvas, leftX, leftY + 2.f, 3.f, 16.f, 1.5f, cyan);
                DrawTextClean(canvas, L"Player Master & Targets", leftX + 12.f, leftY, white, 0.88f);
                leftY += 32.f;

                DrawFilledRect(canvas, leftCardX + 10.f, leftY, colW - 20.f, 1.f, borderCard);
                leftY += 15.f;

                // Master Toggle & Keybind
                DrawCheckbox(canvas, L"Enable Player ESP", leftX, leftY, Config::g_Settings.Player.bEnabled);
                DrawKeybind(canvas, leftX, leftY, colW - 40.f, 14.f, Config::g_Settings.Keybind_PlayerESP);
                leftY += 35.f;

                DrawTextClean(canvas, L"TARGET CATEGORIES & COLORS", leftX, leftY, graySub, 0.65f);
                leftY += 22.f;

                DrawCheckbox(canvas, L"Enemy Players", leftX, leftY, Config::g_Settings.Player.bShowEnemy, Config::g_Settings.Colors.Enemy);
                leftY += 28.f;
                DrawCheckbox(canvas, L"Team / Friendly", leftX, leftY, Config::g_Settings.Player.bShowFriendly, Config::g_Settings.Colors.Friendly);
                leftY += 28.f;
                DrawCheckbox(canvas, L"Sleeping Players", leftX, leftY, Config::g_Settings.Player.bShowSleeping, Config::g_Settings.Colors.Sleeping);
                leftY += 28.f;
                DrawCheckbox(canvas, L"Player Corpses", leftX, leftY, Config::g_Settings.Player.bShowCorpses, Config::g_Settings.Colors.Corpses);
                leftY += 28.f;
                DrawCheckbox(canvas, L"Local Player Self", leftX, leftY, Config::g_Settings.Player.bDrawLocal);

                // Right Panel: Target Specific Display Options
                float rightCardX = mainPanelX + 30.f + colW;
                DrawFilledRoundedRect(canvas, rightCardX, activeY, colW, colH, 8.f, panelBG);
                DrawRoundedRectOutline(canvas, rightCardX, activeY, colW, colH, 8.f, 1.f, borderCard);

                float rightX = rightCardX + 20.f;
                float rightY = activeY + 15.f;

                static int groupSel = 0; // 0=Friendly, 1=Enemy
                DrawRelationTabs2(canvas, rightX, rightY, colW - 40.f, 26.f, L"Team / Friendly", L"Enemies", groupSel);
                rightY += 40.f;

                DrawFilledRect(canvas, rightCardX + 10.f, rightY, colW - 20.f, 1.f, borderCard);
                rightY += 15.f;

                DrawTextClean(canvas, L"DISPLAY ELEMENTS", rightX, rightY, graySub, 0.65f);
                rightY += 22.f;

                Config::RelationSettings& r = (groupSel == 0) ? Config::g_Settings.PlayerFriendly : Config::g_Settings.PlayerEnemy;

                DrawCheckbox(canvas, L"Bounding Box", rightX, rightY, r.bBoxes);
                rightY += 28.f;
                DrawCheckbox(canvas, L"Skeleton Bones", rightX, rightY, r.bSkeleton);
                rightY += 28.f;
                DrawCheckbox(canvas, L"Player Name", rightX, rightY, r.bNames);
                rightY += 28.f;
                DrawCheckbox(canvas, L"Player Level", rightX, rightY, r.bLevel);
                rightY += 28.f;
                DrawCheckbox(canvas, L"Distance Info", rightX, rightY, r.bDistance);
                rightY += 28.f;
                DrawCheckbox(canvas, L"Health Bar & Value", rightX, rightY, r.bHealth);
                rightY += 38.f;

                DrawSlider(canvas, L"Max Distance", rightX, rightY, colW - 40.f, 50.f, 1000.f, r.MaxDistance, L"m");
            }
            else if (m_VisualsTab == 1) { // Dinos Tab
                // Left Panel: Master Control & Target Filters
                float leftCardX = mainPanelX + 20.f;
                DrawFilledRoundedRect(canvas, leftCardX, activeY, colW, colH, 8.f, panelBG);
                DrawRoundedRectOutline(canvas, leftCardX, activeY, colW, colH, 8.f, 1.f, borderCard);

                float leftX = leftCardX + 20.f;
                float leftY = activeY + 18.f;

                DrawFilledRoundedRect(canvas, leftX, leftY + 2.f, 3.f, 16.f, 1.5f, cyan);
                DrawTextClean(canvas, L"Dino Master & Targets", leftX + 12.f, leftY, white, 0.88f);
                leftY += 32.f;

                DrawFilledRect(canvas, leftCardX + 10.f, leftY, colW - 20.f, 1.f, borderCard);
                leftY += 15.f;

                // Master Toggle & Keybind
                DrawCheckbox(canvas, L"Enable Dino ESP", leftX, leftY, Config::g_Settings.Dino.bEnabled);
                DrawKeybind(canvas, leftX, leftY, colW - 40.f, 14.f, Config::g_Settings.Keybind_DinoESP);
                leftY += 35.f;

                DrawTextClean(canvas, L"TARGET CATEGORIES & COLORS", leftX, leftY, graySub, 0.65f);
                leftY += 22.f;

                DrawCheckbox(canvas, L"Wild Dinos", leftX, leftY, Config::g_Settings.Dino.bShowWild, Config::g_Settings.Colors.Wild);
                leftY += 28.f;
                DrawCheckbox(canvas, L"Friendly Tames", leftX, leftY, Config::g_Settings.Dino.bShowFriendly, Config::g_Settings.Colors.Friendly);
                leftY += 28.f;
                DrawCheckbox(canvas, L"Enemy Tames", leftX, leftY, Config::g_Settings.Dino.bShowEnemy, Config::g_Settings.Colors.Enemy);

                // Right Panel: Target Specific Display Options
                float rightCardX = mainPanelX + 30.f + colW;
                DrawFilledRoundedRect(canvas, rightCardX, activeY, colW, colH, 8.f, panelBG);
                DrawRoundedRectOutline(canvas, rightCardX, activeY, colW, colH, 8.f, 1.f, borderCard);

                float rightX = rightCardX + 20.f;
                float rightY = activeY + 15.f;

                static int dinoRelationTab = 0; // 0=Wild, 1=Friendly, 2=Enemy
                DrawRelationTabs3(canvas, rightX, rightY, colW - 40.f, 26.f, L"Wild", L"Friendly", L"Enemy", dinoRelationTab);
                rightY += 40.f;

                DrawFilledRect(canvas, rightCardX + 10.f, rightY, colW - 20.f, 1.f, borderCard);
                rightY += 15.f;

                DrawTextClean(canvas, L"DISPLAY ELEMENTS", rightX, rightY, graySub, 0.65f);
                rightY += 22.f;

                Config::RelationSettings& r = (dinoRelationTab == 0) ? Config::g_Settings.DinoWild : 
                                             ((dinoRelationTab == 1) ? Config::g_Settings.DinoFriendly : Config::g_Settings.DinoEnemy);

                DrawCheckbox(canvas, L"Dino Species Name", rightX, rightY, r.bNames);
                rightY += 28.f;
                DrawCheckbox(canvas, L"Base Level ID", rightX, rightY, r.bLevel);
                rightY += 28.f;
                DrawCheckbox(canvas, L"Distance Info", rightX, rightY, r.bDistance);
                rightY += 28.f;
                DrawCheckbox(canvas, L"Health Value", rightX, rightY, r.bHealth);
                rightY += 36.f;

                if (dinoRelationTab == 0) {
                    DrawSlider(canvas, L"Min Wild Level Filter", rightX, rightY, colW - 40.f, 1.f, 150.f, Config::g_Settings.MinWildLevel);
                    rightY += 50.f;
                }

                DrawSlider(canvas, L"Max Distance", rightX, rightY, colW - 40.f, 50.f, 1000.f, r.MaxDistance, L"m");
            }
            else if (m_VisualsTab == 2) { // Containers / Structures Tab
                // Left Panel: Master Control & Category Filters
                float leftCardX = mainPanelX + 20.f;
                DrawFilledRoundedRect(canvas, leftCardX, activeY, colW, colH, 8.f, panelBG);
                DrawRoundedRectOutline(canvas, leftCardX, activeY, colW, colH, 8.f, 1.f, borderCard);

                float leftX = leftCardX + 20.f;
                float leftY = activeY + 18.f;

                DrawFilledRoundedRect(canvas, leftX, leftY + 2.f, 3.f, 16.f, 1.5f, cyan);
                DrawTextClean(canvas, L"Structure Master & Types", leftX + 12.f, leftY, white, 0.88f);
                leftY += 32.f;

                DrawFilledRect(canvas, leftCardX + 10.f, leftY, colW - 20.f, 1.f, borderCard);
                leftY += 15.f;

                // Master Toggle & Keybind
                DrawCheckbox(canvas, L"Enable Structure ESP", leftX, leftY, Config::g_Settings.Structure.bEnabled);
                DrawKeybind(canvas, leftX, leftY, colW - 40.f, 14.f, Config::g_Settings.Keybind_StructureESP);
                leftY += 32.f;

                DrawTextClean(canvas, L"RELATIONSHIP FILTERS", leftX, leftY, graySub, 0.65f);
                leftY += 20.f;

                DrawCheckbox(canvas, L"Friendly Structures", leftX, leftY, Config::g_Settings.Structure.bShowFriendly, Config::g_Settings.Colors.Friendly);
                leftY += 26.f;
                DrawCheckbox(canvas, L"Enemy Structures", leftX, leftY, Config::g_Settings.Structure.bShowEnemy, Config::g_Settings.Colors.Enemy);
                leftY += 30.f;

                DrawTextClean(canvas, L"STRUCTURE TYPES", leftX, leftY, graySub, 0.65f);
                leftY += 20.f;

                DrawCheckbox(canvas, L"Defense Turrets", leftX, leftY, Config::g_Settings.Structure.bShowTurrets, Config::g_Settings.Colors.Structure);
                leftY += 26.f;
                DrawCheckbox(canvas, L"Storage Containers", leftX, leftY, Config::g_Settings.Structure.bShowContainers, Config::g_Settings.Colors.Structure);
                leftY += 26.f;
                DrawCheckbox(canvas, L"Other Structures", leftX, leftY, Config::g_Settings.Structure.bShowOthers, Config::g_Settings.Colors.Structure);

                // Right Panel: Target Specific Display Options
                float rightCardX = mainPanelX + 30.f + colW;
                DrawFilledRoundedRect(canvas, rightCardX, activeY, colW, colH, 8.f, panelBG);
                DrawRoundedRectOutline(canvas, rightCardX, activeY, colW, colH, 8.f, 1.f, borderCard);

                float rightX = rightCardX + 20.f;
                float rightY = activeY + 15.f;

                static int structRelationTab = 0; // 0=Friendly, 1=Enemy
                DrawRelationTabs2(canvas, rightX, rightY, colW - 40.f, 26.f, L"Friendly", L"Enemy", structRelationTab);
                rightY += 40.f;

                DrawFilledRect(canvas, rightCardX + 10.f, rightY, colW - 20.f, 1.f, borderCard);
                rightY += 15.f;

                DrawTextClean(canvas, L"DISPLAY ELEMENTS", rightX, rightY, graySub, 0.65f);
                rightY += 22.f;

                Config::RelationSettings& r = (structRelationTab == 0) ? Config::g_Settings.StructureFriendly : Config::g_Settings.StructureEnemy;

                DrawCheckbox(canvas, L"Structure Name", rightX, rightY, r.bNames);
                rightY += 28.f;
                DrawCheckbox(canvas, L"Distance Info", rightX, rightY, r.bDistance);
                rightY += 28.f;
                DrawCheckbox(canvas, L"Health Values", rightX, rightY, r.bHealth);
                rightY += 28.f;
                DrawCheckbox(canvas, L"Container Slots", rightX, rightY, r.bShowSlots);
                rightY += 28.f;
                DrawCheckbox(canvas, L"Turret Ammo Count", rightX, rightY, r.bShowBullets);
                rightY += 38.f;

                DrawSlider(canvas, L"Max Distance", rightX, rightY, colW - 40.f, 50.f, 1000.f, r.MaxDistance, L"m");
            }
            else if (m_VisualsTab == 3) { // World Tab
                // Left Panel: Objects & Drops
                float leftCardX = mainPanelX + 20.f;
                DrawFilledRoundedRect(canvas, leftCardX, activeY, colW, colH, 8.f, panelBG);
                DrawRoundedRectOutline(canvas, leftCardX, activeY, colW, colH, 8.f, 1.f, borderCard);

                float leftX = leftCardX + 20.f;
                float leftY = activeY + 18.f;

                DrawFilledRoundedRect(canvas, leftX, leftY + 2.f, 3.f, 16.f, 1.5f, cyan);
                DrawTextClean(canvas, L"World Master & Objects", leftX + 12.f, leftY, white, 0.88f);
                leftY += 32.f;

                DrawFilledRect(canvas, leftCardX + 10.f, leftY, colW - 20.f, 1.f, borderCard);
                leftY += 15.f;

                // Master Toggle & Keybind
                DrawCheckbox(canvas, L"Enable World ESP", leftX, leftY, Config::g_Settings.bWorldEnabled);
                DrawKeybind(canvas, leftX, leftY, colW - 40.f, 14.f, Config::g_Settings.Keybind_WorldESP);
                leftY += 35.f;

                DrawTextClean(canvas, L"WORLD OBJECT TYPES & COLORS", leftX, leftY, graySub, 0.65f);
                leftY += 22.f;

                DrawCheckbox(canvas, L"Supply Drops", leftX, leftY, Config::g_Settings.bSupplyDrops, Config::g_Settings.ColorSupplyDrop);
                leftY += 28.f;
                DrawCheckbox(canvas, L"Cave Drops", leftX, leftY, Config::g_Settings.bCaveDrops, Config::g_Settings.ColorCaveDrop);
                leftY += 28.f;
                DrawCheckbox(canvas, L"Artifacts", leftX, leftY, Config::g_Settings.bArtifacts, Config::g_Settings.ColorArtifact);
                leftY += 28.f;
                DrawCheckbox(canvas, L"Obelisks & Terminals", leftX, leftY, Config::g_Settings.bTerminals, Config::g_Settings.ColorTerminal);

                // Right Panel: Environment & View Overrides
                float rightCardX = mainPanelX + 30.f + colW;
                DrawFilledRoundedRect(canvas, rightCardX, activeY, colW, colH, 8.f, panelBG);
                DrawRoundedRectOutline(canvas, rightCardX, activeY, colW, colH, 8.f, 1.f, borderCard);

                float rightX = rightCardX + 20.f;
                float rightY = activeY + 18.f;

                DrawFilledRoundedRect(canvas, rightX, rightY + 2.f, 3.f, 16.f, 1.5f, cyan);
                DrawTextClean(canvas, L"Environment Overrides", rightX + 12.f, rightY, white, 0.88f);
                rightY += 32.f;

                DrawFilledRect(canvas, rightCardX + 10.f, rightY, colW - 20.f, 1.f, borderCard);
                rightY += 15.f;

                DrawTextClean(canvas, L"RENDER MODES", rightX, rightY, graySub, 0.65f);
                rightY += 22.f;

                DrawCheckbox(canvas, L"Preview Mode (Unlit / Clear Fog)", rightX, rightY, Config::g_Settings.bPreviewMode);
                rightY += 28.f;
                DrawCheckbox(canvas, L"Enable Custom FOV", rightX, rightY, Config::g_Settings.bFOVChanger);
                rightY += 40.f;

                DrawTextClean(canvas, L"SLIDERS & DISTANCES", rightX, rightY, graySub, 0.65f);
                rightY += 22.f;

                DrawSlider(canvas, L"FOV Angle", rightX, rightY, colW - 40.f, 70.f, 150.f, Config::g_Settings.FOVValue, L" deg");
                rightY += 50.f;

                DrawSlider(canvas, L"Max World Distance", rightX, rightY, colW - 40.f, 50.f, 2000.f, Config::g_Settings.WorldMaxDistance, L"m");
            }
        }
        else if (m_ActiveSidebar == 2) { // Automation Tab
            DrawInactiveModuleCard(canvas, mainPanelX + 20.f, activeY, maxW - 40.f, m_Height - dragH - 30.f, L"Automation Utilities");
        }
        else if (m_ActiveSidebar == 3) { // Exploits Tab
            float colW = (maxW - 50.f) / 2.f;
            float colH = m_Height - dragH - 75.f;

            // Left Card: Movement & Character Exploits
            float leftCardX = mainPanelX + 20.f;
            DrawFilledRoundedRect(canvas, leftCardX, activeY, colW, colH, 8.f, panelBG);
            DrawRoundedRectOutline(canvas, leftCardX, activeY, colW, colH, 8.f, 1.f, borderCard);

            float leftX = leftCardX + 20.f;
            float leftY = activeY + 18.f;

            DrawFilledRoundedRect(canvas, leftX, leftY + 2.f, 3.f, 16.f, 1.5f, cyan);
            DrawTextClean(canvas, L"Movement & Inventory", leftX + 12.f, leftY, white, 0.88f);
            leftY += 32.f;

            DrawFilledRect(canvas, leftCardX + 10.f, leftY, colW - 20.f, 1.f, borderCard);
            leftY += 15.f;

            DrawCheckbox(canvas, L"Enable Airstuck", leftX, leftY, Config::g_Settings.bAirstuck);
            DrawKeybind(canvas, leftX, leftY, colW - 40.f, 14.f, Config::g_Settings.Keybind_Airstuck);
            leftY += 30.f;

            if (Config::g_Settings.bAirstuck) {
                DrawSlider(canvas, L"Airstuck Strength", leftX + 15.f, leftY, colW - 55.f, 0.0f, 1.0f, Config::g_Settings.AirstuckDeltaTime);
                leftY += 45.f;
            }

            DrawCheckbox(canvas, L"Infinite Weight", leftX, leftY, Config::g_Settings.bInfiniteWeight);
            leftY += 30.f;

            DrawCheckbox(canvas, L"Long Arms", leftX, leftY, Config::g_Settings.bExtendedReach);
            leftY += 28.f;

            if (Config::g_Settings.bExtendedReach) {
                DrawCheckbox(canvas, L"Unlimited Arm Distance", leftX + 15.f, leftY, Config::g_Settings.bUnlimitedArms);
                leftY += 28.f;
            }

            // Right Card: Combat Exploits
            float rightCardX = mainPanelX + 30.f + colW;
            DrawFilledRoundedRect(canvas, rightCardX, activeY, colW, colH, 8.f, panelBG);
            DrawRoundedRectOutline(canvas, rightCardX, activeY, colW, colH, 8.f, 1.f, borderCard);

            float rightX = rightCardX + 20.f;
            float rightY = activeY + 18.f;

            DrawFilledRoundedRect(canvas, rightX, rightY + 2.f, 3.f, 16.f, 1.5f, cyan);
            DrawTextClean(canvas, L"Weapon & Combat", rightX + 12.f, rightY, white, 0.88f);
            rightY += 32.f;

            DrawFilledRect(canvas, rightCardX + 10.f, rightY, colW - 20.f, 1.f, borderCard);
            rightY += 15.f;

            DrawCheckbox(canvas, L"No Tek Rifle Overheat", rightX, rightY, Config::g_Settings.bNoTekRifleOverheat);
            rightY += 30.f;

            DrawCheckbox(canvas, L"Instant Suicide", rightX, rightY, Config::g_Settings.bSuicide);
            DrawKeybind(canvas, rightX, rightY, colW - 40.f, 14.f, Config::g_Settings.Keybind_Suicide);
            rightY += 30.f;

            DrawCheckbox(canvas, L"Gaunt Float", rightX, rightY, Config::g_Settings.bGauntFloat);
            DrawKeybind(canvas, rightX, rightY, colW - 40.f, 14.f, Config::g_Settings.Keybind_GauntFloat);
            rightY += 30.f;

            DrawCheckbox(canvas, L"Unlock All Explorer Notes", rightX, rightY, Config::g_Settings.bUnlockExplorerNotes);
            rightY += 30.f;

            DrawCheckbox(canvas, L"Run While Reloading", rightX, rightY, Config::g_Settings.bRunWhileReloading);
            rightY += 30.f;

            DrawCheckbox(canvas, L"Remove Scope Overlay", rightX, rightY, Config::g_Settings.bRemoveScopeOverlay);
        }
        else if (m_ActiveSidebar == 4) { // Misc Tab
            DrawInactiveModuleCard(canvas, mainPanelX + 20.f, activeY, maxW - 40.f, m_Height - dragH - 30.f, L"Miscellaneous Features");
        }
        else if (m_ActiveSidebar == 5) { // Settings Panel
            float colW = (maxW - 50.f) / 2.f;
            float colH = m_Height - dragH - 30.f;

            // Left Card: Core HUD & UI Settings
            float leftCardX = mainPanelX + 20.f;
            DrawFilledRoundedRect(canvas, leftCardX, activeY, colW, colH, 8.f, panelBG);
            DrawRoundedRectOutline(canvas, leftCardX, activeY, colW, colH, 8.f, 1.f, borderCard);

            float itemX = leftCardX + 20.f;
            float itemY = activeY + 18.f;

            DrawFilledRoundedRect(canvas, itemX, itemY + 2.f, 3.f, 16.f, 1.5f, cyan);
            DrawTextClean(canvas, L"HUD & Interface Settings", itemX + 12.f, itemY, white, 0.88f);
            itemY += 32.f;

            DrawFilledRect(canvas, leftCardX + 10.f, itemY, colW - 20.f, 1.f, borderCard);
            itemY += 20.f;

            DrawSlider(canvas, L"Global ESP Text Scale", itemX, itemY, colW - 40.f, 0.5f, 3.0f, Config::g_Settings.ESPTextSize, L"x");
            itemY += 50.f;

            DrawCheckbox(canvas, L"Enable Text Outlines", itemX, itemY, Config::g_Settings.bTextOutlines);
            itemY += 30.f;

            DrawCheckbox(canvas, L"Show Bottom-Left HUD (FPS & Actors)", itemX, itemY, Config::g_Settings.bShowBottomHUD);
            itemY += 30.f;

            DrawCheckbox(canvas, L"Show Server FPS", itemX, itemY, Config::g_Settings.bShowServerFPS, Config::g_Settings.ServerFPSColor);
            itemY += 30.f;

            DrawCheckbox(canvas, L"Show Upload Timer", itemX, itemY, Config::g_Settings.bShowUploadTimer, Config::g_Settings.UploadTimerColor);
            itemY += 45.f;

            if (DrawButton(canvas, L"Save Config", itemX, itemY, 130.f, 32.f)) {
            }

            // Right Card: Crosshair Options
            float rightCardX = mainPanelX + 30.f + colW;
            DrawFilledRoundedRect(canvas, rightCardX, activeY, colW, colH, 8.f, panelBG);
            DrawRoundedRectOutline(canvas, rightCardX, activeY, colW, colH, 8.f, 1.f, borderCard);

            float rx = rightCardX + 20.f;
            float ry = activeY + 18.f;

            DrawFilledRoundedRect(canvas, rx, ry + 2.f, 3.f, 16.f, 1.5f, cyan);
            DrawTextClean(canvas, L"Crosshair Customization", rx + 12.f, ry, white, 0.88f);
            ry += 32.f;

            DrawFilledRect(canvas, rightCardX + 10.f, ry, colW - 20.f, 1.f, borderCard);
            ry += 20.f;

            DrawCheckbox(canvas, L"Crosshair", rx, ry, Config::g_Settings.bCrosshair, Config::g_Settings.CrosshairColor);
            ry += 35.f;

            if (Config::g_Settings.bCrosshair) {
                DrawRelationTabs3(canvas, rx, ry, colW - 40.f, 26.f, L"Plus (+)", L"Dot (.)", L"Circle (O)", Config::g_Settings.CrosshairType);
                ry += 45.f;

                DrawSlider(canvas, L"Crosshair Scale", rx, ry, colW - 40.f, 2.f, 40.f, Config::g_Settings.CrosshairSize, L"px");
                ry += 50.f;

                DrawSlider(canvas, L"Line Thickness", rx, ry, colW - 40.f, 1.f, 10.f, Config::g_Settings.CrosshairThickness, L"px");
            }

            if (DrawButton(canvas, L"Load Config", itemX + 150.f, itemY, 130.f, 32.f)) {
            }
            itemY += 65.f;

            if (DrawButton(canvas, L"Unload DLL", itemX, itemY, 130.f, 32.f)) {
                Config::g_Settings.bUnloadRequested = true;
            }
        }

        RenderColorPickerPopover(canvas);

        if (Hooks::g_KeyPendingDown[VK_LBUTTON]) {
            Hooks::g_KeyPendingDown[VK_LBUTTON] = false;
        }
    }
}

