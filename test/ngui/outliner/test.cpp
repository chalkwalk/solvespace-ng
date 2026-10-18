// The outliner panel, driven with the mouse and keys.
#include "../uitest.h"
#include "ui/ngui.h"
#include "ui/properties.h"
#include "harness.h"

using namespace SolveSpace::UiTest;

static Group *Named(const char *name) {
    for(Group &g : SK.group) {
        if(g.name == name) return &g;
    }
    return nullptr;
}

static void ClickId(const std::string &id) {
    std::optional<Driver::Item> item = Driver::FindItem(id);
    if(!item) return;
    Point2d c = item->Center();
    Driver::Click(c.x, c.y);
}

static hGroup NewGroup3d() {
    Driver::ClickMenu({ "New Group", "Sketch in 3D" });
    return SS.GW.activeGroup;
}

TEST_CASE(o_toggles_the_outliner) {
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(Driver::FindItem("Outliner").has_value());
    Driver::Key("O");
    CHECK_FALSE(Driver::FindItem("Outliner").has_value());
    Driver::Key("O");
    CHECK_TRUE(Driver::FindItem("Outliner").has_value());
}

TEST_CASE(outliner_lists_every_group_with_its_status) {
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(Driver::FindItem("Origin").has_value());
    CHECK_TRUE(Driver::FindItem("DOF").has_value()); // the column's header
    std::optional<Driver::Item> origin = Driver::FindItem("status:#references");
    CHECK_TRUE(origin && origin->label == "0"); // fully constrained
    CHECK_TRUE(Driver::FindItem("sketch-in-plane").has_value());
    std::optional<Driver::Item> badge = Driver::FindItem("status:sketch-in-plane");
    CHECK_TRUE(badge && badge->label == std::to_string(Named("sketch-in-plane")->solved.dof));
    NewGroup3d();
    CHECK_TRUE(Driver::FindItem("sketch-in-3d").has_value());
}

TEST_CASE(eye_checkbox_hides_and_shows_a_group) {
    CHECK_LOAD("line.slvs");
    ClickId("shown:sketch-in-plane");
    CHECK_FALSE(Named("sketch-in-plane")->visible);
    CHECK_FALSE(Driver::FindItem("shown:sketch-in-plane")->checked);
    ClickId("shown:sketch-in-plane");
    CHECK_TRUE(Named("sketch-in-plane")->visible);
}

TEST_CASE(space_after_a_panel_click_opens_the_palette_only) {
    CHECK_LOAD("line.slvs");
    ClickId("shown:sketch-in-plane");
    CHECK_FALSE(Named("sketch-in-plane")->visible);
    Driver::Key("Space");
    CHECK_TRUE(Ui::CapturesText());
    CHECK_FALSE(Named("sketch-in-plane")->visible); // the checkbox was not pressed again
    Driver::Key("Escape");
}

TEST_CASE(radio_and_double_click_activate_groups) {
    CHECK_LOAD("line.slvs");
    hGroup sketch = Named("sketch-in-plane")->h;
    hGroup added  = NewGroup3d();
    ClickId("active:sketch-in-plane");
    CHECK_TRUE(SS.GW.activeGroup == sketch);
    Point2d c = Driver::FindItem("sketch-in-3d")->Center();
    Driver::DoubleClick(c.x, c.y);
    CHECK_TRUE(SS.GW.activeGroup == added);
}

TEST_CASE(clicking_a_name_shows_the_group_in_properties) {
    CHECK_LOAD("line.slvs");
    Driver::Key("N"); // even when Properties is hidden
    CHECK_TRUE(Driver::ClickItem("Origin"));
    CHECK_TRUE(Driver::FindItem("Properties").has_value());
    CHECK_TRUE(Ui::PropertiesGroup() == Group::HGROUP_REFERENCES);
}

TEST_CASE(context_menu_renames_a_group) {
    CHECK_LOAD("line.slvs");
    Point2d c = Driver::FindItem("sketch-in-plane")->Center();
    Driver::Click(c.x, c.y, Driver::Button::RIGHT);
    CHECK_TRUE(Driver::ClickItem("Rename"));
    Driver::Type("base");
    Driver::Key("Return");
    CHECK_TRUE(Named("base") != nullptr);
    CHECK_TRUE(Driver::FindItem("base").has_value());
    Driver::Key("Ctrl+Z");
    CHECK_TRUE(Named("sketch-in-plane") != nullptr);
}

