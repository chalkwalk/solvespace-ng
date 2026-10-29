// Fillet and chamfer groups (AGENTS.md section 6, M3b): made from edges of the solid, which are
// the entities a solid group makes (an extrusion's copies of its sketch, its side lines).
// box.slvs: see ../features.h.
#include "solvespace.h"

#include "harness.h"

#include "../features.h"

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

// The kernel's notes are translated (on the main thread), in the language chosen (#5).
TEST_CASE(the_kernels_note_follows_the_language) {
    CHECK_TRUE(SetLocale("de-DE"));
    CHECK_LOAD("filleted.slvs");
    std::string problem;
    for(Group &g : SK.group) {
        if(g.type == Group::Type::FILLET) problem = g.featureProblem;
    }
    CHECK_TRUE(SetLocale("en-US"));
    CHECK_TRUE(problem.find("CASCADE") != std::string::npos);
    CHECK_TRUE(problem.find("need the Open CASCADE kernel") == std::string::npos);
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
    // The extrusion's own solid is as it was (found again: adding the fillet may have moved
    // the groups in memory).
    CHECK_EQ_EPS(Volume(Extrusion()), 6000);
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

TEST_CASE(a_point_on_a_fillet_axis_follows_a_drag_before_the_solid_does) {
    // While dragging, solids are made on the worker; the lines follow every frame anyway.
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    hEntity edge = Edge(5, Group::REMAP_TOP);
    Group  *g    = MakeFeature(Command::GROUP_FILLET, { edge });
    Resize(g, "2");
    hGroup  hg  = g->h; // g moves when the next group is added
    hEntity pt  = PointOn(g->Remap(edge, Group::REMAP_FILLET_AXIS));
    hGroup  box = Extrusion()->h;
    SS.backgroundRegenEnabled = true;
    SS.GW.pending.operation   = GraphicsWindow::Pending::DRAGGING_POINTS;
    int applied               = SS.regenChainsApplied;
    for(double half = 16; half <= 20; half++) {
        SK.GetParam(SK.GetGroup(box)->h.param(2))->val = half;
        SS.MarkGroupDirty(box);
        SS.GenerateAll(SolveSpaceUI::Generate::DIRTY);
        CHECK_EQ_EPS(SK.GetEntity(pt)->PointGetNum().z, 2 * half - 2);
        SS.PollRegeneration();
    }
    SS.GW.pending.operation = GraphicsWindow::Pending::NONE;
    SS.FlushRegeneration();
    CHECK_TRUE(SS.regenChainsApplied > applied); // made on the worker
    CHECK_EQ_EPS(SK.GetEntity(pt)->PointGetNum().z, 38);
    CHECK_TRUE(fabs(8000 - Volume(SK.GetGroup(hg)) - (1 - PI / 4) * 4 * 10) < 1);
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
    hGroup  box  = Extrusion()->h;
    MakeFeature(Command::GROUP_FILLET, { edge });
    CHECK_FALSE(SK.GetEntity(edge)->IsVisible());
    CHECK_TRUE(SK.GetEntity(Edge(7, Group::REMAP_TOP))->IsVisible());
    SS.GW.activeGroup = box;
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_TRUE(SK.GetEntity(edge)->IsVisible());
}

#endif

#if defined(HAVE_OCCT)
// Review 5: derived lines are references, drawn quieter than edges: thin and dashed.
TEST_CASE(derived_lines_are_drawn_as_references) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    hEntity edge = Edge(5, Group::REMAP_TOP);
    Group  *g    = MakeFeature(Command::GROUP_FILLET, { edge });
    hEntity line = g->Remap(edge, Group::REMAP_FILLET_TANGENT_1);
    hStyle  hs   = Style::ForEntity(line);
    CHECK_TRUE(hs.v == Style::REFERENCE);
    Style *ref = Style::Get(hs), *edgeStyle = Style::Get(hStyle{ Style::ACTIVE_GRP });
    CHECK_TRUE(ref->stippleType != StipplePattern::CONTINUOUS);
    CHECK_TRUE(ref->width < edgeStyle->width);
}
#endif

