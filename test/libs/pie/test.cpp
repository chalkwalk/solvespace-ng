#include <cmath>
#include "pie.h"
#include "harness.h"

TEST_CASE(sector_at_compass_points) {
    CHECK_TRUE(pie::SectorAt(0, -10, 8, 5) == 0);  // up
    CHECK_TRUE(pie::SectorAt(10, 0, 8, 5) == 2);   // right
    CHECK_TRUE(pie::SectorAt(0, 10, 8, 5) == 4);   // down
    CHECK_TRUE(pie::SectorAt(-10, 0, 8, 5) == 6);  // left
    CHECK_TRUE(pie::SectorAt(1, 1, 8, 5) == -1);   // dead zone
    CHECK_TRUE(pie::SectorAt(10, 0, 0, 5) == -1);  // no items
    CHECK_TRUE(pie::SectorAt(-3, 9, 1, 5) == 0);   // one item owns the circle
}

TEST_CASE(sector_boundaries) {
    // 8 items: item 0 spans -22.5..22.5 degrees from up, clockwise.
    auto dir = [](double deg, float *x, float *y) {
        *x = (float)(100 * sin(deg * M_PI / 180));
        *y = (float)(-100 * cos(deg * M_PI / 180));
    };
    float x, y;
    dir(20, &x, &y);  CHECK_TRUE(pie::SectorAt(x, y, 8, 5) == 0);
    dir(25, &x, &y);  CHECK_TRUE(pie::SectorAt(x, y, 8, 5) == 1);
    dir(-20, &x, &y); CHECK_TRUE(pie::SectorAt(x, y, 8, 5) == 0);
    dir(-25, &x, &y); CHECK_TRUE(pie::SectorAt(x, y, 8, 5) == 7);
}

TEST_CASE(item_direction_matches_sector) {
    for(int n = 1; n <= 9; n++) {
        for(int i = 0; i < n; i++) {
            float x, y;
            pie::ItemDirection(i, n, &x, &y);
            CHECK_EQ_EPS(x * x + y * y, 1.0);
            CHECK_TRUE(pie::SectorAt(x * 50, y * 50, n, 5) == i);
        }
    }
}

TEST_CASE(pie_keys) {
    CHECK_TRUE(pie::KeyAction("3", false, 8).kind == pie::KeyKind::PICK);
    CHECK_TRUE(pie::KeyAction("3", false, 8).index == 2);
    CHECK_TRUE(pie::KeyAction("Num1", false, 8).index == 0);
    CHECK_TRUE(pie::KeyAction("9", false, 8).kind == pie::KeyKind::NONE); // no 9th item
    CHECK_TRUE(pie::KeyAction("Enter", false, 8).kind == pie::KeyKind::CONFIRM);
    CHECK_TRUE(pie::KeyAction("Esc", false, 8).kind == pie::KeyKind::CANCEL);
    CHECK_TRUE(pie::KeyAction("3", true, 8).kind == pie::KeyKind::NONE);  // with modifiers
}
