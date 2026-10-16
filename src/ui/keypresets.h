//-----------------------------------------------------------------------------
// The built-in keymap presets: "Classic" (generated from the core menu accelerators) and
// "Modern" (Blender-style, from res/keymaps/modern.keymap).
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_UI_KEYPRESETS_H
#define SOLVESPACE_UI_KEYPRESETS_H

#include <optional>
#include <string>
#include "keymap.h"

namespace SolveSpace {
namespace Ui {

keymap::Keymap ClassicPreset();
std::optional<keymap::Keymap> ModernPreset(std::string *error);

}
}

#endif
