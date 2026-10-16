#include "solvespace.h"
#include <SDL3/SDL_keycode.h>
#include "platform/sdl/keychord.h"
#include "ui/keypresets.h"
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
        if(!hasAccel) continue;
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

TEST_CASE(modern_preset_is_valid) {
    std::string err;
    auto modern = SolveSpace::Ui::ModernPreset(&err);
    CHECK_TRUE(modern.has_value());
    for(const std::string &name : modern->Commands()) {
        bool known = name.rfind("ui.", 0) == 0 || CommandFromName(name) != Command::NONE;
        CHECK_TRUE(known);
    }
    CHECK_TRUE(*modern->Lookup(*keymap::ParseChord("A")) == "select-all");
    CHECK_TRUE(*modern->Lookup(*keymap::ParseChord("Shift+C")) == "arc");
    CHECK_TRUE(*modern->Lookup(*keymap::ParseChord("Space")) == "ui.command-palette");
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
