// SPDX-License-Identifier: MIT
#include "pie.h"

#include <cmath>

namespace pie {

static const double Tau = 6.283185307179586;

int SectorAt(float dx, float dy, int count, float deadZone) {
    if(count < 1 || std::hypot(dx, dy) < deadZone) return -1;
    double a = std::atan2((double)dx, (double)-dy); // 0 = up, clockwise positive
    if(a < 0) a += Tau;
    double sector = Tau / count;
    return (int)std::floor((a + sector / 2) / sector) % count;
}

void ItemDirection(int i, int count, float *x, float *y) {
    double a = Tau * i / count;
    *x = (float)std::sin(a);
    *y = (float)-std::cos(a);
}

KeyResult KeyAction(std::string_view key, bool modifiers, int count) {
    if(modifiers) return {};
    std::string_view digit = key;
    if(digit.size() == 4 && digit.substr(0, 3) == "Num") digit.remove_prefix(3);
    if(digit.size() == 1 && digit[0] >= '1' && digit[0] <= '9') {
        int i = digit[0] - '1';
        if(i < count) return { KeyKind::PICK, i };
        return {};
    }
    if(key == "Enter" || key == "NumEnter" || key == "Space") return { KeyKind::CONFIRM, -1 };
    if(key == "Esc") return { KeyKind::CANCEL, -1 };
    return {};
}

}