#if defined(HAVE_OCCT)
// Review 5: Properties lists a feature's edges, faces or points; each can be removed, as long
// as the feature keeps what it needs.
TEST_CASE(a_feature_reference_can_be_removed_but_not_the_last) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    hEntity a = Edge(5, Group::REMAP_TOP), b = Edge(7, Group::REMAP_TOP);
    Group  *g = MakeFeature(Command::GROUP_FILLET, { a, b });
    hGroup  hg = g->h;
    CHECK_TRUE(SS.RemoveGroupRef(hg, a));
    CHECK_TRUE(SK.GetGroup(hg)->edges.size() == 1 && SK.GetGroup(hg)->edges[0] == b);
    CHECK_FALSE(SS.RemoveGroupRef(hg, b));
    CHECK_TRUE(SK.GetGroup(hg)->edges.size() == 1);
    SS.UndoUndo();
    CHECK_TRUE(SK.GetGroup(hg)->edges.size() == 2);
}

TEST_CASE(a_shell_can_lose_its_last_open_face) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    hEntity top = Extrusion()->Remap(Entity::NO_ENTITY, Group::REMAP_TOP);
    Group  *g   = MakeFeature(Command::GROUP_SHELL, { top });
    CHECK_TRUE(SS.RemoveGroupRef(g->h, top));
    CHECK_TRUE(SK.GetGroup(g->h)->faces.empty());
}
#endif

#if defined(HAVE_OCCT)
// M3h: the radius is a solver parameter. The derived points follow it, so constraints on them,
// or dragging them, set it.
namespace {

double Radius(hGroup hg) {
    return SK.GetParam(SK.GetGroup(hg)->h.param(0))->val;
}

}

TEST_CASE(a_constraint_on_a_tangent_point_sets_the_radius) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    hEntity edge = Edge(5, Group::REMAP_TOP); // (0, 0, 30) to (10, 0, 30)
    hGroup  hg   = MakeFeature(Command::GROUP_FILLET, { edge })->h;
    // The tangent line on the top lies r in from the edge: pin its start 4 from the corner.
    hEntity line = SK.GetGroup(hg)->Remap(edge, Group::REMAP_FILLET_TANGENT_1);
    Entity *l    = SK.GetEntity(line);
    if(SK.GetEntity(l->point[0])->PointGetNum().z < 29.5) {
        line = SK.GetGroup(hg)->Remap(edge, Group::REMAP_FILLET_TANGENT_2);
    }
    Constraint c = {};
    c.type       = Constraint::Type::PT_PT_DISTANCE;
    c.group      = hg;
    c.workplane  = Entity::FREE_IN_3D;
    c.ptA        = SK.GetEntity(line)->point[0];
    c.ptB        = Extrusion()->Remap(hRequest{ 5 }.entity(1), Group::REMAP_TOP);
    c.valA       = 4;
    Constraint::AddConstraint(&c, /*rememberForUndo=*/false);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_EQ_EPS(Radius(hg), 4);
    MassProps mp;
    CHECK_TRUE(SS.MassProperties(&mp) && mp.exact);
    CHECK_TRUE(fabs(6000 - mp.volume - (1 - PI / 4) * 16 * 10) < 1e-6);
}

TEST_CASE(dragging_a_tangent_point_sets_the_radius) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    hEntity edge = Edge(5, Group::REMAP_TOP);
    hGroup  hg   = MakeFeature(Command::GROUP_FILLET, { edge })->h;
    Entity *axis = SK.GetEntity(SK.GetGroup(hg)->Remap(edge, Group::REMAP_FILLET_AXIS));
    // The axis lies r in along both faces: dragged to (5, 3, 27), the radius is 3.
    SK.GetEntity(axis->point[0])->PointForceTo({ 0, 3, 27 });
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_EQ_EPS(Radius(hg), 3);
}
#endif

#if defined(HAVE_OCCT)
// A feature's size is free, so its handles drag and constraints set it (owner's feedback after
// M4a). "Hold" adds a FEATURE_SIZE dimension to fix it, edited, made reference or removed like
// any dimension.
namespace {

Constraint *SizeOf(hGroup hg) {
    for(Constraint &c : SK.constraint) {
        if(c.group == hg && c.type == Constraint::Type::FEATURE_SIZE) return &c;
    }
    return nullptr;
}

}

TEST_CASE(a_fillet_starts_with_its_radius_free) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    hGroup hg = MakeFeature(Command::GROUP_FILLET, { Edge(5, Group::REMAP_TOP) })->h;
    CHECK_TRUE(SizeOf(hg) == nullptr);
    CHECK_TRUE(SK.GetGroup(hg)->solved.dof == 1);
    // Properties sets the free size.
    CHECK_TRUE(SS.SetFeatureSize(hg, "2.5"));
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_EQ_EPS(Radius(hg), 2.5);
}

