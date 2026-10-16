#include "solvespace.h"
#include "ui/keyrouter.h"

#include "harness.h"

using namespace SolveSpace::Ui;

static keymap::Chord C(const char *s) { return *keymap::ParseChord(s); }

TEST_CASE(routes_commands_and_ui_actions) {
    std::string err;
    KeyRouter r;
    r.SetKeymap(*keymap::Keymap::Parse("line-segment: L\nui.command-palette: Space\n", &err));
    Command got = Command::NONE;
    std::string action;
    r.onCommand  = [&](Command c) { got = c; };
    r.onUiAction = [&](const std::string &a) { action = a; };
    CHECK_TRUE(r.Route(C("L"), false) == RouteResult::COMMAND);
    CHECK_TRUE(got == Command::LINE_SEGMENT);
    CHECK_TRUE(r.Route(C("Space"), false) == RouteResult::UI_ACTION);
    CHECK_EQ_STR(action, "ui.command-palette");
    CHECK_TRUE(r.Route(C("Q"), false) == RouteResult::UNBOUND);
}

TEST_CASE(text_input_swallows_chords) {
    std::string err;
    KeyRouter r;
    r.SetKeymap(*keymap::Keymap::Parse("line-segment: L\nsave: Ctrl+S\nunselect-all: Esc\n", &err));
    int fired = 0;
    r.onCommand = [&](Command) { fired++; };
    CHECK_TRUE(r.Route(C("L"), true) == RouteResult::SUPPRESSED);
    CHECK_TRUE(fired == 0);
    CHECK_TRUE(r.Route(C("Ctrl+S"), true) == RouteResult::COMMAND);
    // Esc belongs to the focused field (closes the palette, cancels the editor).
    CHECK_TRUE(r.Route(C("Esc"), true) == RouteResult::SUPPRESSED);
    CHECK_TRUE(fired == 1);
    CHECK_TRUE(r.Route(C("Esc"), false) == RouteResult::COMMAND);
}

TEST_CASE(unknown_command_names_are_unbound) {
    std::string err;
    KeyRouter r;
    r.SetKeymap(*keymap::Keymap::Parse("not-a-command: K\n", &err));
    CHECK_TRUE(r.Route(C("K"), false) == RouteResult::UNBOUND);
}

TEST_CASE(bad_user_keymap_falls_back_to_preset) {
    std::string err;
    keymap::Keymap km = LoadUserKeymap("modern", "save: Ctrl+S\nbroken\n", &err);
    CHECK_TRUE(err.find("line 2") != std::string::npos);
    CHECK_TRUE(*km.Lookup(C("Shift+C")) == "arc"); // the modern preset, not the user file
    err.clear();
    km = LoadUserKeymap("classic", "", &err);
    CHECK_TRUE(err.empty());
    CHECK_TRUE(*km.Lookup(C("S")) == "line-segment");
    km = LoadUserKeymap("modern", "undo: F9\n", &err);
    CHECK_TRUE(*km.Lookup(C("F9")) == "undo");
    CHECK_TRUE(km.Lookup(C("Shift+C")) == nullptr); // a valid user file replaces the preset
}
