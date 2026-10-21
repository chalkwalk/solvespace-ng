// Fillet and chamfer groups (AGENTS.md section 6, M3b): made from edges of the solid, which are
// the entities a solid group makes (an extrusion's copies of its sketch, its side lines).
// box.slvs is a 10 x 20 rectangle (requests 5..8, from the origin along +X first; request 4 is a
// construction line) extruded 30 along +Z.
#include "solvespace.h"
#include <set>

#include "harness.h"

namespace {

Group *Extrusion() {
    for(hGroup hg : SK.groupOrder) {
        Group *g = SK.GetGroup(hg);
        if(g->type == Group::Type::EXTRUDE) return g;
    }
    return nullptr;
}

// The extrusion's copy of a sketch line: REMAP_TOP or REMAP_BOTTOM.
hEntity Edge(uint32_t request, int remap) {
    return Extrusion()->Remap(hRequest{ request }.entity(0), remap);
}

void Select(std::vector<hEntity> entities) {
    SS.GW.ClearSelection();
    for(hEntity he : entities) {
        GraphicsWindow::Selection sel = {};
        sel.entity = he;
        SS.GW.selection.Add(&sel);
    }
}

double Volume(Group *g) {
    SMesh m = {};
    m.MakeFromCopyOf(&g->runningMesh);
    g->runningShell.TriangulateInto(&m);
    double v = m.CalculateVolume();
    m.Clear();
    return v;
}

std::set<uint32_t> FaceNames(Group *g) {
    std::set<uint32_t> names;
    for(const STriangle &t : g->runningMesh.l) {
        if(t.meta.face != 0) names.insert(t.meta.face);
    }
    return names;
}

void Resize(Group *g, const char *size) {
    SS.SetFeatureSize(g->h, size);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
}

Group *MakeFeature(Command cmd, std::vector<hEntity> edges) {
    Select(edges);
    int before = SK.groupOrder.n;
    Group::MenuGroup(cmd);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    if(SK.groupOrder.n == before) return nullptr;
    return SK.GetGroup(SS.GW.activeGroup);
}

}

TEST_CASE(fillet_needs_an_edge_of_the_solid) {
    CHECK_LOAD("box.slvs");
    CHECK_TRUE(MakeFeature(Command::GROUP_FILLET, {}) == nullptr);
    // A sketch line is not an edge of the solid.
    CHECK_TRUE(MakeFeature(Command::GROUP_FILLET, { hRequest{ 5 }.entity(0) }) == nullptr);
}

TEST_CASE(fillets_need_the_occt_kernel) {
    CHECK_LOAD("box.slvs");
    CHECK_TRUE(MakeFeature(Command::GROUP_FILLET, { Edge(5, Group::REMAP_TOP) }) == nullptr);
}

#ifdef HAVE_OCCT
TEST_CASE(fillet_group_is_made_from_selected_edges) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    hEntity top = Edge(5, Group::REMAP_TOP);
    Group  *g   = MakeFeature(Command::GROUP_FILLET, { top });
    CHECK_TRUE(g != nullptr);
    CHECK_TRUE(g->type == Group::Type::FILLET);
    CHECK_TRUE(g->edges.size() == 1 && g->edges[0] == top);
    // A tenth of the shortest edge (10 mm).
    CHECK_EQ_EPS(g->valA, 1.0);
}

TEST_CASE(chamfer_group_is_made_from_selected_edges) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    hEntity a = Edge(5, Group::REMAP_TOP), b = Edge(6, Group::REMAP_BOTTOM);
    Group  *g = MakeFeature(Command::GROUP_CHAMFER, { a, b });
    CHECK_TRUE(g != nullptr);
    CHECK_TRUE(g->type == Group::Type::CHAMFER);
    CHECK_TRUE(g->edges.size() == 2);
}

