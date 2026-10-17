#include "solvespace.h"
#include "ui/viewcubemodel.h"
#include "harness.h"

using namespace SolveSpace::Ui;

static bool Same(Vector a, Vector b) {
    return a.Minus(b).Magnitude() < 1e-9;
}

TEST_CASE(cube_faces_snap_to_the_numpad_views) {
    const ViewPreset presets[] = { ViewPreset::FRONT, ViewPreset::BACK, ViewPreset::RIGHT,
                                   ViewPreset::LEFT, ViewPreset::TOP, ViewPreset::BOTTOM };
    for(ViewPreset p : presets) {
        Vector r, u, rr, uu;
        ViewPresetBasis(p, &r, &u);
        ViewBasisFor(r.Cross(u), &rr, &uu); // looking from where the preset looks from
        CHECK_TRUE(Same(r, rr) && Same(u, uu));
    }
}

TEST_CASE(front_view_shows_only_the_front_face) {
    Vector r, u;
    ViewPresetBasis(ViewPreset::FRONT, &r, &u);
    std::vector<CubeFace> faces = ViewCubeFaces(r, u, Point2d::From(100, 100), 30);
    int visible = 0;
    for(const CubeFace &f : faces) {
        if(!f.visible) continue;
        visible++;
        CHECK_EQ_STR(f.label, "Front");
        CHECK_TRUE(Same(f.normal, Vector::From(0, -1, 0)));
    }
    CHECK_TRUE(visible == 1);
}

TEST_CASE(picks_faces_edges_and_corners) {
    Vector r, u;
    ViewPresetBasis(ViewPreset::FRONT, &r, &u);
    Point2d c = Point2d::From(100, 100);
    // The middle of the front face, its right edge band, its top-right corner; nothing outside.
    CHECK_TRUE(Same(*ViewCubePick(r, u, c, 30, c), Vector::From(0, -1, 0)));
    CHECK_TRUE(Same(*ViewCubePick(r, u, c, 30, Point2d::From(127, 100)),
                    Vector::From(1, -1, 0)));
    CHECK_TRUE(Same(*ViewCubePick(r, u, c, 30, Point2d::From(127, 73)),
                    Vector::From(1, -1, 1))); // screen y grows downwards: up is smaller y
    CHECK_FALSE(ViewCubePick(r, u, c, 30, Point2d::From(140, 100)).has_value());
}

TEST_CASE(an_isometric_view_shows_three_faces_and_picks_each) {
    Vector r, u;
    ViewBasisFor(Vector::From(1, -1, 1), &r, &u);
    CHECK_TRUE(fabs(u.z) > 0.5); // Z stays up
    std::vector<CubeFace> faces = ViewCubeFaces(r, u, Point2d::From(0, 0), 30);
    int visible = 0;
    for(const CubeFace &f : faces) {
        if(!f.visible) continue;
        visible++;
        // The middle of each visible face picks that face.
        Point2d mid = Point2d::From(0, 0);
        for(Point2d p : f.corners) mid = mid.Plus(p.ScaledBy(0.25));
        CHECK_TRUE(Same(*ViewCubePick(r, u, Point2d::From(0, 0), 30, mid), f.normal));
    }
    CHECK_TRUE(visible == 3);
}

TEST_CASE(a_view_straight_down_keeps_y_up_on_screen) {
    Vector r, u;
    ViewBasisFor(Vector::From(0, 0, 1), &r, &u);
    CHECK_TRUE(Same(u, Vector::From(0, 1, 0)) && Same(r, Vector::From(1, 0, 0)));
    ViewBasisFor(Vector::From(1, -1, 0).WithMagnitude(1), &r, &u);
    CHECK_TRUE(Same(u, Vector::From(0, 0, 1)));
    CHECK_TRUE(fabs(r.Dot(Vector::From(1, -1, 0))) < 1e-9); // square to the view direction
}
