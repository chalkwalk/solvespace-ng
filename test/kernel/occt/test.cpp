// The Open CASCADE kernel (src/kernel/occtkernel.cpp): the solids SolveSpace's own kernel
// makes, measured the same within the tessellation's tolerance, with the same faces named.
#include "solvespace.h"
#include "kernel/occtshape.h" // windows.h before the OCCT headers
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <GProp_GProps.hxx>
#include <STEPControl_Reader.hxx>
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
    mesh.MakeFromCopyOf(&g->RunningMesh());
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
    for(const STriangle &t : g->ThisMesh().l) {
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
    CHECK_FALSE(g->RunningMesh().IsEmpty());
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

TEST_CASE(choosing_a_kernel_regenerates_every_group_with_it) {
    CHECK_LOAD("../solvespace/extrudes.slvs");
    Group *g = SK.GetGroup(SK.groupOrder[SK.groupOrder.n - 1]);
    CHECK_TRUE(g->runningShape == nullptr);
    SS.SetKernel(KernelKind::OCCT);
    CHECK_TRUE(SS.kernelKind == KernelKind::OCCT);
    CHECK_TRUE(g->runningShape != nullptr);
    CHECK_TRUE(g->runningShell.IsEmpty());
    SS.SetKernel(KernelKind::SOLVESPACE);
    CHECK_TRUE(g->runningShape == nullptr);
    CHECK_FALSE(g->runningShell.IsEmpty());
}

TEST_CASE(step_export_writes_the_occt_b_rep) {
    CHECK_LOAD("../solvespace/extrudes.slvs");
    SS.SetKernel(KernelKind::OCCT);
    Group *g = SK.GetGroup(SK.groupOrder[SK.groupOrder.n - 1]);
    SS.GW.activeGroup = g->h;
    SMesh mesh = {};
    mesh.MakeFromCopyOf(&g->RunningMesh());
    double meshVolume = mesh.CalculateVolume();
    mesh.Clear();

    Platform::Path path = helper->GetAssetPath(__FILE__, "extrudes.step", "out");
    SS.exportScale      = 1;
    StepFileWriter sfw  = {};
    sfw.ExportSurfacesTo(path);

    STEPControl_Reader reader;
    CHECK_TRUE(reader.ReadFile(path.raw.c_str()) == IFSelect_RetDone);
    CHECK_TRUE(reader.TransferRoots() > 0);
    GProp_GProps props;
    BRepGProp::VolumeProperties(reader.OneShape(), props);
    CHECK_TRUE(Near(props.Mass(), meshVolume, 0.01));
    remove(path.raw.c_str());
}

// The outlines the OCCT kernel makes from its triangulation's topology are the sharp edges
// SolveSpace finds by searching the mesh (which draws an edge where four faces meet twice).
static std::set<std::vector<long>> SharpEdges(SOutlineList *ol) {
    std::set<std::vector<long>> edges;
    for(const SOutline &o : ol->l) {
        if(!o.tag) continue;
        std::vector<long> a = { lround(o.a.x * 1e3), lround(o.a.y * 1e3), lround(o.a.z * 1e3) };
        std::vector<long> b = { lround(o.b.x * 1e3), lround(o.b.y * 1e3), lround(o.b.z * 1e3) };
        if(b < a) std::swap(a, b);
        a.insert(a.end(), b.begin(), b.end());
        edges.insert(a);
    }
    return edges;
}

TEST_CASE(kernel_outlines_match_the_mesh_search) {
    for(const char *model : { "../solvespace/extrudes.slvs", "../../group/fillet/filleted.slvs" }) {
        CHECK_TRUE(SS.LoadFromFile(helper->GetAssetPath(__FILE__, model)));
        SS.AfterNewFile();
        RegenerateWith(KernelKind::OCCT);
        Group *g = SK.GetGroup(SK.groupOrder[SK.groupOrder.n - 1]);
        SOutlineList searched = {};
        g->RunningMesh().MakeOutlinesInto(&searched, EdgeKind::SHARP);
        std::set<std::vector<long>> want = SharpEdges(&searched);
        CHECK_TRUE(!want.empty());
        std::set<std::vector<long>> got = SharpEdges(&g->runningOutlines_);
        CHECK_TRUE(got == want);
        searched.Clear();
    }
    RegenerateWith(KernelKind::SOLVESPACE);
}
