//-----------------------------------------------------------------------------
// Per-frame entry points of the SolveSpace NG user interface layer.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "imgui.h"
#include "ui/ngui.h"
#include "ui/palettewidget.h"
#include "ui/theme.h"
#include "ui/uiactions.h"

namespace SolveSpace {
namespace Ui {

static CommandPalette gPalette;

void InitNgUi() {
    ApplyTheme(LoadThemeChoice());
    Actions().Bind("ui.command-palette", [] { gPalette.Open(); });
    Actions().Bind("ui.toggle-theme", [] { ToggleTheme(); });
}

void HandleUiAction(const std::string &action) {
    if(!Actions().Run(action)) {
        dbp("UI action '%s' is unknown, unbound or disabled", action.c_str());
    }
}

std::function<void()> DrawNgUi(Platform::MenuBarSdl *menuBar, const keymap::Keymap &keymap) {
    if(menuBar && gPalette.IsOpen()) {
        return gPalette.Draw(BuildPaletteEntries(Platform::FlattenMenuBar(*menuBar)), keymap);
    }
    return {};
}

bool CapturesText() {
    return gPalette.IsOpen();
}

}
}