TEST_CASE(holding_a_fillets_radius_adds_its_dimension) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    hGroup hg = MakeFeature(Command::GROUP_FILLET, { Edge(5, Group::REMAP_TOP) })->h;
    SS.SetFeatureSize(hg, "2");
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    SS.HoldFeatureSize(hg, true);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Constraint *c = SizeOf(hg);
    CHECK_TRUE(c != nullptr);
    CHECK_EQ_EPS(c->valA, 2);
    CHECK_TRUE(c->Label().find("R") == 0);
    CHECK_TRUE(SK.GetGroup(hg)->solved.dof == 0);
    // Editing it, or the size in Properties, sets the radius.
    CHECK_TRUE(SS.SetConstraintValue(c->h, "3"));
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_EQ_EPS(Radius(hg), 3);
    CHECK_TRUE(SS.SetFeatureSize(hg, "2.5"));
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_EQ_EPS(SizeOf(hg)->valA, 2.5);
    CHECK_EQ_EPS(Radius(hg), 2.5);
    SS.UndoUndo();
    CHECK_EQ_EPS(SizeOf(hg)->valA, 3);
    // Let go again.
    SS.HoldFeatureSize(hg, false);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_TRUE(SizeOf(hg) == nullptr);
    CHECK_EQ_EPS(Radius(hg), 3);
}

TEST_CASE(a_held_radius_holds_against_a_drag) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    hEntity edge = Edge(5, Group::REMAP_TOP);
    hGroup  hg   = MakeFeature(Command::GROUP_FILLET, { edge })->h;
    SS.HoldFeatureSize(hg, true);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Entity *axis = SK.GetEntity(SK.GetGroup(hg)->Remap(edge, Group::REMAP_FILLET_AXIS));
    SK.GetEntity(axis->point[0])->PointForceTo({ 0, 3, 27 });
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_EQ_EPS(Radius(hg), 1);
}

TEST_CASE(a_reference_radius_shows_the_radius_others_set) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    hEntity edge = Edge(5, Group::REMAP_TOP);
    hGroup  hg   = MakeFeature(Command::GROUP_FILLET, { edge })->h;
    SS.HoldFeatureSize(hg, true);
    SizeOf(hg)->reference = true;
    SS.MarkGroupDirty(hg);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_TRUE(SK.GetGroup(hg)->solved.dof == 1);
    Entity *axis = SK.GetEntity(SK.GetGroup(hg)->Remap(edge, Group::REMAP_FILLET_AXIS));
    SK.GetEntity(axis->point[0])->PointForceTo({ 0, 2, 28 });
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_EQ_EPS(Radius(hg), 2);
    CHECK_EQ_EPS(SizeOf(hg)->valA, 2);
}

TEST_CASE(a_size_survives_save_and_reload) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    hGroup hg = MakeFeature(Command::GROUP_CHAMFER, { Edge(5, Group::REMAP_TOP) })->h;
    SS.SetFeatureSize(hg, "2");
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Platform::Path path = helper->GetAssetPath(__FILE__, "sized.slvs", "out");
    CHECK_TRUE(SS.SaveToFile(path));
    CHECK_TRUE(SS.LoadFromFile(path));
    SS.AfterNewFile();
    SS.kernelKind = KernelKind::OCCT;
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_EQ_EPS(Radius(hg), 2);
    remove(path.raw.c_str());
}

// Review 6: a held size shows while its group is active, in the dimensions-only mode too.
TEST_CASE(a_size_dimension_shows_only_while_its_feature_is_active) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    hGroup hg = MakeFeature(Command::GROUP_FILLET, { Edge(5, Group::REMAP_TOP) })->h;
    SS.HoldFeatureSize(hg, true);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    hConstraint hc = SizeOf(hg)->h;
    CHECK_TRUE(SK.GetConstraint(hc)->IsVisible());
    SS.GW.showConstraints = GraphicsWindow::ShowConstraintMode::SCM_SHOW_DIM;
    CHECK_TRUE(SK.GetConstraint(hc)->IsVisible());
    SS.GW.showConstraints = GraphicsWindow::ShowConstraintMode::SCM_SHOW_ALL;
    Group::MenuGroup(Command::GROUP_3D);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_FALSE(SK.GetConstraint(hc)->IsVisible());
}
#endif

