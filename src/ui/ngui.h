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
void DrawNgUi(Platform::MenuBarSdl *menuBar, const keymap::Keymap &keymap);
void HandleUiAction(const std::string &action);

}
}

#endif
