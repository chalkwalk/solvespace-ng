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
    CHECK_EQ_STR(m.title, "LINE SEGMENT");
    const SelectionRow *len = Row(m, "len");
    CHECK_TRUE(len != nullptr);
    Vector a = SK.GetEntity(line.entity(1))->PointGetNum();
    Vector b = SK.GetEntity(line.entity(2))->PointGetNum();
    CHECK_EQ_STR(len->value, SS.MmToString(a.Minus(b).Magnitude()));
    const SelectionRow *thru = Row(m, "thru");
    CHECK_TRUE(thru && thru->entity == line.entity(1)); // a link to the end point
    const SelectionRow *group = Row(m, "in group");
    CHECK_TRUE(group && group->group == SS.GW.activeGroup);
}

TEST_CASE(two_points_show_their_distance) {
    CHECK_LOAD("line.slvs");
    hRequest line = Line();
    SS.GW.MakeSelected(line.entity(1));
    SS.GW.MakeSelected(line.entity(2));
    SelectionModel m = BuildSelection();
    CHECK_EQ_STR(m.title, "TWO POINTS");
    CHECK_TRUE(Row(m, "d") != nullptr);
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
    CHECK_TRUE(!m.sections.empty() && m.sections[0].title == "constrains");
    CHECK_TRUE(m.sections[0].rows.size() == 2);
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
