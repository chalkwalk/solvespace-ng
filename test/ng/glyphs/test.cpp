// Constraint glyphs coloured by state (AGENTS.md section 5): under-constrained in the
// Constraints style's colour, fully constrained quieter, conflicting in the Errors colour.
#include "solvespace.h"

#include "harness.h"

static hRequest Line() {
    for(Request &r : SK.request) {
        if(r.type == Request::Type::LINE_SEGMENT) return r.h;
    }
    return {};
}

static hConstraint Add(Constraint::Type type, hEntity ptA, hEntity ptB, hEntity entityA,
                       double val = 0) {
    Constraint c = {};
    c.type      = type;
    c.group     = SS.GW.activeGroup;
    c.workplane = SS.GW.ActiveWorkplane();
    c.ptA       = ptA;
    c.ptB       = ptB;
    c.entityA   = entityA;
    c.valA      = val;
    return Constraint::AddConstraint(&c, /*rememberForUndo=*/false);
}

static hEntity Origin() {
    return Request::HREQUEST_REFERENCE_XY.entity(1); // the XY plane's point: the origin
}

TEST_CASE(stock_solvespace_draws_every_constraint_in_its_style) {
    // Off unless NG turns it on: the old frontends and the stock render tests look as before.
    CHECK_LOAD("line.slvs");
    CHECK_FALSE(SS.constraintStateColors);
    hRequest line = Line();
    Add(Constraint::Type::HORIZONTAL, line.entity(1), line.entity(2), {});
    Add(Constraint::Type::VERTICAL, {}, {}, line.entity(0));
    Add(Constraint::Type::HORIZONTAL, {}, {}, line.entity(0));
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    for(Constraint &c : SK.constraint) {
        CHECK_TRUE(c.StateColor().Equals(Style::Color(Style::CONSTRAINT)));
    }
}

TEST_CASE(an_under_constrained_sketch_draws_constraints_in_their_style) {
    CHECK_LOAD("line.slvs");
    SS.constraintStateColors = true;
    hRequest    line = Line();
    hConstraint h    = Add(Constraint::Type::HORIZONTAL, {}, {}, line.entity(0));
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Constraint *c = SK.GetConstraint(h);
    CHECK_TRUE(c->SolveState() == Constraint::SolveStateKind::UNDER);
    CHECK_TRUE(c->StateColor().Equals(Style::Color(Style::CONSTRAINT)));
}

TEST_CASE(a_fully_constrained_sketch_draws_them_quieter) {
    CHECK_LOAD("line.slvs");
    SS.constraintStateColors = true;
    hRequest line = Line();
    hConstraint h = Add(Constraint::Type::HORIZONTAL, {}, {}, line.entity(0));
    Add(Constraint::Type::PT_PT_DISTANCE, line.entity(1), line.entity(2), {}, 20);
    Add(Constraint::Type::POINTS_COINCIDENT, line.entity(1), Origin(), {});
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_TRUE(SK.GetGroup(SS.GW.activeGroup)->solved.dof == 0);
    Constraint *c = SK.GetConstraint(h);
    CHECK_TRUE(c->SolveState() == Constraint::SolveStateKind::FULL);
    CHECK_FALSE(c->StateColor().Equals(Style::Color(Style::CONSTRAINT)));
}

TEST_CASE(a_culprit_of_a_failed_solve_draws_as_an_error) {
    CHECK_LOAD("line.slvs");
    SS.constraintStateColors = true;
    hRequest line = Line();
    Add(Constraint::Type::HORIZONTAL, {}, {}, line.entity(0));
    Add(Constraint::Type::VERTICAL, {}, {}, line.entity(0));
    Add(Constraint::Type::PT_PT_DISTANCE, line.entity(1), line.entity(2), {}, 10);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Group *g = SK.GetGroup(SS.GW.activeGroup);
    CHECK_FALSE(g->IsSolvedOkay());
    CHECK_TRUE(g->solved.remove.n > 0);
    for(hConstraint hc : g->solved.remove) {
        Constraint *c = SK.GetConstraint(hc);
        CHECK_TRUE(c->SolveState() == Constraint::SolveStateKind::CONFLICTING);
        // Red, lightened to stand out on the failed solve's dark red canvas.
        RgbaColor col = c->StateColor();
        CHECK_TRUE(col.red > col.green + 60 && col.red > col.blue + 60 && col.green > 60);
    }
}
