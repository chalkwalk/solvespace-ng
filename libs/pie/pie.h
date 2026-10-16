// SPDX-License-Identifier: MIT
// Pie menu geometry and keys. Independent of SolveSpace and of any UI toolkit.
#ifndef PIE_PIE_H
#define PIE_PIE_H

#include <string_view>

namespace pie {

// Item 0 is centred straight up (12 o'clock) and items continue clockwise, each owning an equal
// sector. Screen coordinates: y grows downwards. Returns -1 inside the dead zone or if count < 1.
int SectorAt(float dx, float dy, int count, float deadZone);
void ItemDirection(int i, int count, float *x, float *y);

enum class KeyKind { NONE, PICK, CONFIRM, CANCEL };
struct KeyResult {
    KeyKind kind  = KeyKind::NONE;
    int     index = -1;
};
// "1".."9" / "Num1".."Num9" pick an item; Enter/NumEnter/Space confirm the highlighted one;
// Esc cancels. Keys with modifiers do nothing.
KeyResult KeyAction(std::string_view key, bool modifiers, int count);

}

#endif
