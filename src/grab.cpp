//-----------------------------------------------------------------------------
// G: grab the selection and move it under the solver without holding a mouse button, with
// X/Y/Z axis locks and a typed distance. Confirm with a click or Enter; Esc or right-click
// cancels and restores the sketch exactly.
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {

Vector GraphicsWindow::GrabAxisVector(int axis) {
    if(LockedInWorkplane()) {
        EntityBase *n = SK.GetEntity(ActiveWorkplane())->Normal();
        switch(axis) {
            case 0: return n->NormalU();
            case 1: return n->NormalV();
            case 2: return n->NormalN();
        }
    }
    switch(axis) {
        case 0: return Vector::From(1, 0, 0);
        case 1: return Vector::From(0, 1, 0);
        case 2: return Vector::From(0, 0, 1);
    }
    ssassert(false, "Unexpected grab axis");
}

bool GraphicsWindow::StartGrab(Point2d mouse) {
    if(pending.operation != Pending::NONE) return false;
    SelectHoveredIfNothingSelected();
    pending.points.Clear();
    for(const Selection &s : selection) {
        if(s.entity.v) StartDraggingByEntity(s.entity);
    }
    if(pending.points.n == 0) return false;

    grab.wasUnsaved = SS.unsaved;
    SS.UndoRemember();
    grab.startMouse = mouse;
    grab.lastMouse  = mouse;
    grab.axis       = -1;
    grab.typedMm.reset();
    grab.start.clear();
    for(hEntity h : pending.points) grab.start.push_back({ h, SK.GetEntity(h)->PointGetNum() });
    pending.operation   = Pending::GRABBING;
    pending.description = _("grab: move the mouse; X/Y/Z lock an axis; type a distance");
    return true;
}

void GraphicsWindow::GrabMouseMoved(Point2d mouse) {
    if(pending.operation != Pending::GRABBING) return;
    Vector delta;
    if(grab.axis >= 0 && grab.typedMm) {
        delta = GrabAxisVector(grab.axis).ScaledBy(*grab.typedMm);
    } else {
        delta = projRight.ScaledBy((mouse.x - grab.startMouse.x) / scale)
                    .Plus(projUp.ScaledBy((mouse.y - grab.startMouse.y) / scale));
        if(grab.axis >= 0) {
            Vector ax = GrabAxisVector(grab.axis);
            delta     = ax.ScaledBy(delta.Dot(ax));
        }
    }
    for(auto &[h, p0] : grab.start) {
        SK.GetEntity(h)->PointForceTo(p0.Plus(delta));
        SS.MarkGroupDirtyByEntity(h);
    }
    grab.lastMouse = mouse;
    Invalidate();
}

void GraphicsWindow::GrabSetAxis(int axis) {
    grab.axis = grab.axis == axis ? -1 : axis;
    GrabMouseMoved(grab.lastMouse);
}

void GraphicsWindow::GrabSetTyped(std::optional<double> mm) {
    grab.typedMm = mm;
    GrabMouseMoved(grab.lastMouse);
}

void GraphicsWindow::GrabConfirm() {
    if(pending.operation != Pending::GRABBING) return;
    ClearPending();
    grab.start.clear();
    Invalidate();
}

void GraphicsWindow::GrabCancel() {
    if(pending.operation != Pending::GRABBING) return;
    ClearPending();
    grab.start.clear();
    // StartGrab remembered the state just before the grab; return to it and forget the redo
    // entry the undo would leave.
    SS.UndoUndo();
    SS.UndoClearStack(&SS.redo);
    SS.UndoEnableMenus();
    SS.unsaved = grab.wasUnsaved;
}

} // namespace SolveSpace
