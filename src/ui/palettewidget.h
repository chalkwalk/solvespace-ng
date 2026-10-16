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
    // Draws if open. Returns the chosen enabled item (and closes), or nullptr; the caller runs
    // it after the frame.
    std::shared_ptr<Platform::MenuItemSdl> Draw(const std::vector<Platform::FlatMenuItem> &items,
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