#if defined(HAVE_OCCT)
// Owner's feedback after M4a: pick faces for a fillet or chamfer: all their edges, or only the
// outline of the picked region.
TEST_CASE(a_chamfer_on_a_face_bevels_all_its_edges) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    Group *g = MakeFeature(Command::GROUP_CHAMFER, { Cap(Group::REMAP_TOP) });
    CHECK_TRUE(g != nullptr && g->faces.size() == 1 && g->edges.empty());
    hGroup hg = g->h;
    Resize(g, "1");
    CHECK_TRUE(SK.GetGroup(hg)->featureProblem.empty());
    // The top slab, 1 deep, becomes a frustum from 10 x 20 to 8 x 18.
    MassProps mp;
    CHECK_TRUE(SS.MassProperties(&mp));
    double frustum = 1.0 / 6 * (200 + 4 * 9 * 19 + 8 * 18);
    CHECK_TRUE(fabs(mp.volume - (6000 - 200 + frustum)) < 1e-6);
}

TEST_CASE(the_outline_of_picked_faces_skips_the_edges_between_them) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    hGroup all = MakeFeature(Command::GROUP_CHAMFER, { Cap(Group::REMAP_TOP), SideFace(5) })->h;
    Resize(SK.GetGroup(all), "1");
    MassProps whole;
    SS.MassProperties(&whole);
    SS.SetFeatureOutline(all, true);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_TRUE(SK.GetGroup(all)->featureProblem.empty());
    MassProps outline;
    SS.MassProperties(&outline);
    // The edge between the top and the front stays sharp: less is cut away.
    CHECK_TRUE(outline.volume > whole.volume + 0.1);
}
#endif

#if defined(HAVE_OCCT)
// #6: an edge no entity describes, picked on the solid. The box and a copy of it moved by
// (5, 5, 0), joined: where the copy's y = 5 face meets the box's x = 10 face, at (10, 5), is a
// corner of neither box. A fillet there fills the inside corner: (1 - pi/4) r^2 30 more.
namespace {

Group *JoinedBoxes() {
    SS.GW.ActivateCommand(Command::GROUP_TRANS);
    SS.Refresh();
    Group *t = SK.GetGroup(SS.GW.activeGroup);
    t->valA        = 2; // the box and one copy
    t->meshCombine = Group::CombineAs::UNION;
    // Each copy moves by twice the parameters.
    SK.GetParam(t->h.param(0))->val = 2.5;
    SK.GetParam(t->h.param(1))->val = 2.5;
    SK.GetParam(t->h.param(2))->val = 0;
    SS.MarkGroupDirty(t->h);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    return SK.GetGroup(SS.GW.activeGroup);
}

// The solid's edge through p, as the viewport picks it.
GraphicsWindow::Selection SolidEdgeAt(Group *g, Vector p) {
    GraphicsWindow::Selection sel = {};
    g->GenerateDisplayItems(); // as drawing the viewport does
    for(const SOutline &so : g->ShownOutlines().l) {
        if(so.edge != 0 && p.DistanceToLine(so.a, so.b.Minus(so.a)) < 1e-6 &&
           p.Minus(so.a).Dot(p.Minus(so.b)) <= 0) {
            sel.solidEdge      = so.edge;
            sel.solidEdgeGroup = g->h;
        }
    }
    return sel;
}

}

TEST_CASE(a_fillet_rounds_an_edge_no_entity_describes) {
    CHECK_LOAD("box.slvs");
    SS.SetKernel(KernelKind::OCCT);
    Group *t = JoinedBoxes();
    CHECK_TRUE(t->type == Group::Type::TRANSLATE);
    double joined = Volume(t);
    CHECK_TRUE(fabs(joined - (2 * 6000 - 5 * 15 * 30)) < 1e-3);
    GraphicsWindow::Selection sel = SolidEdgeAt(t, Vector::From(10, 5, 15));
    CHECK_TRUE(sel.solidEdge != 0);
    SS.GW.ClearSelection();
    SS.GW.selection.Add(&sel);
    int before = SK.groupOrder.n;
    Group::MenuGroup(Command::GROUP_FILLET);
    CHECK_TRUE(SK.groupOrder.n == before + 1);
    Group *g = SK.GetGroup(SS.GW.activeGroup);
    CHECK_TRUE(g->type == Group::Type::FILLET);
    CHECK_TRUE(g->edges.empty() && g->edgeMarks.size() == 1);
    Resize(g, "2");
    g = SK.GetGroup(SS.GW.activeGroup);
    CHECK_TRUE(g->featureProblem.empty());
    double added = Volume(g) - joined;
    CHECK_TRUE(fabs(added - (1 - PI / 4) * 4 * 30) < 1.5);

    // The mark is saved.
    hGroup         hg   = g->h;
    Platform::Path path = helper->GetAssetPath(__FILE__, "marked.slvs", "out");
    CHECK_TRUE(SS.SaveToFile(path));
    CHECK_TRUE(SS.LoadFromFile(path));
    SS.AfterNewFile();
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    g = SK.GetGroup(hg);
    CHECK_TRUE(g->edgeMarks.size() == 1);
    CHECK_TRUE(fabs(Volume(g) - joined - added) < 1e-3);
    remove(path.raw.c_str());
}

