// Shell groups (AGENTS.md section 6, M3c): the solid hollowed to a wall thickness, with chosen
// faces left open; each face's inner face is a derived entity, Remap(face, REMAP_SHELL_INNER).
#include "solvespace.h"

#include "harness.h"

#include "../features.h"

namespace {

hEntity Inner(Group *g, hEntity face) {
    return g->Remap(face, Group::REMAP_SHELL_INNER);
}

bool PlaneIs(hEntity face, Vector point, Vector normal) {
    Entity *f = SK.entity.FindByIdNoOops(face);
    if(f == nullptr || !f->IsFace()) return false;
    Vector n = f->FaceGetNormalNum().WithMagnitude(1);
    Vector p = f->FaceGetPointNum();
    return fabs(fabs(n.Dot(normal)) - 1) < 1e-9 && fabs(p.Minus(point).Dot(normal)) < 1e-6;
}

}

TEST_CASE(shell_needs_faces_of_the_solid) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    CHECK_TRUE(MakeFeature(Command::GROUP_SHELL, { hRequest{ 5 }.entity(0) }) == nullptr);
}

TEST_CASE(shell_with_the_solvespace_kernel_is_refused) {
    CHECK_LOAD("box.slvs");
    CHECK_TRUE(MakeFeature(Command::GROUP_SHELL, { Cap(Group::REMAP_TOP) }) == nullptr);
}

#if defined(HAVE_OCCT)
TEST_CASE(shell_opens_the_top_and_hollows_the_box) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    Group *g = MakeFeature(Command::GROUP_SHELL, { Cap(Group::REMAP_TOP) });
    CHECK_TRUE(g != nullptr && g->type == Group::Type::SHELL);
    CHECK_TRUE(g->faces.size() == 1);
    Resize(g, "1");
    CHECK_TRUE(g->featureProblem.empty());
    CHECK_TRUE(fabs(Volume(g) - (6000 - 8 * 18 * 29)) < 0.01);
}

TEST_CASE(a_closed_shell_keeps_the_outside) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    Group *g = MakeFeature(Command::GROUP_SHELL, {});
    CHECK_TRUE(g != nullptr && g->faces.empty());
    Resize(g, "1");
    CHECK_TRUE(fabs(Volume(g) - (6000 - 8 * 18 * 28)) < 0.01);
}

TEST_CASE(a_shell_too_thick_leaves_the_solid_and_says_so) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    Group *g = MakeFeature(Command::GROUP_SHELL, { Cap(Group::REMAP_TOP) });
    Resize(g, "6");
    CHECK_EQ_EPS(Volume(g), 6000);
    CHECK_FALSE(g->featureProblem.empty());
}

TEST_CASE(the_shell_declares_its_inner_faces) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    Group *g = MakeFeature(Command::GROUP_SHELL, { Cap(Group::REMAP_TOP) });
    Resize(g, "1");
    CHECK_TRUE(PlaneIs(Inner(g, SideFace(5)), { 0, 1, 0 }, { 0, 1, 0 }));   // front, y = 0
    CHECK_TRUE(PlaneIs(Inner(g, SideFace(6)), { 9, 0, 0 }, { 1, 0, 0 }));   // right, x = 10
    CHECK_TRUE(PlaneIs(Inner(g, Cap(Group::REMAP_BOTTOM)), { 0, 0, 1 }, { 0, 0, 1 }));
}

TEST_CASE(inner_faces_carry_their_names) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    Group *g = MakeFeature(Command::GROUP_SHELL, { Cap(Group::REMAP_TOP) });
    Resize(g, "1");
    std::set<uint32_t> names = FaceNames(g);
    CHECK_TRUE(names.count(Inner(g, Cap(Group::REMAP_BOTTOM)).v) == 1);
    CHECK_TRUE(names.count(Inner(g, SideFace(5)).v) == 1);
    CHECK_TRUE(names.count(Cap(Group::REMAP_BOTTOM).v) == 1);
    // The opened top leaves the walls' rims, still in its plane.
    for(const STriangle &t : g->runningMesh.l) {
        if(t.meta.face == Cap(Group::REMAP_TOP).v) CHECK_EQ_EPS(t.a.z, 30);
    }
}

TEST_CASE(a_point_on_an_inner_face_follows_a_change_upstream) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    Group *g = MakeFeature(Command::GROUP_SHELL, { Cap(Group::REMAP_TOP) });
    Resize(g, "1");
    hEntity pt = PointInPlane(Inner(g, SideFace(5)));
    CHECK_EQ_EPS(SK.GetEntity(pt)->PointGetNum().y, 1);
    MoveFront(-5);
    CHECK_EQ_EPS(SK.GetEntity(pt)->PointGetNum().y, -4);
}
// Review 5: the wall can go inside (the default), outside, or both sides of the faces.
TEST_CASE(a_shell_outside_wraps_the_solid) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    Group *g = MakeFeature(Command::GROUP_SHELL, { Cap(Group::REMAP_TOP) });
    Resize(g, "1");
    SS.SetShellSide(g->h, Group::ShellSide::OUTSIDE);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_TRUE(g->featureProblem.empty());
    CHECK_TRUE(fabs(Volume(g) - (12 * 22 * 31 - 6000)) < 0.01);
    CHECK_TRUE(PlaneIs(g->Remap(SideFace(5), Group::REMAP_SHELL_OUTER), { 0, -1, 0 }, { 0, 1, 0 }));
    CHECK_TRUE(PlaneIs(Inner(g, SideFace(5)), { 0, 0, 0 }, { 0, 1, 0 }));
    CHECK_TRUE(FaceNames(g).count(g->Remap(SideFace(5), Group::REMAP_SHELL_OUTER).v) == 1);
}

TEST_CASE(a_shell_on_both_sides_straddles_the_faces) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    Group *g = MakeFeature(Command::GROUP_SHELL, { Cap(Group::REMAP_TOP) });
    Resize(g, "1");
    SS.SetShellSide(g->h, Group::ShellSide::BOTH);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_TRUE(g->featureProblem.empty());
    // Half a wall out (the rim stays at z = 30), half in.
    CHECK_TRUE(fabs(Volume(g) - (11 * 21 * 30.5 - 9 * 19 * 29.5)) < 0.01);
    CHECK_TRUE(PlaneIs(g->Remap(SideFace(5), Group::REMAP_SHELL_OUTER), { 0, -0.5, 0 }, { 0, 1, 0 }));
    CHECK_TRUE(PlaneIs(Inner(g, SideFace(5)), { 0, 0.5, 0 }, { 0, 1, 0 }));
}

TEST_CASE(a_closed_shell_outside_keeps_the_solid_as_its_void) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    Group *g = MakeFeature(Command::GROUP_SHELL, {});
    Resize(g, "1");
    SS.SetShellSide(g->h, Group::ShellSide::OUTSIDE);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_TRUE(fabs(Volume(g) - (12 * 22 * 32 - 6000)) < 0.01);
}

TEST_CASE(the_shell_side_survives_save_and_reload) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    Group *g = MakeFeature(Command::GROUP_SHELL, {});
    SS.SetShellSide(g->h, Group::ShellSide::BOTH);
    hGroup hg = g->h;
    Platform::Path path = helper->GetAssetPath(__FILE__, "both.slvs", "out");
    CHECK_TRUE(SS.SaveToFile(path));
    CHECK_TRUE(SS.LoadFromFile(path));
    SS.AfterNewFile();
    CHECK_TRUE(SK.GetGroup(hg)->shellSide == Group::ShellSide::BOTH);
    remove(path.raw.c_str());
}
#endif
