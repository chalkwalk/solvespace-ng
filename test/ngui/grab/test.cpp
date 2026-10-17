// G (grab): drag the selection under the solver with the mouse, axis keys and typed values.
#include "../uitest.h"
#include "ui/grabtool.h"
#include "harness.h"

using namespace SolveSpace::UiTest;

static Vector P(hEntity e) {
    return SK.GetEntity(e)->PointGetNum();
}

static hRequest SelectedLine() {
    hRequest line = DrawLine(Point2d::From(500, 520), Point2d::From(600, 520));
    ClickAt(AtMiddle(line));
    return line;
}

TEST_CASE(grab_follows_the_mouse_and_click_confirms) {
    CHECK_LOAD("line.slvs");
    hRequest line = SelectedLine();
    Vector a0 = P(line.entity(1));
    Point2d m = AtMiddle(line);
    Driver::Key("G");
    CHECK_TRUE(Ui::Grab().Active());
    Driver::Move(m.x + 40, m.y - 30);
    CHECK_TRUE(Near(AtPoint(line.entity(1)), At(a0).Plus(Point2d::From(40, -30))));
    Driver::Click(m.x + 40, m.y - 30);
    CHECK_FALSE(Ui::Grab().Active());
    CHECK_TRUE(P(line.entity(1)).Minus(a0).Magnitude() > 1);
    Driver::Key("Ctrl+Z"); // one undo step for the whole grab
    CHECK_TRUE(P(line.entity(1)).Minus(a0).Magnitude() < 1e-6);
}

TEST_CASE(grab_escape_restores_exactly) {
    CHECK_LOAD("line.slvs");
    hRequest line = SelectedLine();
    Vector a0 = P(line.entity(1)), b0 = P(line.entity(2));
    Point2d m = AtMiddle(line);
    Driver::Key("G");
    Driver::Move(m.x + 60, m.y + 25);
    Driver::Key("Escape");
    CHECK_FALSE(Ui::Grab().Active());
    CHECK_TRUE(P(line.entity(1)).Equals(a0) && P(line.entity(2)).Equals(b0));
}

TEST_CASE(grab_right_click_cancels) {
    CHECK_LOAD("line.slvs");
    hRequest line = SelectedLine();
    Vector a0 = P(line.entity(1));
    Point2d m = AtMiddle(line);
    Driver::Key("G");
    Driver::Move(m.x + 60, m.y + 25);
    Driver::Click(m.x + 60, m.y + 25, Driver::Button::RIGHT);
    CHECK_FALSE(Ui::Grab().Active());
    CHECK_TRUE(P(line.entity(1)).Equals(a0));
}

TEST_CASE(grab_axis_lock_and_typed_distance) {
    CHECK_LOAD("line.slvs");
    hRequest line = SelectedLine();
    Vector a0 = P(line.entity(1));
    Point2d m = AtMiddle(line);
    Driver::Key("G");
    Driver::Move(m.x + 30, m.y - 40);
    Driver::Key("X");
    Driver::Type("15");
    CHECK_TRUE(Ui::Grab().Description().find("15") != std::string::npos);
    Driver::Key("Return");
    CHECK_FALSE(Ui::Grab().Active());
    Vector d = P(line.entity(1)).Minus(a0);
    CHECK_EQ_EPS(d.Magnitude(), 15.0);
    CHECK_TRUE(fabs(d.Dot(SS.GW.GrabAxisVector(0))) > 15 - 1e-6);
    CHECK_TRUE(SK.request.FindByIdNoOops(line) != nullptr); // X never deletes while grabbing
}

TEST_CASE(grab_hover_fallback_moves_the_line_under_the_mouse) {
    CHECK_LOAD("line.slvs");
    hRequest line = DrawLine(Point2d::From(500, 520), Point2d::From(600, 520));
    Vector a0 = P(line.entity(1));
    Point2d m = AtMiddle(line);
    Driver::Move(m.x, m.y);
    Driver::Key("G");
    CHECK_TRUE(Ui::Grab().Active());
    Driver::Move(m.x, m.y + 50);
    Driver::Key("Return");
    CHECK_TRUE(P(line.entity(1)).Minus(a0).Magnitude() > 1);
    // Like a mouse drag, grabbing what was under the mouse doesn't leave it selected.
    CHECK_TRUE(SS.GW.selection.n == 0);
    // Nor does a cancelled one.
    m = AtMiddle(line);
    Driver::Move(m.x, m.y);
    Driver::Key("G");
    Driver::Move(m.x, m.y + 30);
    Driver::Key("Escape");
    CHECK_TRUE(SS.GW.selection.n == 0);
}

