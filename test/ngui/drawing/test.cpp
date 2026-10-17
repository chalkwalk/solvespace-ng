// Drawing with keys, the Add pie and typed values, as a person does it.
#include "../uitest.h"
#include "ui/drawtool.h"
#include "ui/piemenu.h"
#include "harness.h"

using namespace SolveSpace::UiTest;

static double Length(hRequest line) {
    Vector a = SK.GetEntity(line.entity(1))->PointGetNum();
    Vector b = SK.GetEntity(line.entity(2))->PointGetNum();
    return a.Minus(b).Magnitude();
}

TEST_CASE(add_pie_picks_a_tool_with_the_mouse) {
    CHECK_LOAD("line.slvs");
    Driver::Move(500, 500);
    Driver::Key("Shift+A");
    CHECK_TRUE(Ui::Pie().IsOpen());
    CHECK_TRUE(Ui::Pie().Title() == "Add");
    CHECK_TRUE(PickPie("Circle"));
    CHECK_FALSE(Ui::Pie().IsOpen());
    CHECK_TRUE(SS.GW.pending.command == Command::CIRCLE);
}

TEST_CASE(add_pie_picks_a_tool_with_a_number_key) {
    CHECK_LOAD("line.slvs");
    Driver::Move(500, 500);
    Driver::Key("Shift+A");
    Driver::Key("2"); // Rectangle
    CHECK_FALSE(Ui::Pie().IsOpen());
    CHECK_TRUE(SS.GW.pending.command == Command::RECTANGLE);
}

TEST_CASE(pie_escape_and_right_click_cancel) {
    CHECK_LOAD("line.slvs");
    Driver::Move(500, 500);
    Driver::Key("Shift+A");
    Driver::Key("Escape");
    CHECK_FALSE(Ui::Pie().IsOpen());
    Driver::Key("Shift+A");
    Driver::Click(520, 520, Driver::Button::RIGHT);
    CHECK_FALSE(Ui::Pie().IsOpen());
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::NONE);
}

TEST_CASE(pie_closes_on_a_click_in_its_centre) {
    CHECK_LOAD("line.slvs");
    Driver::Move(500, 500);
    Driver::Key("Shift+A");
    CHECK_TRUE(Ui::Pie().IsOpen());
    Driver::Click(Ui::Pie().CenterX(), Ui::Pie().CenterY());
    CHECK_FALSE(Ui::Pie().IsOpen());
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::NONE);
}

TEST_CASE(pie_key_again_closes_it_but_holding_it_does_not) {
    CHECK_LOAD("line.slvs");
    Driver::Move(500, 500);
    Driver::KeyRepeat("Shift+A", 3); // held: opens once and stays open
    CHECK_TRUE(Ui::Pie().IsOpen());
    Driver::Key("Shift+A");
    CHECK_FALSE(Ui::Pie().IsOpen());
    Driver::Key("`");
    CHECK_TRUE(Ui::Pie().IsOpen() && Ui::Pie().Title() == "View");
    Driver::Key("F"); // another pie's key is swallowed, not a toggle
    CHECK_TRUE(Ui::Pie().IsOpen() && Ui::Pie().Title() == "View");
    Driver::Key("`");
    CHECK_FALSE(Ui::Pie().IsOpen());
}

TEST_CASE(pie_click_does_not_reach_the_canvas) {
    CHECK_LOAD("line.slvs");
    int before = SK.request.n;
    Driver::Key("L");
    Driver::Move(500, 500);
    Driver::Key("Shift+A");
    CHECK_TRUE(PickPie("Circle"));
    CHECK_TRUE(SK.request.n == before); // the pick did not also start a line
    CHECK_TRUE(SS.GW.pending.command == Command::CIRCLE);
}

