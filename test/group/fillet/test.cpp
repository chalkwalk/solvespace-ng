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

// filleted.slvs: box.slvs, its top front edge filleted (r = 2), and a point in a 3D sketch on
// the fillet's first tangent line. With SolveSpace's kernel the fillet's lines still exist (on
// the edge), so the constraint on them is kept.
TEST_CASE(derived_lines_exist_without_occt) {
    CHECK_LOAD("filleted.slvs");
    Group *g = nullptr;
    for(Group &gi : SK.group) {
        if(gi.type == Group::Type::FILLET) g = &gi;
    }
    CHECK_TRUE(g != nullptr && g->edges.size() == 1);
    CHECK_TRUE(SK.constraint.n == 5);
    CHECK_TRUE(SK.entity.FindByIdNoOops(g->Remap(g->edges[0], Group::REMAP_FILLET_TANGENT_1)));
    CHECK_TRUE(SK.entity.FindByIdNoOops(g->Remap(g->edges[0], Group::REMAP_FILLET_AXIS)));
    CHECK_TRUE(g->featureProblem.find("Open CASCADE") != std::string::npos);
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

// Derived entities: a fillet's tangent lines and axis, a chamfer's edge lines, made for each
// straight edge between planar faces and handled Remap(edge, REMAP_FILLET_...).
namespace {

Entity *FeatureLine(Group *g, hEntity edge, int which) {
    return SK.entity.FindByIdNoOops(g->Remap(edge, which));
}

bool LineIs(Entity *line, Vector a, Vector b) {
    if(line == nullptr || line->type != Entity::Type::LINE_SEGMENT) return false;
    Vector p = SK.GetEntity(line->point[0])->PointGetNum();
    Vector q = SK.GetEntity(line->point[1])->PointGetNum();
    return (p.Equals(a, 1e-6) && q.Equals(b, 1e-6)) || (p.Equals(b, 1e-6) && q.Equals(a, 1e-6));
}

// One of the two lines is a-b and the other c-d.
bool LinesAre(Entity *one, Entity *two, Vector a, Vector b, Vector c, Vector d) {
    return (LineIs(one, a, b) && LineIs(two, c, d)) || (LineIs(one, c, d) && LineIs(two, a, b));
}

// A point in a new 3D sketch, constrained onto a line.
hEntity PointOn(hEntity line) {
    Group::MenuGroup(Command::GROUP_3D);
    hRequest hr = SS.GW.AddRequest(Request::Type::DATUM_POINT, /*rememberForUndo=*/false);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    SK.GetEntity(hr.entity(0))->PointForceTo(SK.GetEntity(SK.GetEntity(line)->point[0])
                                                  ->PointGetNum().Plus({ 3, 1, 1 }));
    Constraint::Constrain(Constraint::Type::PT_ON_LINE, hr.entity(0), Entity::NO_ENTITY, line);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    return hr.entity(0);
}

}

TEST_CASE(a_fillet_declares_its_tangent_lines_and_axis) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    hEntity edge = Edge(5, Group::REMAP_TOP); // (0, 0, 30) to (10, 0, 30)
    Group  *g    = MakeFeature(Command::GROUP_FILLET, { edge });
    Resize(g, "2");
    CHECK_TRUE(LinesAre(FeatureLine(g, edge, Group::REMAP_FILLET_TANGENT_1),
                        FeatureLine(g, edge, Group::REMAP_FILLET_TANGENT_2),
                        { 0, 2, 30 }, { 10, 2, 30 }, { 0, 0, 28 }, { 10, 0, 28 }));
    CHECK_TRUE(LineIs(FeatureLine(g, edge, Group::REMAP_FILLET_AXIS), { 0, 2, 28 },
                      { 10, 2, 28 }));
}

TEST_CASE(a_chamfer_declares_its_edge_lines) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    hEntity edge = Edge(6, Group::REMAP_TOP); // (10, 0, 30) to (10, 20, 30)
    Group  *g    = MakeFeature(Command::GROUP_CHAMFER, { edge });
    Resize(g, "2");
    CHECK_TRUE(LinesAre(FeatureLine(g, edge, Group::REMAP_CHAMFER_EDGE_1),
                        FeatureLine(g, edge, Group::REMAP_CHAMFER_EDGE_2),
                        { 8, 0, 30 }, { 8, 20, 30 }, { 10, 0, 28 }, { 10, 20, 28 }));
}

