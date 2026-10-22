#include "solvespace.h"
#include <SDL3/SDL_keycode.h>
#include "platform/sdl/keychord.h"
#include "ui/keypresets.h"
#include "ui/menulayout.h"
#include "commandnames.h"

#include "harness.h"

using namespace SolveSpace::Platform;

static std::string F(std::optional<keymap::Chord> c) {
    return c ? keymap::FormatChord(*c) : std::string("<none>");
}

TEST_CASE(letters_digits_and_shift) {
    CHECK_EQ_STR(F(ChordFromKey(SDLK_S, SDLK_S, SDL_KMOD_LCTRL)), "Ctrl+S");
    CHECK_EQ_STR(F(ChordFromKey(SDLK_S, SDLK_S, SDL_KMOD_LSHIFT)), "Shift+S");
    CHECK_EQ_STR(F(ChordFromKey(SDLK_3, SDLK_HASH, SDL_KMOD_LSHIFT)), "Shift+3");
}

TEST_CASE(shifted_symbols_use_produced_char) {
    CHECK_EQ_STR(F(ChordFromKey(SDLK_PERIOD, SDLK_GREATER, SDL_KMOD_LSHIFT)), ">");
    CHECK_EQ_STR(F(ChordFromKey(SDLK_EQUALS, SDLK_PLUS, SDL_KMOD_LSHIFT)), "Plus");
    CHECK_EQ_STR(F(ChordFromKey(SDLK_KP_PLUS, SDLK_KP_PLUS, SDL_KMOD_NONE)), "Plus");
    CHECK_EQ_STR(F(ChordFromKey(SDLK_EQUALS, SDLK_EQUALS, SDL_KMOD_NONE)), "=");
}

TEST_CASE(named_keys) {
    CHECK_EQ_STR(F(ChordFromKey(SDLK_TAB, SDLK_TAB, SDL_KMOD_NONE)), "Tab");
    CHECK_EQ_STR(F(ChordFromKey(SDLK_ESCAPE, SDLK_ESCAPE, SDL_KMOD_NONE)), "Esc");
    CHECK_EQ_STR(F(ChordFromKey(SDLK_F11, SDLK_F11, SDL_KMOD_LCTRL)), "Ctrl+F11");
    CHECK_EQ_STR(F(ChordFromKey(SDLK_KP_5, SDLK_KP_5, SDL_KMOD_NONE)), "Num5");
    CHECK_EQ_STR(F(ChordFromKey(SDLK_SPACE, SDLK_SPACE, SDL_KMOD_NONE)), "Space");
}

TEST_CASE(modifier_only_presses_are_ignored) {
    CHECK_EQ_STR(F(ChordFromKey(SDLK_LSHIFT, SDLK_LSHIFT, SDL_KMOD_LSHIFT)), "<none>");
}

TEST_CASE(altgr_character_is_plain) {
    // German layout: AltGr+Q produces '@'; SDL reports Ctrl+Alt on Windows.
    CHECK_EQ_STR(F(ChordFromKey(SDLK_Q, SDLK_AT, (SDL_Keymod)(SDL_KMOD_LCTRL | SDL_KMOD_RALT))),
                 "@");
}

TEST_CASE(classic_preset_covers_every_accelerator) {
    keymap::Keymap classic = SolveSpace::Ui::ClassicPreset();
    for(Command cmd : GraphicsWindow::MenuCommands()) {
        Platform::KeyboardEvent accel = SS.GW.AcceleratorForCommand(cmd);
        bool hasAccel = accel.key == Platform::KeyboardEvent::Key::FUNCTION || accel.chr != 0;
        if(!hasAccel || SolveSpace::Ui::RemovedFromMenus(cmd)) continue;
        // Every accelerator chord reaches some command. Upstream's menu table binds
        // Ctrl+Shift+S to both save-as and stop-tracing; like GraphicsWindow::KeyboardEvent,
        // the first menu entry wins.
        auto chord = Platform::ChordFromAccelerator(accel);
        CHECK_TRUE(chord.has_value() && classic.Lookup(*chord) != nullptr);
    }
    CHECK_TRUE(*classic.Lookup(*keymap::ParseChord("Ctrl+Shift+S")) == "save-as");
    CHECK_TRUE(*classic.Lookup(*keymap::ParseChord(">")) == "show-grid");
    CHECK_TRUE(*classic.Lookup(*keymap::ParseChord("Shift+3")) == "group-sketch-3d");
    CHECK_TRUE(*classic.Lookup(*keymap::ParseChord("=")) == "zoom-in");
    CHECK_TRUE(*classic.Lookup(*keymap::ParseChord("Backspace")) == "unselect-all");
    CHECK_TRUE(*classic.Lookup(*keymap::ParseChord("F3")) == "nearest-iso");
    CHECK_TRUE(*classic.Lookup(*keymap::ParseChord("Ctrl+K")) == "ui.command-palette");
}

