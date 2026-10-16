#include "solvespace.h"

#include "harness.h"

// Workplane coordinates -> screen coordinates for MouseLeftDown.
static Point2d Screen(double u, double v) {
    Entity *wp = SK.GetEntity(SS.GW.ActiveWorkplane());
    Vector  o  = SK.GetEntity(wp->point[0])->PointGetNum();
    Vector  p  = o.Plus(wp->Normal()->NormalU().ScaledBy(u))
                    .Plus(wp->Normal()->NormalV().ScaledBy(v));
    return SS.GW.ProjectPoint(p);
}

static void Click(double u, double v) {
    Point2d s = Screen(u, v);
    SS.GW.MouseLeftDown(s.x, s.y, false, false);
    SS.GW.MouseLeftUp(s.x, s.y, false, false);
}

static void BeginTool(Command c) {
    SS.showToolbar = false;
    SS.GW.LookOntoActiveWorkplane(false);
    SS.GW.ActivateCommand(c);
}

static int Count(Constraint::Type t) {
    int n = 0;
    for(Constraint &c : SK.constraint) {
        if(c.type == t) n++;
    }
    return n;
}

static double AngleDeg(hRequest hr) {
    Entity *wp = SK.GetEntity(SS.GW.ActiveWorkplane());
    Vector  d  = SK.GetEntity(hr.entity(2))->PointGetNum().Minus(
                 SK.GetEntity(hr.entity(1))->PointGetNum());
    double  a  = atan2(d.Dot(wp->Normal()->NormalV()), d.Dot(wp->Normal()->NormalU())) * 180 / PI;
    return a < 0 ? a + 360 : a;
}

static double Length(hRequest hr) {
    return SK.GetEntity(hr.entity(2))->PointGetNum().Minus(
           SK.GetEntity(hr.entity(1))->PointGetNum()).Magnitude();
}

TEST_CASE(typed_line_length_and_angle) {
    CHECK_LOAD("line.slvs");
    BeginTool(Command::LINE_SEGMENT);
    Click(300, 300);
    CHECK_TRUE(SS.GW.TypedInputKind() == GraphicsWindow::TypedKind::LINE);
    hRequest first = SS.GW.pending.request;
    int dist = Count(Constraint::Type::PT_PT_DISTANCE), ang = Count(Constraint::Type::ANGLE);
    CHECK_TRUE(SS.GW.CommitTypedLine(25.0, 30.0));
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_TRUE(Count(Constraint::Type::PT_PT_DISTANCE) == dist + 1);
    CHECK_TRUE(Count(Constraint::Type::ANGLE) == ang + 1);
    CHECK_EQ_EPS(Length(first), 25.0);
    CHECK_TRUE(fabs(AngleDeg(first) - 30.0) < 1e-6);
    // The chain continues from the new end point, as after a click.
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::DRAGGING_NEW_LINE_POINT);
    CHECK_TRUE(SS.GW.pending.request != first);
}

TEST_CASE(typed_right_angles_become_horizontal_vertical) {
    CHECK_LOAD("line.slvs");
    BeginTool(Command::LINE_SEGMENT);
    Click(300, 300);
    int h = Count(Constraint::Type::HORIZONTAL), v = Count(Constraint::Type::VERTICAL);
    int ang = Count(Constraint::Type::ANGLE);
    CHECK_TRUE(SS.GW.CommitTypedLine(10.0, 180.0));
    CHECK_TRUE(SS.GW.CommitTypedLine(10.0, 270.0));
    CHECK_TRUE(Count(Constraint::Type::HORIZONTAL) == h + 1);
    CHECK_TRUE(Count(Constraint::Type::VERTICAL) == v + 1);
    CHECK_TRUE(Count(Constraint::Type::ANGLE) == ang);
}

TEST_CASE(typed_obtuse_and_negative_angles) {
    CHECK_LOAD("line.slvs");
    BeginTool(Command::LINE_SEGMENT);
    Click(300, 300);
    hRequest a = SS.GW.pending.request;
    CHECK_TRUE(SS.GW.CommitTypedLine(20.0, 150.0));
    hRequest b = SS.GW.pending.request;
    CHECK_TRUE(SS.GW.CommitTypedLine(20.0, -45.0));
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_TRUE(fabs(AngleDeg(a) - 150.0) < 1e-6);
    CHECK_TRUE(fabs(AngleDeg(b) - 315.0) < 1e-6);
}

