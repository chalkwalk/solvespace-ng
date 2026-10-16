//-----------------------------------------------------------------------------
// Number fields typed into the HUD while a modal tool runs (Blender-style "type a value").
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_UI_TYPEDFIELDS_H
#define SOLVESPACE_UI_TYPEDFIELDS_H

#include <optional>
#include <string>
#include <vector>
#include "keymap.h"

namespace SolveSpace {
namespace Ui {

class TypedFields {
public:
    explicit TypedFields(std::vector<std::string> labels);
    // Digits, '.', '-', the numpad, Backspace, Tab / Shift+Tab. True if consumed.
    bool HandleChord(const keymap::Chord &c);
    size_t Count() const { return labels.size(); }
    int Active() const { return active; }
    const std::string &Label(int i) const { return labels[i]; }
    const std::string &Text(int i) const { return texts[i]; }
    bool Any() const;
    void Clear();
    std::optional<double> ValueMm(int i) const;  // display units -> mm
    std::optional<double> ValueRaw(int i) const; // no unit conversion (angles)

private:
    std::vector<std::string> labels, texts;
    int                      active = 0;
};

}
}

#endif
