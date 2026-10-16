//-----------------------------------------------------------------------------
// Translation of SDL key events and legacy menu accelerators into keymap chords.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_SDL_KEYCHORD_H
#define SOLVESPACE_SDL_KEYCHORD_H

#include <optional>
#include <SDL3/SDL_keycode.h>
#include "keymap.h"
#include "solvespace.h"

namespace SolveSpace {
namespace Platform {

// base     = keycode with no modifiers (SDL_GetKeyFromScancode(sc, SDL_KMOD_NONE, false))
// produced = keycode with the event's modifiers (SDL_GetKeyFromScancode(sc, mod, false))
std::optional<keymap::Chord> ChordFromKey(SDL_Keycode base, SDL_Keycode produced, SDL_Keymod mod);
std::optional<keymap::Chord> ChordFromAccelerator(const KeyboardEvent &accel);

}
}

#endif
