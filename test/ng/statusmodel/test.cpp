#include "solvespace.h"
#include "ui/keypresets.h"
#include "ui/statusmodel.h"

#include "harness.h"

using namespace SolveSpace::Ui;

static bool HasHint(const StatusModel &m, const char *keys, const char *label) {
    for(const KeyHint &h : m.hints) {
        if(h.keys == keys && h.label == label) return true;
    }
    return false;
}

TEST_CASE(idle_status_shows_bound_keys_and_solve_state) {
    CHECK_LOAD("line.slvs");
    std::string err;
    keymap::Keymap km = *ModernPreset(&err);
    StatusModel m = BuildStatus(km, "", {});
    CHECK_TRUE(m.tool.empty());
    CHECK_TRUE(HasHint(m, "Space", "Commands"));
    CHECK_TRUE(HasHint(m, "Shift+A", "Add"));
    Group *g = SK.GetGroup(SS.GW.activeGroup);
    CHECK_EQ_STR(m.group, g->name);
    CHECK_TRUE(m.dof == g->solved.dof);
    CHECK_EQ_STR(m.solve, "Solved");
    CHECK_FALSE(m.solveProblem);
}

TEST_CASE(pending_and_modal_text_win) {
    CHECK_LOAD("line.slvs");
    keymap::Keymap km;
    SS.GW.pending.operation   = GraphicsWindow::Pending::COMMAND;
    SS.GW.pending.description = "click first point of line";
    StatusModel m = BuildStatus(km, "", {});
    CHECK_EQ_STR(m.tool, "click first point of line");
    m = BuildStatus(km, "Grab \xC2\xB7 X", { { "Esc", "Cancel" } });
    CHECK_EQ_STR(m.tool, "Grab \xC2\xB7 X");
    CHECK_TRUE(HasHint(m, "Esc", "Cancel"));
    SS.GW.ClearPending();
}

TEST_CASE(unbound_hints_are_left_out) {
    CHECK_LOAD("line.slvs");
    keymap::Keymap km;
    StatusModel m = BuildStatus(km, "", {});
    CHECK_TRUE(m.hints.empty());
}

TEST_CASE(hints_prefer_unmodified_chords) {
    CHECK_LOAD("line.slvs");
    std::string err;
    keymap::Keymap km = *ModernPreset(&err); // unselect-all: Alt+A, Esc
    SS.GW.pending.operation   = GraphicsWindow::Pending::COMMAND;
    SS.GW.pending.description = "click";
    StatusModel m = BuildStatus(km, "", {});
    CHECK_TRUE(HasHint(m, "Esc", "Cancel"));
    SS.GW.ClearPending();
}

TEST_CASE(touch_status_shows_gestures_not_keys) {
    CHECK_LOAD("line.slvs");
    std::string err;
    keymap::Keymap km = *ModernPreset(&err);
    StatusModel m = BuildStatus(km, "", {}, /*touch=*/true);
    CHECK_FALSE(HasHint(m, "Space", "Commands"));
    CHECK_TRUE(HasHint(m, "2 fingers", "Pan, zoom"));
    CHECK_TRUE(HasHint(m, "3", "Orbit"));
    CHECK_TRUE(HasHint(m, "Hold", "Menu"));
    SS.GW.pending.operation   = GraphicsWindow::Pending::COMMAND;
    SS.GW.pending.description = "click first point of line";
    m = BuildStatus(km, "", {}, /*touch=*/true);
    CHECK_EQ_STR(m.tool, "click first point of line");
    CHECK_TRUE(HasHint(m, "Hold", "Cancel"));
    CHECK_FALSE(HasHint(m, "Esc", "Cancel"));
    SS.GW.pending.operation = GraphicsWindow::Pending::NONE;
}
