// The quad mesh export (a spike, after the owner's question in review 5): the B-rep written for
// Blender as USD, a structured quad grid on every four-sided face following the face's own
// (u, v) flow, n-gons on the rest, shared edge points (watertight), and creases on the sharp
// edges for Catmull-Clark subdivision.
#include "solvespace.h"

#include "harness.h"

namespace {

Group *Last() {
    return SK.GetGroup(SK.groupOrder[SK.groupOrder.n - 1]);
}

std::string ReadAll(const Platform::Path &path) {
    std::string s;
    FILE *f = fopen(path.raw.c_str(), "rb");
    if(!f) return s;
    char buf[4096];
    size_t n;
    while((n = fread(buf, 1, sizeof(buf), f)) > 0) s.append(buf, n);
    fclose(f);
    return s;
}

}

TEST_CASE(a_box_is_a_quad_grid_with_creased_edges) {
    CHECK_LOAD("box.slvs"); // 10 x 20 x 30
    SS.SetKernel(KernelKind::OCCT);
    Platform::Path path = helper->GetAssetPath(__FILE__, "box.usda", "out");
    QuadMeshStats  st;
    CHECK_TRUE(KernelExportQuadUsd(Last()->runningShape, path, /*target=*/10, &st));
    // Sides cut into 1, 2 and 3 along x, y and z: 2 (1x2 + 1x3 + 2x3) quads on a 2x3x4 lattice.
    CHECK_TRUE(st.quads == 22);
    CHECK_TRUE(st.ngons == 0 && st.triangles == 0);
    CHECK_TRUE(st.vertices == 24);
    CHECK_TRUE(st.creases == 12);
    CHECK_TRUE(st.openEdges == 0);
    CHECK_TRUE(fabs(st.volume - 6000) < 1e-6);
    std::string usd = ReadAll(path);
    CHECK_TRUE(usd.rfind("#usda 1.0", 0) == 0);
    CHECK_TRUE(usd.find("subdivisionScheme = \"catmullClark\"") != std::string::npos);
    CHECK_TRUE(usd.find("creaseIndices") != std::string::npos);
    remove(path.raw.c_str());
}

TEST_CASE(a_cylinder_is_all_quads) {
    CHECK_LOAD("sketch.slvs");
    for(Request &r : SK.request) {
        if(r.type == Request::Type::LINE_SEGMENT) r.construction = true;
    }
    hRequest c = SS.GW.AddRequest(Request::Type::CIRCLE, /*rememberForUndo=*/false);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    SK.GetEntity(c.entity(1))->PointForceTo({ 0, 0, 0 });
    SK.GetEntity(c.entity(64))->DistanceForceTo(5);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Group::MenuGroup(Command::GROUP_EXTRUDE);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Group *g = Last();
    SK.GetParam(g->h.param(0))->val = 0;
    SK.GetParam(g->h.param(1))->val = 0;
    SK.GetParam(g->h.param(2))->val = 10; // 20 high
    SS.SetKernel(KernelKind::OCCT);
    Platform::Path path = helper->GetAssetPath(__FILE__, "cylinder.usda", "out");
    QuadMeshStats  st;
    CHECK_TRUE(KernelExportQuadUsd(Last()->runningShape, path, /*target=*/4, &st));
    CHECK_TRUE(st.quads > 0);
    // SolveSpace's circle is four arcs: the wall is four quad grids, and each cap, four-sided
    // too, a quad disc; each rim is four creases.
    CHECK_TRUE(st.ngons == 0 && st.triangles == 0);
    CHECK_TRUE(st.creases == 8);
    CHECK_TRUE(st.openEdges == 0);
    // An inscribed polygon prism: a little less than the cylinder.
    CHECK_TRUE(st.volume < PI * 25 * 20 && st.volume > 0.9 * PI * 25 * 20);
    remove(path.raw.c_str());
}

TEST_CASE(a_fillet_is_a_smooth_quad_strip) {
    CHECK_LOAD("box.slvs");
    SS.SetKernel(KernelKind::OCCT);
    Group *box = Last();
    SS.GW.ClearSelection();
    GraphicsWindow::Selection sel = {};
    sel.entity = box->Remap(hRequest{ 5 }.entity(0), Group::REMAP_TOP);
    SS.GW.selection.Add(&sel);
    Group::MenuGroup(Command::GROUP_FILLET);
    SS.SetFeatureSize(SS.GW.activeGroup, "3");
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Platform::Path path = helper->GetAssetPath(__FILE__, "fillet.usda", "out");
    QuadMeshStats  st;
    CHECK_TRUE(KernelExportQuadUsd(Last()->runningShape, path, /*target=*/2, &st));
    CHECK_TRUE(st.openEdges == 0);
    // The two end faces have five sides now (a corner rounded): n-gons; the rest quads.
    CHECK_TRUE(st.ngons == 2 && st.triangles == 0);
    // The rounded edge is gone and its tangent edges are smooth; its two end arcs are sharp.
    CHECK_TRUE(st.creases == 11 + 2);
    // The round as a polygon: a little more is cut off than the true fillet.
    CHECK_TRUE(fabs(st.volume - (6000 - (1 - PI / 4) * 9 * 10)) < 5);
    remove(path.raw.c_str());
}

TEST_CASE(a_face_with_one_hole_is_rings_of_quads) {
    // The shelled box: its rim is a frame round the opening; and with no face left as an
    // n-gon, the mesh subdivides cleanly.
    CHECK_LOAD("box.slvs");
    SS.SetKernel(KernelKind::OCCT);
    SS.GW.ClearSelection();
    GraphicsWindow::Selection sel = {};
    sel.entity = Last()->Remap(Entity::NO_ENTITY, Group::REMAP_TOP);
    SS.GW.selection.Add(&sel);
    Group::MenuGroup(Command::GROUP_SHELL);
    SS.SetFeatureSize(SS.GW.activeGroup, "1.5");
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Platform::Path path = helper->GetAssetPath(__FILE__, "shell.usda", "out");
    QuadMeshStats  st;
    CHECK_TRUE(KernelExportQuadUsd(Last()->runningShape, path, /*target=*/2, &st));
    CHECK_TRUE(st.ngons == 0 && st.triangles == 0);
    CHECK_TRUE(st.openEdges == 0);
    CHECK_TRUE(fabs(st.volume - (6000 - 7 * 17 * 28.5)) < 1e-6);
    remove(path.raw.c_str());
}
