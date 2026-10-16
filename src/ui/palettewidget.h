//-----------------------------------------------------------------------------
// The command palette: fuzzy search over every menu item.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_UI_PALETTEWIDGET_H
#define SOLVESPACE_UI_PALETTEWIDGET_H

#include "keymap.h"
#include "platform/sdl/menumodel.h"

namespace SolveSpace {
namespace Ui {

class CommandPalette {
public:
    void Open();
    bool IsOpen() const;
    // Draws if open. Activates the chosen item's onTrigger (only if enabled) and closes.
    void Draw(const std::vector<Platform::FlatMenuItem> &items, const keymap::Keymap &keymap);

private:
    bool open       = false;
    int  framesOpen = 0;
    char query[256] = {};
    int  selected   = 0;
};

}
}

#endif