TEST_CASE(typed_length_and_angle_make_constraints) {
    CHECK_LOAD("line.slvs");
    Driver::Key("L");
    ClickAt(Point2d::From(500, 500));
    Driver::Move(620, 460);
    CHECK_TRUE(Ui::Draw().Active());
    Driver::Type("25");
    Driver::Key("Tab");
    Driver::Type("30");
    Driver::Key("Return");
    Driver::Key("Escape");
    Driver::Key("Escape");
    // The drawn line is the one with the 25 mm length constraint.
    std::optional<hConstraint> len = LastConstraint(Constraint::Type::PT_PT_DISTANCE);
    std::optional<hConstraint> ang = LastConstraint(Constraint::Type::ANGLE);
    CHECK_TRUE(len.has_value() && ang.has_value());
    CHECK_EQ_EPS(SK.GetConstraint(*len)->valA, 25.0);
    CHECK_EQ_EPS(SK.GetConstraint(*ang)->valA, 30.0);
    Constraint *c = SK.GetConstraint(*len);
    Vector a = SK.GetEntity(c->ptA)->PointGetNum(), b = SK.GetEntity(c->ptB)->PointGetNum();
    CHECK_EQ_EPS(a.Minus(b).Magnitude(), 25.0);
    double deg = atan2(fabs(b.y - a.y), fabs(b.x - a.x)) * 180 / PI;
    CHECK_TRUE(fabs(deg - 30) < 1e-6);
}

TEST_CASE(typed_right_angle_makes_a_vertical_line) {
    CHECK_LOAD("line.slvs");
    int vertical = CountConstraints(Constraint::Type::VERTICAL);
    Driver::Key("L");
    ClickAt(Point2d::From(500, 500));
    Driver::Move(520, 400);
    Driver::Type("10");
    Driver::Key("Tab");
    Driver::Type("90");
    Driver::Key("Return");
    Driver::Key("Escape");
    CHECK_TRUE(CountConstraints(Constraint::Type::VERTICAL) == vertical + 1);
    CHECK_TRUE(CountConstraints(Constraint::Type::ANGLE) == 0);
}

TEST_CASE(typed_circle_diameter) {
    CHECK_LOAD("line.slvs");
    Driver::Key("C");
    ClickAt(Point2d::From(500, 500));
    Driver::Move(540, 500);
    Driver::Type("12");
    Driver::Key("Return");
    std::optional<hConstraint> d = LastConstraint(Constraint::Type::DIAMETER);
    CHECK_TRUE(d.has_value());
    CHECK_EQ_EPS(SK.GetConstraint(*d)->valA, 12.0);
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::NONE);
}

TEST_CASE(typed_rectangle_size) {
    CHECK_LOAD("line.slvs");
    int dims = CountConstraints(Constraint::Type::PT_PT_DISTANCE);
    Driver::Key("B");
    ClickAt(Point2d::From(450, 550));
    Driver::Move(560, 480);
    Driver::Type("30");
    Driver::Key("Tab");
    Driver::Type("20");
    Driver::Key("Return");
    CHECK_TRUE(CountConstraints(Constraint::Type::PT_PT_DISTANCE) == dims + 2);
    std::vector<double> sizes;
    for(const Constraint &c : SK.constraint) {
        if(c.type == Constraint::Type::PT_PT_DISTANCE) sizes.push_back(c.valA);
    }
    std::sort(sizes.begin(), sizes.end());
    CHECK_TRUE(std::find(sizes.begin(), sizes.end(), 30.0) != sizes.end());
    CHECK_TRUE(std::find(sizes.begin(), sizes.end(), 20.0) != sizes.end());
}

TEST_CASE(backspace_edits_a_typed_value) {
    CHECK_LOAD("line.slvs");
    Driver::Key("C");
    ClickAt(Point2d::From(500, 500));
    Driver::Move(540, 500);
    Driver::Type("123");
    Driver::Key("Backspace");
    Driver::Key("Return");
    CHECK_EQ_EPS(SK.GetConstraint(*LastConstraint(Constraint::Type::DIAMETER))->valA, 12.0);
}

TEST_CASE(escape_clears_typed_values_then_cancels_the_tool) {
    CHECK_LOAD("line.slvs");
    int before = SK.request.n;
    Driver::Key("L");
    ClickAt(Point2d::From(500, 500));
    Driver::Type("25");
    Driver::Key("Escape"); // clears the field, still drawing
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::DRAGGING_NEW_LINE_POINT);
    Driver::Key("Escape"); // cancels the tool
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::NONE);
    CHECK_TRUE(CountConstraints(Constraint::Type::PT_PT_DISTANCE) == 0);
    CHECK_TRUE(SK.request.n <= before + 1);
}

TEST_CASE(line_chain_continues_until_escape) {
    CHECK_LOAD("line.slvs");
    int before = SK.request.n;
    Driver::Key("L");
    ClickAt(Point2d::From(450, 500));
    ClickAt(Point2d::From(550, 500));
    ClickAt(Point2d::From(550, 600));
    Driver::Key("Escape");
    CHECK_TRUE(SK.request.n == before + 2);
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::NONE);
}

