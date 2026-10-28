//-----------------------------------------------------------------------------
// Help > Keyboard Shortcuts: every key in the current keymap, by what it does; a click on a
// key changes it.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_UI_SHORTCUTS_H
#define SOLVESPACE_UI_SHORTCUTS_H

#include <functional>
#include "keymap.h"
#include "platform/sdl/menumodel.h"

namespace SolveSpace {
namespace Ui {

// Draws the panel if shown. Returns what the user asked for, to run after the frame.
std::function<void()> DrawShortcuts(const Platform::MenuBarSdl &menuBar,
                                    const keymap::Keymap &keymap);
// While a key is being changed, the next chord is the new key (Backspace or Delete: none,
// Esc: as it was); true if the chord was taken. ModalKey asks first.
bool ShortcutsTakeKey(const keymap::Chord &c);
// What the last change took from another command ("F3 no longer runs ..."), or empty.
const std::string &ShortcutsNote();
// What saves an edited keymap: its file's text, or "" for the preset's keys again.
void SetKeymapWriter(std::function<void(const std::string &)> writer);

}
}

#endif
