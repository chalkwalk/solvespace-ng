#include "solvespace.h"

#include "harness.h"

// A point in the active workplane, and where it is on the screen.
static Vector InPlane(double u, double v) {
    Entity *wp = SK.GetEntity(SS.GW.ActiveWorkplane());
    Vector  o  = SK.GetEntity(wp->point[0])->PointGetNum();
    return o.Plus(wp->Normal()->NormalU().ScaledBy(u)).Plus(wp->Normal()->NormalV().ScaledBy(v));
}

// A tap on a touch screen: a press with no move before it since the last frame was drawn (the
// classic web interface), then a release.
static void Tap(double u, double v) {
    Point2d s = SS.GW.ProjectPoint(InPlane(u, v));
    SS.GW.havePainted = true;
    SS.GW.MouseLeftDown(s.x, s.y, false, false);
    SS.GW.MouseLeftUp(s.x, s.y, false, false);
}

// Within half a millimetre: a tap lands on a whole pixel.
static bool PointAt(Vector p) {
    for(Entity &e : SK.entity) {
        if(e.IsPoint() && e.group == SS.GW.activeGroup && e.PointGetNum().Equals(p, 0.5)) {
            return true;
        }
    }
    return false;
}

TEST_CASE(tapped_rectangle_has_its_corners_where_tapped) {
    CHECK_LOAD("line.slvs");
    SS.showToolbar = false;
    SS.GW.ActivateCommand(Command::RECTANGLE);
    Tap(0, 0);
    Tap(30, 20);
    SS.GenerateAll(); // as the next frame does
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::NONE);
    CHECK_TRUE(PointAt(InPlane(0, 0)));
    CHECK_TRUE(PointAt(InPlane(30, 20)));
}

// Two fingers on a touch screen pan (a right-button drag) and pinch to zoom at once: a zoom in
// the middle of a pan keeps what is under the fingers there.
TEST_CASE(a_zoom_during_a_pan_stays_about_the_mouse) {
    CHECK_LOAD("line.slvs");
    Point2d c     = Point2d::From(40, 30);
    Vector  under = SS.GW.UnProjectPoint(c);
    SS.GW.MouseMiddleOrRightDown(c.x, c.y);
    SS.GW.MouseMoved(c.x + 10, c.y, false, false, /*rightDown=*/true, false, false);
    SS.GW.MouseMoved(c.x, c.y, false, false, /*rightDown=*/true, false, false);
    SS.GW.MouseScroll(2);
    SS.GW.MouseMoved(c.x, c.y, false, false, /*rightDown=*/true, false, false);
    CHECK_TRUE(SS.GW.ProjectPoint(under).DistanceTo(c) < 1e-6);
}
