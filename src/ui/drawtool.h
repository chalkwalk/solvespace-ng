//-----------------------------------------------------------------------------
// Typed values while drawing lines, rectangles and circles: HUD fields that become
// constraints on Enter.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_UI_DRAWTOOL_H
#define SOLVESPACE_UI_DRAWTOOL_H

#include "ui/statusmodel.h"
#include "ui/typedfields.h"

namespace SolveSpace {
namespace Ui {

class DrawTool {
public:
    bool Active(); // the pending drawing operation accepts typed values
    // Typed-field keys and Tab/Shift+Tab; Enter applies when a field is filled; Esc clears
    // filled fields, else falls through to the keymap (which cancels drawing).
    bool HandleChord(const keymap::Chord &c);
    std::string          Description(); // "Line · length 25 · angle 30_"
    std::vector<KeyHint> Hints();

private:
    void                      Sync(); // new fields whenever the tool or its target changes
    TypedFields               fields{ std::vector<std::string>{} };
    GraphicsWindow::TypedKind lastKind   = GraphicsWindow::TypedKind::NONE;
    uint32_t                  lastTarget = 0;
};

DrawTool &Draw();

}
}

#endif
