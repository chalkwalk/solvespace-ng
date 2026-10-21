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
    r.SetKeymap(*keymap::Keymap::Parse(
        "line-segment: L\nsave: Ctrl+S\nunselect-all: Esc\nselect-all: Alt+A\n", &err));
    int fired = 0;
    r.onCommand = [&](Command) { fired++; };
    CHECK_TRUE(r.Route(C("L"), true) == RouteResult::SUPPRESSED);
    CHECK_TRUE(fired == 0);
    CHECK_TRUE(r.Route(C("Ctrl+S"), true) == RouteResult::COMMAND);
    // Esc belongs to the focused field (closes the palette, cancels the editor).
    CHECK_TRUE(r.Route(C("Esc"), true) == RouteResult::SUPPRESSED);
    CHECK_TRUE(fired == 1);
    CHECK_TRUE(r.Route(C("Esc"), false) == RouteResult::COMMAND);
    CHECK_TRUE(r.Route(C("Alt+A"), true) == RouteResult::SUPPRESSED); // AltGr text on Windows
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
    CHECK_TRUE(*km.Lookup(C("A")) == "arc"); // the modern preset, not the user file
    err.clear();
    km = LoadUserKeymap("classic", "", &err);
    CHECK_TRUE(err.empty());
    CHECK_TRUE(*km.Lookup(C("S")) == "line-segment");
    km = LoadUserKeymap("modern", "undo: F9\n", &err);
    CHECK_TRUE(*km.Lookup(C("F9")) == "undo");
    CHECK_TRUE(km.Lookup(C("A")) == nullptr); // a valid user file replaces the preset
}

TEST_CASE(user_keymap_with_unknown_command_falls_back) {
    std::string err;
    keymap::Keymap km = LoadUserKeymap("modern", "undo: F9\nline-segmnet: L\n", &err);
    CHECK_TRUE(err.find("line 2") != std::string::npos);
    CHECK_TRUE(err.find("line-segmnet") != std::string::npos);
    CHECK_TRUE(*km.Lookup(C("A")) == "arc");
}

TEST_CASE(user_keymap_without_bindings_uses_preset) {
    std::string err;
    keymap::Keymap km = LoadUserKeymap("modern", "# just a comment\n\n   \n", &err);
    CHECK_TRUE(err.empty());
    CHECK_TRUE(*km.Lookup(C("Space")) == "ui.command-palette");
}

TEST_CASE(modal_tools_get_chords_first) {
    std::string err;
    KeyRouter r;
    r.SetKeymap(*keymap::Keymap::Parse("delete: X\nline-segment: L\n", &err));
    int fired = 0;
    r.onCommand = [&](Command) { fired++; };
    r.modal = [](const keymap::Chord &c) { return c.key == "x"; };
    CHECK_TRUE(r.Route(C("X"), false) == RouteResult::MODAL);
    CHECK_TRUE(fired == 0);
    CHECK_TRUE(r.Route(C("L"), false) == RouteResult::COMMAND); // the tool declined it
}

TEST_CASE(text_input_beats_modal_tools) {
    std::string err;
    KeyRouter r;
    r.SetKeymap(*keymap::Keymap::Parse("delete: X\n", &err));
    int modalCalls = 0;
    r.modal = [&](const keymap::Chord &) { modalCalls++; return true; };
    CHECK_TRUE(r.Route(C("X"), true) == RouteResult::SUPPRESSED);
    CHECK_TRUE(modalCalls == 0);
}

TEST_CASE(key_repeat_reaches_only_the_modal_tool) {
    std::string err;
    KeyRouter r;
    r.SetKeymap(*keymap::Keymap::Parse("line-segment: L\nundo: Ctrl+Z\n", &err));
    int commands = 0, modalKeys = 0;
    bool toolActive = false;
    r.onCommand = [&](Command) { commands++; };
    r.modal     = [&](const keymap::Chord &) { if(toolActive) modalKeys++; return toolActive; };
    // No tool: a held unmodified key must not start the same command over and over.
    CHECK_TRUE(r.RouteRepeat(C("L"), false) == RouteResult::SUPPRESSED);
    CHECK_TRUE(commands == 0);
    // Ctrl chords repeat as before (holding Ctrl+Z undoes step after step).
    CHECK_TRUE(r.RouteRepeat(C("Ctrl+Z"), false) == RouteResult::COMMAND);
    // A typed field in a tool: holding Backspace keeps deleting.
    toolActive = true;
    CHECK_TRUE(r.RouteRepeat(C("Backspace"), false) == RouteResult::MODAL);
    CHECK_TRUE(modalKeys == 1);
    // While a text field has the keyboard, the field handles repeats itself.
    CHECK_TRUE(r.RouteRepeat(C("Backspace"), true) == RouteResult::SUPPRESSED);
}

TEST_CASE(a_separate_modal_hook_can_take_held_keys) {
    std::string err;
    KeyRouter r;
    r.SetKeymap(*keymap::Keymap::Parse("ui.pie-add: Shift+A\n", &err));
    int fresh = 0, held = 0;
    r.modal       = [&](const keymap::Chord &) { fresh++; return true; };
    r.modalRepeat = [&](const keymap::Chord &) { held++; return true; };
    CHECK_TRUE(r.Route(C("Shift+A"), false) == RouteResult::MODAL);
    CHECK_TRUE(r.RouteRepeat(C("Shift+A"), false) == RouteResult::MODAL);
    CHECK_TRUE(fresh == 1 && held == 1);
}

TEST_CASE(text_input_keeps_its_editing_chords) {
    // Ctrl+A in a field selects its text, not the drawing; likewise copy, paste, undo.
    std::string err;
    KeyRouter r;
    r.SetKeymap(*keymap::Keymap::Parse("select-all: Ctrl+A\ncopy: Ctrl+C\npaste: Ctrl+V\n"
                                       "cut: Ctrl+X\nundo: Ctrl+Z\nredo: Ctrl+Shift+Z, Ctrl+Y\n"
                                       "save: Ctrl+S\n", &err));
    int fired = 0;
    r.onCommand = [&](Command) { fired++; };
    for(const char *c : { "Ctrl+A", "Ctrl+C", "Ctrl+V", "Ctrl+X", "Ctrl+Z", "Ctrl+Shift+Z",
                          "Ctrl+Y" }) {
        CHECK_TRUE(r.Route(C(c), true) == RouteResult::SUPPRESSED);
    }
    CHECK_TRUE(fired == 0);
    CHECK_TRUE(r.Route(C("Ctrl+S"), true) == RouteResult::COMMAND); // not a text edit
    CHECK_TRUE(r.Route(C("Ctrl+A"), false) == RouteResult::COMMAND);
}

TEST_CASE(an_open_menu_still_passes_ctrl_z) {
    std::string err;
    KeyRouter r;
    r.SetKeymap(*keymap::Keymap::Parse("undo: Ctrl+Z\nline-segment: L\n", &err));
    CHECK_TRUE(r.Route(C("Ctrl+Z"), true, /*inMenu=*/true) == RouteResult::COMMAND);
    CHECK_TRUE(r.Route(C("L"), true, /*inMenu=*/true) == RouteResult::SUPPRESSED);
}
