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
    m.MakeFromCopyOf(&g->RunningMesh());
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
    Driver::Key("F");
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
    Driver::Key("F");
    CHECK_TRUE(EditField("feature-size", "50"));
    CHECK_FALSE(Active()->featureProblem.empty());
    CHECK_TRUE(Driver::FindItem("Problems").has_value()); // opens on a new problem
}

// Review 5: the fillet's edges are listed in Properties; a click selects one, x removes it.
TEST_CASE(ng_lists_a_fillets_edges_to_select_or_remove) {
    CHECK_LOAD("box.slvs");
    hEntity a = Active()->Remap(hRequest{ 5 }.entity(0), Group::REMAP_TOP);
    hEntity b = Active()->Remap(hRequest{ 7 }.entity(0), Group::REMAP_TOP);
    SS.GW.ClearSelection();
    for(hEntity he : { a, b }) {
        GraphicsWindow::Selection sel = {};
        sel.entity = he;
        SS.GW.selection.Add(&sel);
    }
    Driver::Key("F");
    hGroup fillet = SS.GW.activeGroup;
    CHECK_TRUE(Active()->edges.size() == 2);
    CHECK_TRUE(Driver::ClickItem(ssprintf("remove-%08x", a.v)));
    CHECK_TRUE(SK.GetGroup(fillet)->edges.size() == 1 && SK.GetGroup(fillet)->edges[0] == b);
    CHECK_TRUE(Driver::ClickItem(ssprintf("ref-%08x", b.v)));
    CHECK_TRUE(SS.GW.IsSelected(b));
}

// The radius is free: drag the fillet's axis to set it. Held, it is a dimension on the canvas,
// double-clicked to type a new one (the owner's feedback after M4a).
TEST_CASE(ng_drags_holds_and_edits_a_fillets_radius) {
    CHECK_LOAD("box.slvs");
    hEntity edge = ClickTopFrontEdge();
    Driver::Key("F");
    hGroup  fillet = SS.GW.activeGroup;
    auto    radius = [&] { return SK.GetParam(SK.GetGroup(fillet)->h.param(0))->val; };
    Entity *axis   = SK.GetEntity(SK.GetGroup(fillet)->Remap(edge, Group::REMAP_FILLET_AXIS));
    Vector  at     = SK.GetEntity(axis->point[0])->PointGetNum(); // (0, 1, 29)
    Point2d from = At(at), to = At(at.Plus({ 0, 1, -1 }));
    Driver::Drag(from.x, from.y, to.x, to.y);
    CHECK_TRUE(fabs(radius() - 2) < 0.2);

    CHECK_TRUE(Driver::ClickItem("hold this size (a dimension)"));
    hConstraint size = {};
    for(Constraint &c : SK.constraint) {
        if(c.group == fillet && c.type == Constraint::Type::FEATURE_SIZE) size = c.h;
    }
    CHECK_TRUE(size.v != 0);
    DoubleClickAt(AtLabel(size));
    CHECK_TRUE(SS.GW.window->IsEditorVisible());
    Driver::Type("3");
    Driver::Key("Return");
    CHECK_EQ_EPS(SK.GetConstraint(size)->valA, 3.0);
    CHECK_EQ_EPS(radius(), 3);
    CHECK_TRUE(Driver::ClickItem("hold this size (a dimension)"));
    CHECK_TRUE(SK.constraint.FindByIdNoOops(size) == nullptr);
}

// #6: the box and a copy moved (5, 5, 0), joined; the edge where the copy's front face meets
// the box's right face, at (10, 5), is no entity, but it is picked on the solid and rounded.
TEST_CASE(ng_fillets_an_edge_no_entity_describes) {
    CHECK_LOAD("box.slvs");
    SS.GW.ActivateCommand(Command::GROUP_TRANS);
    Group *t       = Active();
    t->valA        = 2;
    t->meshCombine = Group::CombineAs::UNION;
    SK.GetParam(t->h.param(0))->val = 2.5; // each copy moves twice this
    SK.GetParam(t->h.param(1))->val = 2.5;
    SK.GetParam(t->h.param(2))->val = 0;
    SS.MarkGroupDirty(t->h);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    // The isometric view (from (1, 1, 1)) looks at the corner from behind the copy: hidden,
    // so not picked.
    Driver::Key("Shift+7");
    Point2d hidden = At({ 10, 5, 15 });
    Driver::Move(hidden.x, hidden.y);
    CHECK_TRUE(SS.GW.hover.solidEdge == 0);
    // Seen from (1, -1, 1), into the corner.
    SS.GW.projRight = Vector::From(1, 1, 0).WithMagnitude(1);
    SS.GW.projUp    = Vector::From(-1, 1, 2).WithMagnitude(1);
    SS.GW.offset    = Vector::From(-10, -12, -15);
    SS.GW.Invalidate();
    Driver::Key("Escape");
    Point2d edge = At({ 10, 5, 15 });
    Driver::Move(edge.x, edge.y);
    CHECK_TRUE(SS.GW.hover.solidEdge != 0);
    ClickAt(edge);
    CHECK_TRUE(SS.GW.selection.n == 1 && SS.GW.selection[0].solidEdge != 0);
    Driver::Key("F");
    CHECK_TRUE(Active()->type == Group::Type::FILLET);
    CHECK_TRUE(Active()->edgeMarks.size() == 1);
    CHECK_TRUE(Active()->featureProblem.empty());
}
