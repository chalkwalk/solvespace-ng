// STEP import (AGENTS.md section 6, M3f): a linked group can take a STEP file, whose B-rep
// becomes the group's solid, placed by the link's position and rotation, combined as a B-rep.
#include "solvespace.h"

#include "harness.h"

#if defined(HAVE_OCCT)
namespace {

// box.slvs (10 x 20 x 30 from the origin) written as STEP.
Platform::Path BoxStep(Test::Helper *helper) {
    Platform::Path path = helper->GetAssetPath(__FILE__, "box.step", "out");
    SS.LoadFromFile(helper->GetAssetPath(__FILE__, "box.slvs"));
    SS.AfterNewFile();
    SS.SetKernel(KernelKind::OCCT);
    Group *last = SK.GetGroup(SK.groupOrder[SK.groupOrder.n - 1]);
    KernelExportStep(last->runningShape, path, 1);
    return path;
}

Group *LinkInto(Test::Helper *helper, const Platform::Path &step) {
    SS.LoadFromFile(helper->GetAssetPath(__FILE__, "sketch.slvs"));
    SS.AfterNewFile();
    SS.kernelKind = KernelKind::OCCT;
    Group::MenuGroup(Command::GROUP_LINK, step);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    return SK.GetGroup(SS.GW.activeGroup);
}

}

TEST_CASE(a_step_link_brings_in_the_exact_solid) {
    Platform::Path step = BoxStep(helper);
    Group *g = LinkInto(helper, step);
    CHECK_TRUE(g->type == Group::Type::LINKED);
    CHECK_TRUE(g->impShape != nullptr);
    CHECK_FALSE(g->IsForcedToMesh());
    CHECK_TRUE(g->runningShape != nullptr);
    MassProps mp;
    CHECK_TRUE(SS.MassProperties(&mp));
    CHECK_TRUE(mp.exact);
    CHECK_TRUE(fabs(mp.volume - 6000) < 1e-6);
    remove(step.raw.c_str());
}

TEST_CASE(a_step_link_is_placed_by_the_link) {
    Platform::Path step = BoxStep(helper);
    Group *g = LinkInto(helper, step);
    SK.GetParam(g->h.param(0))->val = 100;
    SK.GetParam(g->h.param(1))->val = 0;
    SK.GetParam(g->h.param(2))->val = 0;
    SS.MarkGroupDirty(g->h);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    MassProps mp;
    CHECK_TRUE(SS.MassProperties(&mp));
    CHECK_TRUE(mp.centre.Equals({ 105, 10, 15 }, 1e-6));
    remove(step.raw.c_str());
}

TEST_CASE(a_step_link_with_the_solvespace_kernel_is_a_mesh) {
    Platform::Path step = BoxStep(helper);
    Group *g = LinkInto(helper, step);
    SS.SetKernel(KernelKind::SOLVESPACE);
    MassProps mp;
    CHECK_TRUE(SS.MassProperties(&mp));
    CHECK_FALSE(mp.exact);
    CHECK_TRUE(fabs(mp.volume - 6000) < 1);
    CHECK_FALSE(g->impMesh.IsEmpty());
    remove(step.raw.c_str());
}

TEST_CASE(a_bad_step_file_does_not_load) {
    Platform::Path bad = helper->GetAssetPath(__FILE__, "bad.step", "out");
    FILE *f = fopen(bad.raw.c_str(), "w");
    fputs("not a step file\n", f);
    fclose(f);
    std::shared_ptr<const KernelShape> shape;
    SMesh mesh = {};
    CHECK_FALSE(SS.LinkStep(bad, &shape, &mesh));
    CHECK_TRUE(shape == nullptr && mesh.IsEmpty());
    remove(bad.raw.c_str());
}
#endif
