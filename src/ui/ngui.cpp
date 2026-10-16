//-----------------------------------------------------------------------------
// Per-frame entry points of the SolveSpace NG user interface layer.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "imgui.h"
#include "ui/ngui.h"
#include "ui/palettewidget.h"

namespace SolveSpace {
namespace Ui {

static CommandPalette gPalette;

void InitNgUi() {
}

void HandleUiAction(const std::string &action) {
    if(action == "ui.command-palette") gPalette.Open();
    else dbp("unknown UI action '%s'", action.c_str());
}

void DrawNgUi(Platform::MenuBarSdl *menuBar, const keymap::Keymap &keymap) {
    if(menuBar && gPalette.IsOpen()) {
        gPalette.Draw(Platform::FlattenMenuBar(*menuBar), keymap);
    }
}

}
}
