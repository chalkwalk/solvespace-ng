//-----------------------------------------------------------------------------
// Preset views (numpad 1/3/7 and their opposites), the hover fallback for commands, and
// frame-selected.
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {

void ViewPresetBasis(ViewPreset p, Vector *right, Vector *up) {
    switch(p) {
        case ViewPreset::FRONT:  *right = Vector::From( 1, 0, 0); *up = Vector::From(0, 0, 1); return;
        case ViewPreset::BACK:   *right = Vector::From(-1, 0, 0); *up = Vector::From(0, 0, 1); return;
        case ViewPreset::RIGHT:  *right = Vector::From(0,  1, 0); *up = Vector::From(0, 0, 1); return;
        case ViewPreset::LEFT:   *right = Vector::From(0, -1, 0); *up = Vector::From(0, 0, 1); return;
        case ViewPreset::TOP:    *right = Vector::From( 1, 0, 0); *up = Vector::From(0,  1, 0); return;
        case ViewPreset::BOTTOM: *right = Vector::From( 1, 0, 0); *up = Vector::From(0, -1, 0); return;
    }
    ssassert(false, "Unexpected view preset");
}

void GraphicsWindow::SnapToView(ViewPreset p, bool animate) {
    Vector right, up;
    ViewPresetBasis(p, &right, &up);
    if(animate && window) {
        AnimateOnto(Quaternion::From(right, up), offset);
    } else {
        projRight = right;
        projUp    = up;
        Invalidate();
    }
    SS.ScheduleShowTW();
}

bool GraphicsWindow::SelectHoveredIfNothingSelected() {
    if(selection.n > 0 || hover.IsEmpty()) return false;
    MakeSelected(&hover);
    return true;
}

void GraphicsWindow::FrameSelected() {
    SelectHoveredIfNothingSelected();
    ZoomToFit(/*includingInvisibles=*/false, /*useSelection=*/true);
    Invalidate();
}

} // namespace SolveSpace
