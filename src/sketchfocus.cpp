//-----------------------------------------------------------------------------
// Tab: focus the sketch that owns the selection (activate its group, look onto its workplane),
// and Tab again to go back. Blender's Edit Mode, SolveSpace-style.
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {

void GraphicsWindow::ActivateGroup(hGroup hg) {
    activeGroup = hg;
    SK.GetGroup(hg)->Activate();
    ClearSuper();
    SS.GenerateAll();
}

void GraphicsWindow::LookOntoActiveWorkplane(bool animate) {
    if(!LockedInWorkplane()) return;
    if(animate && window) {
        AnimateOntoWorkplane();
        return;
    }
    Entity    *w = SK.GetEntity(ActiveWorkplane());
    Quaternion q = w->Normal()->NormalGetNum();
    projRight    = q.RotationU();
    projUp       = q.RotationV();
    offset       = SK.GetEntity(w->point[0])->PointGetNum().ScaledBy(-1);
    Invalidate();
}

bool GraphicsWindow::ToggleSketchFocus(bool animate) {
    if(sketchFocusReturn.active) {
        sketchFocusReturn.active = false;
        hGroup back = sketchFocusReturn.group;
        if(!SK.group.FindByIdNoOops(back)) back = SK.groupOrder[SK.groupOrder.n - 1];
        if(back != activeGroup) ActivateGroup(back);
        if(animate && window) {
            AnimateOnto(sketchFocusReturn.view, sketchFocusReturn.offset);
        } else {
            projRight = sketchFocusReturn.view.RotationU();
            projUp    = sketchFocusReturn.view.RotationV();
            offset    = sketchFocusReturn.offset;
            Invalidate();
        }
        return true;
    }

    hGroup              target = activeGroup;
    std::vector<hGroup> groups = GroupsOfSelection();
    if(!groups.empty()) target = groups[0];
    if(target == activeGroup && !LockedInWorkplane()) return false;

    sketchFocusReturn.active = true;
    sketchFocusReturn.group  = activeGroup;
    sketchFocusReturn.view   = Quaternion::From(projRight, projUp);
    sketchFocusReturn.offset = offset;
    if(target != activeGroup) ActivateGroup(target);
    LookOntoActiveWorkplane(animate);
    return true;
}

} // namespace SolveSpace
