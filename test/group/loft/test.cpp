// Loft groups (AGENTS.md section 6, M3e): a solid through two or more closed profiles, each the
// outer loop of a sketch, picked by one of its curves, in order. sketch.slvs: a workplane
// sketch in XY (group 2) with one line, made construction here.
#include "solvespace.h"

#include "harness.h"

#include "../features.h"

namespace {

hRequest AddLine(Vector a, Vector b) {
    hRequest hr = SS.GW.AddRequest(Request::Type::LINE_SEGMENT, /*rememberForUndo=*/false);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    SK.GetEntity(hr.entity(1))->PointForceTo(a);
    SK.GetEntity(hr.entity(2))->PointForceTo(b);
    return hr;
}

// A closed polygon in the active group; its first side.
hEntity Polygon(std::vector<Vector> pts) {
    std::vector<hRequest> sides;
    for(size_t i = 0; i < pts.size(); i++) sides.push_back(AddLine(pts[i], pts[(i + 1) % pts.size()]));
    for(size_t i = 0; i < sides.size(); i++) {
        Constraint::ConstrainCoincident(sides[i].entity(2), sides[(i + 1) % sides.size()].entity(1));
    }
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    return sides[0].entity(0);
}

// A square of side s centred on the Z axis at height z.
std::vector<Vector> Square(double s, double z) {
    double h = s / 2;
    return { { -h, -h, z }, { h, -h, z }, { h, h, z }, { -h, h, z } };
}

// The fixture's line out of the way; then a square in a new 3D sketch.
void Begin() {
    for(Request &r : SK.request) {
        if(r.type == Request::Type::LINE_SEGMENT) r.construction = true;
    }
    SS.kernelKind = KernelKind::OCCT;
}

hEntity SquareIn3D(double s, double z) {
    Group::MenuGroup(Command::GROUP_3D);
    return Polygon(Square(s, z));
}

// A square of side s with a square hole of side h, centred on the Z axis at height z.
hEntity SquareWithHoleIn3D(double s, double h, double z) {
    Group::MenuGroup(Command::GROUP_3D);
    hEntity outer = Polygon(Square(s, z));
    Polygon(Square(h, z));
    return outer;
}

double ExactVolume() {
    MassProps mp;
    SS.MassProperties(&mp);
    return mp.volume;
}

}

TEST_CASE(loft_needs_two_profiles) {
    CHECK_LOAD("sketch.slvs");
    Begin();
    hEntity a = SquareIn3D(10, 0);
    CHECK_TRUE(MakeFeature(Command::GROUP_LOFT, { a }) == nullptr);
    CHECK_TRUE(MakeFeature(Command::GROUP_LOFT, {}) == nullptr);
}

TEST_CASE(loft_with_the_solvespace_kernel_is_refused) {
    CHECK_LOAD("sketch.slvs");
    Begin();
    hEntity a = SquareIn3D(10, 0), b = SquareIn3D(10, 20);
    SS.kernelKind = KernelKind::SOLVESPACE;
    CHECK_TRUE(MakeFeature(Command::GROUP_LOFT, { a, b }) == nullptr);
}

#if defined(HAVE_OCCT)
TEST_CASE(loft_between_equal_squares_is_a_prism) {
    CHECK_LOAD("sketch.slvs");
    Begin();
    hEntity a = SquareIn3D(10, 0), b = SquareIn3D(10, 20);
    Group  *g = MakeFeature(Command::GROUP_LOFT, { a, b });
    CHECK_TRUE(g != nullptr && g->type == Group::Type::LOFT && g->profiles.size() == 2);
    CHECK_TRUE(g->featureProblem.empty());
    CHECK_TRUE(fabs(ExactVolume() - 2000) < 1e-6);
}

TEST_CASE(loft_from_a_big_square_to_a_small_one_is_a_frustum) {
    CHECK_LOAD("sketch.slvs");
    Begin();
    hEntity a = SquareIn3D(10, 0), b = SquareIn3D(4, 20);
    MakeFeature(Command::GROUP_LOFT, { a, b });
    CHECK_TRUE(fabs(ExactVolume() - 20.0 / 3 * (100 + 16 + 40)) < 1e-6);
}

