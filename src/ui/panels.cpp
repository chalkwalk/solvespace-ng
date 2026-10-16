//-----------------------------------------------------------------------------
// Floating panels.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "imgui.h"
#include "ui/panels.h"

namespace SolveSpace {
namespace Ui {

Panel::Panel(std::string name, std::string title, PanelEdge edge, float width,
             bool shownByDefault)
    : name(std::move(name)), title(std::move(title)), edge(edge), width(width),
      shownByDefault(shownByDefault), shown(shownByDefault) {}

void Panel::Load() {
    auto settings = Platform::GetSettings();
    shown  = settings->ThawBool("NG_Panel_" + name + "_Shown", shownByDefault);
    savedX = (float)settings->ThawFloat("NG_Panel_" + name + "_X", -1);
    savedY = (float)settings->ThawFloat("NG_Panel_" + name + "_Y", -1);
    savedW = (float)settings->ThawFloat("NG_Panel_" + name + "_W", -1);
    savedH = (float)settings->ThawFloat("NG_Panel_" + name + "_H", -1);
}

void Panel::SetShown(bool s) {
    shown = s;
    Platform::GetSettings()->FreezeBool("NG_Panel_" + name + "_Shown", shown);
}

bool Panel::Begin() {
    if(!shown) return false;
    ImGuiIO &io = ImGui::GetIO();
    if(savedW > 0 && savedH > 0) {
        ImGui::SetNextWindowPos(ImVec2(savedX, savedY), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(savedW, savedH), ImGuiCond_FirstUseEver);
    } else {
        // Below the menu bar, clear of the toolbar on the left and the status bar below.
        float top = ImGui::GetFrameHeight() + 16;
        float x   = edge == PanelEdge::LEFT ? 80.0f : io.DisplaySize.x - width - 12;
        float h   = std::max(200.0f, std::min(520.0f, io.DisplaySize.y - top - 60));
        ImGui::SetNextWindowPos(ImVec2(x, top), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(width, h), ImGuiCond_FirstUseEver);
    }
    bool open    = true;
    std::string id = title + "###panel-" + name;
    bool visible = ImGui::Begin(id.c_str(), &open, ImGuiWindowFlags_NoSavedSettings);
    if(!open) SetShown(false);
    if(!visible) ImGui::End();
    return visible;
}

void Panel::End() {
    ImVec2 p = ImGui::GetWindowPos(), s = ImGui::GetWindowSize();
    if(p.x != savedX || p.y != savedY || s.x != savedW || s.y != savedH) {
        savedX = p.x; savedY = p.y; savedW = s.x; savedH = s.y;
        auto settings = Platform::GetSettings();
        settings->FreezeFloat("NG_Panel_" + name + "_X", savedX);
        settings->FreezeFloat("NG_Panel_" + name + "_Y", savedY);
        settings->FreezeFloat("NG_Panel_" + name + "_W", savedW);
        settings->FreezeFloat("NG_Panel_" + name + "_H", savedH);
    }
    ImGui::End();
}

Panel &OutlinerPanel() {
    static Panel panel("Outliner", "Outliner", PanelEdge::LEFT, 260, /*shownByDefault=*/true);
    return panel;
}

}
}
