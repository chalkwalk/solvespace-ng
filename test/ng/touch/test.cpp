#include <cmath>
#include "solvespace.h"
#include "platform/sdl/touchgestures.h"
#include "harness.h"

using namespace SolveSpace::Platform;
using Out = TouchGestures::Out;

// The mouse events as text, e.g. "move 10 20 L; press L 10 20; release L 10 20".
static std::string Text(const std::vector<Out> &outs) {
    std::string s;
    for(const Out &o : outs) {
        if(!s.empty()) s += "; ";
        const char *b = o.button == Out::Button::LEFT     ? " L"
                        : o.button == Out::Button::RIGHT  ? " R"
                        : o.button == Out::Button::MIDDLE ? " M"
                        : o.button == Out::Button::PAN    ? " P"
                                                          : "";
        char buf[96];
        switch(o.kind) {
            case Out::Kind::MOTION:
                snprintf(buf, sizeof(buf), "move %g %g%s", o.x, o.y, b);
                break;
            case Out::Kind::PRESS:
                snprintf(buf, sizeof(buf), "press%s %g %g", b, o.x, o.y);
                break;
            case Out::Kind::RELEASE:
                snprintf(buf, sizeof(buf), "release%s %g %g", b, o.x, o.y);
                break;
            case Out::Kind::WHEEL:
                snprintf(buf, sizeof(buf), "wheel %.2f at %g %g", o.wheel, o.x, o.y);
                break;
        }
        s += buf;
    }
    return s;
}

TEST_CASE(a_tap_is_a_click_where_the_finger_was) {
    TouchGestures t;
    t.FingerDown(1, 100, 200, 0);
    CHECK_EQ_STR(Text(t.Take()), ""); // a second finger may still come
    t.FingerUp(1, 100, 200, 60);
    CHECK_EQ_STR(Text(t.Take()), "move 100 200; press L 100 200; release L 100 200");
    CHECK_FALSE(t.Active());
}

TEST_CASE(a_long_press_is_a_right_click) {
    TouchGestures t;
    t.FingerDown(1, 100, 200, 0);
    t.Tick(TouchGestures::LONG_PRESS_MS - 1);
    CHECK_EQ_STR(Text(t.Take()), "");
    CHECK_TRUE(t.Waiting());
    t.Tick(TouchGestures::LONG_PRESS_MS);
    CHECK_EQ_STR(Text(t.Take()), "move 100 200; press R 100 200; release R 100 200");
    CHECK_FALSE(t.Waiting());
    t.FingerMove(1, 150, 200, 700); // the menu is open: the finger does nothing more
    t.FingerUp(1, 150, 200, 800);
    CHECK_EQ_STR(Text(t.Take()), "");
}

TEST_CASE(one_finger_drags_with_the_left_button) {
    TouchGestures t;
    t.FingerDown(1, 100, 200, 0);
    t.FingerMove(1, 102, 201, 10); // within the slop: still waiting
    CHECK_EQ_STR(Text(t.Take()), "");
    t.FingerMove(1, 130, 200, 20);
    CHECK_EQ_STR(Text(t.Take()), "move 100 200; press L 100 200; move 130 200 L");
    t.FingerMove(1, 150, 210, 30);
    t.FingerUp(1, 150, 210, 40);
    CHECK_EQ_STR(Text(t.Take()), "move 150 210 L; release L 150 210");
}

TEST_CASE(two_fingers_pan) {
    TouchGestures t;
    t.FingerDown(1, 100, 200, 0);
    t.FingerDown(2, 200, 200, 30);
    CHECK_EQ_STR(Text(t.Take()), "move 150 200; press P 150 200");
    t.FingerMove(1, 110, 220, 40);
    t.FingerMove(2, 210, 220, 40);
    CHECK_EQ_STR(Text(t.Take()), "move 160 220 P");
    // Lifting a finger ends the gesture; the other finger does nothing until it lifts too.
    t.FingerUp(1, 110, 220, 50);
    t.FingerMove(2, 300, 300, 60);
    t.FingerUp(2, 300, 300, 70);
    CHECK_EQ_STR(Text(t.Take()), "release P 160 220");
    CHECK_FALSE(t.Active());
}

TEST_CASE(pinching_zooms_about_the_fingers) {
    TouchGestures t;
    t.FingerDown(1, 100, 200, 0);
    t.FingerDown(2, 200, 200, 10);
    t.Take();
    // Twice as far apart: 1.2^wheel = 2, about the fingers' centre.
    t.FingerMove(1, 50, 200, 20);
    t.FingerMove(2, 250, 200, 20);
    std::vector<Out> outs = t.Take();
    double wheel = 0;
    for(const Out &o : outs) {
        if(o.kind != Out::Kind::WHEEL) continue;
        wheel += o.wheel;
        CHECK_EQ_EPS(o.x, 150);
        CHECK_EQ_EPS(o.y, 200);
    }
    CHECK_EQ_EPS(wheel, std::log(2.0) / std::log(1.2));
}

TEST_CASE(three_fingers_orbit_with_the_middle_button) {
    TouchGestures t;
    t.FingerDown(1, 100, 200, 0);
    t.FingerDown(2, 200, 200, 10);
    t.FingerDown(3, 150, 300, 20);
    CHECK_EQ_STR(Text(t.Take()), "move 150 200; press P 150 200; release P 150 200; "
                                 "move 150 233.333; press M 150 233.333");
    t.FingerMove(1, 130, 200, 30);
    t.FingerMove(2, 230, 200, 30);
    t.FingerMove(3, 180, 300, 30);
    CHECK_EQ_STR(Text(t.Take()), "move 180 233.333 M");
    t.FingerUp(2, 230, 200, 40);
    t.FingerUp(1, 130, 200, 40);
    t.FingerUp(3, 180, 300, 40);
    CHECK_EQ_STR(Text(t.Take()), "release M 180 233.333");
}

TEST_CASE(a_second_finger_after_a_drag_began_is_ignored) {
    TouchGestures t;
    t.FingerDown(1, 100, 200, 0);
    t.FingerMove(1, 140, 200, 10);
    t.Take();
    t.FingerDown(2, 300, 300, 20);
    t.FingerMove(2, 320, 300, 30);
    t.FingerMove(1, 150, 200, 30);
    t.FingerUp(2, 320, 300, 40);
    t.FingerUp(1, 150, 200, 50);
    CHECK_EQ_STR(Text(t.Take()), "move 150 200 L; release L 150 200");
}

TEST_CASE(a_cancelled_touch_releases_its_button) {
    TouchGestures t;
    t.FingerDown(1, 100, 200, 0);
    t.FingerDown(2, 200, 200, 10);
    t.Take();
    t.Cancel();
    CHECK_EQ_STR(Text(t.Take()), "release P 150 200");
    CHECK_FALSE(t.Active());
}
