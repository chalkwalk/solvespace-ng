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
    ClearSelection();
    IsolateGroups(keep);
    return (int)keep.size();
}

void GraphicsWindow::IsolateGroups(const std::vector<hGroup> &keep) {
    for(hGroup hg : SK.groupOrder) {
        bool k = hg == Group::HGROUP_REFERENCES ||
                 std::find(keep.begin(), keep.end(), hg) != keep.end();
        SK.GetGroup(hg)->visible = k;
    }
    persistentDirty = true;
    SS.GenerateAll();
    Invalidate();
    SS.ScheduleShowTW();
}

void GraphicsWindow::ToggleGroupVisible(hGroup hg) {
    Group *g   = SK.GetGroup(hg);
    g->visible = !g->visible;
    persistentDirty = true;
    // A group that was just shown may never have been generated.
    SS.GenerateAll();
    Invalidate();
    SS.ScheduleShowTW();
}

void GraphicsWindow::DeleteGroup(hGroup hg) {
    SS.UndoRemember();
    if(hg == activeGroup) activeGroup = SK.GetGroup(activeGroup)->PreviousGroup()->h;
    // The property browser may be showing the group.
    SS.TW.ClearSuper();
    // A major change: solve everything again.
    SK.group.RemoveById(hg);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    // Also recreates the default group if it was removed.
    ClearSuper();
}

bool SolveSpaceUI::RenameGroup(hGroup hg, const std::string &name) {
    if(name.empty()) {
        Error(_("Group name cannot be empty"));
        return false;
    }
    UndoRemember();
    SK.GetGroup(hg)->name = name;
    ScheduleShowTW();
    return true;
}

} // namespace SolveSpace
