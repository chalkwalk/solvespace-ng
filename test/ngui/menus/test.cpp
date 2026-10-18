#include "solvespace.h"
#include "../uitest.h"
#include "platform/sdl/uidriver.h"
#include "ui/ngui.h"

#include "harness.h"

namespace Driver = SolveSpace::Platform::Driver;
using namespace SolveSpace::UiTest;

TEST_CASE(menu_bar_click_starts_the_line_tool) {
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(Driver::ClickMenu({ "Sketch", "Line" }));
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::COMMAND);
    CHECK_TRUE(SS.GW.pending.command == Command::LINE_SEGMENT);
    CHECK_FALSE(Driver::FindItem("Line").has_value()); // the menu closed
}

TEST_CASE(checkable_menu_item_toggles_and_shows_its_state) {
    CHECK_LOAD("line.slvs");
    bool before = SS.GW.showSnapGrid;
    CHECK_TRUE(Driver::ClickItem("View"));
    std::optional<Driver::Item> item = Driver::FindItem("Snap Grid");
    CHECK_TRUE(item && item->checked == before);
    CHECK_TRUE(Driver::ClickItem("Snap Grid"));
    CHECK_TRUE(SS.GW.showSnapGrid == !before);
    CHECK_TRUE(Driver::ClickItem("View"));
    item = Driver::FindItem("Snap Grid");
    CHECK_TRUE(item && item->checked == !before);
    Driver::Key("Escape");
    CHECK_FALSE(Driver::FindItem("Snap Grid").has_value());
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
    // NG's toolbar is the tool shelf (View > Tool Shelf).
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(Driver::ClickMenu({ "View", "Tool Shelf" }));
    CHECK_TRUE(Driver::ClickItem("Rectangle"));
    CHECK_TRUE(SS.GW.pending.command == Command::RECTANGLE);
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
    CHECK_TRUE(Driver::FindItem("Line").has_value()); // the dropdown is open
    Driver::Key("L");
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::NONE);
    Driver::Key("Escape"); // closes the dropdown
    CHECK_FALSE(Driver::FindItem("Line").has_value());
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

// The NG menu layout (owner review 2026-10-17, ui/menulayout.cpp).
TEST_CASE(export_formats_are_in_an_export_submenu_and_exit_is_quit) {
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(Driver::ClickItem("File"));
    CHECK_TRUE(Driver::FindItem("Quit").has_value());
    std::optional<Driver::Item> exp = Driver::FindItem("Export");
    CHECK_TRUE(exp && exp->openable);
    if(exp) Driver::Move(exp->Center().x, exp->Center().y);
    CHECK_TRUE(Driver::FindItem("Triangle Mesh...").has_value());
    Driver::Key("Escape");
    Driver::Key("Escape");
}

TEST_CASE(view_menu_toggles_panels_with_check_marks) {
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(Driver::FindItem("Outliner").has_value());
    CHECK_TRUE(Driver::ClickItem("View"));
    std::optional<Driver::Item> outliner = Driver::FindItem("Outliner");
    CHECK_TRUE(outliner && outliner->checked);
    CHECK_TRUE(Driver::ClickItem("Outliner"));
    CHECK_FALSE(Driver::FindItem("Outliner").has_value()); // the panel (and the menu) closed
    CHECK_TRUE(Driver::ClickItem("View"));
    outliner = Driver::FindItem("Outliner");
    CHECK_TRUE(outliner && !outliner->checked);
    Driver::Key("Escape");
}

TEST_CASE(standard_views_submenu_snaps_the_view) {
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(Driver::ClickItem("View"));
    std::optional<Driver::Item> views = Driver::FindItem("Standard Views");
    CHECK_TRUE(views && views->openable);
    if(views) Driver::Move(views->Center().x, views->Center().y);
    CHECK_TRUE(Driver::ClickItem("Top"));
    Vector r, u;
    ViewPresetBasis(ViewPreset::TOP, &r, &u);
    CHECK_TRUE(SS.GW.projUp.Minus(u).Magnitude() < 1e-6);
}

TEST_CASE(help_lists_the_keyboard_shortcuts) {
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(Driver::ClickMenu({ "Help", "Keyboard Shortcuts" }));
    CHECK_TRUE(Driver::FindItem("Keyboard Shortcuts").has_value()); // the panel
    CHECK_TRUE(SolveSpace::UiTest::EditField("shortcuts-filter", "sketch line"));
    CHECK_TRUE(Driver::FindItem("Sketch \xE2\x80\xBA Line").has_value());
}

TEST_CASE(the_palette_still_finds_old_names) {
    CHECK_LOAD("line.slvs");
    Driver::Key("Space");
    Driver::Type("configuration");
    Driver::Key("Return");
    CHECK_TRUE(Driver::FindItem("Preferences").has_value());
}

TEST_CASE(context_menu_offers_grab_for_the_selection) {
    // (P13) Right-click a line: Grab is the first item and starts grabbing it.
    CHECK_LOAD("line.slvs");
    hRequest line = *LastRequest(Request::Type::LINE_SEGMENT);
    Point2d  m    = AtMiddle(line);
    CHECK_TRUE(ContextMenu(m, "Grab"));
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::GRABBING);
    Driver::Key("Escape");
}
