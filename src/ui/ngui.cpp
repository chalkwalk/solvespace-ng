//-----------------------------------------------------------------------------
// Per-frame entry points of the SolveSpace NG user interface layer.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "imgui.h"
#include "ui/ngui.h"
#include "ui/palettewidget.h"
#include "ui/theme.h"

namespace SolveSpace {
namespace Ui {

static CommandPalette gPalette;

void InitNgUi() {
    ApplyTheme(LoadThemeChoice());
}

void HandleUiAction(const std::string &action) {
    if(action == "ui.command-palette") gPalette.Open();
    else if(action == "ui.toggle-theme") ToggleTheme();
    else dbp("unknown UI action '%s'", action.c_str());
}

std::shared_ptr<Platform::MenuItemSdl> DrawNgUi(Platform::MenuBarSdl *menuBar,
                                                const keymap::Keymap &keymap) {
    if(menuBar && gPalette.IsOpen()) {
        return gPalette.Draw(Platform::FlattenMenuBar(*menuBar), keymap);
    }
    return nullptr;
}

bool CapturesText() {
    return gPalette.IsOpen();
}

}
}