TEST_CASE(fillet_edges_and_size_survive_save_and_reload) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    hEntity a = Edge(5, Group::REMAP_TOP), b = Edge(7, Group::REMAP_TOP);
    Group  *g = MakeFeature(Command::GROUP_FILLET, { a, b });
    CHECK_TRUE(SS.SetFeatureSize(g->h, "2.5"));
    CHECK_FALSE(SS.SetFeatureSize(g->h, "-1"));
    hGroup hg = g->h;

    Platform::Path path = helper->GetAssetPath(__FILE__, "filleted.slvs", "out");
    CHECK_TRUE(SS.SaveToFile(path));
    CHECK_TRUE(SS.LoadFromFile(path));
    SS.AfterNewFile();
    g = SK.GetGroup(hg);
    CHECK_TRUE(g->type == Group::Type::FILLET);
    CHECK_TRUE(g->edges.size() == 2 && g->edges[0] == a && g->edges[1] == b);
    CHECK_EQ_EPS(g->valA, 2.5);
    remove(path.raw.c_str());
}

TEST_CASE(deleting_the_extrusion_deletes_the_fillet) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    Group *g  = MakeFeature(Command::GROUP_FILLET, { Edge(5, Group::REMAP_TOP) });
    hGroup hg = g->h;
    SS.GW.DeleteGroup(Extrusion()->h);
    CHECK_TRUE(SK.group.FindByIdNoOops(hg) == nullptr);
}
// The box less the corner a radius r rounds off along a 10 long edge: (1 - pi/4) r^2 10.
TEST_CASE(fillet_rounds_a_box_edge) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    Group *box = Extrusion();
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    std::set<uint32_t> named = FaceNames(box);
    CHECK_EQ_EPS(Volume(box), 6000);
    Group *g = MakeFeature(Command::GROUP_FILLET, { Edge(5, Group::REMAP_TOP) });
    Resize(g, "5");
    double removed = 6000 - Volume(g);
    CHECK_TRUE(fabs(removed - (1 - PI / 4) * 25 * 10) < 1);
    CHECK_TRUE(g->featureProblem.empty());
    // The faces named before keep their names; the rounded face is new.
    CHECK_TRUE(FaceNames(g) == named);
    // The extrusion's own solid is as it was.
    CHECK_EQ_EPS(Volume(box), 6000);
}

TEST_CASE(chamfer_bevels_a_box_edge) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    Group *g = MakeFeature(Command::GROUP_CHAMFER, { Edge(5, Group::REMAP_TOP) });
    Resize(g, "5");
    CHECK_TRUE(fabs(6000 - Volume(g) - 5 * 5 / 2.0 * 10) < 0.01);
    CHECK_TRUE(g->featureProblem.empty());
}

TEST_CASE(a_fillet_too_big_leaves_the_solid_and_says_so) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    Group *g = MakeFeature(Command::GROUP_FILLET, { Edge(5, Group::REMAP_TOP) });
    Resize(g, "50");
    CHECK_EQ_EPS(Volume(g), 6000);
    CHECK_FALSE(g->featureProblem.empty());
}

TEST_CASE(an_edge_rounded_away_is_reported_and_the_rest_round) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    Group *first = MakeFeature(Command::GROUP_CHAMFER, { Edge(5, Group::REMAP_TOP) });
    Resize(first, "5");
    hGroup hfirst = first->h;
    Group *g = MakeFeature(Command::GROUP_CHAMFER,
                           { Edge(5, Group::REMAP_TOP), Edge(7, Group::REMAP_TOP) });
    Resize(g, "5");
    CHECK_TRUE(fabs(6000 - Volume(g) - 2 * 125) < 0.01);
    CHECK_TRUE(g->featureProblem.find("not found") != std::string::npos);
    CHECK_TRUE(SK.GetGroup(hfirst)->featureProblem.empty());
}

TEST_CASE(a_fillet_with_the_solvespace_kernel_keeps_the_solid_and_says_so) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    Group *g = MakeFeature(Command::GROUP_FILLET, { Edge(5, Group::REMAP_TOP) });
    hGroup hg = g->h;
    SS.SetKernel(KernelKind::SOLVESPACE);
    g = SK.GetGroup(hg);
    CHECK_EQ_EPS(Volume(g), 6000);
    CHECK_TRUE(g->featureProblem.find("Open CASCADE") != std::string::npos);
}

#endif
