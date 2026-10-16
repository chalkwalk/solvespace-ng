#include "solvespace.h"
#include "ui/coreactions.h"
#include "ui/uiactions.h"

#include "harness.h"

using namespace SolveSpace::Ui;

TEST_CASE(view_actions_are_bound_and_run) {
    CHECK_LOAD("line.slvs");
    RegisterCoreUiActions(/*animate=*/false);
    CHECK_TRUE(Actions().Run("ui.view-right"));
    CHECK_TRUE(SS.GW.projRight.Minus(Vector::From(0, 1, 0)).Magnitude() < 1e-9);
    CHECK_TRUE(Actions().Run("ui.view-frame-selected"));
}

TEST_CASE(hide_actions_are_bound) {
    RegisterCoreUiActions(false);
    CHECK_TRUE(Actions().IsEnabled("ui.hide"));
    CHECK_TRUE(Actions().IsEnabled("ui.unhide-all"));
    CHECK_TRUE(Actions().IsEnabled("ui.isolate"));
}
