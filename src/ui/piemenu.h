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
    // Centred at the mouse; `openedBy` is the action that opened it, whose key closes it again.
    void Open(std::string title, std::vector<PieItem> items, std::string openedBy = "");
    const std::string &OpenedBy() const { return openedBy; }
    void Cancel() { cancelled = true; }
    bool IsOpen() const { return open; }
    const std::string          &Title() const { return title; }
    const std::vector<PieItem> &Items() const { return items; }
    float CenterX() const { return cx; } // window coordinates
    float CenterY() const { return cy; }
    bool HandleChord(const keymap::Chord &c);                 // consumes every chord while open
    // Draws when open. Returns the chosen item's command name (and closes), or "".
    std::string Draw();

private:
    bool                 open = false;
    std::string          title, openedBy;
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
