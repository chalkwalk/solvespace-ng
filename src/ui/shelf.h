//-----------------------------------------------------------------------------
// The tool shelf panel (View > Show Toolbar): icon buttons for the tools, in place of the old
// custom-drawn toolbar. Hidden by default (AGENTS.md section 5).
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_UI_SHELF_H
#define SOLVESPACE_UI_SHELF_H

#include <functional>
#include "keymap.h"

namespace SolveSpace {
namespace Ui {

// Draws the shelf if SS.showToolbar. Returns what the user asked for, to run after the frame.
std::function<void()> DrawShelf(const keymap::Keymap &keymap);
int ShelfTextFallbacks(); // buttons drawn as text last frame, for want of an icon

}
}

#endif
