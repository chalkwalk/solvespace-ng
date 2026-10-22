// Implied selection scope (the owner's feedback after M4a, as in Blender): a selection stands
// for what an operation needs. Edges that bound a face whole stand for the face, for the
// operations on faces (shell, draft, sketching on a face); faces stand for their edges for
// fillet and chamfer (test/group/fillet).
#include "solvespace.h"

#include "harness.h"

#include "../features.h"

#if defined(HAVE_OCCT)
namespace {

// The top face's four edges; the front face's four.
std::vector<hEntity> TopEdges() {
    return { Edge(5, Group::REMAP_TOP), Edge(6, Group::REMAP_TOP), Edge(7, Group::REMAP_TOP),
             Edge(8, Group::REMAP_TOP) };
}

std::vector<hEntity> FrontEdges() {
    return { Edge(5, Group::REMAP_TOP), Edge(5, Group::REMAP_BOTTOM),
             Extrusion()->Remap(hRequest{ 5 }.entity(1), Group::REMAP_PT_TO_LINE),
             Extrusion()->Remap(hRequest{ 5 }.entity(2), Group::REMAP_PT_TO_LINE) };
}

}

TEST_CASE(a_faces_edges_stand_for_it_in_a_shell) {
    CHECK_LOAD("box.slvs");
    SS.SetKernel(KernelKind::OCCT);
    Group *g = MakeFeature(Command::GROUP_SHELL, TopEdges());
    CHECK_TRUE(g != nullptr && g->faces.size() == 1 && g->faces[0] == Cap(Group::REMAP_TOP));
}

TEST_CASE(a_faces_edges_stand_for_it_in_a_draft) {
    CHECK_LOAD("box.slvs");
    SS.SetKernel(KernelKind::OCCT);
    Group *g = MakeFeature(Command::GROUP_DRAFT, FrontEdges());
    CHECK_TRUE(g != nullptr && g->faces.size() == 1 && g->faces[0] == SideFace(5));
}

TEST_CASE(a_faces_edges_stand_for_it_when_sketching_on_it) {
    CHECK_LOAD("box.slvs");
    SS.SetKernel(KernelKind::OCCT);
    SS.GW.projRight = Vector::From(1, 0, 0);
    SS.GW.projUp    = Vector::From(0, 1, 0);
    Group *g = MakeFeature(Command::GROUP_WRKPL, TopEdges());
    CHECK_TRUE(g != nullptr && g->subtype == Group::Subtype::WORKPLANE_BY_FACE);
    CHECK_TRUE(g->predef.entityB == Cap(Group::REMAP_TOP));
}

TEST_CASE(edges_that_bound_no_face_whole_stand_for_nothing) {
    CHECK_LOAD("box.slvs");
    SS.SetKernel(KernelKind::OCCT);
    std::vector<hEntity> three = TopEdges();
    three.pop_back();
    CHECK_TRUE(MakeFeature(Command::GROUP_SHELL, three) == nullptr);
}
#endif