TEST_CASE(an_edge_held_by_where_it_is_can_be_lost) {
    CHECK_LOAD("box.slvs");
    SS.SetKernel(KernelKind::OCCT);
    Group *t = JoinedBoxes();
    GraphicsWindow::Selection sel = SolidEdgeAt(t, Vector::From(10, 5, 15));
    SS.GW.ClearSelection();
    SS.GW.selection.Add(&sel);
    Group::MenuGroup(Command::GROUP_FILLET);
    hGroup hg = SS.GW.activeGroup;
    Resize(SK.GetGroup(hg), "2");
    // The copy moves: the edge is at (10, 7) now, not where it was picked.
    t = SK.GetGroup(t->h);
    SK.GetParam(t->h.param(1))->val = 3.5;
    SS.MarkGroupDirty(t->h);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_FALSE(SK.GetGroup(hg)->featureProblem.empty());
}

// #7: along a circular edge between a flat face and a cylinder meeting square, a fillet
// declares tangent circles and an axis circle, a chamfer its edge circles. box.slvs with a
// hole of radius 2 through it at (5, 10).
namespace {

hEntity HoleRim() {
    SS.checkClosedContour = false;
    hGroup extrude = Extrusion()->h;
    SS.GW.activeGroup = SK.groupOrder[1]; // the sketch
    hRequest c = SS.GW.AddRequest(Request::Type::CIRCLE, /*rememberForUndo=*/false);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    SK.GetEntity(c.entity(1))->PointForceTo({ 5, 10, 0 });
    SK.GetEntity(c.entity(64))->DistanceForceTo(2);
    SS.GW.activeGroup = extrude;
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    return Extrusion()->Remap(c.entity(0), Group::REMAP_TOP);
}

bool CircleIs(Group *g, hEntity edge, int which, Vector centre, double radius) {
    Entity *e = SK.entity.FindByIdNoOops(g->Remap(edge, which));
    if(e == nullptr || e->type != Entity::Type::CIRCLE) return false;
    return SK.GetEntity(e->point[0])->PointGetNum().Equals(centre, 1e-6) &&
           fabs(SK.GetEntity(e->distance)->DistanceGetNum() - radius) < 1e-6 &&
           fabs(fabs(e->Normal()->NormalN().z) - 1) < 1e-6;
}

}

TEST_CASE(a_fillet_on_a_hole_rim_declares_its_circles) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    hEntity rim = HoleRim();
    Group  *g   = MakeFeature(Command::GROUP_FILLET, { rim });
    Resize(g, "0.5");
    g = SK.GetGroup(SS.GW.activeGroup);
    // On the top, out from the hole; down the hole's wall; and the round's centre.
    CHECK_TRUE(CircleIs(g, rim, Group::REMAP_FILLET_TANGENT_1, { 5, 10, 30 }, 2.5));
    CHECK_TRUE(CircleIs(g, rim, Group::REMAP_FILLET_TANGENT_2, { 5, 10, 29.5 }, 2));
    CHECK_TRUE(CircleIs(g, rim, Group::REMAP_FILLET_AXIS, { 5, 10, 29.5 }, 2.5));
    CHECK_TRUE(g->curvedEdges == 0);
    Resize(g, "1");
    g = SK.GetGroup(SS.GW.activeGroup);
    CHECK_TRUE(CircleIs(g, rim, Group::REMAP_FILLET_AXIS, { 5, 10, 29 }, 3));
}

TEST_CASE(a_chamfer_on_a_hole_rim_declares_its_circles) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    hEntity rim = HoleRim();
    Group  *g   = MakeFeature(Command::GROUP_CHAMFER, { rim });
    Resize(g, "0.5");
    g = SK.GetGroup(SS.GW.activeGroup);
    CHECK_TRUE(CircleIs(g, rim, Group::REMAP_CHAMFER_EDGE_1, { 5, 10, 30 }, 2.5));
    CHECK_TRUE(CircleIs(g, rim, Group::REMAP_CHAMFER_EDGE_2, { 5, 10, 29.5 }, 2));
}

