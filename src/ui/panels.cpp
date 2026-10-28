//-----------------------------------------------------------------------------
// Floating panels.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "ui/panels.h"

namespace SolveSpace {
namespace Ui {

Panel::Panel(std::string name, std::string title, PanelEdge edge, float width,
             bool shownByDefault, bool remembersShown)
    : name(std::move(name)), title(std::move(title)), edge(edge), width(width),
      shownByDefault(shownByDefault), shown(shownByDefault), remembersShown(remembersShown) {}

// Where the panels were left is saved per layout, so a new default layout isn't overridden by
// positions remembered from the old one.
static const std::string LayoutTag = "c";

static std::string Key(const std::string &name, const char *what) {
    return "NG_Panel_" + name + "_" + LayoutTag + what;
}

// The height of the top panel of the right-hand stack.
static const float StackTopHeight = 220;

static bool gCompact = false;

void SetCompactLayout(bool compact) {
    gCompact = compact;
}

bool CompactLayout() {
    return gCompact;
}

void Panel::Load() {
    auto settings = Platform::GetSettings();
    // The right-hand stack would cover a phone's screen.
    bool byDefault = shownByDefault &&
                     !(gCompact && (edge == PanelEdge::RIGHT_TOP || edge == PanelEdge::RIGHT_BELOW));
    shown  = remembersShown ? settings->ThawBool("NG_Panel_" + name + "_Shown", byDefault)
                            : byDefault;
    savedX = (float)settings->ThawFloat(Key(name, "X"), -1);
    savedY = (float)settings->ThawFloat(Key(name, "Y"), -1);
    savedW = (float)settings->ThawFloat(Key(name, "W"), -1);
    savedH = (float)settings->ThawFloat(Key(name, "H"), -1);
}

void Panel::SetShown(bool s) {
    if(s == shown) return;
    shown = s;
    if(remembersShown) Platform::GetSettings()->FreezeBool("NG_Panel_" + name + "_Shown", shown);
}

bool Panel::Begin() {
    if(!shown) return false;
    ImGuiIO &io = ImGui::GetIO();
    if(savedW > 0 && savedH > 0) {
        ImGui::SetNextWindowPos(ImVec2(savedX, savedY), ImGuiCond_FirstUseEver);
        if(!autoSize) ImGui::SetNextWindowSize(ImVec2(savedW, savedH), ImGuiCond_FirstUseEver);
    } else {
        // Below the menu bar, clear of the status bar below.
        float top = ImGui::GetFrameHeight() + 16;
        float x   = edge == PanelEdge::LEFT ? 12.0f : io.DisplaySize.x - width - 12;
        float h   = std::max(200.0f, std::min(520.0f, io.DisplaySize.y - top - 60));
        if(edge == PanelEdge::RIGHT_TOP) {
            h = StackTopHeight;
        } else if(edge == PanelEdge::RIGHT_BELOW) {
            // Below the stack's top panel when that is shown, down to the status bar.
            if(OutlinerPanel().Shown()) top += StackTopHeight + 10;
            h = std::max(200.0f, io.DisplaySize.y - top - 52);
        } else if(edge == PanelEdge::CENTER || edge == PanelEdge::TOP) {
            // Over the middle of the viewport, clear of the side panels.
            x = (io.DisplaySize.x - std::min(width, io.DisplaySize.x - 24)) / 2;
        } else if(edge == PanelEdge::BOTTOM_RIGHT) {
            // At the bottom, left of the right-hand stack (not over Properties), above the
            // status bar.
            h   = 220;
            top = std::max(top, io.DisplaySize.y - h - 52);
            if(OutlinerPanel().Shown() || PropertiesPanel().Shown()) x -= 320 + 12;
            x = std::max(12.0f, x);
        }
        ImGui::SetNextWindowPos(ImVec2(x, top), ImGuiCond_FirstUseEver);
        if(!autoSize) ImGui::SetNextWindowSize(ImVec2(width, h), ImGuiCond_FirstUseEver);
    }
    bool open    = true;
    std::string id = std::string(_(title.c_str())) + "###panel-" + name;
    // Keep the panel on screen when the window has shrunk under it: smaller if it must be,
    // then moved back inside.
    if(ImGuiWindow *w = ImGui::FindWindowByName(id.c_str())) {
        ImVec2 size = w->Size;
        if(!autoSize) {
            size.x = std::min(size.x, io.DisplaySize.x);
            size.y = std::min(size.y, io.DisplaySize.y);
            if(size.x != w->Size.x || size.y != w->Size.y) {
                ImGui::SetNextWindowSize(size);
                SS.GW.Invalidate(); // and draw again, now that it fits
            }
        }
        float x = std::max(0.0f, std::min(w->Pos.x, io.DisplaySize.x - size.x));
        float y = std::max(0.0f, std::min(w->Pos.y, io.DisplaySize.y - size.y));
        if(x != w->Pos.x || y != w->Pos.y) {
            ImGui::SetNextWindowPos(ImVec2(x, y));
            SS.GW.Invalidate();
        }
    }
    // NoNav: a panel that has been clicked must not take Tab, Space, Enter or the arrows from
    // the keymap; only a field being typed in gets keys.
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav;
    if(autoSize) flags |= ImGuiWindowFlags_AlwaysAutoResize;
    bool visible = ImGui::Begin(id.c_str(), &open, flags);
    if(!open) SetShown(false);
    if(!visible) ImGui::End();
    return visible;
}

void Panel::End() {
    ImVec2 p = ImGui::GetWindowPos(), s = ImGui::GetWindowSize();
    if(p.x != savedX || p.y != savedY || s.x != savedW || s.y != savedH) {
        savedX = p.x; savedY = p.y; savedW = s.x; savedH = s.y;
        auto settings = Platform::GetSettings();
        settings->FreezeFloat(Key(name, "X"), savedX);
        settings->FreezeFloat(Key(name, "Y"), savedY);
        settings->FreezeFloat(Key(name, "W"), savedW);
        settings->FreezeFloat(Key(name, "H"), savedH);
    }
    ImGui::End();
}

Panel &ShelfPanel() {
    // Shown or not is SS.showToolbar (View > Show Toolbar), which the core saves.
    static Panel panel = [] {
        Panel p("Shelf", N_("Tools"), PanelEdge::LEFT, 100, /*shownByDefault=*/false,
                /*remembersShown=*/false);
        p.autoSize = true;
        return p;
    }();
    return panel;
}

Panel &ShortcutsPanel() {
    static Panel panel("Shortcuts", N_("Keyboard Shortcuts"), PanelEdge::CENTER, 460,
                       /*shownByDefault=*/false, /*remembersShown=*/false);
    return panel;
}

Panel &PropertiesPanel() {
    static Panel panel("Properties", N_("Properties"), PanelEdge::RIGHT_BELOW, 320,
                       /*shownByDefault=*/true);
    return panel;
}

Panel &ProblemsPanel() {
    static Panel panel("Problems", N_("Problems"), PanelEdge::BOTTOM_RIGHT, 320,
                       /*shownByDefault=*/false);
    return panel;
}

Panel &PreferencesPanel() {
    static Panel panel("Preferences", N_("Preferences"), PanelEdge::CENTER, 380,
                       /*shownByDefault=*/false);
    return panel;
}

Panel &StylesPanel() {
    static Panel panel("Styles", N_("Styles"), PanelEdge::CENTER, 380, /*shownByDefault=*/false);
    return panel;
}

Panel &ViewPanel() {
    static Panel panel("View", N_("View"), PanelEdge::CENTER, 380, /*shownByDefault=*/false);
    return panel;
}

Panel &PastePanel() {
    static Panel panel("Paste", N_("Paste Transformed"), PanelEdge::CENTER, 380, false,
                       /*remembersShown=*/false);
    return panel;
}

Panel &StepDimensionPanel() {
    static Panel panel("StepDimension", N_("Step Dimension"), PanelEdge::CENTER, 380, false,
                       /*remembersShown=*/false);
    return panel;
}

Panel &TangentArcPanel() {
    static Panel panel("TangentArc", N_("Tangent Arc"), PanelEdge::CENTER, 380, false,
                       /*remembersShown=*/false);
    return panel;
}

Panel &OutlinerPanel() {
    static Panel panel("Outliner", N_("Outliner"), PanelEdge::RIGHT_TOP, 320,
                       /*shownByDefault=*/true);
    return panel;
}

}
}