TEST_CASE(loft_from_a_circle_to_a_square) {
    CHECK_LOAD("sketch.slvs");
    Begin();
    // A circle of radius 5 in the XY sketch.
    hRequest c = SS.GW.AddRequest(Request::Type::CIRCLE, /*rememberForUndo=*/false);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    SK.GetEntity(c.entity(1))->PointForceTo({ 0, 0, 0 });
    SK.GetEntity(c.entity(64))->DistanceForceTo(5);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    hEntity b = SquareIn3D(10, 20);
    Group  *g = MakeFeature(Command::GROUP_LOFT, { c.entity(0), b });
    CHECK_TRUE(g != nullptr && g->featureProblem.empty());
    double v = ExactVolume();
    CHECK_TRUE(v > PI * 25 * 20 && v < 100 * 20);
}

TEST_CASE(loft_through_three_profiles) {
    CHECK_LOAD("sketch.slvs");
    Begin();
    hEntity a = SquareIn3D(10, 0), b = SquareIn3D(6, 10), c = SquareIn3D(10, 20);
    Group  *g = MakeFeature(Command::GROUP_LOFT, { a, b, c });
    CHECK_TRUE(g != nullptr && g->profiles.size() == 3 && g->featureProblem.empty());
    double v = ExactVolume();
    CHECK_TRUE(v > 36 * 20 && v < 100 * 20);
}

TEST_CASE(a_loft_declares_its_end_faces) {
    CHECK_LOAD("sketch.slvs");
    Begin();
    hEntity a = SquareIn3D(10, 0), b = SquareIn3D(4, 20);
    Group  *g = MakeFeature(Command::GROUP_LOFT, { a, b });
    Entity *start = SK.GetEntity(g->Remap(a, Group::REMAP_LOFT_START));
    Entity *end   = SK.GetEntity(g->Remap(b, Group::REMAP_LOFT_END));
    CHECK_TRUE(start->IsFace() && end->IsFace());
    CHECK_EQ_EPS(start->FaceGetPointNum().z, 0);
    CHECK_TRUE(start->FaceGetNormalNum().WithMagnitude(1).Equals({ 0, 0, -1 }));
    CHECK_EQ_EPS(end->FaceGetPointNum().z, 20);
    CHECK_TRUE(end->FaceGetNormalNum().WithMagnitude(1).Equals({ 0, 0, 1 }));
    std::set<uint32_t> names = FaceNames(g);
    CHECK_TRUE(names.count(g->Remap(a, Group::REMAP_LOFT_START).v) == 1);
    CHECK_TRUE(names.count(g->Remap(b, Group::REMAP_LOFT_END).v) == 1);
}

TEST_CASE(the_loft_survives_save_and_reload) {
    CHECK_LOAD("sketch.slvs");
    Begin();
    hEntity a = SquareIn3D(10, 0), b = SquareIn3D(4, 20);
    hGroup  hg = MakeFeature(Command::GROUP_LOFT, { a, b })->h;
    Platform::Path path = helper->GetAssetPath(__FILE__, "loft.slvs", "out");
    CHECK_TRUE(SS.SaveToFile(path));
    CHECK_TRUE(SS.LoadFromFile(path));
    SS.AfterNewFile();
    SS.kernelKind = KernelKind::OCCT;
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Group *g = SK.GetGroup(hg);
    CHECK_TRUE(g->profiles.size() == 2 && g->profiles[0] == a && g->profiles[1] == b);
    CHECK_TRUE(fabs(ExactVolume() - 1040) < 1e-6);
    remove(path.raw.c_str());
}
// #10: holes inside the profiles are lofted too, each to the matching hole of the next profile.
TEST_CASE(loft_between_squares_with_holes_is_a_hollow_prism) {
    CHECK_LOAD("sketch.slvs");
    Begin();
    hEntity a = SquareWithHoleIn3D(10, 4, 0), b = SquareWithHoleIn3D(10, 4, 20);
    Group  *g = MakeFeature(Command::GROUP_LOFT, { a, b });
    CHECK_TRUE(g != nullptr && g->featureProblem.empty());
    CHECK_TRUE(fabs(ExactVolume() - (100 - 16) * 20) < 1e-6);
}

// Profiles with different numbers of holes: the holes are left out, with a note.
TEST_CASE(loft_with_unmatched_holes_leaves_them_out) {
    CHECK_LOAD("sketch.slvs");
    Begin();
    hEntity a = SquareWithHoleIn3D(10, 4, 0), b = SquareIn3D(10, 20);
    Group  *g = MakeFeature(Command::GROUP_LOFT, { a, b });
    CHECK_TRUE(g != nullptr && !g->featureProblem.empty());
    CHECK_TRUE(fabs(ExactVolume() - 2000) < 1e-6);
}
#endif
