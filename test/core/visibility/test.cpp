#include "solvespace.h"

#include "harness.h"

// The first line segment in the given group.
static hEntity LineIn(hGroup hg) {
    for(Entity &e : SK.entity) {
        if(e.group == hg && e.type == Entity::Type::LINE_SEGMENT) return e.h;
    }
    return Entity::NO_ENTITY;
}

// The first group after #references that owns a line segment.
static hGroup SketchGroup() {
    for(hGroup hg : SK.groupOrder) {
        if(hg != Group::HGROUP_REFERENCES && LineIn(hg).v) return hg;
    }
    return {};
}

TEST_CASE(hide_selected_then_show_all) {
    CHECK_LOAD("groups.slvs");
    hGroup sketch = SketchGroup();
    CHECK_TRUE(sketch.v != 0);
    SS.GW.ClearSelection();
    SS.GW.MakeSelected(LineIn(sketch));
    CHECK_TRUE(SS.GW.HideSelectedGroups() == 1);
    CHECK_FALSE(SK.GetGroup(sketch)->visible);
    CHECK_TRUE(SS.GW.selection.n == 0);
    SS.GW.ShowAllGroups();
    for(hGroup hg : SK.groupOrder) CHECK_TRUE(SK.GetGroup(hg)->visible);
}

TEST_CASE(isolate_keeps_selection_groups_and_references) {
    CHECK_LOAD("groups.slvs");
    hGroup sketch = SketchGroup();
    SS.GW.ClearSelection();
    SS.GW.MakeSelected(LineIn(sketch));
    CHECK_TRUE(SS.GW.IsolateSelectedGroups() == 1);
    for(hGroup hg : SK.groupOrder) {
        bool keep = hg == sketch || hg == Group::HGROUP_REFERENCES;
        CHECK_TRUE(SK.GetGroup(hg)->visible == keep);
    }
}

TEST_CASE(hide_and_isolate_without_target_do_nothing) {
    CHECK_LOAD("groups.slvs");
    SS.GW.ClearSelection();
    SS.GW.hover.Clear();
    CHECK_TRUE(SS.GW.HideSelectedGroups() == 0);
    CHECK_TRUE(SS.GW.IsolateSelectedGroups() == 0);
    for(hGroup hg : SK.groupOrder) CHECK_TRUE(SK.GetGroup(hg)->visible);
}

TEST_CASE(hide_uses_hover_when_nothing_selected) {
    CHECK_LOAD("groups.slvs");
    hGroup sketch = SketchGroup();
    SS.GW.ClearSelection();
    SS.GW.hover.Clear();
    SS.GW.hover.entity = LineIn(sketch);
    CHECK_TRUE(SS.GW.HideSelectedGroups() == 1);
    CHECK_FALSE(SK.GetGroup(sketch)->visible);
}
