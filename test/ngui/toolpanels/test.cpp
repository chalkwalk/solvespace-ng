// The View panel and the tool panels: Paste Transformed, Step Dimension and Tangent Arc.
#include "../uitest.h"
#include "harness.h"

using namespace SolveSpace::UiTest;

static bool Shown(const char *title) {
    return Driver::FindItem(title).has_value();
}

TEST_CASE(view_projection_command_opens_the_view_panel) {
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(Driver::ClickMenu({ "View", "View Settings..." }));
    CHECK_TRUE(Shown("View"));
    CHECK_FALSE(SS.GW.showTextWindow);
    CHECK_TRUE(EditField("view-scale", "20"));
    CHECK_EQ_EPS(SS.GW.scale, 20 / SS.MmPerUnit());
    CHECK_TRUE(EditField("view-origin", "1, 2, 3"));
    CHECK_TRUE(SS.GW.offset.Equals(Vector::From(-1, -2, -3).ScaledBy(SS.MmPerUnit())));
    CHECK_TRUE(EditField("view-right", "0, 1, 0"));
    CHECK_TRUE(SS.GW.projRight.Equals(Vector::From(0, 1, 0)));
}

TEST_CASE(paste_transformed_repeats_rotated_copies) {
    CHECK_LOAD("line.slvs");
    hRequest line = DrawLine(Point2d::From(600, 520), Point2d::From(700, 520));
    ClickAt(AtMiddle(line));
    Driver::Key("Ctrl+C");
    int before = SK.request.n;
    CHECK_TRUE(Driver::ClickMenu({ "Edit", "Paste Transformed..." }));
    CHECK_TRUE(Shown("Paste Transformed"));
    CHECK_TRUE(EditField("paste-times", "3"));
    CHECK_TRUE(EditField("paste-angle", "90"));
    CHECK_TRUE(Driver::ClickItem("paste transformed now"));
    CHECK_TRUE(SK.request.n == before + 3);
    CHECK_FALSE(Shown("Paste Transformed")); // done
}

TEST_CASE(paste_transformed_refuses_a_bad_scale) {
    CHECK_LOAD("line.slvs");
    hRequest line = *LastRequest(Request::Type::LINE_SEGMENT);
    ClickAt(AtMiddle(line));
    Driver::Key("Ctrl+C");
    Driver::ClickMenu({ "Edit", "Paste Transformed..." });
    CHECK_TRUE(EditField("paste-scale", "2*"));
    CHECK_TRUE(Driver::Dialogs().size() == 1);
    CHECK_EQ_EPS(SS.TW.shown.paste.scale, 1);
    CHECK_TRUE(ClosePanel("Paste Transformed"));
    CHECK_TRUE(SS.TW.shown.screen == TextWindow::Screen::LIST_OF_GROUPS);
}

TEST_CASE(step_dimension_steps_to_the_finish) {
    CHECK_LOAD("line.slvs");
    hRequest line = DrawLine(Point2d::From(560, 520), Point2d::From(720, 520));
    ClickAt(AtMiddle(line));
    Driver::Key("D");
    Driver::Key("Return");
    hConstraint dim = *LastConstraint(Constraint::Type::PT_PT_DISTANCE);
    ClickAt(AtLabel(dim));
    CHECK_TRUE(Driver::ClickMenu({ "Inspect", "Step Dimension..." }));
    CHECK_TRUE(Shown("Step Dimension"));
    CHECK_TRUE(EditField("step-finish", "30"));
    CHECK_TRUE(EditField("step-steps", "3"));
    CHECK_TRUE(Driver::ClickItem("step dimension now"));
    Driver::Settle(2000); // the steps run on a timer
    CHECK_EQ_EPS(SK.GetConstraint(dim)->valA, 30.0);
    CHECK_FALSE(Shown("Step Dimension"));
}

TEST_CASE(tangent_arc_settings) {
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(Driver::ClickMenu({ "Sketch", "Tangent Arc at Point" }));
    CHECK_TRUE(Shown("Tangent Arc"));
    bool manual = SS.tangentArcManual;
    CHECK_TRUE(Driver::ClickItem("choose radius automatically"));
    CHECK_TRUE(SS.tangentArcManual == !manual);
    if(SS.tangentArcManual) {
        CHECK_TRUE(EditField("tangent-radius", "5"));
        CHECK_EQ_EPS(SS.tangentArcRadius, 5 * SS.MmPerUnit());
    }
    CHECK_TRUE(ClosePanel("Tangent Arc"));
    CHECK_FALSE(Shown("Tangent Arc"));
}
