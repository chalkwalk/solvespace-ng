// Dragging with solids made on the kernel's worker (src/regen.cpp): the drag never waits for
// the kernel, and when it ends the solid is the one a full regeneration makes.
#include "../uitest.h"
#include "harness.h"

using namespace SolveSpace::UiTest;

static Group *LastGroup() {
    return SK.GetGroup(SK.groupOrder[SK.groupOrder.n - 1]);
}

static std::string Describe(Group *g) {
    SMesh m = {};
    m.MakeFromCopyOf(&g->runningMesh);
    g->runningShell.TriangulateInto(&m);
    std::string s = ssprintf("surfaces %d volume %.3f", g->runningShell.surface.n,
                             m.CalculateVolume());
    m.Clear();
    return s;
}

TEST_CASE(dragging_an_extrusion_makes_its_solid_on_the_worker) {
    CHECK_LOAD("extrudes.slvs");
    Group *g = LastGroup();
    SS.GW.activeGroup = g->h;
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Driver::Settle();
    // A point the extrusion made (its depth follows it).
    hEntity point = {};
    for(Entity &e : SK.entity) {
        if(e.group == g->h && e.IsPoint()) { point = e.h; break; }
    }
    CHECK_TRUE(point.v != 0);
    Point2d from = At(SK.GetEntity(point)->PointGetNum());
    int     applied = SS.regenChainsApplied;
    Driver::Drag(from.x, from.y, from.x + 30, from.y - 30);
    CHECK_TRUE(SS.regenChainsApplied > applied); // made on the worker
    CHECK_FALSE(g->regenPending);
    std::string dragged = Describe(g);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_EQ_STR(dragged, Describe(g));
}
