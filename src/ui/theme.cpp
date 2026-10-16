//-----------------------------------------------------------------------------
// UI fonts and the dark/light ImGui theme, built from design tokens.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "imgui.h"
#include "ui/theme.h"

namespace SolveSpace {
namespace Ui {

static ImFont *gUiFont = nullptr, *gMonoFont = nullptr;

static ImVec4 Hex(uint32_t rgb, float a = 1.0f) {
    return ImVec4(((rgb >> 16) & 0xFF) / 255.0f, ((rgb >> 8) & 0xFF) / 255.0f,
                  (rgb & 0xFF) / 255.0f, a);
}

struct Tokens {
    uint32_t bg, panel, panelBorder, text, textMuted, accent, accentHover, danger;
    float    panelAlpha;
};

static const Tokens Dark  = { 0x16181D, 0x1E2128, 0x2C313A, 0xE6E8EC, 0x8B93A1,
                              0x4F8EF7, 0x6AA0FA, 0xF26D6D, 0.96f };
static const Tokens Light = { 0xF4F5F7, 0xFFFFFF, 0xD9DCE2, 0x1C1F24, 0x6A7280,
                              0x2F6FE0, 0x4A82E8, 0xD64545, 0.97f };

static void LoadFontsOnce() {
    // Once per ImGui context (the test harness makes a new one for each main window).
    ImGuiIO &io = ImGui::GetIO();
    if(gUiFont && io.Fonts->Fonts.contains(gUiFont)) return;
    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false; // the data lives in the embedded resource
    size_t size;
    // ImGui 1.92 rasterises at the framebuffer scale itself, so sizes are logical pixels.
    void *inter = (void *)Platform::LoadResource("fonts/ng/Inter-Regular.ttf", &size);
    gUiFont = io.Fonts->AddFontFromMemoryTTF(inter, (int)size, 15.0f, &cfg);
    void *mono = (void *)Platform::LoadResource("fonts/ng/JetBrainsMono-Regular.ttf", &size);
    gMonoFont = io.Fonts->AddFontFromMemoryTTF(mono, (int)size, 14.0f, &cfg);
    io.FontDefault = gUiFont;
}

ThemeChoice LoadThemeChoice() {
    return Platform::GetSettings()->ThawString("NG_Theme", "dark") == "light"
               ? ThemeChoice::LIGHT
               : ThemeChoice::DARK;
}

void ApplyTheme(ThemeChoice choice) {
    LoadFontsOnce();
    const Tokens &t = choice == ThemeChoice::DARK ? Dark : Light;
    ImGuiStyle &s = ImGui::GetStyle();
    s.WindowRounding    = 10;
    s.ChildRounding     = 8;
    s.FrameRounding     = 6;
    s.PopupRounding     = 8;
    s.GrabRounding      = 6;
    s.ScrollbarRounding = 6;
    s.WindowPadding     = ImVec2(12, 10);
    s.FramePadding      = ImVec2(8, 5);
    s.ItemSpacing       = ImVec2(8, 6);
    s.WindowBorderSize  = 1;
    s.PopupBorderSize   = 1;
    ImVec4 *c = s.Colors;
    c[ImGuiCol_Text]             = Hex(t.text);
    c[ImGuiCol_TextDisabled]     = Hex(t.textMuted);
    c[ImGuiCol_WindowBg]         = Hex(t.panel, t.panelAlpha);
    c[ImGuiCol_PopupBg]          = Hex(t.panel, 0.98f);
    c[ImGuiCol_Border]           = Hex(t.panelBorder);
    c[ImGuiCol_FrameBg]          = Hex(t.bg);
    c[ImGuiCol_FrameBgHovered]   = Hex(t.panelBorder);
    c[ImGuiCol_FrameBgActive]    = Hex(t.panelBorder);
    c[ImGuiCol_TitleBg]          = Hex(t.panel);
    c[ImGuiCol_TitleBgActive]    = Hex(t.panel);
    c[ImGuiCol_TitleBgCollapsed] = Hex(t.panel);
    c[ImGuiCol_MenuBarBg]        = Hex(t.panel, t.panelAlpha);
    c[ImGuiCol_Header]           = Hex(t.accent, 0.25f);
    c[ImGuiCol_HeaderHovered]    = Hex(t.accent, 0.35f);
    c[ImGuiCol_HeaderActive]     = Hex(t.accent, 0.50f);
    c[ImGuiCol_Button]           = Hex(t.accent, 0.20f);
    c[ImGuiCol_ButtonHovered]    = Hex(t.accentHover, 0.35f);
    c[ImGuiCol_ButtonActive]     = Hex(t.accent, 0.55f);
    c[ImGuiCol_CheckMark]        = Hex(t.accent);
    c[ImGuiCol_SliderGrab]       = Hex(t.accent);
    c[ImGuiCol_SliderGrabActive] = Hex(t.accentHover);
    c[ImGuiCol_Separator]        = Hex(t.panelBorder);
    c[ImGuiCol_NavCursor]        = Hex(t.accent);
    c[ImGuiCol_TextSelectedBg]   = Hex(t.accent, 0.35f);
    c[ImGuiCol_ResizeGrip]       = Hex(t.panelBorder);
    c[ImGuiCol_ResizeGripHovered] = Hex(t.accent, 0.6f);
    c[ImGuiCol_ResizeGripActive] = Hex(t.accent);
    // The status bar's solver state: fine / problem.
    c[ImGuiCol_PlotHistogram]    = Hex(t.accent);
    c[ImGuiCol_PlotLinesHovered] = Hex(t.danger);
}

void ToggleTheme() {
    ThemeChoice next =
        LoadThemeChoice() == ThemeChoice::DARK ? ThemeChoice::LIGHT : ThemeChoice::DARK;
    Platform::GetSettings()->FreezeString("NG_Theme",
                                          next == ThemeChoice::LIGHT ? "light" : "dark");
    ApplyTheme(next);
}

ImFont *MonoFont() {
    return gMonoFont;
}

}
}
