//-----------------------------------------------------------------------------
// Per-frame entry points of the SolveSpace NG user interface layer, called by the SDL backend.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_UI_NGUI_H
#define SOLVESPACE_UI_NGUI_H

#include "keymap.h"
#include "platform/sdl/menumodel.h"
#include "ui/statusmodel.h"

namespace SolveSpace {
namespace Ui {

void InitNgUi(); // after the ImGui context exists
// Returns what the user picked this frame (to be run after the frame), or an empty function.
std::function<void()> DrawNgUi(Platform::MenuBarSdl *menuBar, const keymap::Keymap &keymap);
// The active modal tool's status text and key hints (empty when none is active).
void SetModalStatus(std::string tool, std::vector<KeyHint> hints);
// True while a UI-layer text field (the palette) owns the keyboard.
bool CapturesText();
void HandleUiAction(const std::string &action);
// Opens the NG panel for the property browser screen a command picked (SS.TW.shown); false if
// that screen has no panel yet.
bool ShowScreenInPanel();
// The active modal tool's key handler (KeyRouter::modal). True if it consumed the chord.
bool ModalKey(const keymap::Chord &c);

}
}

#endif
