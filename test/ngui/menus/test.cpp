#include "solvespace.h"
#include "../uitest.h"
#include "platform/sdl/uidriver.h"
#include "ui/ngui.h"

#include "harness.h"

namespace Driver = SolveSpace::Platform::Driver;

TEST_CASE(menu_bar_click_starts_the_line_tool) {
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(Driver::ClickMenu({ "Sketch", "Line Segment" }));
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::COMMAND);
    CHECK_TRUE(SS.GW.pending.command == Command::LINE_SEGMENT);
    CHECK_FALSE(Driver::FindItem("Line Segment").has_value()); // the menu closed
}

TEST_CASE(checkable_menu_item_toggles_and_shows_its_state) {
    CHECK_LOAD("line.slvs");
    bool before = SS.GW.showSnapGrid;
    CHECK_TRUE(Driver::ClickItem("View"));
    CHECK_TRUE(Driver::FindItem("Show Snap Grid")->checked == before);
    CHECK_TRUE(Driver::ClickItem("Show Snap Grid"));
    CHECK_TRUE(SS.GW.showSnapGrid == !before);
    CHECK_TRUE(Driver::ClickItem("View"));
    CHECK_TRUE(Driver::FindItem("Show Snap Grid")->checked == !before);
    Driver::Key("Escape");
    CHECK_FALSE(Driver::FindItem("Show Snap Grid").has_value());
}

TEST_CASE(edit_menu_undo_reverts_a_drawn_line) {
    CHECK_LOAD("line.slvs");
    int before = SK.request.n;
    Driver::Key("L");
    Driver::Click(600, 420);
    Driver::Click(760, 300);
    Driver::Key("Escape");
    CHECK_TRUE(SK.request.n == before + 1);
    CHECK_TRUE(Driver::ClickMenu({ "Edit", "Undo" }));
    CHECK_TRUE(SK.request.n == before);
    CHECK_TRUE(Driver::ClickMenu({ "Edit", "Redo" }));
    CHECK_TRUE(SK.request.n == before + 1);
}

TEST_CASE(palette_row_click_runs_its_command) {
    CHECK_LOAD("line.slvs");
    Driver::Key("Space");
    Driver::Type("circle");
    CHECK_TRUE(Driver::ClickItem("Sketch \xE2\x80\xBA Circle"));
    CHECK_FALSE(Ui::CapturesText());
    CHECK_TRUE(SS.GW.pending.command == Command::CIRCLE);
}

TEST_CASE(palette_arrow_keys_pick_the_next_match) {
    CHECK_LOAD("line.slvs");
    Driver::Key("Space");
    Driver::Type("line segment");
    Driver::Key("Down"); // Sketch > Line Segment, then Construction Line Segment
    Driver::Key("Return");
    CHECK_TRUE(SS.GW.pending.command == Command::CONSTR_SEGMENT);
}

TEST_CASE(palette_escape_closes_without_running) {
    CHECK_LOAD("line.slvs");
    Driver::Key("Space");
    Driver::Type("circle");
    Driver::Key("Escape");
    CHECK_FALSE(Ui::CapturesText());
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::NONE);
    Driver::Key("C"); // keys reach the keymap again
    CHECK_TRUE(SS.GW.pending.command == Command::CIRCLE);
}

TEST_CASE(toolbar_icon_click_starts_its_tool) {
    CHECK_LOAD("line.slvs");
    Driver::Click(17, 53); // the toolbar's first icon: Line Segment
    CHECK_TRUE(SS.GW.pending.command == Command::LINE_SEGMENT);
}

TEST_CASE(n_toggles_properties_and_shift_n_the_old_browser) {
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(Driver::FindItem("Properties").has_value());
    Driver::Key("N");
    CHECK_FALSE(Driver::FindItem("Properties").has_value());
    Driver::Key("N");
    CHECK_TRUE(Driver::FindItem("Properties").has_value());
    CHECK_FALSE(Driver::FindItem("Property Browser").has_value());
    Driver::Key("Shift+N");
    CHECK_TRUE(Driver::FindItem("Property Browser").has_value());
}


TEST_CASE(keys_with_a_menu_open_do_not_run_tools) {
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(Driver::ClickItem("Sketch"));
    CHECK_TRUE(Driver::FindItem("Line Segment").has_value()); // the dropdown is open
    Driver::Key("L");
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::NONE);
    Driver::Key("Escape"); // closes the dropdown
    CHECK_FALSE(Driver::FindItem("Line Segment").has_value());
    Driver::Move(600, 600);
    Driver::Key("L");
    CHECK_TRUE(SS.GW.pending.command == Command::LINE_SEGMENT);
}

TEST_CASE(ctrl_shortcuts_still_work_with_a_menu_open) {
    CHECK_LOAD("line.slvs");
    int requests = SK.request.n;
    SolveSpace::UiTest::DrawLine(Point2d::From(500, 620), Point2d::From(600, 620));
    CHECK_TRUE(Driver::ClickItem("Edit"));
    Driver::Key("Ctrl+Z");
    CHECK_TRUE(SK.request.n == requests);
}
