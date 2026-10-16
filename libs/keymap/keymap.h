// SPDX-License-Identifier: MIT
// Keyboard chords and command keymaps. Independent of SolveSpace.
#ifndef KEYMAP_KEYMAP_H
#define KEYMAP_KEYMAP_H

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
    const std::string *Lookup(const Chord &chord) const;
    std::vector<Chord> ChordsFor(const std::string &command) const;
    std::vector<std::string> Commands() const;

    static std::optional<Keymap> Parse(std::string_view text, std::string *error);
    std::string Serialize() const;

private:
    std::map<std::string, std::string>        commandByChord;  // FormatChord -> command
    std::map<std::string, std::vector<Chord>> chordsByCommand;
};

}

#endif