TEST_CASE(a_diameter_on_a_tangent_circle_sets_the_radius) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    hEntity rim = HoleRim();
    hGroup  hg  = MakeFeature(Command::GROUP_FILLET, { rim })->h;
    Constraint c = {};
    c.type       = Constraint::Type::DIAMETER;
    c.group      = hg;
    c.workplane  = Entity::FREE_IN_3D;
    c.entityA    = SK.GetGroup(hg)->Remap(rim, Group::REMAP_FILLET_TANGENT_1);
    c.valA       = 7; // radius 3.5: the round is 1.5
    Constraint::AddConstraint(&c, /*rememberForUndo=*/false);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_EQ_EPS(Radius(hg), 1.5);
    // The constraint on the derived circle survives saving and reloading.
    Platform::Path path = helper->GetAssetPath(__FILE__, "rim.slvs", "out");
    CHECK_TRUE(SS.SaveToFile(path));
    CHECK_TRUE(SS.LoadFromFile(path));
    SS.AfterNewFile();
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_EQ_EPS(Radius(hg), 1.5);
    remove(path.raw.c_str());
}

TEST_CASE(a_fillet_on_an_arc_rim_declares_its_arcs) {
    // A D-shaped hole: half a circle of radius 2 about (5, 10), closed by its diameter.
    CHECK_LOAD("box.slvs");
    SS.kernelKind         = KernelKind::OCCT;
    SS.checkClosedContour = false;
    hGroup extrude    = Extrusion()->h;
    SS.GW.activeGroup = SK.groupOrder[1];
    hRequest arc  = SS.GW.AddRequest(Request::Type::ARC_OF_CIRCLE, /*rememberForUndo=*/false);
    hRequest line = SS.GW.AddRequest(Request::Type::LINE_SEGMENT, /*rememberForUndo=*/false);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    SK.GetEntity(arc.entity(1))->PointForceTo({ 5, 10, 0 });
    SK.GetEntity(arc.entity(2))->PointForceTo({ 7, 10, 0 });
    SK.GetEntity(arc.entity(3))->PointForceTo({ 3, 10, 0 });
    SK.GetEntity(line.entity(1))->PointForceTo({ 3, 10, 0 });
    SK.GetEntity(line.entity(2))->PointForceTo({ 7, 10, 0 });
    Constraint::Constrain(Constraint::Type::POINTS_COINCIDENT, arc.entity(3), line.entity(1),
                          Entity::NO_ENTITY);
    Constraint::Constrain(Constraint::Type::POINTS_COINCIDENT, arc.entity(2), line.entity(2),
                          Entity::NO_ENTITY);
    SS.GW.activeGroup = extrude;
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    hEntity rim = Extrusion()->Remap(arc.entity(0), Group::REMAP_TOP);
    Group  *g   = MakeFeature(Command::GROUP_FILLET, { rim });
    Resize(g, "0.5");
    g = SK.GetGroup(SS.GW.activeGroup);
    CHECK_TRUE(g->curvedEdges == 0);
    Entity *t1 = SK.entity.FindByIdNoOops(g->Remap(rim, Group::REMAP_FILLET_TANGENT_1));
    CHECK_TRUE(t1 != nullptr && t1->type == Entity::Type::ARC_OF_CIRCLE);
    if(t1 != nullptr) {
        CHECK_TRUE(SK.GetEntity(t1->point[0])->PointGetNum().Equals({ 5, 10, 30 }, 1e-6));
        CHECK_TRUE(SK.GetEntity(t1->point[1])->PointGetNum().Equals({ 7.5, 10, 30 }, 1e-6));
        CHECK_TRUE(SK.GetEntity(t1->point[2])->PointGetNum().Equals({ 2.5, 10, 30 }, 1e-6));
    }
    Entity *ax = SK.entity.FindByIdNoOops(g->Remap(rim, Group::REMAP_FILLET_AXIS));
    CHECK_TRUE(ax != nullptr && ax->type == Entity::Type::ARC_OF_CIRCLE);
    if(ax != nullptr) {
        CHECK_TRUE(SK.GetEntity(ax->point[0])->PointGetNum().Equals({ 5, 10, 29.5 }, 1e-6));
        CHECK_TRUE(SK.GetEntity(ax->point[1])->PointGetNum().Equals({ 7.5, 10, 29.5 }, 1e-6));
    }
}
#endif
