#include "solvespace.h"
#include "ui/selectionmodel.h"

#include "harness.h"

using namespace SolveSpace::Ui;

static hRequest Line() {
    for(Request &r : SK.request) {
        if(r.type == Request::Type::LINE_SEGMENT) return r.h;
    }
    return {};
}

static hConstraint Dimension(hRequest line, double mm) {
    Constraint c = {};
    c.type      = Constraint::Type::PT_PT_DISTANCE;
    c.group     = SS.GW.activeGroup;
    c.workplane = SS.GW.ActiveWorkplane();
    c.ptA       = line.entity(1);
    c.ptB       = line.entity(2);
    c.valA      = mm;
    hConstraint hc = Constraint::AddConstraint(&c, /*rememberForUndo=*/false);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    return hc;
}

static const SelectionRow *Row(const SelectionModel &m, const char *label) {
    for(const SelectionRow &r : m.rows) {
        if(r.label == label) return &r;
    }
    return nullptr;
}

TEST_CASE(nothing_selected_is_empty) {
    CHECK_LOAD("line.slvs");
    SS.GW.ClearSelection();
    CHECK_TRUE(BuildSelection().empty);
}

TEST_CASE(a_line_shows_its_ends_length_and_group) {
    CHECK_LOAD("line.slvs");
    hRequest line = Line();
    SS.GW.MakeSelected(line.entity(0));
    SelectionModel m = BuildSelection();
    // Plain labels and units (owner review 2026-10-17, P9).
    CHECK_EQ_STR(m.title, "Line segment");
    const SelectionRow *len = Row(m, "length");
    CHECK_TRUE(len != nullptr);
    Vector a = SK.GetEntity(line.entity(1))->PointGetNum();
    Vector b = SK.GetEntity(line.entity(2))->PointGetNum();
    if(len) CHECK_EQ_STR(len->value, SS.MmToString(a.Minus(b).Magnitude()) + " mm");
    const SelectionRow *from = Row(m, "from"), *to = Row(m, "to");
    CHECK_TRUE(from && from->entity == line.entity(1)); // links to the end points
    CHECK_TRUE(to && to->entity == line.entity(2));
    const SelectionRow *group = Row(m, "group");
    CHECK_TRUE(group && group->group == SS.GW.activeGroup);
}

TEST_CASE(two_points_show_their_distance) {
    CHECK_LOAD("line.slvs");
    hRequest line = Line();
    SS.GW.MakeSelected(line.entity(1));
    SS.GW.MakeSelected(line.entity(2));
    SelectionModel m = BuildSelection();
    CHECK_EQ_STR(m.title, "Two points");
    CHECK_TRUE(Row(m, "distance") != nullptr);
}

TEST_CASE(a_dimension_offers_its_value_and_lists_what_it_constrains) {
    CHECK_LOAD("line.slvs");
    hRequest    line = Line();
    hConstraint dim  = Dimension(line, 12.5);
    GraphicsWindow::Selection sel = {};
    sel.constraint = dim;
    SS.GW.selection.Add(&sel);
    SelectionModel m = BuildSelection();
    CHECK_TRUE(m.constraint == dim && m.hasValue && !m.reference);
    CHECK_EQ_STR(m.valueText, "12.50");
    CHECK_EQ_STR(m.unit, "mm");
    CHECK_TRUE(!m.sections.empty() && m.sections[0].title == "constrains");
    // Both end points belong to the one line: listed once, as the line.
    CHECK_TRUE(m.sections[0].rows.size() == 1);
}

TEST_CASE(a_dimension_shows_the_display_precision) {
    // (P8) 16.4924225025 reads 16.49 mm; editing still starts from the field's text.
    CHECK_LOAD("line.slvs");
    hRequest    line = Line();
    hConstraint dim  = Dimension(line, 16.4924225025);
    GraphicsWindow::Selection sel = {};
    sel.constraint = dim;
    SS.GW.selection.Add(&sel);
    SelectionModel m = BuildSelection();
    CHECK_EQ_STR(m.valueText, "16.49");
}

TEST_CASE(set_constraint_value_parses_units_and_is_undoable) {
    CHECK_LOAD("line.slvs");
    hConstraint dim = Dimension(Line(), 10);
    CHECK_TRUE(SS.SetConstraintValue(dim, "2*7"));
    CHECK_EQ_EPS(SK.GetConstraint(dim)->valA, 14.0);
    CHECK_FALSE(SS.SetConstraintValue(dim, "2*"));
    CHECK_EQ_EPS(SK.GetConstraint(dim)->valA, 14.0);
    SS.UndoUndo();
    CHECK_EQ_EPS(SK.GetConstraint(dim)->valA, 10.0);
    CHECK_EQ_STR(SS.ConstraintEditText(dim), "10.00");
}

TEST_CASE(a_line_lists_the_constraints_on_it) {
    CHECK_LOAD("line.slvs");
    hRequest line = Line();
    Dimension(line, 10);
    SS.GW.MakeSelected(line.entity(0));
    SelectionModel m = BuildSelection();
    bool found = false;
    for(const SelectionSection &s : m.sections) {
        if(s.title == "constrained by" && s.rows.size() == 1 && s.rows[0].constraint.v) found = true;
    }
    CHECK_TRUE(found);
}
