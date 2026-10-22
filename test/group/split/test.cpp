// Split with automatic joints (AGENTS.md section 7, M5b): a part cut by a plane into two
// parts, with dowel pockets at the cut. box.slvs: a 10 x 20 x 30 box (part 1).
#include "solvespace.h"

#include "harness.h"

#include "../features.h"

#if defined(HAVE_OCCT)
namespace {

double PartVolume(hPart hp) {
    MassProps mp;
    if(!SS.MassProperties(SS.PartSolid(hp), &mp)) return -1;
    return mp.volume;
}

// A workplane through p, square to the view (normal right x up).
hEntity PlaneAt(Vector p, Vector right, Vector up) {
    SS.GW.projRight = right;
    SS.GW.projUp    = up;
    SS.GW.ClearSelection();
    Group::MenuGroup(Command::GROUP_3D);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    hRequest hr = SS.GW.AddRequest(Request::Type::DATUM_POINT, /*rememberForUndo=*/false);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    SK.GetEntity(hr.entity(0))->PointForceTo(p);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Select({ hr.entity(0) });
    Group::MenuGroup(Command::GROUP_WRKPL);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    return SS.GW.activeGroup.entity(0);
}

// The split's two groups: the one kept in the part, and the new part's.
struct Halves {
    hGroup kept, cut;
};

Halves Split(hEntity plane) {
    Select({ plane });
    Halves h;
    int    before = SK.groupOrder.n;
    Group::MenuGroup(Command::GROUP_SPLIT);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    if(SK.groupOrder.n != before + 2) return h;
    for(Group &g : SK.group) {
        if(g.type != Group::Type::SPLIT) continue;
        (g.fit.female ? h.cut : h.kept) = g.h;
    }
    return h;
}

void Dowels(hGroup hg, bool on, double diameter) {
    Group::FitSpec spec = SK.GetGroup(hg)->fit;
    spec.dowel          = on;
    SS.SetSplit(hg, spec, diameter);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
}

}

// Across the middle: the part keeps the bottom half, a new part gets the top, and two dowels
// (Ø4, 10 long, pressed) go on the cut face's long axis, a pocket in each half.
TEST_CASE(a_split_cuts_a_part_in_two_with_dowels) {
    CHECK_LOAD("box.slvs");
    SS.SetKernel(KernelKind::OCCT);
    Halves h = Split(PlaneAt({ 0, 0, 15 }, { 1, 0, 0 }, { 0, 1, 0 }));
    CHECK_TRUE(h.kept.v && h.cut.v);
    hPart below = SK.GetGroup(h.kept)->PartOf(), above = SK.GetGroup(h.cut)->PartOf();
    CHECK_TRUE(below == Part::FIRST && above != below);
    CHECK_TRUE(SK.GetGroup(h.kept)->fit.partner == h.cut);
    CHECK_EQ_EPS(SK.GetGroup(h.kept)->valA, 4);
    double pocket = PI * 1.9 * 1.9 * 5;
    CHECK_TRUE(fabs(PartVolume(below) - (3000 - 2 * pocket)) < 1e-3);
    CHECK_TRUE(fabs(PartVolume(above) - (3000 - 2 * pocket)) < 1e-3);
    // The halves are where they were: the top one above z = 15.
    Group *top = SS.PartSolid(above);
    Vector lo, hi;
    top->RunningMesh().GetBounding(&hi, &lo);
    CHECK_EQ_EPS(lo.z, 15);
    CHECK_EQ_EPS(hi.z, 30);
    // No dowels: plain halves.
    Dowels(h.cut, false, 4);
    CHECK_TRUE(fabs(PartVolume(below) - 3000) < 1e-3);
    CHECK_TRUE(fabs(PartVolume(above) - 3000) < 1e-3);
    SS.UndoUndo();
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_TRUE(fabs(PartVolume(above) - (3000 - 2 * pocket)) < 1e-3);
}

// Down the middle of the 20 x 30 side: a Ø8 dowel needs 8 clear all round; two don't fit
// along the 30, one does at the centre. A Ø12 doesn't fit at all, and Problems says so.
TEST_CASE(a_small_cut_face_gets_fewer_dowels) {
    CHECK_LOAD("box.slvs");
    SS.SetKernel(KernelKind::OCCT);
    Halves h = Split(PlaneAt({ 5, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 }));
    CHECK_TRUE(h.kept.v && h.cut.v);
    hPart a = SK.GetGroup(h.kept)->PartOf(), b = SK.GetGroup(h.cut)->PartOf();
    Dowels(h.kept, true, 8);
    double pocket = PI * 3.9 * 3.9 * 5;
    CHECK_TRUE(fabs(PartVolume(a) - (3000 - pocket)) < 1e-3);
    CHECK_TRUE(fabs(PartVolume(b) - (3000 - pocket)) < 1e-3);
    CHECK_TRUE(SK.GetGroup(h.kept)->featureProblem.empty());
    Dowels(h.kept, true, 12);
    CHECK_TRUE(fabs(PartVolume(a) - 3000) < 1e-3);
    CHECK_FALSE(SK.GetGroup(h.kept)->featureProblem.empty());
}

TEST_CASE(a_split_survives_save_and_goes_whole) {
    CHECK_LOAD("box.slvs");
    SS.SetKernel(KernelKind::OCCT);
    Halves h = Split(PlaneAt({ 0, 0, 15 }, { 1, 0, 0 }, { 0, 1, 0 }));
    hPart  above = SK.GetGroup(h.cut)->PartOf();
    Platform::Path path = helper->GetAssetPath(__FILE__, "split.slvs", "out");
    CHECK_TRUE(SS.SaveToFile(path));
    CHECK_TRUE(SS.LoadFromFile(path));
    SS.AfterNewFile();
    remove(path.raw.c_str());
    SS.SetKernel(KernelKind::OCCT);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    double pocket = PI * 1.9 * 1.9 * 5;
    CHECK_TRUE(fabs(PartVolume(above) - (3000 - 2 * pocket)) < 1e-3);
    SS.GW.DeleteGroup(h.kept);
    CHECK_TRUE(SK.group.FindByIdNoOops(h.cut) == nullptr);
    CHECK_TRUE(fabs(PartVolume(Part::FIRST) - 6000) < 1e-3);
}

// A split needs a plane: a workplane or a flat face.
TEST_CASE(a_split_needs_a_plane) {
    CHECK_LOAD("box.slvs");
    SS.SetKernel(KernelKind::OCCT);
    int before = SK.groupOrder.n;
    Select({ Edge(5, Group::REMAP_TOP) });
    Group::MenuGroup(Command::GROUP_SPLIT);
    CHECK_TRUE(SK.groupOrder.n == before);
}
#endif