TEST_CASE(a_point_on_a_tangent_line_follows_a_change_upstream) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    hEntity edge = Edge(5, Group::REMAP_TOP);
    Group  *g    = MakeFeature(Command::GROUP_FILLET, { edge });
    Resize(g, "2");
    hEntity axis = g->Remap(edge, Group::REMAP_FILLET_AXIS);
    hEntity pt   = PointOn(axis);
    CHECK_EQ_EPS(SK.GetEntity(pt)->PointGetNum().z, 28);
    // Forty deep: the axis, and the point on it, move up with the top face in one
    // regeneration, without waiting for the solid.
    SK.GetParam(Extrusion()->h.param(2))->val = 20;
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_EQ_EPS(SK.GetEntity(pt)->PointGetNum().z, 38);
    CHECK_EQ_EPS(SK.GetEntity(pt)->PointGetNum().y, 2);
}

TEST_CASE(a_point_on_a_tangent_line_follows_a_face_turning_upstream) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    hEntity edge = Edge(5, Group::REMAP_TOP);
    Group  *g    = MakeFeature(Command::GROUP_FILLET, { edge });
    Resize(g, "2");
    hEntity top = g->Remap(edge, Group::REMAP_FILLET_TANGENT_1);
    if(SK.GetEntity(SK.GetEntity(top)->point[0])->PointGetNum().z < 29.5) {
        top = g->Remap(edge, Group::REMAP_FILLET_TANGENT_2);
    }
    hEntity pt = PointOn(top);
    // The front face turns: its far corner moves from (10, 0) to (10, -5). The tangent line on
    // the top stays 2 from the turned edge, in the plane of the top, in one regeneration.
    SK.GetEntity(hRequest{ 5 }.entity(2))->PointForceTo({ 10, -5, 0 });
    SK.GetEntity(hRequest{ 6 }.entity(1))->PointForceTo({ 10, -5, 0 });
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Vector p = SK.GetEntity(pt)->PointGetNum();
    CHECK_EQ_EPS(p.z, 30);
    CHECK_EQ_EPS(p.DistanceToLine({ 0, 0, 30 }, { 10, -5, 0 }), 2);
}

TEST_CASE(a_constraint_on_a_tangent_line_survives_undo_and_reload) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    hEntity edge = Edge(5, Group::REMAP_TOP);
    Group  *g    = MakeFeature(Command::GROUP_FILLET, { edge });
    Resize(g, "2");
    hEntity line = g->Remap(edge, Group::REMAP_FILLET_TANGENT_1);
    hEntity pt   = PointOn(line);
    int     constraints = SK.constraint.n;
    SS.UndoRemember();
    SK.GetParam(Extrusion()->h.param(2))->val = 20;
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    SS.UndoUndo();
    CHECK_TRUE(SK.constraint.n == constraints);

    Platform::Path path = helper->GetAssetPath(__FILE__, "tangent.slvs", "out");
    CHECK_TRUE(SS.SaveToFile(path));
    CHECK_TRUE(SS.LoadFromFile(path));
    SS.AfterNewFile();
    SS.kernelKind = KernelKind::OCCT;
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_TRUE(SK.constraint.n == constraints);
    Entity *l = SK.GetEntity(line);
    Vector  a = SK.GetEntity(l->point[0])->PointGetNum();
    Vector  b = SK.GetEntity(l->point[1])->PointGetNum();
    CHECK_TRUE(SK.GetEntity(pt)->PointGetNum().DistanceToLine(a, b.Minus(a)) < 1e-6);
    remove(path.raw.c_str());
}

TEST_CASE(a_rounded_edge_is_hidden_while_the_fillet_is_shown) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    hEntity edge = Edge(5, Group::REMAP_TOP);
    Group  *box  = Extrusion();
    MakeFeature(Command::GROUP_FILLET, { edge });
    CHECK_FALSE(SK.GetEntity(edge)->IsVisible());
    CHECK_TRUE(SK.GetEntity(Edge(7, Group::REMAP_TOP))->IsVisible());
    SS.GW.activeGroup = box->h;
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_TRUE(SK.GetEntity(edge)->IsVisible());
}

#endif
