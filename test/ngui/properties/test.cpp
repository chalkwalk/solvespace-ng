// The Properties panel's group settings, edited with the mouse and keyboard.
#include "../uitest.h"
#include "ui/properties.h"
#include "harness.h"

using namespace SolveSpace::UiTest;

static Group *Active() {
    return SK.GetGroup(SS.GW.activeGroup);
}

static bool Click(const std::string &labelOrId) {
    return Driver::ClickItem(labelOrId);
}

// A rectangle extruded: the extrusion is the active group.
static void Extrude() {
    Driver::Key("B");
    ClickAt(Point2d::From(450, 450));
    ClickAt(Point2d::From(560, 540));
    Driver::Key("E");
}

TEST_CASE(properties_shows_the_active_group_by_default) {
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(Ui::PropertiesGroup() == SS.GW.activeGroup);
    CHECK_TRUE(Driver::FindItem("group-name").has_value());
    CHECK_TRUE(Driver::FindItem("show entities from this group").has_value());
}

TEST_CASE(name_field_renames_and_typing_does_not_reach_the_keymap) {
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(EditField("group-name", "plate"));
    CHECK_EQ_STR(Active()->name, "plate");
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::NONE); // 'l' in "plate"
    Driver::Key("Ctrl+Z");
    CHECK_EQ_STR(Active()->name, "sketch-in-plane");
}

TEST_CASE(checkboxes_change_options_with_one_undo_step_each) {
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(Click("relax constraints and dimensions"));
    CHECK_TRUE(Active()->relaxConstraints);
    CHECK_TRUE(Click("treat all dimensions as reference"));
    CHECK_TRUE(Active()->allDimsReference);
    Driver::Key("Ctrl+Z");
    CHECK_FALSE(Active()->allDimsReference);
    CHECK_TRUE(Active()->relaxConstraints);
}

TEST_CASE(extrude_sides_and_solid_model) {
    CHECK_LOAD("line.slvs");
    Extrude();
    CHECK_TRUE(Active()->type == Group::Type::EXTRUDE);
    CHECK_TRUE(Click("two-sided"));
    CHECK_TRUE(Active()->subtype == Group::Subtype::TWO_SIDED);
    CHECK_TRUE(Click("skewed"));
    CHECK_TRUE(Active()->subtype == Group::Subtype::TWO_SKEWED);
    CHECK_TRUE(Click("difference"));
    CHECK_TRUE(Active()->meshCombine == Group::CombineAs::DIFFERENCE);
    CHECK_TRUE(Click("suppress this group's solid model"));
    CHECK_TRUE(Active()->suppress);
}

TEST_CASE(opacity_field_takes_an_expression) {
    CHECK_LOAD("line.slvs");
    Extrude();
    CHECK_TRUE(EditField("opacity", "1/4"));
    CHECK_TRUE(Active()->color.alpha == 63);
}

TEST_CASE(bad_value_is_refused_with_a_message) {
    CHECK_LOAD("line.slvs");
    Extrude();
    int alpha = Active()->color.alpha;
    CHECK_TRUE(EditField("opacity", "2"));
    CHECK_TRUE(Active()->color.alpha == alpha);
    CHECK_TRUE(Driver::Dialogs().size() == 1);
    CHECK_TRUE(Driver::Dialogs()[0].message.find("Opacity") != std::string::npos);
}

TEST_CASE(step_translate_repeat_count) {
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(Driver::ClickMenu({ "New Group", "Linear Pattern" }));
    CHECK_TRUE(Active()->type == Group::Type::TRANSLATE);
    CHECK_TRUE(EditField("repeat", "4"));
    CHECK_EQ_EPS(Active()->valA, 4.0);
    CHECK_TRUE(Click("start with copy #1"));
    CHECK_TRUE(Active()->skipFirst);
}

TEST_CASE(request_list_click_selects_the_request) {
    CHECK_LOAD("line.slvs");
    hRequest line = *LastRequest(Request::Type::LINE_SEGMENT);
    CHECK_TRUE(Click("request:" + std::to_string(line.v)));
    CHECK_TRUE(SS.GW.selection.n == 1 && SS.GW.selection[0].entity == line.entity(0));
}

