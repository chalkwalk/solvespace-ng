#include "solvespace.h"
#include "harness.h"

TEST_CASE(normal_roundtrip) {
    CHECK_LOAD("normal.slvs");
    CHECK_RENDER("normal.png");
    CHECK_SAVE("normal.slvs");
}

TEST_CASE(normal_migrate_from_v20) {
    CHECK_LOAD("normal_v20.slvs");
    CHECK_SAVE("normal.slvs");
}

TEST_CASE(normal_migrate_from_v22) {
    CHECK_LOAD("normal_v22.slvs");
    CHECK_SAVE("normal.slvs");
}

TEST_CASE(hover_finds_the_line_under_the_mouse) {
    CHECK_LOAD("normal.slvs");
    hRequest line = {};
    for(Request &r : SK.request) {
        if(r.type == Request::Type::LINE_SEGMENT) line = r.h;
    }
    Vector a = SK.GetEntity(line.entity(1))->PointGetNum();
    Vector b = SK.GetEntity(line.entity(2))->PointGetNum();
    Point2d mid = SS.GW.ProjectPoint(a.Plus(b).ScaledBy(0.5));
    // Twice: each hit test replaces the previous hover list (and must free it).
    for(int i = 0; i < 2; i++) {
        SS.GW.HitTestMakeSelection(mid);
        CHECK_TRUE(SS.GW.hover.entity == line.entity(0));
    }
}
