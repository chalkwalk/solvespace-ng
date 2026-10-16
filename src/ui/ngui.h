//-----------------------------------------------------------------------------
// Per-frame entry points of the SolveSpace NG user interface layer, called by the SDL backend.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_UI_NGUI_H
#define SOLVESPACE_UI_NGUI_H

#include "keymap.h"
#include "platform/sdl/menumodel.h"

namespace SolveSpace {
namespace Ui {

void InitNgUi(); // after the ImGui context exists
// Returns the menu item the user picked this frame (to be run after the frame), or nullptr.
std::shared_ptr<Platform::MenuItemSdl> DrawNgUi(Platform::MenuBarSdl *menuBar,
                                                const keymap::Keymap &keymap);
// True while a UI-layer text field (the palette) owns the keyboard.
bool CapturesText();
void HandleUiAction(const std::string &action);

}
}

#endif
