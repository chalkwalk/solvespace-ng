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

void ViewBasisFor(Vector toViewer, Vector *right, Vector *up) {
    Vector d = toViewer.WithMagnitude(1);
    Vector z = Vector::From(0, 0, 1);
    Vector u = z.Minus(d.ScaledBy(z.Dot(d)));
    if(u.Magnitude() < 1e-6) {
        // Straight down (or up) the Z axis: +Y up from above, -Y up from below.
        u = Vector::From(0, d.z > 0 ? 1 : -1, 0);
    }
    u      = u.WithMagnitude(1);
    *up    = u;
    *right = u.Cross(d);
}

void GraphicsWindow::SnapToDirection(Vector toViewer, bool animate) {
    Vector right, up;
    ViewBasisFor(toViewer, &right, &up);
    if(animate && window) {
        AnimateOnto(Quaternion::From(right, up), offset);
    } else {
        projRight = right;
        projUp    = up;
        Invalidate();
    }
    SS.ScheduleShowTW();
}

void GraphicsWindow::OrbitBy(double dx, double dy) {
    // As a middle-button drag of (dx, dy) pixels orbits (MouseMoved).
    double s = 0.3 * (PI / 180) * (SS.cameraNav ? -1.0 : 1.0); // radians per pixel
    if(SS.turntableNav) {
        // Z stays vertical: turn about Z, then tilt about the new screen right.
        projRight = projRight.RotatedAbout({ 0, 0, 1 }, -s * dx);
        projUp    = projUp.RotatedAbout({ 0, 0, 1 }, -s * dx).RotatedAbout(projRight, s * dy);
    } else {
        Vector r = projRight, u = projUp;
        projRight = r.RotatedAbout(u, -s * dx);
        projUp    = u.RotatedAbout(r, s * dy);
    }
    NormalizeProjectionVectors();
    Invalidate();
    SS.ScheduleShowTW();
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
