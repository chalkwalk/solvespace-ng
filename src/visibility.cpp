//-----------------------------------------------------------------------------
// Hide / unhide all / isolate, at group level (SolveSpace has no per-entity hide).
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {

std::vector<hGroup> GraphicsWindow::GroupsOfSelection() {
    SelectHoveredIfNothingSelected();
    std::vector<hGroup> out;
    auto add = [&](hGroup hg) {
        if(std::find(out.begin(), out.end(), hg) == out.end()) out.push_back(hg);
    };
    for(const Selection &s : selection) {
        if(s.entity.v) {
            if(Entity *e = SK.entity.FindByIdNoOops(s.entity)) add(e->group);
        }
        if(s.constraint.v) {
            if(Constraint *c = SK.constraint.FindByIdNoOops(s.constraint)) add(c->group);
        }
    }
    return out;
}

int GraphicsWindow::HideSelectedGroups() {
    std::vector<hGroup> groups = GroupsOfSelection();
    for(hGroup hg : groups) SK.GetGroup(hg)->visible = false;
    ClearSelection();
    if(!groups.empty()) {
        persistentDirty = true;
        Invalidate();
        SS.ScheduleShowTW();
    }
    return (int)groups.size();
}

void GraphicsWindow::ShowAllGroups() {
    for(hGroup hg : SK.groupOrder) SK.GetGroup(hg)->visible = true;
    persistentDirty = true;
    // Shown groups may never have been generated.
    SS.GenerateAll();
    Invalidate();
    SS.ScheduleShowTW();
}

int GraphicsWindow::IsolateSelectedGroups() {
    std::vector<hGroup> keep = GroupsOfSelection();
    if(keep.empty()) return 0;
    for(hGroup hg : SK.groupOrder) {
        bool k = hg == Group::HGROUP_REFERENCES ||
                 std::find(keep.begin(), keep.end(), hg) != keep.end();
        SK.GetGroup(hg)->visible = k;
    }
    ClearSelection();
    persistentDirty = true;
    SS.GenerateAll();
    Invalidate();
    SS.ScheduleShowTW();
    return (int)keep.size();
}

} // namespace SolveSpace
