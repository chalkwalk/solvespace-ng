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

enum class RouteResult { UNBOUND, UI_ACTION, COMMAND, SUPPRESSED };

class KeyRouter {
public:
    std::function<void(const std::string &uiAction)> onUiAction;
    std::function<void(Command)>                     onCommand; // default: SS.GW.ActivateCommand

    void SetKeymap(keymap::Keymap km);
    const keymap::Keymap &Keymap() const;
    // With textInputActive, only chords with Ctrl/Alt, Esc and F-keys are routed; everything
    // else is SUPPRESSED so typing never triggers commands.
    RouteResult Route(const keymap::Chord &chord, bool textInputActive);

private:
    keymap::Keymap map;
};

// The preset ("modern" | "classic"); a non-empty, valid user keymap file replaces it. On a
// parse error the preset is returned and *error describes the problem.
keymap::Keymap LoadUserKeymap(const std::string &preset, const std::string &userFileText,
                              std::string *error);

}
}

#endif
