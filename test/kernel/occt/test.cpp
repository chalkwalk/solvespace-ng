// The Open CASCADE kernel (src/kernel/occtkernel.cpp): the solids SolveSpace's own kernel
// makes, measured the same within the tessellation's tolerance, with the same faces named.
#include "solvespace.h"
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <GProp_GProps.hxx>
#include <set>

#include "harness.h"

TEST_CASE(occt_measures_a_box) {
    TopoDS_Shape box = BRepPrimAPI_MakeBox(10, 20, 30).Shape();
    GProp_GProps props;
    BRepGProp::VolumeProperties(box, props);
    CHECK_EQ_EPS(props.Mass(), 6000);
}

namespace {

struct Measure {
    double                 volume = 0;
    size_t                 naked  = 0;
    std::set<uint32_t>     faces; // the names the group's own solid gives its faces
};

Measure MeasureGroup(Group *g) {
    Measure m;
    SMesh   mesh = {};
    mesh.MakeFromCopyOf(&g->runningMesh);
    g->runningShell.TriangulateInto(&mesh);
    m.volume = mesh.CalculateVolume();
    SEdgeList naked = {};
    SKdNode  *root  = SKdNode::From(&mesh);
    root->MakeCertainEdgesInto(&naked, EdgeKind::NAKED_OR_SELF_INTER, /*coplanarIsInter=*/false,
                               NULL, NULL);
    m.naked = naked.l.n;
    naked.Clear();
    mesh.Clear();
    for(const SSurface &ss : g->thisShell.surface) {
        if(ss.face != 0) m.faces.insert(ss.face);
    }
    for(const STriangle &t : g->thisMesh.l) {
        if(t.meta.face != 0) m.faces.insert(t.meta.face);
    }
    return m;
}

void RegenerateWith(KernelKind kind) {
    SS.kernelKind = kind;
    SS.MarkGroupDirty(SK.groupOrder[0]);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
}

bool Near(double a, double b, double relative) {
    return fabs(a - b) <= relative * std::max(fabs(a), fabs(b)) + 1e-9;
}

hRequest Line(Vector a, Vector b, bool construction = false) {
    hRequest hr = SS.GW.AddRequest(Request::Type::LINE_SEGMENT, /*rememberForUndo=*/false);
    SK.GetRequest(hr)->construction = construction;
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    SK.GetEntity(hr.entity(1))->PointForceTo(a);
    SK.GetEntity(hr.entity(2))->PointForceTo(b);
    return hr;
}

// A 10 x 10 square beside the Y axis, and the axis as a construction line; then the group,
// made from the axis (as test/kernel/revolved).
Group *Revolved(Command cmd) {
    for(Request &r : SK.request) {
        if(r.type == Request::Type::LINE_SEGMENT) r.construction = true;
    }
    Vector   p[4] = { { 10, 0, 0 }, { 20, 0, 0 }, { 20, 10, 0 }, { 10, 10, 0 } };
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

// The group made with each kernel: SolveSpace's measures, then OCCT's.
std::pair<Measure, Measure> BothKernels(hGroup hg) {
    RegenerateWith(KernelKind::SOLVESPACE);
    Measure ss = MeasureGroup(SK.GetGroup(hg));
    RegenerateWith(KernelKind::OCCT);
    Measure occt = MeasureGroup(SK.GetGroup(hg));
    RegenerateWith(KernelKind::SOLVESPACE);
    return { ss, occt };
}

}

TEST_CASE(extrusions_match_solvespaces) {
    CHECK_LOAD("../solvespace/extrudes.slvs");
    int compared = 0;
    std::vector<hGroup> order(SK.groupOrder.begin(), SK.groupOrder.end());
    for(hGroup hg : order) {
        if(SK.GetGroup(hg)->type != Group::Type::EXTRUDE) continue;
        auto [ss, occt] = BothKernels(hg);
        CHECK_TRUE(ss.volume > 0);
        CHECK_TRUE(Near(occt.volume, ss.volume, 0.005));
        CHECK_TRUE(occt.naked == 0);
        CHECK_TRUE(occt.faces == ss.faces);
        compared++;
    }
    CHECK_TRUE(compared == 4);
}

TEST_CASE(the_occt_body_keeps_its_b_rep_and_true_normals) {
    CHECK_LOAD("../solvespace/extrudes.slvs");
    RegenerateWith(KernelKind::OCCT);
    Group *g = SK.GetGroup(SK.groupOrder[SK.groupOrder.n - 1]);
    CHECK_TRUE(g->runningShape != nullptr);
    CHECK_TRUE(g->runningTessellated);
    CHECK_TRUE(g->runningShell.IsEmpty());
    CHECK_FALSE(g->runningMesh.IsEmpty());
    RegenerateWith(KernelKind::SOLVESPACE);
    CHECK_TRUE(g->runningShape == nullptr);
}

TEST_CASE(lathe_matches_solvespaces) {
    CHECK_LOAD("../revolved/sketch.slvs");
    Group *g = Revolved(Command::GROUP_LATHE);
    auto [ss, occt] = BothKernels(g->h);
    CHECK_TRUE(Near(occt.volume, ss.volume, 0.005));
    CHECK_TRUE(occt.naked == 0);
    CHECK_TRUE(occt.faces == ss.faces);
}

TEST_CASE(revolve_matches_solvespaces) {
    CHECK_LOAD("../revolved/sketch.slvs");
    Group *g = Revolved(Command::GROUP_REVOLVE);
    auto [ss, occt] = BothKernels(g->h);
    CHECK_TRUE(Near(occt.volume, ss.volume, 0.005));
    CHECK_TRUE(occt.naked == 0);
    CHECK_TRUE(occt.faces == ss.faces);
}

TEST_CASE(helix_matches_solvespaces) {
    CHECK_LOAD("../revolved/sketch.slvs");
    Group *g = Revolved(Command::GROUP_HELIX);
    auto [ss, occt] = BothKernels(g->h);
    CHECK_TRUE(Near(occt.volume, ss.volume, 0.01));
    CHECK_TRUE(occt.naked == 0);
    CHECK_TRUE(occt.faces == ss.faces);
}
