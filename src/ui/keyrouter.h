//-----------------------------------------------------------------------------
// Routes key chords through the active keymap to UI actions ("ui.*") or core Commands.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_UI_KEYROUTER_H
#define SOLVESPACE_UI_KEYROUTER_H

#include <functional>
#include "keymap.h"
#include "solvespace.h"

namespace SolveSpace {
namespace Ui {

enum class RouteResult { UNBOUND, UI_ACTION, COMMAND, SUPPRESSED, MODAL };

class KeyRouter {
public:
    std::function<void(const std::string &uiAction)> onUiAction;
    std::function<void(Command)>                     onCommand; // default: SS.GW.ActivateCommand
    // The active modal tool (pie, grab, typed drawing) sees chords before the keymap; return
    // true to consume one. Never called while a text field has the keyboard.
    std::function<bool(const keymap::Chord &)>       modal;

    void SetKeymap(keymap::Keymap km);
    const keymap::Keymap &Keymap() const;
    // With textInputActive, only Ctrl chords and F-keys are routed; everything else (including
    // Esc, which the focused field handles, and Alt, which may be AltGr text) is SUPPRESSED.
    RouteResult Route(const keymap::Chord &chord, bool textInputActive);
    // A key held down (auto-repeat). Ctrl/Alt chords route as usual; other keys go only to the
    // modal tool (Backspace or digits in a typed field), never to the keymap, so holding a key
    // doesn't start the same tool over and over.
    RouteResult RouteRepeat(const keymap::Chord &chord, bool textInputActive);

private:
    keymap::Keymap map;
};

// The preset ("modern" | "classic"); a non-empty, valid user keymap file replaces it. On a
// parse error the preset is returned and *error describes the problem.
// The "ui.*" actions the UI layer implements.
bool IsKnownUiAction(const std::string &name);

keymap::Keymap LoadUserKeymap(const std::string &preset, const std::string &userFileText,
                              std::string *error);

}
}

#endif
