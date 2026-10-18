//-----------------------------------------------------------------------------
// NG's icons in the current colours.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "platform/sdl/icontextures.h"
#include "ui/icons.h"

namespace SolveSpace {
namespace Ui {

static uint32_t Rgb(RgbaColor c) {
    return ((uint32_t)c.red << 16) | ((uint32_t)c.green << 8) | (uint32_t)c.blue;
}

static uint32_t ImRgb(ImGuiCol col) {
    ImVec4 c = ImGui::GetStyleColorVec4(col);
    return ((uint32_t)(c.x * 255 + 0.5f) << 16) | ((uint32_t)(c.y * 255 + 0.5f) << 8) |
           (uint32_t)(c.z * 255 + 0.5f);
}

IconPalette CurrentIconPalette() {
    // The icons mirror the drawing area: its line styles' colours, and the theme's.
    IconPalette palette;
    palette.text         = ImRgb(ImGuiCol_Text);
    palette.entity       = Rgb(Style::Color(hStyle{ Style::ACTIVE_GRP }));
    palette.point        = Rgb(Style::Color(hStyle{ Style::DATUM }));
    palette.constraint   = Rgb(Style::Color(hStyle{ Style::CONSTRAINT }));
    palette.construction = Rgb(Style::Color(hStyle{ Style::CONSTRUCTION }));
    palette.solid        = ImRgb(ImGuiCol_CheckMark); // the theme's accent
    return palette;
}

ImTextureID Icon(const std::string &name, float size) {
    float scale = std::max(1.0f, ImGui::GetIO().DisplayFramebufferScale.x);
    return Platform::IconTexture(name, (int)(size * scale + 0.5f), CurrentIconPalette());
}

}
}
