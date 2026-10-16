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
