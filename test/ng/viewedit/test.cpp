#include "solvespace.h"

#include "harness.h"

TEST_CASE(view_scale_origin_and_projection_from_text) {
    CHECK_FALSE(SS.GW.SetViewScale("0"));
    CHECK_TRUE(SS.GW.SetViewScale("2*5"));
    CHECK_EQ_EPS(SS.GW.scale, 10.0 / SS.MmPerUnit());
    CHECK_FALSE(SS.GW.SetViewOrigin("1, 2"));
    CHECK_TRUE(SS.GW.SetViewOrigin("1, 2, 3"));
    CHECK_TRUE(SS.GW.offset.Equals(Vector::From(-1, -2, -3).ScaledBy(SS.MmPerUnit())));
    CHECK_TRUE(SS.GW.SetViewProjection(/*right=*/true, "0, 1, 0"));
    CHECK_TRUE(SS.GW.SetViewProjection(/*right=*/false, "0, 0, 1"));
    CHECK_TRUE(SS.GW.projRight.Equals(Vector::From(0, 1, 0)));
    CHECK_TRUE(SS.GW.projUp.Equals(Vector::From(0, 0, 1)));
    CHECK_FALSE(SS.GW.SetViewProjection(true, "up"));
}

TEST_CASE(snap_to_a_direction_looks_from_there_with_z_up) {
    SS.GW.SnapToDirection(Vector::From(1, -1, 1), /*animate=*/false);
    Vector toViewer = SS.GW.projRight.Cross(SS.GW.projUp);
    CHECK_TRUE(toViewer.Minus(Vector::From(1, -1, 1).WithMagnitude(1)).Magnitude() < 1e-9);
    CHECK_TRUE(SS.GW.projUp.z > 0.5);
    CHECK_TRUE(fabs(SS.GW.projRight.z) < 1e-9);
}

TEST_CASE(orbit_by_turns_the_view_and_keeps_it_square) {
    SS.GW.SnapToView(ViewPreset::FRONT, false);
    for(bool turntable : { false, true }) {
        SS.turntableNav = turntable;
        Vector r0 = SS.GW.projRight;
        SS.GW.OrbitBy(100, 40);
        CHECK_TRUE(SS.GW.projRight.Minus(r0).Magnitude() > 0.1);
        CHECK_TRUE(fabs(SS.GW.projRight.Dot(SS.GW.projUp)) < 1e-9);
        CHECK_TRUE(fabs(SS.GW.projRight.Magnitude() - 1) < 1e-9);
        if(turntable) CHECK_TRUE(fabs(SS.GW.projRight.z) < 1e-9); // Z stays vertical
    }
    SS.turntableNav = false;
}

TEST_CASE(reference_axes_drawn_at_an_anchor_can_be_hovered) {
    // NG draws SolveSpace's corner X/Y/Z axes out of its view cube; they stay selectable.
    SS.GW.SnapToView(ViewPreset::FRONT, false);
    SS.GW.referenceAxesAnchor = Point2d::From(-300, -200); // pixels from the centre, y up
    SS.GW.referenceAxesLength = 60;
    hEntity zAxis = SK.GetEntity(Request::HREQUEST_REFERENCE_XY.entity(0))->normal;
    hEntity xAxis = SK.GetEntity(Request::HREQUEST_REFERENCE_YZ.entity(0))->normal;
    // From the front, Z points up the screen and X to the right.
    SS.GW.HitTestMakeSelection(Point2d::From(-300, -200 + 50));
    CHECK_TRUE(SS.GW.hover.entity == zAxis);
    SS.GW.HitTestMakeSelection(Point2d::From(-300 + 50, -200));
    CHECK_TRUE(SS.GW.hover.entity == xAxis);
    SS.GW.referenceAxesAnchor.reset();
    SS.GW.referenceAxesLength = 50;
}
