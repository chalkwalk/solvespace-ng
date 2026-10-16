// Numpad views, the View pie, frame selected, wheel zoom, and mouse orbit / pan.
#include "../uitest.h"
#include "ui/piemenu.h"
#include "harness.h"

using namespace SolveSpace::UiTest;

static bool ViewIs(ViewPreset p) {
    Vector r, u;
    ViewPresetBasis(p, &r, &u);
    return SS.GW.projRight.Minus(r).Magnitude() < 1e-6 && SS.GW.projUp.Minus(u).Magnitude() < 1e-6;
}

TEST_CASE(numpad_snaps_to_views) {
    CHECK_LOAD("line.slvs");
    Driver::Key("Keypad 1");
    CHECK_TRUE(ViewIs(ViewPreset::FRONT));
    Driver::Key("Keypad 3");
    CHECK_TRUE(ViewIs(ViewPreset::RIGHT));
    Driver::Key("Keypad 7");
    CHECK_TRUE(ViewIs(ViewPreset::TOP));
    Driver::Key("Ctrl+Keypad 1");
    CHECK_TRUE(ViewIs(ViewPreset::BACK));
    Driver::Key("Ctrl+Keypad 7");
    CHECK_TRUE(ViewIs(ViewPreset::BOTTOM));
}

TEST_CASE(view_pie_picks_a_view) {
    CHECK_LOAD("line.slvs");
    Driver::Key("Keypad 1");
    Driver::Move(640, 450);
    Driver::Key("`");
    CHECK_TRUE(Ui::Pie().Title() == "View");
    CHECK_TRUE(PickPie("Right"));
    CHECK_TRUE(ViewIs(ViewPreset::RIGHT));
}

TEST_CASE(period_frames_the_selection) {
    CHECK_LOAD("line.slvs");
    hRequest line = DrawLine(Point2d::From(200, 650), Point2d::From(260, 680));
    ClickAt(AtMiddle(line));
    double scale = SS.GW.scale;
    Driver::Key(".");
    Point2d mid = AtMiddle(line), size = Driver::WindowSize();
    CHECK_TRUE(fabs(mid.x - size.x / 2) < 2 && fabs(mid.y - size.y / 2) < 2);
    CHECK_TRUE(SS.GW.scale > scale);
}

TEST_CASE(wheel_zooms_about_the_mouse) {
    CHECK_LOAD("line.slvs");
    Vector under = Vector::From(0, 0, 0);
    Point2d p = At(under);
    double scale = SS.GW.scale;
    Driver::Wheel(p.x, p.y, 1);
    CHECK_TRUE(SS.GW.scale > scale);
    CHECK_TRUE(Near(At(under), p));
    Driver::Wheel(p.x, p.y, -1);
    CHECK_TRUE(fabs(SS.GW.scale - scale) < 1e-9);
}

TEST_CASE(middle_drag_orbits_and_shift_middle_drag_pans) {
    CHECK_LOAD("line.slvs");
    Vector right = SS.GW.projRight, offset = SS.GW.offset;
    Driver::Drag(500, 500, 560, 520, Driver::Button::MIDDLE);
    CHECK_TRUE(SS.GW.projRight.Minus(right).Magnitude() > 1e-3);
    right = SS.GW.projRight;
    Driver::KeyDown("Shift");
    Driver::Drag(500, 500, 560, 520, Driver::Button::MIDDLE);
    Driver::KeyUp("Shift");
    CHECK_TRUE(SS.GW.projRight.Minus(right).Magnitude() < 1e-9);
    CHECK_TRUE(SS.GW.offset.Minus(offset).Magnitude() > 1e-3);
}