TEST_CASE(drawn_line_end_snaps_to_an_existing_point) {
    CHECK_LOAD("line.slvs");
    hRequest first = DrawLine(Point2d::From(450, 500), Point2d::From(550, 500));
    int coincident = CountConstraints(Constraint::Type::POINTS_COINCIDENT);
    hRequest second = DrawLine(AtPoint(first.entity(2)), Point2d::From(550, 600));
    CHECK_TRUE(CountConstraints(Constraint::Type::POINTS_COINCIDENT) == coincident + 1);
    CHECK_TRUE(Length(second) > 0);
}

TEST_CASE(undo_after_a_typed_line_removes_line_and_constraints) {
    CHECK_LOAD("line.slvs");
    int requests = SK.request.n, constraints = SK.constraint.n;
    Driver::Key("L");
    ClickAt(Point2d::From(500, 500));
    Driver::Move(620, 460);
    Driver::Type("25");
    Driver::Key("Tab");
    Driver::Type("30");
    Driver::Key("Return");
    Driver::Key("Escape");
    Driver::Key("Escape");
    CHECK_TRUE(SK.constraint.n > constraints);
    Driver::Key("Ctrl+Z");
    CHECK_TRUE(SK.request.n == requests);
    CHECK_TRUE(SK.constraint.n == constraints);
}

TEST_CASE(holding_backspace_keeps_deleting_a_typed_value) {
    CHECK_LOAD("line.slvs");
    Driver::Key("C");
    ClickAt(Point2d::From(500, 500));
    Driver::Move(540, 500);
    Driver::Type("1234");
    Driver::KeyRepeat("Backspace", 2); // the press and two repeats: three characters
    Driver::Key("Return");
    CHECK_EQ_EPS(SK.GetConstraint(*LastConstraint(Constraint::Type::DIAMETER))->valA, 1.0);
}

TEST_CASE(holding_a_tool_key_starts_the_tool_once) {
    CHECK_LOAD("line.slvs");
    int before = SK.request.n;
    Driver::KeyRepeat("L", 5);
    CHECK_TRUE(SS.GW.pending.command == Command::LINE_SEGMENT);
    ClickAt(Point2d::From(500, 500));
    ClickAt(Point2d::From(600, 500));
    Driver::Key("Escape");
    CHECK_TRUE(SK.request.n == before + 1);
}

// Ending a line chain (decided 2026-10-17, as Inkscape, FreeCAD, Fusion 360 and Onshape do): the
// segment following the cursor is a preview, never part of the result.
static bool EndsNear(hRequest line, Point2d p) {
    return At(SK.GetEntity(line.entity(2))->PointGetNum()).DistanceTo(p) < 2;
}

static hRequest LastLine() {
    hRequest last = {};
    for(Request &r : SK.request) {
        if(r.type == Request::Type::LINE_SEGMENT) last = r.h;
    }
    return last;
}

TEST_CASE(right_click_ends_a_chain_without_the_segment_to_the_cursor) {
    CHECK_LOAD("line.slvs");
    int before = SK.request.n;
    Driver::Key("L");
    ClickAt(Point2d::From(450, 500));
    ClickAt(Point2d::From(550, 500));
    ClickAt(Point2d::From(550, 600));
    Driver::Click(600, 650, Driver::Button::RIGHT);
    CHECK_TRUE(SK.request.n == before + 2);
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::NONE);
    CHECK_TRUE(EndsNear(LastLine(), Point2d::From(550, 600)));
}

TEST_CASE(double_click_ends_a_chain_at_that_point) {
    CHECK_LOAD("line.slvs");
    int before = SK.request.n;
    Driver::Key("L");
    ClickAt(Point2d::From(450, 500));
    ClickAt(Point2d::From(550, 500));
    DoubleClickAt(Point2d::From(550, 600));
    CHECK_TRUE(SK.request.n == before + 2);
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::NONE);
    CHECK_TRUE(EndsNear(LastLine(), Point2d::From(550, 600)));
}

TEST_CASE(clicking_the_last_point_again_ends_a_chain) {
    CHECK_LOAD("line.slvs");
    int before = SK.request.n;
    Driver::Key("L");
    ClickAt(Point2d::From(450, 500));
    ClickAt(Point2d::From(550, 500));
    Driver::Move(600, 560);
    ClickAt(Point2d::From(550, 500));
    CHECK_TRUE(SK.request.n == before + 1);
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::NONE);
}

