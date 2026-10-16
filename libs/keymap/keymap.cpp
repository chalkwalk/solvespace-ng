// SPDX-License-Identifier: MIT
#include "keymap.h"

#include <algorithm>
#include <cctype>

namespace keymap {

static const char *const NamedKeys[] = {
    "Space", "Tab", "Esc", "Enter", "Backspace", "Delete", "Insert", "Home", "End",
    "PageUp", "PageDown", "Up", "Down", "Left", "Right", "Plus",
    "Num0", "Num1", "Num2", "Num3", "Num4", "Num5", "Num6", "Num7", "Num8", "Num9",
    "Num.", "Num/", "Num*", "Num-", "NumEnter",
};

static std::string Lower(std::string_view s) {
    std::string r(s);
    for(char &c : r) c = (char)std::tolower((unsigned char)c);
    return r;
}

static std::optional<std::string> NormalizeKey(std::string_view k) {
    if(k.empty()) return std::nullopt;
    if(k.size() == 1) {
        unsigned char c = (unsigned char)k[0];
        if(std::isalpha(c)) return std::string(1, (char)std::tolower(c));
        if(std::isgraph(c) && c != '+') return std::string(k);
        return std::nullopt;
    }
    std::string lk = Lower(k);
    for(const char *name : NamedKeys) {
        if(Lower(name) == lk) return std::string(name);
    }
    if(lk[0] == 'f' && lk.size() <= 3 &&
       std::all_of(lk.begin() + 1, lk.end(), [](char c) { return std::isdigit((unsigned char)c); })) {
        int n = std::stoi(lk.substr(1));
        if(n >= 1 && n <= 24) return "F" + std::to_string(n);
    }
    return std::nullopt;
}

std::optional<Chord> ParseChord(std::string_view text) {
    Chord chord;
    std::vector<std::string_view> parts;
    size_t start = 0;
    // '+' as a key is spelled "Plus", so every '+' is a separator.
    while(true) {
        size_t plus = text.find('+', start);
        parts.push_back(text.substr(start, plus == std::string_view::npos ? plus : plus - start));
        if(plus == std::string_view::npos) break;
        start = plus + 1;
    }
    for(size_t i = 0; i + 1 < parts.size(); i++) {
        std::string m = Lower(parts[i]);
        bool *flag = m == "ctrl" ? &chord.ctrl : m == "shift" ? &chord.shift
                   : m == "alt"  ? &chord.alt  : nullptr;
        if(flag == nullptr || *flag) return std::nullopt;
        *flag = true;
    }
    std::optional<std::string> key = NormalizeKey(parts.back());
    if(!key) return std::nullopt;
    chord.key = *key;
    return chord;
}

std::string FormatChord(const Chord &chord) {
    std::string out;
    if(chord.ctrl)  out += "Ctrl+";
    if(chord.alt)   out += "Alt+";
    if(chord.shift) out += "Shift+";
    if(chord.key.size() == 1 && std::isalpha((unsigned char)chord.key[0])) {
        out += (char)std::toupper((unsigned char)chord.key[0]);
    } else {
        out += chord.key;
    }
    return out;
}

bool Keymap::Bind(const std::string &command, const Chord &chord, std::string *error) {
    std::string name = FormatChord(chord);
    auto it = commandByChord.find(name);
    if(it != commandByChord.end()) {
        if(error) *error = name + " is already bound to " + it->second;
        return false;
    }
    commandByChord[name] = command;
    chordsByCommand[command].push_back(chord);
    return true;
}

const std::string *Keymap::Lookup(const Chord &chord) const {
    auto it = commandByChord.find(FormatChord(chord));
    return it == commandByChord.end() ? nullptr : &it->second;
}

std::vector<Chord> Keymap::ChordsFor(const std::string &command) const {
    auto it = chordsByCommand.find(command);
    return it == chordsByCommand.end() ? std::vector<Chord>{} : it->second;
}

std::vector<std::string> Keymap::Commands() const {
    std::vector<std::string> out;
    for(const auto &kv : chordsByCommand) out.push_back(kv.first);
    return out;
}

static std::string_view Trim(std::string_view s) {
    while(!s.empty() && std::isspace((unsigned char)s.front())) s.remove_prefix(1);
    while(!s.empty() && std::isspace((unsigned char)s.back()))  s.remove_suffix(1);
    return s;
}

std::optional<Keymap> Keymap::Parse(std::string_view text, std::string *error) {
    Keymap km;
    int lineNo = 0;
    size_t pos = 0;
    while(pos <= text.size()) {
        size_t nl = text.find('\n', pos);
        std::string_view line = text.substr(pos, nl == std::string_view::npos ? nl : nl - pos);
        pos = (nl == std::string_view::npos) ? text.size() + 1 : nl + 1;
        lineNo++;
        size_t hash = line.find('#');
        if(hash != std::string_view::npos) line = line.substr(0, hash);
        line = Trim(line);
        if(line.empty()) continue;
        auto fail = [&](const std::string &why) -> std::optional<Keymap> {
            if(error) *error = "line " + std::to_string(lineNo) + ": " + why;
            return std::nullopt;
        };
        size_t colon = line.find(':');
        if(colon == std::string_view::npos) return fail("expected '<command>: <chord>[, ...]'");
        std::string command(Trim(line.substr(0, colon)));
        if(command.empty()) return fail("missing command name");
        std::string_view rest = line.substr(colon + 1);
        while(true) {
            // A comma is the "," key itself, not a separator, when it directly follows '+'
            // ("Ctrl+,") or when nothing precedes it in the current chord (a bare ",").
            size_t comma = rest.find(',');
            while(comma != std::string_view::npos &&
                  ((comma > 0 && rest[comma - 1] == '+') || Trim(rest.substr(0, comma)).empty())) {
                comma = rest.find(',', comma + 1);
            }
            std::string_view item = Trim(rest.substr(0, comma));
            auto chord = ParseChord(item);
            if(!chord) return fail("bad chord '" + std::string(item) + "'");
            std::string bindError;
            if(!km.Bind(command, *chord, &bindError)) return fail(bindError);
            if(comma == std::string_view::npos) break;
            rest = rest.substr(comma + 1);
        }
    }
    return km;
}

std::string Keymap::Serialize() const {
    std::string out;
    for(const auto &kv : chordsByCommand) {
        out += kv.first + ":";
        for(size_t i = 0; i < kv.second.size(); i++) {
            out += (i == 0 ? " " : ", ") + FormatChord(kv.second[i]);
        }
        out += "\n";
    }
    return out;
}

}