TEST_CASE(escape_cancels_a_rename) {
    CHECK_LOAD("line.slvs");
    Point2d c = Driver::FindItem("sketch-in-plane")->Center();
    Driver::Click(c.x, c.y, Driver::Button::RIGHT);
    Driver::ClickItem("Rename");
    Driver::Type("zzz");
    Driver::Key("Escape");
    CHECK_TRUE(Named("sketch-in-plane") != nullptr);
    CHECK_TRUE(Driver::FindItem("sketch-in-plane").has_value());
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::NONE); // no keys leaked
}

TEST_CASE(context_menu_deletes_and_undo_restores) {
    CHECK_LOAD("line.slvs");
    hGroup added = NewGroup3d();
    Point2d c = Driver::FindItem("sketch-in-3d")->Center();
    Driver::Click(c.x, c.y, Driver::Button::RIGHT);
    CHECK_TRUE(Driver::ClickItem("Delete"));
    CHECK_TRUE(SK.group.FindByIdNoOops(added) == nullptr);
    CHECK_FALSE(Driver::FindItem("sketch-in-3d").has_value());
    Driver::Key("Ctrl+Z");
    CHECK_TRUE(SK.group.FindByIdNoOops(added) != nullptr);
}

TEST_CASE(context_menu_isolates_a_group) {
    CHECK_LOAD("line.slvs");
    NewGroup3d();
    Point2d c = Driver::FindItem("sketch-in-plane")->Center();
    Driver::Click(c.x, c.y, Driver::Button::RIGHT);
    CHECK_TRUE(Driver::ClickItem("Isolate"));
    CHECK_TRUE(Named("sketch-in-plane")->visible);
    CHECK_FALSE(Named("sketch-in-3d")->visible);
    CHECK_TRUE(Named("#references")->visible);
    // The active group was hidden too; drawing in it shows it again, so nothing new is
    // invisible.
    DrawLine(Point2d::From(500, 640), Point2d::From(600, 640));
    CHECK_TRUE(Named("sketch-in-3d")->visible);
}

TEST_CASE(panel_clicks_never_reach_the_canvas) {
    CHECK_LOAD("line.slvs");
    int before = SK.request.n;
    Driver::Key("L");
    CHECK_TRUE(Driver::ClickItem("sketch-in-plane"));
    CHECK_TRUE(SK.request.n == before); // no line point was placed under the panel
}

TEST_CASE(first_move_off_a_panel_reaches_the_canvas) {
    CHECK_LOAD("line.slvs");
    Point2d row = Driver::FindItem("sketch-in-plane")->Center();
    Driver::Move(row.x, row.y); // over the outliner
    hRequest line = *LastRequest(Request::Type::LINE_SEGMENT);
    Point2d  m    = AtMiddle(line);
    Driver::Move(m.x, m.y);     // a single move onto the canvas
    CHECK_TRUE(SS.GW.hover.entity == line.entity(0));
}

TEST_CASE(clicking_a_group_shows_it_even_with_a_selection) {
    CHECK_LOAD("line.slvs");
    hRequest line = *LastRequest(Request::Type::LINE_SEGMENT);
    ClickAt(AtMiddle(line));
    CHECK_TRUE(SS.GW.selection.n == 1);
    CHECK_TRUE(Driver::ClickItem("Origin"));
    CHECK_TRUE(SS.GW.selection.n == 0);
    CHECK_TRUE(Ui::PropertiesGroup() == Group::HGROUP_REFERENCES);
    CHECK_FALSE(Driver::FindItem("unselect all").has_value()); // the group, not the selection
}

TEST_CASE(moving_onto_a_panel_unhovers_the_canvas) {
    CHECK_LOAD("line.slvs");
    hRequest line = DrawLine(Point2d::From(500, 620), Point2d::From(600, 620));
    Point2d m = AtMiddle(line);
    Driver::Move(m.x, m.y);
    CHECK_TRUE(SS.GW.hover.entity.v != 0);
    Point2d row = Driver::FindItem("sketch-in-plane")->Center();
    Driver::Move(row.x, row.y);
    CHECK_TRUE(SS.GW.hover.IsEmpty());
}
