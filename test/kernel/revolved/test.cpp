// Lathe, revolve and helix through the kernel: the solids SolveSpace made before the kernel seam
// (numbers recorded from the code before M2 task 2), so moving the code changed nothing.
#include "solvespace.h"

#include "harness.h"

static hRequest Line(Vector a, Vector b, bool construction = false) {
    hRequest hr = SS.GW.AddRequest(Request::Type::LINE_SEGMENT, /*rememberForUndo=*/false);
    SK.GetRequest(hr)->construction = construction;
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    SK.GetEntity(hr.entity(1))->PointForceTo(a);
    SK.GetEntity(hr.entity(2))->PointForceTo(b);
    return hr;
}

// A 10 x 10 square beside the Y axis, and the axis as a construction line; then the group,
// made from the axis.
static Group *Revolved(Command cmd) {
    // The fixture's own line would leave the profile open.
    for(Request &r : SK.request) {
        if(r.type == Request::Type::LINE_SEGMENT) r.construction = true;
    }
    Vector p[4] = { { 10, 0, 0 }, { 20, 0, 0 }, { 20, 10, 0 }, { 10, 10, 0 } };
    hRequest side[4];
    for(int i = 0; i < 4; i++) side[i] = Line(p[i], p[(i + 1) % 4]);
    for(int i = 0; i < 4; i++) {
        Constraint::ConstrainCoincident(side[i].entity(2), side[(i + 1) % 4].entity(1));
    }
    hRequest axis = Line({ 0, 0, 0 }, { 0, 10, 0 }, /*construction=*/true);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    SS.GW.ClearSelection();
    GraphicsWindow::Selection sel = {};
    sel.entity = axis.entity(0);
    SS.GW.selection.Add(&sel);
    Group::MenuGroup(cmd);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    return SK.GetGroup(SS.GW.activeGroup);
}

static std::string Describe(Group *g) {
    SMesh m = {};
    m.MakeFromCopyOf(&g->RunningMesh());
    g->runningShell.TriangulateInto(&m);
    std::string s = ssprintf("surfaces %d volume %.4f faces", g->runningShell.surface.n,
                             m.CalculateVolume());
    m.Clear();
    for(const SSurface &ss : g->thisShell.surface) s += ssprintf(" %x", ss.face);
    return s;
}

TEST_CASE(lathe_is_unchanged) {
    CHECK_LOAD("sketch.slvs");
    Group *g = Revolved(Command::GROUP_LATHE);
    CHECK_TRUE(g->type == Group::Type::LATHE);
    CHECK_EQ_STR(Describe(g),
                 "surfaces 10 volume 9409.5476 faces 0 0 0 0 80030021 0 0 0 0 8003000e");
}

TEST_CASE(revolve_is_unchanged) {
    CHECK_LOAD("sketch.slvs");
    Group *g = Revolved(Command::GROUP_REVOLVE);
    CHECK_TRUE(g->type == Group::Type::REVOLVE);
    CHECK_EQ_STR(Describe(g),
                 "surfaces 8 volume 3141.5905 faces 8003002f 8003002e 0 0 8003001b 0 0 8003000e");
}

TEST_CASE(helix_is_unchanged) {
    CHECK_LOAD("sketch.slvs");
    Group *g = Revolved(Command::GROUP_HELIX);
    CHECK_TRUE(g->type == Group::Type::HELIX);
    CHECK_EQ_STR(Describe(g),
                 "surfaces 10 volume 3124.1269 faces 8003002f 8003002e 0 0 0 0 0 0 0 0");
}
