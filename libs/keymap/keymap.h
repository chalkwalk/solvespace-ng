// SPDX-License-Identifier: MIT
// Keyboard chords and command keymaps. Independent of SolveSpace.
#ifndef KEYMAP_KEYMAP_H
#define KEYMAP_KEYMAP_H

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace keymap {

struct Chord {
    std::string key;    // lowercase letter, single printable char, or a named key
    bool        ctrl  = false;
    bool        shift = false;
    bool        alt   = false;

    bool operator==(const Chord &) const = default;
};

std::optional<Chord> ParseChord(std::string_view text);
std::string FormatChord(const Chord &chord);

class Keymap {
public:
    // Adds a binding. Fails (returning false and describing the clash in *error) if the
    // chord is already bound to any command.
    bool Bind(const std::string &command, const Chord &chord, std::string *error);
    // Removes a binding: the command the chord was bound to, or "" if none. A command left
    // with no chord is no longer listed.
    std::string Unbind(const Chord &chord);
    void        UnbindCommand(const std::string &command);
    const std::string *Lookup(const Chord &chord) const;
    std::vector<Chord> ChordsFor(const std::string &command) const;
    std::vector<std::string> Commands() const;

    // If isKnownCommand is given, a binding for a command it rejects is a parse error.
    static std::optional<Keymap> Parse(
        std::string_view text, std::string *error,
        const std::function<bool(const std::string &)> &isKnownCommand = nullptr);
    std::string Serialize() const;

private:
    std::map<std::string, std::string>        commandByChord;  // FormatChord -> command
    std::map<std::string, std::vector<Chord>> chordsByCommand;
};

}

#endif