TEST_CASE(properties_follows_the_outliner_and_survives_deletion) {
    CHECK_LOAD("line.slvs");
    Driver::ClickMenu({ "New Group", "Sketch in 3D" });
    hGroup added = SS.GW.activeGroup;
    CHECK_TRUE(Click("Origin"));
    CHECK_TRUE(Ui::PropertiesGroup() == Group::HGROUP_REFERENCES);
    CHECK_TRUE(Click("sketch-in-3d"));
    CHECK_TRUE(Ui::PropertiesGroup() == added);
    Point2d c = Driver::FindItem("sketch-in-3d")->Center();
    Driver::Click(c.x, c.y, Driver::Button::RIGHT);
    CHECK_TRUE(Driver::ClickItem("Delete"));
    CHECK_TRUE(Ui::PropertiesGroup() == SS.GW.activeGroup); // back to the active group
    CHECK_TRUE(Driver::FindItem("group-name").has_value());
}


TEST_CASE(selecting_a_line_shows_it_and_unselect_all_returns_to_the_group) {
    CHECK_LOAD("line.slvs");
    hRequest line = *LastRequest(Request::Type::LINE_SEGMENT);
    ClickAt(AtMiddle(line));
    CHECK_TRUE(Driver::FindItem("unselect all").has_value());
    CHECK_FALSE(Driver::FindItem("group-name").has_value());
    CHECK_TRUE(Driver::ClickItem("unselect all"));
    CHECK_TRUE(SS.GW.selection.n == 0);
    CHECK_TRUE(Driver::FindItem("group-name").has_value());
}

TEST_CASE(a_point_link_in_the_selection_selects_the_point) {
    CHECK_LOAD("line.slvs");
    hRequest line = *LastRequest(Request::Type::LINE_SEGMENT);
    ClickAt(AtMiddle(line));
    CHECK_TRUE(Driver::ClickItem("row:0")); // "thru": the first end point
    CHECK_TRUE(SS.GW.selection.n == 1 && SS.GW.selection[0].entity == line.entity(1));
}

TEST_CASE(dimension_value_field_and_reference_checkbox) {
    CHECK_LOAD("line.slvs");
    hRequest line = DrawLine(Point2d::From(560, 500), Point2d::From(720, 500));
    ClickAt(AtMiddle(line));
    Driver::Key("D");
    Driver::Key("Return"); // keep the measured value in the on-canvas editor
    hConstraint dim = *LastConstraint(Constraint::Type::PT_PT_DISTANCE);
    ClickAt(AtLabel(dim));
    CHECK_TRUE(SS.GW.selection.n == 1 && SS.GW.selection[0].constraint == dim);
    CHECK_TRUE(EditField("value", "30"));
    CHECK_EQ_EPS(SK.GetConstraint(dim)->valA, 30.0);
    Vector a = SK.GetEntity(line.entity(1))->PointGetNum();
    Vector b = SK.GetEntity(line.entity(2))->PointGetNum();
    CHECK_EQ_EPS(a.Minus(b).Magnitude(), 30.0);
    ClickAt(AtLabel(dim));
    CHECK_TRUE(Driver::ClickItem("reference"));
    CHECK_TRUE(SK.GetConstraint(dim)->reference);
}

TEST_CASE(constrained_by_lists_a_lines_constraints) {
    CHECK_LOAD("line.slvs");
    hRequest line = DrawLine(Point2d::From(560, 500), Point2d::From(720, 500));
    ClickAt(AtMiddle(line));
    Driver::Key("D");
    Driver::Key("Return");
    hConstraint dim = *LastConstraint(Constraint::Type::PT_PT_DISTANCE);
    // The line was drawn horizontally, so it is constrained by that and the dimension.
    bool found = false;
    for(int i = 0; i < 2; i++) {
        ClickAt(AtMiddle(line));
        CHECK_TRUE(Driver::FindItem("constrained by").has_value());
        CHECK_TRUE(Driver::ClickItem("section0:" + std::to_string(i)));
        CHECK_TRUE(SS.GW.selection.n == 1 && SS.GW.selection[0].constraint.v != 0);
        if(SS.GW.selection[0].constraint == dim) found = true;
    }
    CHECK_TRUE(found);
}


TEST_CASE(dragging_in_the_colour_picker_is_one_undo_step) {
    CHECK_LOAD("line.slvs");
    Extrude();
    CHECK_TRUE(Driver::ClickItem("color")); // opens the picker
    std::optional<Driver::Item> sv = Driver::FindItem("sv");
    CHECK_TRUE(sv.has_value());
    int undo = SS.undo.cnt;
    Driver::Drag(sv->x0 + 5, sv->y0 + 5, sv->x1 - 5, sv->y1 - 5, Driver::Button::LEFT, 10);
    CHECK_TRUE(SS.undo.cnt == undo + 1);
    Driver::Key("Escape");
    RgbaColor dragged = Active()->color;
    Driver::Key("Ctrl+Z");
    CHECK_FALSE(Active()->color.Equals(dragged)); // the whole drag undone at once
}
