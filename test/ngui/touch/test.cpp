// A touch screen, as on a phone or tablet: one finger for tools, two to pan and pinch to zoom,
// three to orbit (platform/sdl/touchgestures.h).
#include "../uitest.h"
#include "ui/panels.h"
#include "harness.h"

using namespace SolveSpace::UiTest;

static bool TapItem(const std::string &label) {
    std::optional<Driver::Item> item = Driver::FindItem(label);
    if(!item) return false;
    Point2d c = item->Center();
    Driver::Tap(c.x, c.y);
    return true;
}

// Whether a point of the sketch is drawn at p.
static bool PointAt(Point2d p) {
    for(const Entity &e : SK.entity) {
        if(e.IsPoint() && e.group == SS.GW.activeGroup && Near(At(e.PointGetNum()), p)) {
            return true;
        }
    }
    return false;
}

TEST_CASE(taps_draw_a_rectangle_from_the_menu) {
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(TapItem("Sketch"));
    CHECK_TRUE(TapItem("Rectangle"));
    Driver::Tap(450, 550);
    Driver::Tap(560, 480);
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::NONE);
    CHECK_TRUE(PointAt(Point2d::From(450, 550)));
    CHECK_TRUE(PointAt(Point2d::From(560, 480)));
}

TEST_CASE(a_long_press_opens_the_context_menu) {
    CHECK_LOAD("line.slvs");
    hRequest line = DrawLine(Point2d::From(450, 520), Point2d::From(600, 520));
    Point2d  m    = AtMiddle(line);
    bool     found = false;
    Driver::DuringModal([&] { found = Driver::ClickItem("Delete"); });
    Driver::LongPress(m.x, m.y);
    CHECK_TRUE(found);
    CHECK_TRUE(SK.request.FindByIdNoOops(line) == nullptr);
}

TEST_CASE(a_long_press_ends_a_line_chain) {
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(TapItem("Sketch"));
    CHECK_TRUE(TapItem("Line"));
    Driver::Tap(450, 550);
    Driver::Tap(560, 480);
    Driver::LongPress(600, 400);
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::NONE);
}

// A tap that misses (or a double tap) can leave a sketch with nothing in it to extrude.
TEST_CASE(extruding_a_zero_size_rectangle_does_not_crash) {
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(TapItem("Sketch"));
    CHECK_TRUE(TapItem("Rectangle"));
    Driver::Tap(450, 550);
    Driver::Tap(450, 550);
    CHECK_TRUE(TapItem("New Group"));
    CHECK_TRUE(TapItem("Extrude"));
    CHECK_TRUE(SK.GetGroup(SS.GW.activeGroup)->type == Group::Type::EXTRUDE);
    Driver::TouchDrag({ Point2d::From(400, 400) }, { Point2d::From(500, 300) });
}

TEST_CASE(one_finger_drags_a_point) {
    CHECK_LOAD("line.slvs");
    hRequest line = DrawLine(Point2d::From(450, 520), Point2d::From(600, 520));
    Point2d a = AtPoint(line.entity(1));
    Driver::TouchDrag({ a }, { a.Plus(Point2d::From(40, -60)) });
    CHECK_TRUE(Near(AtPoint(line.entity(1)), a.Plus(Point2d::From(40, -60))));
}

TEST_CASE(two_fingers_pan_the_view) {
    CHECK_LOAD("line.slvs");
    Point2d o = At(Vector::From(0, 0, 0));
    Driver::TouchDrag({ Point2d::From(400, 400), Point2d::From(500, 400) },
                      { Point2d::From(450, 430), Point2d::From(550, 430) });
    CHECK_TRUE(Near(At(Vector::From(0, 0, 0)), o.Plus(Point2d::From(50, 30))));
}

TEST_CASE(pinching_zooms_about_the_fingers) {
    CHECK_LOAD("line.slvs");
    hRequest line  = DrawLine(Point2d::From(450, 520), Point2d::From(600, 520));
    Point2d  p     = AtPoint(line.entity(1));
    double   scale = SS.GW.scale;
    Driver::TouchDrag({ p.Plus(Point2d::From(-50, 0)), p.Plus(Point2d::From(50, 0)) },
                      { p.Plus(Point2d::From(-100, 0)), p.Plus(Point2d::From(100, 0)) });
    CHECK_EQ_EPS(SS.GW.scale / scale, 2.0);
    CHECK_TRUE(Near(AtPoint(line.entity(1)), p)); // what was under the fingers stays there
}

TEST_CASE(three_fingers_orbit_the_view) {
    CHECK_LOAD("line.slvs");
    Vector right = SS.GW.projRight;
    Driver::TouchDrag({ Point2d::From(400, 400), Point2d::From(500, 400), Point2d::From(450, 480) },
                      { Point2d::From(480, 400), Point2d::From(580, 400), Point2d::From(530, 480) });
    CHECK_TRUE(SS.GW.projRight.Minus(right).Magnitude() > 0.1);
}

TEST_CASE(a_phone_sized_window_starts_without_the_side_panels) {
    SolveSpace::Ui::SetCompactLayout(true);
    SolveSpace::Ui::OutlinerPanel().Load();
    SolveSpace::Ui::PropertiesPanel().Load();
    bool outliner = SolveSpace::Ui::OutlinerPanel().Shown();
    bool props    = SolveSpace::Ui::PropertiesPanel().Shown();
    SolveSpace::Ui::SetCompactLayout(false);
    SolveSpace::Ui::OutlinerPanel().Load();
    SolveSpace::Ui::PropertiesPanel().Load();
    CHECK_FALSE(outliner);
    CHECK_FALSE(props);
    CHECK_TRUE(SolveSpace::Ui::OutlinerPanel().Shown());
    CHECK_TRUE(SolveSpace::Ui::PropertiesPanel().Shown());
}

TEST_CASE(taps_choose_menu_items) {
    CHECK_LOAD("line.slvs");
    bool shown = SolveSpace::Ui::PropertiesPanel().Shown();
    CHECK_TRUE(TapItem("View"));
    CHECK_TRUE(TapItem("Properties"));
    CHECK_TRUE(SolveSpace::Ui::PropertiesPanel().Shown() != shown);
}

TEST_CASE(taps_choose_menu_items_in_a_phone_sized_window) {
    CHECK_LOAD("line.slvs");
    Driver::Resize(412, 915);
    bool shown = SolveSpace::Ui::PropertiesPanel().Shown();
    CHECK_TRUE(TapItem("View"));
    CHECK_TRUE(TapItem("Properties"));
    CHECK_TRUE(SolveSpace::Ui::PropertiesPanel().Shown() != shown);
}
