//-----------------------------------------------------------------------------
// A tiny scripting language for driving solvespace-ng in development and tests:
// SOLVESPACE_NG_SCRIPT="action ui.command-palette; keys line; key Return; wait 5; shot a.png; quit"
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_SDL_DEBUGSCRIPT_H
#define SOLVESPACE_SDL_DEBUGSCRIPT_H

#include <optional>
#include <string>
#include <vector>

namespace SolveSpace {
namespace Platform {

struct DebugStep {
    enum class Kind {
        ACTION, // run a "ui.*" action
        KEYS,   // type text: key down/text/key up per character, all in one event batch
        KEY,    // press and release one key by SDL name ("Return", "Escape", "F3")
        WAIT,   // render this many frames
        SHOT,   // save the back buffer as PNG
        QUIT,
    };
    Kind        kind;
    std::string arg;
    int         frames = 0;
};

// Steps are separated by ';'. Returns nullopt with "step N: ..." in *error on a bad step.
std::optional<std::vector<DebugStep>> ParseDebugScript(const std::string &text,
                                                       std::string *error);

}
}

#endif
