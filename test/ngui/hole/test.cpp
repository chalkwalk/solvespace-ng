// Holes made in the app (holebox.slvs: the 10 x 20 x 30 box of test/group/features.h, and a
// sketch on its top face, active, with a point at (5, 10, 30)).
#include "../uitest.h"
#include "harness.h"

using namespace SolveSpace::UiTest;

static Group *Active() {
    return SK.GetGroup(SS.GW.activeGroup);
}

static double Volume() {
    MassProps mp;
    SS.MassProperties(&mp);
    return mp.volume;
}

static hEntity ThePoint() {
    for(Request &r : SK.request) {
        if(r.type == Request::Type::DATUM_POINT) return r.h.entity(0);
    }
    return {};
}

TEST_CASE(ng_drills_a_counterbored_hole_at_a_picked_point) {
    CHECK_LOAD("holebox.slvs");
    hEntity point = ThePoint();
    ClickAt(At({ 5, 10, 30 }));
    CHECK_TRUE(SS.GW.IsSelected(point));
    Driver::Key("Shift+H");
    CHECK_TRUE(Active()->type == Group::Type::HOLE);
    CHECK_TRUE(EditField("hole-diameter", "4"));
    CHECK_TRUE(fabs(Volume() - (6000 - PI * 4 * 30)) < 1e-6);
    CHECK_TRUE(Driver::ClickItem("counterbore"));
    CHECK_TRUE(Active()->hole.kind == Group::HoleKind::COUNTERBORE);
    CHECK_TRUE(EditField("hole-head-diameter", "8"));
    CHECK_TRUE(fabs(Volume() - (6000 - PI * 4 * 30 - PI * 12 * 3)) < 1e-6);
    Driver::Key("Ctrl+Z");
    CHECK_EQ_EPS(Active()->hole.headDiameter, 10);
}

TEST_CASE(ng_threads_a_hole) {
    CHECK_LOAD("holebox.slvs");
    ClickAt(At({ 5, 10, 30 }));
    CHECK_TRUE(Driver::ClickMenu({ "New Group", "Hole" }));
    CHECK_TRUE(EditField("hole-depth", "6"));
    CHECK_TRUE(EditField("hole-thread", "M3"));
    CHECK_EQ_EPS(Active()->hole.thread, 3);
    CHECK_EQ_EPS(Active()->hole.pitch, 0.5);
    CHECK_FALSE(Driver::FindItem("hole-diameter").has_value()); // the thread sets it
    CHECK_TRUE(Active()->featureProblem.empty());
}
