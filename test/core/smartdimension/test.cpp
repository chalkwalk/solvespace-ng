#include "solvespace.h"

#include "harness.h"

static int Count(Constraint::Type t) {
    int n = 0;
    for(Constraint &c : SK.constraint) {
        if(c.type == t) n++;
    }
    return n;
}

// A point in the active workplane, by workplane coordinates.
static Vector At(double u, double v) {
    Entity *wp = SK.GetEntity(SS.GW.ActiveWorkplane());
    Vector  o  = SK.GetEntity(wp->point[0])->PointGetNum();
    return o.Plus(wp->Normal()->NormalU().ScaledBy(u)).Plus(wp->Normal()->NormalV().ScaledBy(v));
}

static hRequest AddLine(double u0, double v0, double u1, double v1) {
    hRequest hr = SS.GW.AddRequest(Request::Type::LINE_SEGMENT, false);
    SK.GetEntity(hr.entity(1))->PointForceTo(At(u0, v0));
    SK.GetEntity(hr.entity(2))->PointForceTo(At(u1, v1));
    return hr;
}

TEST_CASE(two_lines_get_an_angle) {
    CHECK_LOAD("line.slvs");
    SS.immediatelyEditDimension = false;
    hRequest a = AddLine(100, 100, 160, 120), b = AddLine(100, 200, 130, 260);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    int before = Count(Constraint::Type::ANGLE);
    SS.GW.ClearSelection();
    SS.GW.MakeSelected(a.entity(0));
    SS.GW.MakeSelected(b.entity(0));
    SS.GW.SmartDimension();
    CHECK_TRUE(Count(Constraint::Type::ANGLE) == before + 1);
}

TEST_CASE(one_line_gets_a_length) {
    CHECK_LOAD("line.slvs");
    SS.immediatelyEditDimension = false;
    hRequest a = AddLine(100, 100, 160, 120);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    int before = Count(Constraint::Type::PT_PT_DISTANCE);
    SS.GW.ClearSelection();
    SS.GW.MakeSelected(a.entity(0));
    SS.GW.SmartDimension();
    CHECK_TRUE(Count(Constraint::Type::PT_PT_DISTANCE) == before + 1);
}

TEST_CASE(circle_gets_a_diameter) {
    CHECK_LOAD("line.slvs");
    SS.immediatelyEditDimension = false;
    hRequest c = SS.GW.AddRequest(Request::Type::CIRCLE, false);
    SK.GetEntity(c.entity(1))->PointForceTo(At(300, 100));
    SK.GetEntity(c.entity(64))->DistanceForceTo(20);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    int before = Count(Constraint::Type::DIAMETER);
    SS.GW.ClearSelection();
    SS.GW.MakeSelected(c.entity(0));
    SS.GW.SmartDimension();
    CHECK_TRUE(Count(Constraint::Type::DIAMETER) == before + 1);
}

TEST_CASE(smart_dimension_uses_hover) {
    CHECK_LOAD("line.slvs");
    SS.immediatelyEditDimension = false;
    hRequest a = AddLine(100, 100, 160, 120);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    int before = SK.constraint.n;
    SS.GW.ClearSelection();
    SS.GW.hover.Clear();
    SS.GW.hover.entity = a.entity(0);
    SS.GW.SmartDimension();
    CHECK_TRUE(SK.constraint.n == before + 1);
}
