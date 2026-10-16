//-----------------------------------------------------------------------------
// Translation of SDL key events and legacy menu accelerators into keymap chords.
//
// Both functions share one normalisation so they always agree: letters and digits use the
// unshifted key and keep Shift; other printable characters use the produced character with
// Shift dropped ("Shift+." is ">"); named keys keep Shift.
//-----------------------------------------------------------------------------
#include <cctype>
#include "platform/sdl/keychord.h"

namespace SolveSpace {
namespace Platform {

static std::optional<std::string> NamedKey(SDL_Keycode k) {
    switch(k) {
        case SDLK_SPACE:       return "Space";
        case SDLK_TAB:         return "Tab";
        case SDLK_ESCAPE:      return "Esc";
        case SDLK_RETURN:      return "Enter";
        case SDLK_BACKSPACE:   return "Backspace";
        case SDLK_DELETE:      return "Delete";
        case SDLK_INSERT:      return "Insert";
        case SDLK_HOME:        return "Home";
        case SDLK_END:         return "End";
        case SDLK_PAGEUP:      return "PageUp";
        case SDLK_PAGEDOWN:    return "PageDown";
        case SDLK_UP:          return "Up";
        case SDLK_DOWN:        return "Down";
        case SDLK_LEFT:        return "Left";
        case SDLK_RIGHT:       return "Right";
        case SDLK_KP_PLUS:     return "Plus";
        case SDLK_KP_MINUS:    return "Num-";
        case SDLK_KP_MULTIPLY: return "Num*";
        case SDLK_KP_DIVIDE:   return "Num/";
        case SDLK_KP_PERIOD:   return "Num.";
        case SDLK_KP_ENTER:    return "NumEnter";
        case SDLK_KP_0:        return "Num0";
        default: break;
    }
    if(k >= SDLK_KP_1 && k <= SDLK_KP_9) return "Num" + std::to_string(1 + (k - SDLK_KP_1));
    if(k >= SDLK_F1 && k <= SDLK_F12) return "F" + std::to_string(1 + (k - SDLK_F1));
    if(k >= SDLK_F13 && k <= SDLK_F24) return "F" + std::to_string(13 + (k - SDLK_F13));
    return std::nullopt;
}

static bool IsModifierKey(SDL_Keycode k) {
    return k == SDLK_LSHIFT || k == SDLK_RSHIFT || k == SDLK_LCTRL || k == SDLK_RCTRL ||
           k == SDLK_LALT || k == SDLK_RALT || k == SDLK_LGUI || k == SDLK_RGUI ||
           k == SDLK_MODE || k == SDLK_CAPSLOCK;
}

std::optional<keymap::Chord> ChordFromKey(SDL_Keycode base, SDL_Keycode produced,
                                          SDL_Keymod mod) {
    if(IsModifierKey(base)) return std::nullopt;
    keymap::Chord c;
#if defined(__APPLE__)
    c.ctrl = (mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0;
#else
    c.ctrl = (mod & SDL_KMOD_CTRL) != 0;
#endif
    c.alt   = (mod & SDL_KMOD_ALT) != 0;
    c.shift = (mod & SDL_KMOD_SHIFT) != 0;

    if(auto named = NamedKey(base)) {
        c.key = *named;
        if(c.key == "Plus") c.shift = false;
        return c;
    }
    bool baseAlnum         = base < 0x80 && std::isalnum((int)base);
    bool producedPrintable = produced >= 0x21 && produced < 0x7f;
    // AltGr (reported as Ctrl+Alt) producing a different printable character: plain char.
    if(c.ctrl && c.alt && producedPrintable && produced != base) {
        keymap::Chord plain;
        plain.key = std::string(1, (char)produced);
        return plain;
    }
    if(baseAlnum) {
        c.key = std::string(1, (char)std::tolower((int)base));
        return c;
    }
    if(producedPrintable) {
        c.shift = false;
        c.key   = (produced == '+') ? "Plus" : std::string(1, (char)produced);
        return c;
    }
    return std::nullopt;
}

std::optional<keymap::Chord> ChordFromAccelerator(const KeyboardEvent &accel) {
    keymap::Chord c;
    c.ctrl  = accel.controlDown;
    c.shift = accel.shiftDown;
    if(accel.key == KeyboardEvent::Key::FUNCTION) {
        if(accel.num < 1 || accel.num > 24) return std::nullopt;
        c.key = "F" + std::to_string(accel.num);
        return c;
    }
    char32_t ch = accel.chr;
    switch(ch) {
        case 0:      return std::nullopt;
        case '\t':   c.key = "Tab";       return c;
        case '\x1b': c.key = "Esc";       return c;
        case '\x7f': c.key = "Delete";    return c;
        case '\b':   c.key = "Backspace"; return c;
        case ' ':    c.key = "Space";     return c;
        case '+':    c.key = "Plus"; c.shift = false; return c;
    }
    if(ch < 0x80 && std::isalnum((int)ch)) {
        c.key = std::string(1, (char)std::tolower((int)ch));
        return c;
    }
    if(ch >= 0x21 && ch < 0x7f) {
        c.key   = std::string(1, (char)ch);
        c.shift = false;
        return c;
    }
    return std::nullopt;
}

}
}