TEST_CASE(typed_length_only_keeps_direction) {
    CHECK_LOAD("line.slvs");
    BeginTool(Command::LINE_SEGMENT);
    Click(300, 300);
    hRequest first = SS.GW.pending.request;
    Point2d s = Screen(340, 300); // drag towards +u (headless: set the point directly)
    SK.GetEntity(first.entity(2))->PointForceTo(SS.GW.UnProjectPoint(s));
    CHECK_TRUE(SS.GW.CommitTypedLine(12.0, std::nullopt));
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_EQ_EPS(Length(first), 12.0);
    CHECK_TRUE(fabs(AngleDeg(first)) < 1e-6 || fabs(AngleDeg(first) - 360) < 1e-6);
}

TEST_CASE(typed_circle_diameter) {
    CHECK_LOAD("line.slvs");
    BeginTool(Command::CIRCLE);
    Click(300, 300);
    CHECK_TRUE(SS.GW.TypedInputKind() == GraphicsWindow::TypedKind::CIRCLE);
    hEntity circle = SS.GW.pending.circle;
    int d = Count(Constraint::Type::DIAMETER);
    CHECK_TRUE(SS.GW.CommitTypedCircle(40.0));
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_TRUE(Count(Constraint::Type::DIAMETER) == d + 1);
    CHECK_EQ_EPS(SK.GetEntity(circle)->CircleGetRadiusNum(), 20.0);
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::NONE);
}

TEST_CASE(typed_rectangle_size) {
    CHECK_LOAD("line.slvs");
    BeginTool(Command::RECTANGLE);
    Click(300, 300);
    CHECK_TRUE(SS.GW.TypedInputKind() == GraphicsWindow::TypedKind::RECTANGLE);
    hEntity corner = SS.GW.pending.point;
    int dist = Count(Constraint::Type::PT_PT_DISTANCE);
    CHECK_TRUE(SS.GW.CommitTypedRectangle(30.0, 20.0));
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_TRUE(Count(Constraint::Type::PT_PT_DISTANCE) == dist + 2);
    Entity *wp = SK.GetEntity(SS.GW.ActiveWorkplane());
    Vector  o  = SK.GetEntity(wp->point[0])->PointGetNum();
    Vector  p  = SK.GetEntity(corner)->PointGetNum().Minus(o);
    CHECK_EQ_EPS(fabs(p.Dot(wp->Normal()->NormalU()) - 300), 30.0);
    CHECK_EQ_EPS(fabs(p.Dot(wp->Normal()->NormalV()) - 300), 20.0);
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::NONE);
}

TEST_CASE(typed_input_outside_a_tool_is_refused) {
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(SS.GW.TypedInputKind() == GraphicsWindow::TypedKind::NONE);
    CHECK_FALSE(SS.GW.CommitTypedLine(10.0, 0.0));
    CHECK_FALSE(SS.GW.CommitTypedCircle(10.0));
}

TEST_CASE(typed_dimensions_get_a_label_offset) {
    CHECK_LOAD("line.slvs");
    BeginTool(Command::LINE_SEGMENT);
    Click(300, 300);
    CHECK_TRUE(SS.GW.CommitTypedLine(25.0, 30.0));
    int checked = 0;
    for(Constraint &c : SK.constraint) {
        if(c.type == Constraint::Type::PT_PT_DISTANCE || c.type == Constraint::Type::ANGLE) {
            CHECK_TRUE(c.disp.offset.Magnitude() > 1e-9);
            checked++;
        }
    }
    CHECK_TRUE(checked == 2);
}

static int ActiveDof() {
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    return SK.GetGroup(SS.GW.activeGroup)->solved.dof;
}

TEST_CASE(typed_angle_adds_no_extra_freedom) {
    CHECK_LOAD("line.slvs");
    BeginTool(Command::LINE_SEGMENT);
    Click(300, 300);
    CHECK_TRUE(SS.GW.CommitTypedLine(25.0, 90.0)); // vertical: no reference line
    SS.GW.ClearPending();
    int dofRight = ActiveDof();

    CHECK_LOAD("line.slvs");
    BeginTool(Command::LINE_SEGMENT);
    Click(300, 300);
    CHECK_TRUE(SS.GW.CommitTypedLine(25.0, 30.0)); // reference line + angle
    SS.GW.ClearPending();
    CHECK_TRUE(ActiveDof() == dofRight);
}

TEST_CASE(typed_rectangle_rejects_zero_and_negative_sizes) {
    CHECK_LOAD("line.slvs");
    BeginTool(Command::RECTANGLE);
    Click(300, 300);
    int n = SK.constraint.n;
    CHECK_FALSE(SS.GW.CommitTypedRectangle(0.0, 20.0));
    CHECK_FALSE(SS.GW.CommitTypedRectangle(30.0, -20.0));
    CHECK_TRUE(SK.constraint.n == n);
    CHECK_TRUE(SS.GW.TypedInputKind() == GraphicsWindow::TypedKind::RECTANGLE);
}
