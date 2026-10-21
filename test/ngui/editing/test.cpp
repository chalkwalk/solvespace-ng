// Selection, delete, undo and redo, the dimension editor and context menus, with keys and the
// mouse.
#include "../uitest.h"
#include "harness.h"

using namespace SolveSpace::UiTest;

TEST_CASE(ctrl_a_selects_all_and_alt_a_clears) {
    CHECK_LOAD("line.slvs");
    DrawLine(Point2d::From(450, 520), Point2d::From(560, 520));
    Driver::Key("Ctrl+A");
    CHECK_TRUE(SS.GW.selection.n > 0);
    Driver::Key("Alt+A");
    CHECK_TRUE(SS.GW.selection.n == 0);
}

TEST_CASE(shift_click_adds_and_plain_click_replaces) {
    CHECK_LOAD("line.slvs");
    hRequest a = DrawLine(Point2d::From(450, 520), Point2d::From(560, 520));
    hRequest b = DrawLine(Point2d::From(450, 600), Point2d::From(560, 600));
    ClickAt(AtMiddle(a));
    CHECK_TRUE(SS.GW.selection.n == 1);
    ShiftClickAt(AtMiddle(b));
    CHECK_TRUE(SS.GW.selection.n == 2);
    Driver::Key("Escape");
    CHECK_TRUE(SS.GW.selection.n == 0);
}

TEST_CASE(marquee_drag_selects_what_it_encloses) {
    CHECK_LOAD("line.slvs");
    DrawLine(Point2d::From(450, 520), Point2d::From(560, 520));
    SS.GW.ClearSelection();
    Driver::Drag(420, 490, 600, 560);
    // The line and its two end points.
    CHECK_TRUE(SS.GW.selection.n == 3);
}

TEST_CASE(delete_and_backspace_delete_the_selection) {
    CHECK_LOAD("line.slvs");
    hRequest a = DrawLine(Point2d::From(450, 520), Point2d::From(560, 520));
    hRequest b = DrawLine(Point2d::From(450, 600), Point2d::From(560, 600));
    ClickAt(AtMiddle(a));
    Driver::Key("Delete");
    CHECK_TRUE(SK.request.FindByIdNoOops(a) == nullptr);
    ClickAt(AtMiddle(b));
    Driver::Key("Backspace");
    CHECK_TRUE(SK.request.FindByIdNoOops(b) == nullptr);
}

TEST_CASE(ctrl_z_undoes_and_both_redo_chords_redo) {
    CHECK_LOAD("line.slvs");
    int before = SK.request.n;
    DrawLine(Point2d::From(450, 520), Point2d::From(560, 520));
    Driver::Key("Ctrl+Z");
    CHECK_TRUE(SK.request.n == before);
    Driver::Key("Ctrl+Shift+Z");
    CHECK_TRUE(SK.request.n == before + 1);
    Driver::Key("Ctrl+Z");
    Driver::Key("Ctrl+Y");
    CHECK_TRUE(SK.request.n == before + 1);
}

TEST_CASE(dragging_a_point_moves_it) {
    CHECK_LOAD("line.slvs");
    hRequest a = DrawLine(Point2d::From(450, 520), Point2d::From(560, 520));
    Point2d p = AtPoint(a.entity(2));
    Driver::Drag(p.x, p.y, p.x + 30, p.y + 40);
    CHECK_TRUE(Near(AtPoint(a.entity(2)), p.Plus(Point2d::From(30, 40))));
}

TEST_CASE(x_toggles_construction) {
    CHECK_LOAD("line.slvs");
    hRequest a = DrawLine(Point2d::From(450, 520), Point2d::From(560, 520));
    ClickAt(AtMiddle(a));
    Driver::Key("X");
    CHECK_TRUE(SK.GetRequest(a)->construction);
}

TEST_CASE(new_dimension_opens_its_editor_for_a_typed_value) {
    CHECK_LOAD("line.slvs");
    hRequest line = DrawLine(Point2d::From(560, 500), Point2d::From(720, 500));
    ClickAt(AtMiddle(line));
    Driver::Key("D");
    std::optional<hConstraint> dim = LastConstraint(Constraint::Type::PT_PT_DISTANCE);
    CHECK_TRUE(dim.has_value());
    CHECK_TRUE(SS.GW.window->IsEditorVisible()); // "edit newly added dimensions" is on
    Driver::Type("40");
    Driver::Key("Return");
    CHECK_FALSE(SS.GW.window->IsEditorVisible());
    CHECK_EQ_EPS(SK.GetConstraint(*dim)->valA, 40.0);
    Vector a = SK.GetEntity(line.entity(1))->PointGetNum();
    Vector b = SK.GetEntity(line.entity(2))->PointGetNum();
    CHECK_EQ_EPS(a.Minus(b).Magnitude(), 40.0);
}

TEST_CASE(double_click_dimension_edits_its_value) {
    CHECK_LOAD("line.slvs");
    hRequest line = DrawLine(Point2d::From(560, 500), Point2d::From(720, 500));
    ClickAt(AtMiddle(line));
    Driver::Key("D");
    Driver::Key("Return"); // keep the measured value
    hConstraint dim = *LastConstraint(Constraint::Type::PT_PT_DISTANCE);
    CHECK_FALSE(SS.GW.window->IsEditorVisible());
    DoubleClickAt(AtLabel(dim));
    CHECK_TRUE(SS.GW.window->IsEditorVisible());
    Driver::Type("25");
    Driver::Key("Return");
    CHECK_EQ_EPS(SK.GetConstraint(dim)->valA, 25.0);
}

TEST_CASE(escape_closes_the_dimension_editor_unchanged) {
    CHECK_LOAD("line.slvs");
    hRequest line = DrawLine(Point2d::From(560, 500), Point2d::From(720, 500));
    ClickAt(AtMiddle(line));
    Driver::Key("D");
    hConstraint dim = *LastConstraint(Constraint::Type::PT_PT_DISTANCE);
    double before = SK.GetConstraint(dim)->valA;
    Driver::Type("99");
    Driver::Key("Escape");
    CHECK_FALSE(SS.GW.window->IsEditorVisible());
    CHECK_EQ_EPS(SK.GetConstraint(dim)->valA, before);
}

TEST_CASE(context_menu_delete_removes_the_hovered_line) {
    CHECK_LOAD("line.slvs");
    hRequest line = DrawLine(Point2d::From(560, 500), Point2d::From(720, 500));
    Point2d mid = AtMiddle(line);
    Driver::Move(mid.x, mid.y);
    CHECK_TRUE(ContextMenu(mid, "Delete"));
    CHECK_TRUE(SK.request.FindByIdNoOops(line) == nullptr);
}

TEST_CASE(context_menu_dismissed_by_escape_changes_nothing) {
    CHECK_LOAD("line.slvs");
    hRequest line = DrawLine(Point2d::From(560, 500), Point2d::From(720, 500));
    Point2d mid = AtMiddle(line);
    bool shown = false;
    Driver::DuringModal([&] {
        shown = Driver::FindItem("Delete").has_value();
        Driver::Key("Escape");
    });
    Driver::Click(mid.x, mid.y, Driver::Button::RIGHT);
    CHECK_TRUE(shown);
    CHECK_TRUE(SK.request.FindByIdNoOops(line) != nullptr);
}

