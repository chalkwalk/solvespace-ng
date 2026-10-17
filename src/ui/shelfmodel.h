//-----------------------------------------------------------------------------
// The tool shelf's contents: NG's replacement for the old left toolbar (src/toolbar.cpp),
// grouped in sections, each button a command with its icon, menu label and key.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_UI_SHELFMODEL_H
#define SOLVESPACE_UI_SHELFMODEL_H

#include <string>
#include <vector>
#include "keymap.h"
#include "solvespace.h"

namespace SolveSpace {
namespace Ui {

struct ShelfItem {
    Command     command;
    std::string icon;  // resource "icons/ng/<icon>.svg"
    std::string label; // the menu label without '&', e.g. "Line Segment"
    std::string keys;  // the first chord bound to the command, e.g. "L"; "" if none
};

struct ShelfSection {
    std::string            title; // "Sketch", "Constrain", "Groups", "View"
    std::vector<ShelfItem> items;
};

std::vector<ShelfSection> BuildShelf(const keymap::Keymap &keymap);

}
}

#endif
