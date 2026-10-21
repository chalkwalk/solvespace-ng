// Fillet and chamfer groups made in the app: pick an edge of the solid, make the group, type
// its size in Properties (box.slvs: a 10 x 20 rectangle extruded 30, as test/group/fillet).
#include "../uitest.h"
#include "harness.h"

using namespace SolveSpace::UiTest;

static Group *Active() {
    return SK.GetGroup(SS.GW.activeGroup);
}

static double Volume(Group *g) {
    SMesh m = {};
    m.MakeFromCopyOf(&g->runningMesh);
    g->runningShell.TriangulateInto(&m);
    double v = m.CalculateVolume();
    m.Clear();
    return v;
}

// The box seen from the isometric view, its top front edge (0, 0, 30) to (10, 0, 30) clicked.
static hEntity ClickTopFrontEdge() {
    Driver::Key("Shift+7");
    hEntity edge = Active()->Remap(hRequest{ 5 }.entity(0), Group::REMAP_TOP);
    ClickAt(At({ 5, 0, 30 }));
    return edge;
}

TEST_CASE(ng_fillets_a_picked_edge) {
    CHECK_LOAD("box.slvs");
    CHECK_TRUE(SS.kernelKind == KernelKind::OCCT); // NG's default
    hGroup  box  = SS.GW.activeGroup;
    hEntity edge = ClickTopFrontEdge();
    CHECK_TRUE(SS.GW.IsSelected(edge));
    Driver::Key("Shift+F");
    CHECK_TRUE(Active()->type == Group::Type::FILLET);
    CHECK_TRUE(Active()->edges.size() == 1 && Active()->edges[0] == edge);
    CHECK_TRUE(EditField("feature-size", "3"));
    CHECK_EQ_EPS(Active()->valA, 3);
    CHECK_TRUE(Volume(Active()) < 6000 - 15);
    Driver::Key("Ctrl+Z");
    CHECK_EQ_EPS(Active()->valA, 1);
    Driver::Key("Ctrl+Z");
    CHECK_TRUE(SS.GW.activeGroup == box);
}

TEST_CASE(ng_chamfers_from_the_menu) {
    CHECK_LOAD("box.slvs");
    hEntity edge = ClickTopFrontEdge();
    CHECK_TRUE(Driver::ClickMenu({ "New Group", "Chamfer Edges" }));
    CHECK_TRUE(Active()->type == Group::Type::CHAMFER);
    CHECK_TRUE(Active()->edges.size() == 1 && Active()->edges[0] == edge);
    CHECK_TRUE(Driver::FindItem("feature-size").has_value());
    CHECK_TRUE(Volume(Active()) < 6000 - 4);
}

TEST_CASE(ng_opens_problems_on_a_failed_fillet) {
    CHECK_LOAD("box.slvs");
    ClickTopFrontEdge();
    Driver::Key("Shift+F");
    CHECK_TRUE(EditField("feature-size", "50"));
    CHECK_FALSE(Active()->featureProblem.empty());
    CHECK_TRUE(Driver::FindItem("Problems").has_value()); // opens on a new problem
}
