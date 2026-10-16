//-----------------------------------------------------------------------------
// A radial (pie) menu drawn over the viewport, Blender-style.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_UI_PIEMENU_H
#define SOLVESPACE_UI_PIEMENU_H

#include <string>
#include <vector>
#include "keymap.h"

namespace SolveSpace {
namespace Ui {

struct PieItem {
    std::string label;
    std::string commandName; // core command or ui.* action
    bool        enabled = true;
};

class PieMenu {
public:
    void Open(std::string title, std::vector<PieItem> items); // centred at the mouse
    bool IsOpen() const { return open; }
    bool HandleChord(const keymap::Chord &c);                 // consumes every chord while open
    // Draws when open. Returns the chosen item's command name (and closes), or "".
    std::string Draw();

private:
    bool                 open = false;
    std::string          title;
    std::vector<PieItem> items;
    float                cx = 0, cy = 0;
    int                  highlighted = -1;
    int                  picked      = -1;
    bool                 cancelled   = false;
};

PieMenu &Pie();

}
}

#endif
