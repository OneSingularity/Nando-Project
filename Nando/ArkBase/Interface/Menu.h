#pragma once

namespace Menu {
    extern bool g_Open;
    extern int* m_BindingKey;
    extern ULONGLONG g_BindingStartTime;
    
    void ToggleMenuState(bool state);
    void RenderMenu(SDK::UCanvas* canvas);
}
