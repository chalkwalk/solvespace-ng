// The Problems panel: it appears when a solve fails and leads to the culprits.
#include "../uitest.h"
#include "ui/piemenu.h"
#include "ui/problemsmodel.h"
#include "harness.h"

using namespace SolveSpace::UiTest;

static bool ProblemsShown() {
    return Driver::FindItem("Problems").has_value();
}

// A dimensioned line, then made both horizontal and vertical: unsolvable.
static hRequest OverConstrainedLine() {
    hRequest line = DrawLine(Point2d::From(560, 520), Point2d::From(700, 470));
    ClickAt(AtMiddle(line));
    Driver::Key("D");
    Driver::Key("Return");
    for(const char *c : { "Horizontal", "Vertical" }) {
        Point2d m = AtMiddle(line);
        Driver::Move(m.x, m.y);
        Driver::Key("F");
        PickPie(c);
    }
    return line;
}

TEST_CASE(shift_p_toggles_problems) {
    CHECK_LOAD("line.slvs");
    CHECK_FALSE(ProblemsShown());
    Driver::Key("Shift+P");
    CHECK_TRUE(ProblemsShown());
    Driver::Key("Shift+P");
    CHECK_FALSE(ProblemsShown());
}

TEST_CASE(a_failed_solve_opens_problems_and_lists_the_culprits) {
    CHECK_LOAD("line.slvs");
    OverConstrainedLine();
    CHECK_FALSE(SS.ActiveGroupsOkay());
    CHECK_TRUE(ProblemsShown());
    CHECK_FALSE(SS.GW.showTextWindow); // NG doesn't pop the old browser
    std::vector<Ui::Problem> problems = Ui::BuildProblems();
    CHECK_TRUE(problems.size() == 1 && !problems[0].culprits.empty());
    CHECK_TRUE(Driver::ClickItem("select all"));
    CHECK_TRUE(SS.GW.selection.n == (int)problems[0].culprits.size());
    for(const GraphicsWindow::Selection &s : SS.GW.selection) CHECK_TRUE(s.constraint.v != 0);
    Driver::Key("Ctrl+Z");
    CHECK_TRUE(SS.ActiveGroupsOkay());
    CHECK_FALSE(Driver::FindItem("select all").has_value());
}

TEST_CASE(redundant_constraint_can_be_allowed) {
    CHECK_LOAD("line.slvs");
    hRequest line = DrawLine(Point2d::From(560, 520), Point2d::From(700, 520));
    // Horizontal on the line, and again on its two end points.
    Point2d m = AtMiddle(line);
    Driver::Move(m.x, m.y);
    Driver::Key("F");
    PickPie("Horizontal");
    ClickAt(AtPoint(line.entity(1)));
    ShiftClickAt(AtPoint(line.entity(2)));
    Driver::Move(700, 650);
    Driver::Key("F");
    CHECK_TRUE(PickPie("Horizontal"));
    CHECK_TRUE(ProblemsShown());
    CHECK_TRUE(Driver::ClickItem("allow redundant constraints"));
    CHECK_TRUE(SK.GetGroup(SS.GW.activeGroup)->allowRedundant);
    CHECK_TRUE(SS.ActiveGroupsOkay());
}

TEST_CASE(outliner_status_badge_shows_problems) {
    CHECK_LOAD("line.slvs");
    std::optional<Driver::Item> badge = Driver::FindItem("status:sketch-in-plane");
    CHECK_TRUE(badge.has_value());
    Point2d c = badge->Center();
    Driver::Click(c.x, c.y);
    CHECK_TRUE(ProblemsShown());
}

TEST_CASE(an_open_contour_does_not_pop_problems_up) {
    CHECK_LOAD("line.slvs");
    SS.checkClosedContour = true;
    DrawLine(Point2d::From(560, 520), Point2d::From(700, 520));
    CHECK_FALSE(ProblemsShown());
    Driver::Key("Shift+P");
    CHECK_TRUE(Driver::FindItem("sketch-in-plane").has_value()); // the outliner row, at least
}

