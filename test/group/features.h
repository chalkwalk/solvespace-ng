// Helpers for the feature group tests (fillet, chamfer, shell, draft) on box.slvs: a 10 x 20
// rectangle (requests 5..8, from the origin along +X first; request 4 is a construction line)
// extruded 30 along +Z, so x 0..10, y 0..20, z 0..30.
#include <set>

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

// The extrusion's side face made from a sketch line, and its caps.
hEntity SideFace(uint32_t request) {
    return Extrusion()->Remap(hRequest{ request }.entity(0), Group::REMAP_LINE_TO_FACE);
}
hEntity Cap(int remap) {
    return Extrusion()->Remap(Entity::NO_ENTITY, remap);
}

// A point in a new 3D sketch, constrained into a plane (a face entity).
hEntity PointInPlane(hEntity face) {
    Group::MenuGroup(Command::GROUP_3D);
    hRequest hr = SS.GW.AddRequest(Request::Type::DATUM_POINT, /*rememberForUndo=*/false);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    SK.GetEntity(hr.entity(0))->PointForceTo(SK.GetEntity(face)->FaceGetPointNum().Plus({ 1, 1, 1 }));
    Constraint::Constrain(Constraint::Type::PT_ON_FACE, hr.entity(0), Entity::NO_ENTITY, face);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    return hr.entity(0);
}

// Moves the front side (request 5, y = 0) to y.
void MoveFront(double y) {
    SK.GetEntity(hRequest{ 5 }.entity(1))->PointForceTo({ 0, y, 0 });
    SK.GetEntity(hRequest{ 5 }.entity(2))->PointForceTo({ 10, y, 0 });
    SK.GetEntity(hRequest{ 6 }.entity(1))->PointForceTo({ 10, y, 0 });
    SK.GetEntity(hRequest{ 8 }.entity(2))->PointForceTo({ 0, y, 0 });
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
}

}
