#include <set>
#include "solvespace.h"
#include "ui/uiactions.h"
#include "ui/keyrouter.h"

#include "harness.h"

using namespace SolveSpace::Ui;

TEST_CASE(action_list_names_are_unique_ui_names) {
    std::set<std::string> seen;
    for(const UiActionInfo &a : UiActionList()) {
        CHECK_TRUE(std::string(a.name).rfind("ui.", 0) == 0);
        CHECK_TRUE(seen.insert(a.name).second);
        CHECK_TRUE(IsKnownUiAction(a.name));
        CHECK_TRUE(std::string(a.label).find(" \xE2\x80\xBA ") != std::string::npos); // "A › B"
    }
    CHECK_FALSE(IsKnownUiAction("ui.no-such-action"));
}

TEST_CASE(bind_and_run_actions) {
    UiActions acts;
    CHECK_FALSE(acts.Run("ui.toggle-theme")); // unbound
    int runs = 0;
    bool enabled = false;
    acts.Bind("ui.toggle-theme", [&] { runs++; }, [&] { return enabled; });
    CHECK_FALSE(acts.Run("ui.toggle-theme")); // disabled
    enabled = true;
    CHECK_TRUE(acts.Run("ui.toggle-theme"));
    CHECK_TRUE(runs == 1);
    CHECK_FALSE(acts.Run("ui.no-such-action"));
    auto unbound = acts.Unbound();
    CHECK_TRUE(std::find(unbound.begin(), unbound.end(), "ui.toggle-theme") == unbound.end());
    CHECK_TRUE(std::find(unbound.begin(), unbound.end(), "ui.command-palette") != unbound.end());
}

TEST_CASE(run_named_command_dispatches_core_commands) {
    CHECK_LOAD("line.slvs");
    SS.GW.ClearSelection();
    CHECK_TRUE(RunNamedCommand("select-all"));
    CHECK_TRUE(SS.GW.selection.n > 0);
    CHECK_FALSE(RunNamedCommand("not-a-command"));
}

TEST_CASE(palette_entries_include_ui_actions) {
    Actions().Bind("ui.toggle-theme", [] {});
    auto entries = BuildPaletteEntries({});
    bool found = false;
    for(const PaletteEntry &e : entries) {
        if(e.commandName == "ui.toggle-theme") {
            found = true;
            CHECK_TRUE(e.enabled);
            CHECK_TRUE(bool(e.run));
        }
    }
    CHECK_TRUE(found);
}
