#pragma once
#ifdef _WIN32
#include <windows.h>
namespace krmemory {
enum class UiArea : int { general=0,dashboard=1,explorer=2,chat=3,management=4,connections=5,count=6 };
struct AppearanceSettings {
    int theme{};
    int font_sizes[static_cast<int>(UiArea::count)]{1,1,1,1,1,1};
    int colors[static_cast<int>(UiArea::count)]{};
};
struct UiPalette {
    COLORREF background{},sidebar{},surface{},surface_hover{},subtle{},border{};
    COLORREF text{},muted{},accent{},accent_hover{};
    bool dark{};
};
int scale_for_dpi(int logical,unsigned dpi);
UiPalette current_ui_palette(UiArea area=UiArea::general);
AppearanceSettings appearance_settings();
bool save_appearance_settings(const AppearanceSettings& settings);
void refresh_open_window_themes();
void apply_windows_11_theme(HWND window,UiArea area=UiArea::general);
void release_windows_11_theme(HWND window);
HBRUSH themed_control_brush(HDC dc,bool field=false);
}
#endif
