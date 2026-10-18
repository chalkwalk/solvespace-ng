// Numpad views, the View pie, frame selected, wheel zoom, and mouse orbit / pan.
#include "../uitest.h"
#include "ui/piemenu.h"
#include "ui/viewcube.h"
#include "ui/viewcubemodel.h"
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

// A point on the view cube, by face and position on it (u, v in -1..1 along the face's
// screen-right and screen-up corners), from the cube's own geometry.
static Point2d OnCube(const char *label, double u = 0, double v = 0) {
    for(const Ui::CubeFace &f : Ui::ViewCubeFaces(SS.GW.projRight, SS.GW.projUp,
                                                  Ui::ViewCubeCenter(), Ui::ViewCubeSize())) {
        if(f.label != label || !f.visible) continue;
        Point2d c = Point2d::From(0, 0);
        for(Point2d p : f.corners) c = c.Plus(p.ScaledBy(0.25));
        // corners: 0 (-a,-b), 1 (+a,-b), 2 (+a,+b), 3 (-a,+b)
        Point2d du = f.corners[1].Minus(f.corners[0]).ScaledBy(0.5);
        Point2d dv = f.corners[3].Minus(f.corners[0]).ScaledBy(0.5);
        return c.Plus(du.ScaledBy(u)).Plus(dv.ScaledBy(v));
    }
    return Point2d::From(-1, -1);
}

TEST_CASE(view_cube_faces_edges_and_corners_snap_the_view) {
    CHECK_LOAD("line.slvs");
    Driver::Key("Keypad 1");
    CHECK_TRUE(ViewIs(ViewPreset::FRONT));
    // The front face's top edge: between front and top.
    ClickAt(OnCube("Front", 0, 0.85));
    Vector toViewer = SS.GW.projRight.Cross(SS.GW.projUp);
    CHECK_TRUE(toViewer.Minus(Vector::From(0, -1, 1).WithMagnitude(1)).Magnitude() < 1e-6);
    // Now the top face shows: its middle looks straight down.
    ClickAt(OnCube("Top"));
    CHECK_TRUE(ViewIs(ViewPreset::TOP));
    // A corner of the top face: an isometric view.
    ClickAt(OnCube("Top", 0.9, -0.9));
    toViewer = SS.GW.projRight.Cross(SS.GW.projUp);
    CHECK_TRUE(fabs(toViewer.x) > 0.5 && fabs(toViewer.y) > 0.5 && toViewer.z > 0.5);
}

TEST_CASE(dragging_the_view_cube_orbits) {
    CHECK_LOAD("line.slvs");
    Driver::Key("Keypad 1");
    int selected = SS.GW.selection.n;
    Point2d c = Ui::ViewCubeCenter();
    Driver::Drag(c.x, c.y, c.x + 60, c.y + 20);
    CHECK_FALSE(ViewIs(ViewPreset::FRONT));
    CHECK_TRUE(SS.GW.selection.n == selected); // the drag never reached the canvas
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::NONE);
}

TEST_CASE(view_cube_sits_bottom_left_with_the_axes) {
    CHECK_LOAD("line.slvs");
    std::optional<Driver::Item> cube = Driver::FindItem("viewcube");
    CHECK_TRUE(cube.has_value());
    if(cube) CHECK_TRUE(cube->x0 < 40 && cube->y1 > Driver::WindowSize().y - 80);
    CHECK_TRUE(SS.GW.referenceAxesAnchor.has_value());
}

TEST_CASE(an_axis_beside_the_cube_can_be_selected) {
    // The axes are SolveSpace's reference normals: selectable, e.g. for "parallel to Z".
    CHECK_LOAD("line.slvs");
    Driver::Key("Keypad 1"); // Z points up the screen
    Point2d c = Ui::ViewCubeCenter();
    double  s = Ui::ViewCubeSize(), len = SS.GW.referenceAxesLength;
    // The axes start at the cube's -X -Y -Z corner: from the front, its bottom left. Z runs up
    // the cube's left edge and past its top.
    Point2d origin = Ui::ViewCubeAxesOrigin();
    CHECK_TRUE(Near(origin, Point2d::From(c.x - s, c.y + s)));
    CHECK_TRUE(len > 2 * s + 10);
    ClickAt(Point2d::From(origin.x, origin.y - len + 6)); // past the cube, on the Z axis
    hEntity zAxis = SK.GetEntity(Request::HREQUEST_REFERENCE_XY.entity(0))->normal;
    CHECK_TRUE(SS.GW.selection.n == 1 && SS.GW.selection[0].entity == zAxis);
    CHECK_TRUE(ViewIs(ViewPreset::FRONT)); // and the view stayed
}

TEST_CASE(clicks_beside_the_cube_reach_the_canvas) {
    CHECK_LOAD("line.slvs");
    Driver::Key("Keypad 1");
    Point2d c = Ui::ViewCubeCenter();
    ClickAt(Point2d::From(c.x + 50, c.y - 50)); // inside its area, off the cube and the axes
    CHECK_TRUE(ViewIs(ViewPreset::FRONT));
    CHECK_TRUE(SS.GW.selection.n == 0);
}

TEST_CASE(view_cube_moves_right_of_a_tall_outliner) {
    CHECK_LOAD("line.slvs");
    Driver::Resize(720, 480); // the app's minimum height stops it at 636
    Point2d ws = Driver::WindowSize();
    // With both side panels there is no free spot: it stays in its corner, on screen.
    std::optional<Driver::Item> cube = Driver::FindItem("viewcube");
    CHECK_TRUE(cube && cube->x1 <= ws.x);
    // Without Properties it moves right of the Outliner, which reaches down beside it.
    CHECK_TRUE(ClosePanel("Properties"));
    std::optional<Driver::Item> outliner = Driver::FindItem("Outliner");
    cube = Driver::FindItem("viewcube");
    CHECK_TRUE(outliner && cube);
    if(outliner && cube) CHECK_TRUE(cube->x0 >= outliner->x1 && cube->x1 <= ws.x);
}
