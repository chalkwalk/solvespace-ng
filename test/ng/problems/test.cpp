#include "solvespace.h"
#include "ui/problemsmodel.h"

#include "harness.h"

using namespace SolveSpace::Ui;

static hRequest Line() {
    for(Request &r : SK.request) {
        if(r.type == Request::Type::LINE_SEGMENT) return r.h;
    }
    return {};
}

static hConstraint Length(hRequest line, double mm) {
    Constraint c = {};
    c.type      = Constraint::Type::PT_PT_DISTANCE;
    c.group     = SS.GW.activeGroup;
    c.workplane = SS.GW.ActiveWorkplane();
    c.ptA       = line.entity(1);
    c.ptB       = line.entity(2);
    c.valA      = mm;
    return Constraint::AddConstraint(&c, /*rememberForUndo=*/false);
}

TEST_CASE(a_good_sketch_has_no_problems) {
    CHECK_LOAD("line.slvs");
    SS.checkClosedContour = false;
    CHECK_TRUE(BuildProblems().empty());
}

TEST_CASE(incompatible_constraints_are_listed_with_their_culprits) {
    CHECK_LOAD("line.slvs");
    hRequest line = Line();
    Constraint::Constrain(Constraint::Type::HORIZONTAL, Entity::NO_ENTITY, Entity::NO_ENTITY,
                          line.entity(0));
    Constraint::Constrain(Constraint::Type::VERTICAL, Entity::NO_ENTITY, Entity::NO_ENTITY,
                          line.entity(0));
    Length(line, 10);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    std::vector<Problem> problems = BuildProblems();
    CHECK_TRUE(problems.size() == 1);
    CHECK_TRUE(problems[0].kind == Problem::Kind::UNSOLVABLE ||
               problems[0].kind == Problem::Kind::UNSATISFIED);
    CHECK_TRUE(problems[0].group == SS.GW.activeGroup);
    CHECK_FALSE(problems[0].culprits.empty());
    CHECK_FALSE(problems[0].canAllowRedundant);
}

TEST_CASE(redundant_constraints_offer_to_allow_them) {
    CHECK_LOAD("line.slvs");
    hRequest line = Line();
    Length(line, 10);
    Length(line, 10);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    std::vector<Problem> problems = BuildProblems();
    CHECK_TRUE(problems.size() == 1);
    CHECK_TRUE(problems[0].kind == Problem::Kind::REDUNDANT);
    CHECK_TRUE(problems[0].canAllowRedundant);
    CHECK_TRUE(problems[0].culprits.size() >= 2);
}

TEST_CASE(an_open_contour_is_a_problem_when_contours_are_checked) {
    CHECK_LOAD("line.slvs"); // a lone line: not a closed contour
    SS.checkClosedContour = true;
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    std::vector<Problem> problems = BuildProblems();
    CHECK_TRUE(problems.size() == 1 && problems[0].kind == Problem::Kind::CONTOUR);
    SS.checkClosedContour = false;
    CHECK_TRUE(BuildProblems().empty());
}

TEST_CASE(a_problem_names_its_group_by_name_only) {
    // (P14) The group as the Outliner names it, not "g002-sketch-in-3d".
    CHECK_LOAD("line.slvs");
    hRequest line = Line();
    Length(line, 10);
    Length(line, 10);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    std::vector<Problem> problems = BuildProblems();
    CHECK_TRUE(problems.size() == 1);
    if(problems.size() == 1) CHECK_EQ_STR(problems[0].groupName, SK.GetGroup(SS.GW.activeGroup)->name);
}
