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
