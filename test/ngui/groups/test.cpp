// Tab (sketch focus), H / Alt+H / Shift+H (group visibility), E (extrude).
#include "../uitest.h"
#include "harness.h"

using namespace SolveSpace::UiTest;

static Group *Named(const char *name) {
    for(Group &g : SK.group) {
        if(g.name == name) return &g;
    }
    return nullptr;
}

// A closed rectangle in the sketch, extruded; returns the sketch group.
static hGroup RectangleThenExtrude() {
    Driver::Key("B");
    ClickAt(Point2d::From(450, 450));
    ClickAt(Point2d::From(560, 540));
    hGroup sketch = SS.GW.activeGroup;
    Driver::Key("E");
    return sketch;
}

TEST_CASE(e_extrudes_the_sketch_into_a_new_group) {
    CHECK_LOAD("line.slvs");
    int groups = SK.group.n;
    hGroup sketch = RectangleThenExtrude();
    CHECK_TRUE(SK.group.n == groups + 1);
    CHECK_TRUE(SS.GW.activeGroup != sketch);
    CHECK_TRUE(SK.GetGroup(SS.GW.activeGroup)->type == Group::Type::EXTRUDE);
}

// A new 3D sketch group after the fixture's sketch; returns the fixture's line.
static hRequest FixtureLineThenNewGroup() {
    hRequest line = *LastRequest(Request::Type::LINE_SEGMENT);
    Driver::ClickMenu({ "New Group", "Sketch in 3D" });
    return line;
}

TEST_CASE(tab_with_nothing_targeted_looks_onto_the_workplane_and_back) {
    CHECK_LOAD("line.slvs");
    Driver::Key("Keypad 1");
    Vector right = SS.GW.projRight, up = SS.GW.projUp;
    Driver::Move(200, 650);
    Driver::Key("Tab");
    Vector n = SS.GW.projRight.Cross(SS.GW.projUp);
    CHECK_TRUE(fabs(fabs(n.z) - 1) < 1e-6); // looking onto the XY workplane
    Driver::Key("Tab");
    CHECK_TRUE(SS.GW.projRight.Minus(right).Magnitude() < 1e-6);
    CHECK_TRUE(SS.GW.projUp.Minus(up).Magnitude() < 1e-6);
}

TEST_CASE(tab_focuses_the_hovered_sketch_and_tab_returns) {
    CHECK_LOAD("line.slvs");
    hRequest line = FixtureLineThenNewGroup();
    hGroup sketch = SK.GetRequest(line)->group, added = SS.GW.activeGroup;
    CHECK_TRUE(added != sketch);
    Point2d m = AtMiddle(line);
    Driver::Move(m.x, m.y);
    Driver::Key("Tab");
    CHECK_TRUE(SS.GW.activeGroup == sketch);
    Driver::Key("Tab");
    CHECK_TRUE(SS.GW.activeGroup == added);
}

TEST_CASE(h_hides_hovered_group_and_alt_h_shows_all) {
    CHECK_LOAD("line.slvs");
    hRequest line = DrawLine(Point2d::From(450, 520), Point2d::From(560, 520));
    Point2d m = AtMiddle(line);
    Driver::Move(m.x, m.y);
    Driver::Key("H");
    CHECK_FALSE(Named("sketch-in-plane")->visible);
    Driver::Key("Alt+H");
    CHECK_TRUE(Named("sketch-in-plane")->visible);
}

TEST_CASE(shift_h_isolates_the_hovered_group) {
    CHECK_LOAD("line.slvs");
    hRequest line = FixtureLineThenNewGroup();
    hGroup sketch = SK.GetRequest(line)->group;
    Point2d m = AtMiddle(line);
    Driver::Move(m.x, m.y);
    Driver::Key("Shift+H");
    for(Group &g : SK.group) {
        bool references = g.h == Group::HGROUP_REFERENCES;
        CHECK_TRUE(g.visible == (g.h == sketch || references));
    }
    Driver::Key("Alt+H");
    for(Group &g : SK.group) CHECK_TRUE(g.visible);
}

