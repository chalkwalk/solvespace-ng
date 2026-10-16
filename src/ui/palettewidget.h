//-----------------------------------------------------------------------------
// The command palette: fuzzy search over every menu item and ui.* action.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_UI_PALETTEWIDGET_H
#define SOLVESPACE_UI_PALETTEWIDGET_H

#include "keymap.h"
#include "ui/uiactions.h"

namespace SolveSpace {
namespace Ui {

class CommandPalette {
public:
    void Open();
    bool IsOpen() const;
    // Draws if open. Returns the chosen enabled entry's action (and closes), or an empty
    // function; the caller runs it after the frame.
    std::function<void()> Draw(const std::vector<PaletteEntry> &entries,
                               const keymap::Keymap &keymap);

private:
    bool open       = false;
    int  framesOpen = 0;
    char query[256] = {};
    int  selected   = 0;
    bool fieldActive = false; // the query field was active last frame
};

}
}

#endif