TEST_CASE(grab_with_nothing_to_grab_does_nothing) {
    CHECK_LOAD("line.slvs");
    Driver::Move(200, 650);
    Driver::Key("G");
    CHECK_FALSE(Ui::Grab().Active());
}

TEST_CASE(right_drag_pans_during_a_grab_without_cancelling) {
    CHECK_LOAD("line.slvs");
    hRequest line = SelectedLine();
    Vector a0 = P(line.entity(1));
    Point2d m = AtMiddle(line);
    Driver::Key("G");
    Driver::Move(m.x + 40, m.y);
    Vector moved = P(line.entity(1));
    Driver::Drag(m.x + 40, m.y, m.x + 140, m.y + 60, Driver::Button::RIGHT);
    CHECK_TRUE(Ui::Grab().Active());
    // Panning moves the view, not the grabbed points.
    CHECK_TRUE(P(line.entity(1)).Minus(moved).Magnitude() < 1e-6);
    Driver::Key("Return");
    CHECK_TRUE(P(line.entity(1)).Minus(a0).Magnitude() > 1);
}

TEST_CASE(zooming_during_a_grab_keeps_the_points_in_place) {
    CHECK_LOAD("line.slvs");
    hRequest line = SelectedLine();
    Point2d m = AtMiddle(line);
    Driver::Key("G");
    Driver::Move(m.x + 40, m.y);
    Vector moved = P(line.entity(1));
    Driver::Wheel(m.x + 40, m.y, 3);
    CHECK_TRUE(Ui::Grab().Active());
    // The next small move continues from there instead of jumping by the zoom factor.
    Driver::Move(m.x + 41, m.y);
    CHECK_TRUE(P(line.entity(1)).Minus(moved).Magnitude() < 2 / SS.GW.scale);
    Driver::Key("Escape");
}

TEST_CASE(orbiting_during_a_grab_keeps_the_points_in_place) {
    CHECK_LOAD("line.slvs");
    hRequest line = SelectedLine();
    Point2d m = AtMiddle(line);
    Driver::Key("G");
    Driver::Move(m.x + 40, m.y);
    Vector moved = P(line.entity(1));
    Driver::Drag(m.x + 40, m.y, m.x + 90, m.y + 40, Driver::Button::MIDDLE);
    CHECK_TRUE(Ui::Grab().Active());
    Driver::Move(m.x + 91, m.y + 40);
    CHECK_TRUE(P(line.entity(1)).Minus(moved).Magnitude() < 2 / SS.GW.scale);
    Driver::Key("Escape");
}

TEST_CASE(a_cancelled_grab_keeps_redo) {
    CHECK_LOAD("line.slvs");
    int requests = SK.request.n;
    DrawLine(Point2d::From(500, 620), Point2d::From(600, 620));
    Driver::Key("Ctrl+Z");
    CHECK_TRUE(SK.request.n == requests);
    hRequest line = *LastRequest(Request::Type::LINE_SEGMENT);
    ClickAt(AtMiddle(line));
    Point2d m = AtMiddle(line);
    Driver::Key("G");
    Driver::Move(m.x + 30, m.y + 20);
    Driver::Key("Escape");
    Driver::Key("Ctrl+Shift+Z"); // the undone line comes back
    CHECK_TRUE(SK.request.n == requests + 1);
}

TEST_CASE(a_confirmed_grab_ends_redo) {
    CHECK_LOAD("line.slvs");
    int requests = SK.request.n;
    DrawLine(Point2d::From(500, 620), Point2d::From(600, 620));
    Driver::Key("Ctrl+Z");
    hRequest line = *LastRequest(Request::Type::LINE_SEGMENT);
    ClickAt(AtMiddle(line));
    Point2d m = AtMiddle(line);
    Driver::Key("G");
    Driver::Move(m.x + 30, m.y + 20);
    Driver::Key("Return");
    Driver::Key("Ctrl+Shift+Z"); // nothing to redo after a new change
    CHECK_TRUE(SK.request.n == requests);
}

TEST_CASE(z_does_nothing_in_a_workplane) {
    // Sketch points can't leave their workplane: locking to its normal would freeze the grab.
    CHECK_LOAD("line.slvs");
    hRequest line = SelectedLine();
    Vector a0 = P(line.entity(1));
    Point2d m = AtMiddle(line);
    Driver::Key("G");
    Driver::Key("Z");
    CHECK_TRUE(SS.GW.grab.axis == -1);
    Driver::Move(m.x + 40, m.y - 30);
    CHECK_TRUE(P(line.entity(1)).Minus(a0).Magnitude() > 1);
    bool offersZ = false;
    for(const Ui::KeyHint &h : Ui::Grab().Hints()) offersZ |= h.keys.find('Z') != std::string::npos;
    CHECK_FALSE(offersZ);
    Driver::Key("Escape");
}
