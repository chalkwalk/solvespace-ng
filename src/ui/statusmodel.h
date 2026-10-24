//-----------------------------------------------------------------------------
// What the status/HUD bar shows: the active tool, the keys that work now, DOF and solver state.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_UI_STATUSMODEL_H
#define SOLVESPACE_UI_STATUSMODEL_H

#include <string>
#include <vector>
#include "keymap.h"

namespace SolveSpace {
namespace Ui {

struct KeyHint {
    std::string keys;  // "Shift+A"
    std::string label; // "Add"
};

struct StatusModel {
    std::string          tool;  // pending description or the modal tool's text
    std::vector<KeyHint> hints;
    std::string          group; // active group's name
    int                  dof = -1;
    std::string          solve; // "Solved", "Redundant", "Didn't converge", "Too many unknowns"
    bool                 solveProblem = false;
};

// `touch`: the last input was a touch screen, so the hints are gestures rather than keys.
StatusModel BuildStatus(const keymap::Keymap &km, const std::string &modalTool,
                        const std::vector<KeyHint> &modalHints, bool touch = false);

}
}

#endif