TEST_CASE(enter_with_nothing_typed_ends_a_chain) {
    CHECK_LOAD("line.slvs");
    int before = SK.request.n;
    Driver::Key("L");
    ClickAt(Point2d::From(450, 500));
    ClickAt(Point2d::From(550, 500));
    Driver::Move(600, 560);
    Driver::Key("Return");
    CHECK_TRUE(SK.request.n == before + 1);
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::NONE);
}

TEST_CASE(backspace_removes_the_last_segment_and_keeps_drawing) {
    CHECK_LOAD("line.slvs");
    int before = SK.request.n;
    Driver::Key("L");
    ClickAt(Point2d::From(450, 500));
    ClickAt(Point2d::From(550, 500));
    ClickAt(Point2d::From(550, 600));
    Driver::Key("Backspace");
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::DRAGGING_NEW_LINE_POINT);
    CHECK_TRUE(SK.request.n == before + 2); // the first segment and the one being drawn
    ClickAt(Point2d::From(650, 500));
    Driver::Key("Escape");
    CHECK_TRUE(SK.request.n == before + 2);
    CHECK_TRUE(EndsNear(LastLine(), Point2d::From(650, 500)));
    // Undo goes back through what is left, one segment at a time.
    Driver::Key("Ctrl+Z");
    CHECK_TRUE(SK.request.n == before + 1);
}

TEST_CASE(backspace_removes_a_typed_segment_with_its_constraints) {
    CHECK_LOAD("line.slvs");
    int requests = SK.request.n, constraints = SK.constraint.n;
    Driver::Key("L");
    ClickAt(Point2d::From(500, 500));
    Driver::Move(620, 460);
    Driver::Type("25");
    Driver::Key("Tab");
    Driver::Type("30");
    Driver::Key("Return");
    CHECK_TRUE(SK.constraint.n > constraints);
    Driver::Key("Backspace");
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::DRAGGING_NEW_LINE_POINT);
    CHECK_TRUE(SK.request.n == requests + 1);
    CHECK_TRUE(SK.constraint.n == constraints);
    Driver::Key("Escape");
    CHECK_TRUE(SK.request.n == requests);
}

TEST_CASE(backspace_before_the_second_point_cancels_the_line) {
    CHECK_LOAD("line.slvs");
    int before = SK.request.n;
    Driver::Key("L");
    ClickAt(Point2d::From(450, 500));
    Driver::Move(520, 520);
    Driver::Key("Backspace");
    CHECK_TRUE(SK.request.n == before);
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::NONE);
}

TEST_CASE(a_quick_move_then_click_suggests_from_where_the_mouse_is) {
    // One motion event from a horizontal stub straight down: the segment must come out
    // vertical, not horizontal from where the previous event left it.
    CHECK_LOAD("line.slvs");
    int vertical = CountConstraints(Constraint::Type::VERTICAL);
    int horizontal = CountConstraints(Constraint::Type::HORIZONTAL);
    Driver::Key("L");
    ClickAt(Point2d::From(450, 500));
    ClickAt(Point2d::From(550, 500)); // horizontal; the next segment starts as a stub
    ClickAt(Point2d::From(550, 600));
    Driver::Key("Escape");
    CHECK_TRUE(CountConstraints(Constraint::Type::VERTICAL) == vertical + 1);
    CHECK_TRUE(CountConstraints(Constraint::Type::HORIZONTAL) == horizontal + 1);
    CHECK_TRUE(EndsNear(LastLine(), Point2d::From(550, 600)));
}

TEST_CASE(a_line_in_3d_offers_no_angle_field) {
    // Outside a workplane there is no plane to measure an angle in.
    CHECK_LOAD("line.slvs");
    Driver::ClickMenu({ "New Group", "Sketch In 3d" });
    CHECK_FALSE(SS.GW.LockedInWorkplane());
    Driver::Key("L");
    ClickAt(Point2d::From(500, 600));
    Driver::Move(560, 600);
    Driver::Type("25");
    Driver::Key("Tab");
    CHECK_TRUE(Ui::Draw().Description().find("angle") == std::string::npos);
    Driver::Key("Escape");
    Driver::Key("Escape");
}
