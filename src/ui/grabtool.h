//-----------------------------------------------------------------------------
// The G grab tool: keys and HUD text over GraphicsWindow's grab mode.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_UI_GRABTOOL_H
#define SOLVESPACE_UI_GRABTOOL_H

#include "ui/statusmodel.h"
#include "ui/typedfields.h"

namespace SolveSpace {
namespace Ui {

class GrabTool {
public:
    bool Start();  // grabs at the current mouse position; false if there is nothing to grab
    bool Active() const;
    // X/Y/Z lock an axis, typed digits set the distance, Enter confirms, Esc cancels; every
    // other chord without Ctrl is swallowed while grabbing.
    bool HandleChord(const keymap::Chord &c);
    std::string          Description() const; // "Grab · X · 12"
    std::vector<KeyHint> Hints() const;

private:
    TypedFields field{ { "distance" } };
};

GrabTool &Grab();

}
}

#endif
