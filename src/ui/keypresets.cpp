//-----------------------------------------------------------------------------
// The built-in keymap presets.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "commandnames.h"
#include "platform/sdl/keychord.h"
#include "ui/keypresets.h"

namespace SolveSpace {
namespace Ui {

keymap::Keymap ClassicPreset() {
    keymap::Keymap km;
    std::string err;
    for(Command cmd : GraphicsWindow::MenuCommands()) {
        auto chord = Platform::ChordFromAccelerator(SS.GW.AcceleratorForCommand(cmd));
        if(!chord) continue;
        if(!km.Bind(CommandName(cmd), *chord, &err)) dbp("classic keymap: %s", err.c_str());
    }
    // GraphicsWindow::KeyboardEvent's hard-coded extras.
    km.Bind("unselect-all", *keymap::ParseChord("Backspace"), &err);
    km.Bind("zoom-in", *keymap::ParseChord("="), &err);
    // F3 is Nearest Isometric View in Classic, so the palette gets a free chord instead.
    km.Bind("ui.command-palette", *keymap::ParseChord("Ctrl+K"), &err);
    return km;
}

std::optional<keymap::Keymap> ModernPreset(std::string *error) {
    size_t size;
    const char *data = (const char *)Platform::LoadResource("keymaps/modern.keymap", &size);
    return keymap::Keymap::Parse(std::string_view(data, size), error);
}

}
}
