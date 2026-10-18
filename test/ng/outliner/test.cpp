#include "solvespace.h"
#include "ui/outlinermodel.h"

#include "harness.h"

using namespace SolveSpace::Ui;

static const OutlinerRow *Row(const std::vector<OutlinerRow> &rows, const char *name) {
    for(const OutlinerRow &r : rows) {
        if(r.name == name) return &r;
    }
    return nullptr;
}

TEST_CASE(outliner_lists_groups_in_order_with_state) {
    CHECK_LOAD("line.slvs");
    std::vector<OutlinerRow> rows = BuildOutliner();
    CHECK_TRUE(rows.size() == 2);
    CHECK_TRUE(rows[0].references && !rows[0].active);
    // Fully constrained shows 0 in the DOF column; the references are the Origin, as Fusion.
    CHECK_EQ_STR(rows[0].StatusText(), "0");
    CHECK_EQ_STR(rows[0].displayName, "Origin");
    CHECK_EQ_STR(rows[0].icon, "origin");
    const OutlinerRow *sketch = Row(rows, "sketch-in-plane");
    CHECK_TRUE(sketch && sketch->active && sketch->visible && !sketch->afterActive);
    CHECK_TRUE(sketch->status == OutlinerRow::Status::DOF);
    CHECK_EQ_STR(sketch->StatusText(), std::to_string(sketch->dof));
    CHECK_EQ_STR(sketch->displayName, "sketch-in-plane");
    CHECK_EQ_STR(sketch->icon, "new-sketch"); // the shelf's icon for a sketch in a workplane
}

TEST_CASE(outliner_marks_groups_after_the_active_one) {
    CHECK_LOAD("line.slvs");
    hGroup sketch = Row(BuildOutliner(), "sketch-in-plane")->group;
    SS.GW.ActivateCommand(Command::GROUP_3D);
    SS.Refresh();
    SS.GW.ActivateGroup(sketch);
    std::vector<OutlinerRow> rows = BuildOutliner();
    CHECK_TRUE(rows.size() == 3);
    CHECK_TRUE(rows[2].afterActive && !rows[1].afterActive);
}

TEST_CASE(outliner_reports_a_failed_solve) {
    CHECK_LOAD("line.slvs");
    hRequest line;
    for(Request &r : SK.request) {
        if(r.type == Request::Type::LINE_SEGMENT) line = r.h;
    }
    Constraint::Constrain(Constraint::Type::HORIZONTAL, Entity::NO_ENTITY, Entity::NO_ENTITY,
                          line.entity(0));
    Constraint::Constrain(Constraint::Type::VERTICAL, Entity::NO_ENTITY, Entity::NO_ENTITY,
                          line.entity(0));
    Constraint c = {};
    c.type      = Constraint::Type::PT_PT_DISTANCE;
    c.group     = SS.GW.activeGroup;
    c.workplane = SS.GW.ActiveWorkplane();
    c.ptA       = line.entity(1);
    c.ptB       = line.entity(2);
    c.valA      = 10.0;
    Constraint::AddConstraint(&c, /*rememberForUndo=*/false);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    std::vector<OutlinerRow> rows = BuildOutliner();
    const OutlinerRow *sketch = Row(rows, "sketch-in-plane");
    CHECK_TRUE(sketch->status == OutlinerRow::Status::ERROR);
    CHECK_EQ_STR(sketch->StatusText(), "ERR");
}
