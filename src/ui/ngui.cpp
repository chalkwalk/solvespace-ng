//-----------------------------------------------------------------------------
// Per-frame entry points of the SolveSpace NG user interface layer.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "imgui.h"
#include "ui/ngui.h"
#include "ui/palettewidget.h"
#include "ui/piemenu.h"
#include "ui/theme.h"
#include "ui/uiactions.h"

namespace SolveSpace {
namespace Ui {

static CommandPalette gPalette;

void InitNgUi() {
    ApplyTheme(LoadThemeChoice());
    Actions().Bind("ui.command-palette", [] { gPalette.Open(); });
    Actions().Bind("ui.toggle-theme", [] { ToggleTheme(); });
    Actions().Bind("ui.pie-add", [] {
        Pie().Open("Add", {
            { "Line", "line-segment" }, { "Rectangle", "rectangle" }, { "Circle", "circle" },
            { "Arc", "arc" }, { "Spline", "cubic-spline" }, { "Point", "datum-point" },
            { "Text", "ttf-text" }, { "Workplane", "workplane" } });
    });
    Actions().Bind("ui.pie-view", [] {
        Pie().Open("View", {
            { "Top", "ui.view-top" }, { "Right", "ui.view-right" },
            { "Isometric", "nearest-iso" }, { "Front", "ui.view-front" },
            { "Bottom", "ui.view-bottom" }, { "Left", "ui.view-left" },
            { "Frame Selected", "ui.view-frame-selected" }, { "Back", "ui.view-back" } });
    });
}

void HandleUiAction(const std::string &action) {
    if(!Actions().Run(action)) {
        dbp("UI action '%s' is unknown, unbound or disabled", action.c_str());
    }
}

std::function<void()> DrawNgUi(Platform::MenuBarSdl *menuBar, const keymap::Keymap &keymap) {
    if(Pie().IsOpen()) {
        std::string name = Pie().Draw();
        if(!name.empty()) return [name] { RunNamedCommand(name); };
    }
    if(menuBar && gPalette.IsOpen()) {
        return gPalette.Draw(BuildPaletteEntries(Platform::FlattenMenuBar(*menuBar)), keymap);
    }
    return {};
}

bool ModalKey(const keymap::Chord &c) {
    if(Pie().IsOpen()) return Pie().HandleChord(c);
    return false;
}

bool CapturesText() {
    return gPalette.IsOpen();
}

}
}
