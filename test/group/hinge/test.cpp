// M5's "done when" (AGENTS.md section 8): a printed hinge turns on its joint and reports its
// clearance. box.slvs: a 10 x 20 x 30 box (part 1).
#include "solvespace.h"

#include "harness.h"

#include "../features.h"

#if defined(HAVE_OCCT)
namespace {

hRequest AddLine(Vector a, Vector b) {
    hRequest hr = SS.GW.AddRequest(Request::Type::LINE_SEGMENT, /*rememberForUndo=*/false);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    SK.GetEntity(hr.entity(1))->PointForceTo(a);
    SK.GetEntity(hr.entity(2))->PointForceTo(b);
    return hr;
}

void Rectangle(Vector a, Vector c) {
    Vector   p[4] = { a, { c.x, a.y, a.z }, c, { a.x, c.y, a.z } };
    hRequest side[4];
    for(int i = 0; i < 4; i++) side[i] = AddLine(p[i], p[(i + 1) % 4]);
    for(int i = 0; i < 4; i++) {
        Constraint::ConstrainCoincident(side[i].entity(2), side[(i + 1) % 4].entity(1));
    }
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
}

hEntity Circle(Vector centre, double r, bool construction = false) {
    hRequest c = SS.GW.AddRequest(Request::Type::CIRCLE, /*rememberForUndo=*/false);
    SK.GetRequest(c)->construction = construction;
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    SK.GetEntity(c.entity(1))->PointForceTo(centre);
    SK.GetEntity(c.entity(64))->DistanceForceTo(r);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    return c.entity(0);
}

// The active sketch extruded 30 down from the box's top, joined or cut.
Group *ExtrudeDown(Group::CombineAs combine) {
    Group::MenuGroup(Command::GROUP_EXTRUDE);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Group *e       = SK.GetGroup(SS.GW.activeGroup);
    e->meshCombine = combine;
    SK.GetParam(e->h.param(0))->val = 0;
    SK.GetParam(e->h.param(1))->val = 0;
    SK.GetParam(e->h.param(2))->val = -15;
    e->clean = false;
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    return SK.GetGroup(SS.GW.activeGroup);
}

void TopView() {
    SS.GW.projRight = Vector::From(1, 0, 0);
    SS.GW.projUp    = Vector::From(0, 1, 0);
}

}

TEST_CASE(a_pin_turns_in_its_hole_with_the_clearance_drawn) {
    CHECK_LOAD("box.slvs");
    SS.SetKernel(KernelKind::OCCT);
    TopView();
    // The knuckle: the box with a hole of 5.4 through it.
    Select({ Cap(Group::REMAP_TOP) });
    Group::MenuGroup(Command::GROUP_WRKPL);
    hEntity hole = Circle({ 5, 10, 30 }, 2.7);
    Group  *cut  = ExtrudeDown(Group::CombineAs::DIFFERENCE);
    MassProps mp;
    CHECK_TRUE(SS.MassProperties(cut, &mp));
    CHECK_TRUE(fabs(mp.volume - (6000 - PI * 2.7 * 2.7 * 30)) < 1e-6);
    // The pin, 5 across, a part of its own drawn in the hole.
    Select({ Cap(Group::REMAP_TOP) });
    hPart  pin  = SS.NewPart();
    hEntity rod = Circle({ 5, 10, 30 }, 2.5);
    ExtrudeDown(Group::CombineAs::UNION);
    CHECK_TRUE(SS.MassProperties(SS.PartSolid(pin), &mp));
    CHECK_TRUE(fabs(mp.volume - PI * 2.5 * 2.5 * 30) < 1e-6);
    // Hinged about the hole's axis.
    Select({ hole, rod });
    hJoint hj = SS.MakeJoint(Joint::Type::REVOLUTE);
    CHECK_TRUE(hj.v != 0 && SK.GetJoint(hj)->child == pin);
    SolveSpaceUI::ClearanceSweep sweep = SS.SweepClearance(hj, 12);
    CHECK_TRUE(sweep.ok);
    CHECK_TRUE(sweep.collisions.empty());
    CHECK_TRUE(fabs(sweep.gap - 0.2) < 1e-3);
    CHECK_TRUE(sweep.text.find("Minimum gap 0.20") == 0);
    // Turned, it still clears by as much; the sweep leaves the joint where it was.
    SS.SetJointValue(hj, 47);
    CHECK_TRUE(fabs(SS.SweepClearance(hj, 12).gap - 0.2) < 1e-3);
    CHECK_EQ_EPS(SK.GetJoint(hj)->value, 47);
}

TEST_CASE(a_swinging_block_reports_where_it_hits) {
    CHECK_LOAD("box.slvs");
    SS.SetKernel(KernelKind::OCCT);
    TopView();
    // An axis at the box's corner (10, 0), drawn in each part as a construction circle.
    Select({ Cap(Group::REMAP_TOP) });
    Group::MenuGroup(Command::GROUP_WRKPL);
    hEntity axisA = Circle({ 10, 0, 30 }, 1, /*construction=*/true);
    // A 4 x 4 block 2 beyond the box's right side, turning about that corner.
    Select({ Cap(Group::REMAP_TOP) });
    SS.NewPart();
    hEntity axisB = Circle({ 10, 0, 30 }, 1, /*construction=*/true);
    Rectangle({ 12, 0, 30 }, { 16, 4, 30 });
    ExtrudeDown(Group::CombineAs::UNION);
    Select({ axisA, axisB });
    hJoint hj = SS.MakeJoint(Joint::Type::REVOLUTE);
    CHECK_TRUE(hj.v != 0);
    SolveSpaceUI::ClearanceSweep sweep = SS.SweepClearance(hj, 36);
    CHECK_TRUE(sweep.ok);
    CHECK_EQ_EPS(sweep.gap, 0);
    // Turned a quarter, the block is inside the box; not at the start, 2 clear.
    bool quarter = false, start = false;
    for(const auto &c : sweep.collisions) {
        quarter = quarter || (c.first <= 90 && 90 <= c.second);
        start   = start || c.first <= 0;
    }
    CHECK_TRUE(quarter);
    CHECK_FALSE(start);
    CHECK_TRUE(sweep.text.find("touch or overlap from") != std::string::npos);
    // Limited to turning away from the box, it clears by 2 (its corner circles the box's).
    SS.SetJointLimits(hj, true, -60, 0);
    sweep = SS.SweepClearance(hj, 13);
    CHECK_TRUE(sweep.collisions.empty());
    CHECK_TRUE(fabs(sweep.gap - 2) < 1e-6);
}
#endif
