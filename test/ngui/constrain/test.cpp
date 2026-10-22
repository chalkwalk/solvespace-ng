// F (constrain pie) and D (smart dimension) on mouse selections.
#include "../uitest.h"
#include "ui/piemenu.h"
#include "harness.h"

using namespace SolveSpace::UiTest;

static bool Has(const std::vector<std::string> &v, const std::string &s) {
    return std::find(v.begin(), v.end(), s) != v.end();
}

TEST_CASE(k_pie_offers_parallel_for_two_lines) {
    CHECK_LOAD("line.slvs");
    hRequest a = DrawLine(Point2d::From(450, 520), Point2d::From(560, 540));
    hRequest b = DrawLine(Point2d::From(450, 600), Point2d::From(560, 640));
    ClickAt(AtMiddle(a));
    ShiftClickAt(AtMiddle(b));
    CHECK_TRUE(SS.GW.selection.n == 2);
    Driver::Move(700, 600);
    Driver::Key("K");
    std::vector<std::string> labels = PieLabels();
    CHECK_TRUE(Has(labels, "Parallel / Tangent"));
    CHECK_TRUE(Has(labels, "Horizontal"));  // applies to every selected line
    CHECK_FALSE(Has(labels, "Diameter"));   // needs a circle or arc
    int before = CountConstraints(Constraint::Type::PARALLEL);
    CHECK_TRUE(PickPie("Parallel / Tangent"));
    CHECK_TRUE(CountConstraints(Constraint::Type::PARALLEL) == before + 1);
}

TEST_CASE(k_pie_horizontal_on_the_hovered_line) {
    CHECK_LOAD("line.slvs");
    hRequest a = DrawLine(Point2d::From(450, 520), Point2d::From(560, 560));
    Point2d m = AtMiddle(a);
    Driver::Move(m.x, m.y);
    Driver::Key("K");
    CHECK_TRUE(Has(PieLabels(), "Horizontal"));
    CHECK_TRUE(PickPie("Horizontal"));
    CHECK_TRUE(CountConstraints(Constraint::Type::HORIZONTAL) == 1);
    Vector p = SK.GetEntity(a.entity(1))->PointGetNum(), q = SK.GetEntity(a.entity(2))->PointGetNum();
    CHECK_TRUE(fabs(p.y - q.y) < 1e-6);
}

TEST_CASE(k_pie_with_nothing_applicable_explains_and_closes) {
    CHECK_LOAD("line.slvs");
    Driver::Move(200, 650);
    Driver::Key("K");
    CHECK_TRUE(Ui::Pie().IsOpen());
    CHECK_TRUE(PieLabels().empty());
    Driver::Click(200, 650);
    CHECK_FALSE(Ui::Pie().IsOpen());
}

TEST_CASE(d_dimensions_a_circle_as_a_diameter) {
    CHECK_LOAD("line.slvs");
    Driver::Key("C");
    ClickAt(Point2d::From(500, 550));
    ClickAt(Point2d::From(540, 550));
    Point2d edge = Point2d::From(540, 550);
    ClickAt(edge);
    Driver::Key("D");
    CHECK_TRUE(LastConstraint(Constraint::Type::DIAMETER).has_value());
    Driver::Key("Return");
}

TEST_CASE(d_dimensions_two_lines_as_an_angle) {
    CHECK_LOAD("line.slvs");
    hRequest a = DrawLine(Point2d::From(450, 520), Point2d::From(560, 520));
    hRequest b = DrawLine(Point2d::From(450, 600), Point2d::From(560, 650));
    ClickAt(AtMiddle(a));
    ShiftClickAt(AtMiddle(b));
    Driver::Key("D");
    CHECK_TRUE(LastConstraint(Constraint::Type::ANGLE).has_value());
    Driver::Key("Return");
}

TEST_CASE(d_with_bad_selection_reports_an_error) {
    CHECK_LOAD("line.slvs");
    hRequest a = DrawLine(Point2d::From(450, 520), Point2d::From(560, 520));
    ClickAt(AtPoint(a.entity(1))); // one point: nothing to dimension
    Driver::Key("D");
    CHECK_TRUE(Driver::Dialogs().size() == 1);
    CHECK_TRUE(Driver::Dialogs()[0].message.find("Bad selection") != std::string::npos);
}