TEST_CASE(classic_preset_binds_nothing_ng_removed) {
    // Tab showed the old property browser; NG has no browser now (M1c task 10).
    keymap::Keymap classic = SolveSpace::Ui::ClassicPreset();
    CHECK_TRUE(classic.ChordsFor("show-property-browser").empty());
}

TEST_CASE(modern_preset_is_valid) {
    std::string err;
    auto modern = SolveSpace::Ui::ModernPreset(&err);
    CHECK_TRUE(modern.has_value());
    for(const std::string &name : modern->Commands()) {
        bool known = name.rfind("ui.", 0) == 0 || CommandFromName(name) != Command::NONE;
        CHECK_TRUE(known);
    }
    CHECK_TRUE(*modern->Lookup(*keymap::ParseChord("Space")) == "ui.command-palette");
}

TEST_CASE(modern_preset_has_the_keymap_pass_keys) {
    // Owner review 4 (2026-10-17, K1-K6, all option a), then the keymap pass after M3-M5
    // (2026-10-22, docs/research/keymap-pass.md, P1-P6 all option a): bare = the command,
    // Shift = its counterpart, Alt = its variant, Ctrl = the document; pies for big families.
    std::string err;
    auto modern = SolveSpace::Ui::ModernPreset(&err);
    CHECK_TRUE(modern.has_value());
    if(!modern) return;
    const char *expected[][2] = {
        // Sketch tools
        { "L", "line-segment" }, { "R", "rectangle" }, { "C", "circle" }, { "A", "arc" },
        { "Alt+A", "tangent-arc" }, { "P", "datum-point" }, { "S", "cubic-spline" },
        { "Ctrl+A", "select-all" }, { "Esc", "unselect-all" },
        // Constraints; the constrain pie on K
        { "H", "constrain-horizontal" }, { "V", "constrain-vertical" },
        { "I", "constrain-on-entity" }, { "B", "constrain-parallel" },
        { "T", "constrain-parallel" }, { "Alt+B", "constrain-perpendicular" },
        { "Q", "constrain-equal" }, { "M", "constrain-midpoint" },
        { "Alt+M", "constrain-symmetric" }, { "U", "other-supplementary-angle" },
        { "D", "ui.smart-dimension" }, { "Shift+D", "constrain-reference-distance" },
        { "Alt+D", "constrain-angle" }, { "K", "ui.pie-constrain" },
        // Construction and delete
        { "X", "toggle-construction" }, { "Shift+X", "split-curves" },
        { "Delete", "delete" }, { "Backspace", "delete" },
        // Hiding: Y, its counterpart and its variant
        { "Y", "ui.hide" }, { "Shift+Y", "ui.unhide-all" }, { "Alt+Y", "ui.isolate" },
        // Solids and features
        { "E", "group-extrude" }, { "Shift+E", "group-extrude-cut" },
        { "Alt+E", "group-revolve" }, { "F", "group-fillet" }, { "Alt+F", "group-chamfer" },
        { "Shift+H", "group-hole" }, { "W", "align-to-workplane" },
        { "Alt+W", "group-sketch-workplane" },
        // Parts and joints
        { "J", "ui.pie-joint" }, { "Shift+J", "design-pose" }, { "Ctrl+Shift+N", "part-new" },
        // Panels
        { "N", "ui.toggle-properties" }, { "O", "ui.toggle-outliner" },
        { "F8", "ui.toggle-problems" },
        // Views
        { "Shift+1", "ui.view-front" }, { "Shift+2", "ui.view-back" },
        { "Shift+3", "ui.view-left" }, { "Shift+4", "ui.view-right" },
        { "Shift+5", "ui.view-top" }, { "Shift+6", "ui.view-bottom" },
        { "Shift+7", "nearest-iso" }, { "Num1", "ui.view-front" }, { "Shift+A", "ui.pie-add" },
    };
    for(auto &e : expected) {
        const std::string *cmd = modern->Lookup(*keymap::ParseChord(e[0]));
        if(!cmd || *cmd != e[1]) {
            dbp("%s: expected %s, got %s", e[0], e[1], cmd ? cmd->c_str() : "<none>");
        }
        CHECK_TRUE(cmd && *cmd == e[1]);
    }
    // Free now: the Shift chords that only named another command on the letter, the old Alt
    // ones, and the bare keys of the moved pies.
    for(const char *gone : { "Shift+F", "Shift+L", "Shift+S", "Shift+R", "Shift+I", "Shift+W",
                             "Shift+T", "Shift+P", "Shift+G", "Shift+C", "Shift+B", "Alt+C",
                             "Alt+R", "Alt+H", "Shift+N", "Z" }) {
        if(modern->Lookup(*keymap::ParseChord(gone))) dbp("%s is still bound", gone);
        CHECK_TRUE(modern->Lookup(*keymap::ParseChord(gone)) == nullptr);
    }
}

