//-----------------------------------------------------------------------------
// Help > Keyboard Shortcuts: every key in the current keymap, by what it does.
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

}
}

#endif
