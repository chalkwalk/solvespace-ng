#include "solvespace.h"

#include "harness.h"

static hEntity LineIn(hGroup hg) {
    for(Entity &e : SK.entity) {
        if(e.group == hg && e.type == Entity::Type::LINE_SEGMENT) return e.h;
    }
    return Entity::NO_ENTITY;
}

static hGroup FirstSketch() {
    for(hGroup hg : SK.groupOrder) {
        if(hg != Group::HGROUP_REFERENCES && LineIn(hg).v) return hg;
    }
    return {};
}

TEST_CASE(tab_into_group_of_selection_and_back) {
    CHECK_LOAD("groups.slvs");
    hGroup last = SK.groupOrder[SK.groupOrder.n - 1];
    SS.GW.ActivateGroup(last);
    Vector right0 = SS.GW.projRight;
    hGroup sketch = FirstSketch();
    CHECK_TRUE(sketch != last);
    SS.GW.ClearSelection();
    SS.GW.MakeSelected(LineIn(sketch));
    CHECK_TRUE(SS.GW.ToggleSketchFocus(/*animate=*/false));
    CHECK_TRUE(SS.GW.activeGroup == sketch);
    CHECK_TRUE(SS.GW.sketchFocusReturn.active);
    if(SS.GW.LockedInWorkplane()) {
        Entity *wp = SK.GetEntity(SS.GW.ActiveWorkplane());
        Vector u = wp->Normal()->NormalU();
        CHECK_TRUE(SS.GW.projRight.Minus(u).Magnitude() < 1e-6);
    }
    CHECK_TRUE(SS.GW.ToggleSketchFocus(false));
    CHECK_TRUE(SS.GW.activeGroup == last);
    CHECK_TRUE(SS.GW.projRight.Minus(right0).Magnitude() < 1e-6);
    CHECK_FALSE(SS.GW.sketchFocusReturn.active);
}

TEST_CASE(tab_with_nothing_targets_active_workplane) {
    CHECK_LOAD("groups.slvs");
    hGroup sketch = FirstSketch();
    SS.GW.ActivateGroup(sketch);
    SS.GW.ClearSelection();
    SS.GW.hover.Clear();
    SS.GW.SnapToView(ViewPreset::FRONT, false);
    CHECK_TRUE(SS.GW.ToggleSketchFocus(false));
    CHECK_TRUE(SS.GW.activeGroup == sketch);
    CHECK_TRUE(SS.GW.sketchFocusReturn.active);
    CHECK_TRUE(SS.GW.ToggleSketchFocus(false)); // back to the front view
    CHECK_TRUE(SS.GW.projUp.Minus(Vector::From(0, 0, 1)).Magnitude() < 1e-6);
}

TEST_CASE(tab_back_after_group_deleted_falls_back) {
    CHECK_LOAD("groups.slvs");
    hGroup last = SK.groupOrder[SK.groupOrder.n - 1];
    SS.GW.ActivateGroup(last);
    hGroup sketch = FirstSketch();
    SS.GW.ClearSelection();
    SS.GW.MakeSelected(LineIn(sketch));
    CHECK_TRUE(SS.GW.ToggleSketchFocus(false));
    // The remembered group disappears (as after an undo or a delete).
    SS.GW.sketchFocusReturn.group = { 0x7fffffff };
    CHECK_TRUE(SS.GW.ToggleSketchFocus(false));
    CHECK_TRUE(SS.GW.activeGroup == SK.groupOrder[SK.groupOrder.n - 1]);
}

TEST_CASE(tab_never_activates_the_references) {
    // Activating #references would hide every group after it.
    CHECK_LOAD("groups.slvs");
    hGroup last = SK.groupOrder[SK.groupOrder.n - 1];
    SS.GW.ActivateGroup(last);
    SS.GW.ClearSelection();
    SS.GW.MakeSelected(Request::HREQUEST_REFERENCE_XY.entity(0));
    SS.GW.ToggleSketchFocus(false);
    CHECK_TRUE(SS.GW.activeGroup == last);
}

TEST_CASE(tab_while_drawing_keeps_the_group) {
    CHECK_LOAD("groups.slvs");
    hGroup last = SK.groupOrder[SK.groupOrder.n - 1];
    SS.GW.ActivateGroup(last);
    SS.GW.ClearSelection();
    SS.GW.MakeSelected(LineIn(FirstSketch()));
    SS.GW.pending.operation = GraphicsWindow::Pending::DRAGGING_NEW_ARC_POINT;
    CHECK_FALSE(SS.GW.ToggleSketchFocus(false));
    CHECK_TRUE(SS.GW.activeGroup == last);
    SS.GW.pending.operation = GraphicsWindow::Pending::NONE;
}