TEST_CASE(altgr_on_linux_and_windows_is_text_not_a_chord) {
    // Linux reports AltGr as MODE, Windows as RALT; the caller passes the AltGr-layer keycode.
    CHECK_EQ_STR(F(ChordFromKey(SDLK_Q, SDLK_AT, SDL_KMOD_MODE)), "@");
    CHECK_EQ_STR(F(ChordFromKey(SDLK_Q, SDLK_AT, SDL_KMOD_RALT)), "@");
    // Polish AltGr+A produces U+0105: plain text, never "Alt+A".
    CHECK_EQ_STR(F(ChordFromKey(SDLK_A, (SDL_Keycode)0x105, SDL_KMOD_RALT)), "<none>");
    // Right Alt on a layout without an AltGr level is an ordinary Alt chord.
    CHECK_EQ_STR(F(ChordFromKey(SDLK_A, SDLK_A, SDL_KMOD_RALT)), "Alt+A");
}

// Chords that desktops (GNOME, KDE, Windows, X) take before the app sees them, so binding one
// makes a command unreachable or does something else entirely (Ctrl+Alt+T opens a terminal).
static const char *const DesktopChords[] = {
    "Ctrl+Alt+T", "Ctrl+Alt+L", "Ctrl+Alt+D", "Ctrl+Alt+Delete", "Ctrl+Alt+Backspace",
    "Ctrl+Alt+Esc", "Ctrl+Alt+Tab", "Ctrl+Alt+Left", "Ctrl+Alt+Right", "Ctrl+Alt+Up",
    "Ctrl+Alt+Down", "Ctrl+Shift+Alt+Left", "Ctrl+Shift+Alt+Right", "Ctrl+Shift+Alt+Up",
    "Ctrl+Shift+Alt+Down", "Alt+Tab", "Alt+Shift+Tab", "Alt+Esc", "Alt+Space", "Alt+`",
    "Alt+F1", "Alt+F2", "Alt+F4", "Alt+F7", "Alt+F8", "Alt+F10", "Ctrl+Esc", "Ctrl+Shift+Esc",
};

static std::vector<std::string> DesktopClashes(const keymap::Keymap &km, bool uiOnly) {
    std::vector<std::string> clashes;
    for(const char *text : DesktopChords) {
        std::optional<keymap::Chord> chord = keymap::ParseChord(text);
        if(!chord) {
            clashes.push_back(std::string("unparsable ") + text);
            continue;
        }
        const std::string *cmd = km.Lookup(*chord);
        if(cmd && (!uiOnly || cmd->rfind("ui.", 0) == 0)) clashes.push_back(*cmd + ": " + text);
    }
    for(int f = 1; f <= 12; f++) {
        std::string text = "Ctrl+Alt+F" + std::to_string(f); // switches virtual terminal
        const std::string *cmd = km.Lookup(*keymap::ParseChord(text));
        if(cmd && (!uiOnly || cmd->rfind("ui.", 0) == 0)) clashes.push_back(*cmd + ": " + text);
    }
    return clashes;
}

TEST_CASE(modern_preset_avoids_desktop_shortcuts) {
    std::string err;
    auto modern = SolveSpace::Ui::ModernPreset(&err);
    CHECK_TRUE(modern.has_value());
    std::vector<std::string> clashes = DesktopClashes(*modern, /*uiOnly=*/false);
    for(const std::string &c : clashes) dbp("desktop shortcut bound: %s", c.c_str());
    CHECK_TRUE(clashes.empty());
}

TEST_CASE(classic_preset_adds_no_desktop_shortcuts) {
    // Classic keeps SolveSpace's own accelerators as they are; the NG actions it adds must not
    // clash.
    std::vector<std::string> clashes =
        DesktopClashes(SolveSpace::Ui::ClassicPreset(), /*uiOnly=*/true);
    for(const std::string &c : clashes) dbp("desktop shortcut bound: %s", c.c_str());
    CHECK_TRUE(clashes.empty());
}
