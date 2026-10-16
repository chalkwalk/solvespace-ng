//-----------------------------------------------------------------------------
// Routes key chords through the active keymap to UI actions or core Commands.
//-----------------------------------------------------------------------------
#include "ui/keyrouter.h"
#include "ui/keypresets.h"
#include "commandnames.h"

namespace SolveSpace {
namespace Ui {

void KeyRouter::SetKeymap(keymap::Keymap km) {
    map = std::move(km);
}

const keymap::Keymap &KeyRouter::Keymap() const {
    return map;
}

static bool AllowedDuringTextInput(const keymap::Chord &c) {
    if(c.ctrl) return true;
    return c.key.size() >= 2 && c.key[0] == 'F' && isdigit((unsigned char)c.key[1]);
}

RouteResult KeyRouter::Route(const keymap::Chord &chord, bool textInputActive) {
    if(textInputActive && !AllowedDuringTextInput(chord)) return RouteResult::SUPPRESSED;
    const std::string *name = map.Lookup(chord);
    if(!name) return RouteResult::UNBOUND;
    if(name->rfind("ui.", 0) == 0) {
        if(onUiAction) onUiAction(*name);
        return RouteResult::UI_ACTION;
    }
    Command cmd = CommandFromName(*name);
    if(cmd == Command::NONE) {
        dbp("keymap: unknown command '%s'", name->c_str());
        return RouteResult::UNBOUND;
    }
    if(onCommand) onCommand(cmd); else SS.GW.ActivateCommand(cmd);
    return RouteResult::COMMAND;
}

bool IsKnownUiAction(const std::string &name) {
    return name == "ui.command-palette" || name == "ui.toggle-menu-bar" ||
           name == "ui.toggle-theme";
}

keymap::Keymap LoadUserKeymap(const std::string &preset, const std::string &userFileText,
                              std::string *error) {
    keymap::Keymap base;
    if(preset == "classic") {
        base = ClassicPreset();
    } else {
        std::string presetError;
        auto modern = ModernPreset(&presetError);
        ssassert(modern.has_value(), "built-in modern keymap must parse");
        base = *modern;
    }
    if(userFileText.empty()) return base;
    std::string parseError;
    auto isKnown = [](const std::string &name) {
        return IsKnownUiAction(name) || CommandFromName(name) != Command::NONE;
    };
    auto user = keymap::Keymap::Parse(userFileText, &parseError, isKnown);
    if(!user) {
        std::string message = "keymap.txt " + parseError;
        dbp("%s; using the %s preset", message.c_str(), preset.c_str());
        if(error) *error = message;
        return base;
    }
    // A file with only comments or blank lines would otherwise unbind everything, including
    // the palette, leaving no way back.
    if(user->Commands().empty()) return base;
    return *user;
}

}
}
