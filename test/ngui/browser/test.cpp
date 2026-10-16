// The property browser, the dimension editor and context menus, driven with the mouse.
#include "../uitest.h"
#include "harness.h"

using namespace SolveSpace::UiTest;

static Group *GroupNamed(const char *name) {
    for(Group &g : SK.group) {
        if(g.name == name) return &g;
    }
    return nullptr;
}

TEST_CASE(browser_starts_hidden_and_the_view_menu_shows_it) {
    CHECK_LOAD("line.slvs");
    CHECK_FALSE(Driver::FindItem("Property Browser").has_value());
    ShowBrowser();
    CHECK_TRUE(Driver::FindItem("Property Browser").has_value());
}

TEST_CASE(browser_group_link_opens_group_info) {
    CHECK_LOAD("line.slvs");
    ShowBrowser();
    CHECK_TRUE(BrowserShows("sketch-in-plane"));
    CHECK_TRUE(ClickBrowserText("g002-sketch-in-plane", /*occurrence=*/1)); // 0: the header
    CHECK_TRUE(SS.TW.shown.screen == TextWindow::Screen::GROUP_INFO);
    CHECK_TRUE(ClickBrowserText("home"));
    CHECK_TRUE(SS.TW.shown.screen == TextWindow::Screen::LIST_OF_GROUPS);
}

TEST_CASE(browser_hide_all_then_show_all) {
    CHECK_LOAD("line.slvs");
    ShowBrowser();
    CHECK_TRUE(ClickBrowserText("hide all"));
    CHECK_FALSE(GroupNamed("#references")->visible);
    CHECK_TRUE(ClickBrowserText("show all"));
    CHECK_TRUE(GroupNamed("#references")->visible);
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

