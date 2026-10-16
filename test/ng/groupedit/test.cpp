#include "solvespace.h"

#include "harness.h"

static Group *Named(const char *name) {
    for(Group &g : SK.group) {
        if(g.name == name) return &g;
    }
    return nullptr;
}

static hGroup AddSketch3d() {
    SS.GW.ActivateCommand(Command::GROUP_3D);
    SS.Refresh();
    return SS.GW.activeGroup;
}

TEST_CASE(rename_group_rejects_empty_and_is_undoable) {
    CHECK_LOAD("line.slvs");
    hGroup hg = Named("sketch-in-plane")->h;
    CHECK_FALSE(SS.RenameGroup(hg, ""));
    CHECK_EQ_STR(SK.GetGroup(hg)->name, "sketch-in-plane");
    CHECK_TRUE(SS.RenameGroup(hg, "base"));
    CHECK_EQ_STR(SK.GetGroup(hg)->name, "base");
    SS.UndoUndo();
    CHECK_EQ_STR(SK.GetGroup(hg)->name, "sketch-in-plane");
}

TEST_CASE(toggle_group_visible_flips_and_marks_dirty) {
    CHECK_LOAD("line.slvs");
    hGroup hg = Named("sketch-in-plane")->h;
    SS.GW.ToggleGroupVisible(hg);
    CHECK_FALSE(SK.GetGroup(hg)->visible);
    SS.GW.ToggleGroupVisible(hg);
    CHECK_TRUE(SK.GetGroup(hg)->visible);
}

TEST_CASE(delete_active_group_activates_the_previous_one) {
    CHECK_LOAD("line.slvs");
    hGroup sketch = Named("sketch-in-plane")->h;
    hGroup added  = AddSketch3d();
    CHECK_TRUE(added != sketch);
    SS.GW.DeleteGroup(added);
    CHECK_TRUE(SK.group.FindByIdNoOops(added) == nullptr);
    CHECK_TRUE(SS.GW.activeGroup == sketch);
    SS.UndoUndo();
    CHECK_TRUE(SK.group.FindByIdNoOops(added) != nullptr);
}

TEST_CASE(isolate_groups_keeps_references) {
    CHECK_LOAD("line.slvs");
    hGroup sketch = Named("sketch-in-plane")->h;
    hGroup added  = AddSketch3d();
    SS.GW.IsolateGroups({ added });
    CHECK_TRUE(SK.GetGroup(Group::HGROUP_REFERENCES)->visible);
    CHECK_FALSE(SK.GetGroup(sketch)->visible);
    CHECK_TRUE(SK.GetGroup(added)->visible);
}
