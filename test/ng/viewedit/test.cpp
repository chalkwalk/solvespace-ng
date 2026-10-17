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
