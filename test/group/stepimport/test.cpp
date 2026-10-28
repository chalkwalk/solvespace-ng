// STEP import (AGENTS.md section 6, M3f): a linked group can take a STEP file, whose B-rep
// becomes the group's solid, placed by the link's position and rotation, combined as a B-rep.
#include "solvespace.h"

#include <set>

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

// #9: the planar faces of a linked STEP file are face entities of the link, named on its
// solid, so a sketch can go on one, and the sketch follows the link when it moves.
TEST_CASE(a_sketch_goes_on_a_face_of_a_step_link) {
    Platform::Path step = BoxStep(helper);
    Group *g  = LinkInto(helper, step);
    hGroup hg = g->h;
    std::set<uint32_t> names;
    for(const STriangle &t : g->RunningMesh().l) names.insert(t.meta.face);
    hEntity top = {};
    int     faces = 0;
    for(Entity &e : SK.entity) {
        if(e.group != hg || !e.IsFace()) continue;
        faces++;
        CHECK_TRUE(names.count(e.h.v) == 1);
        if(e.FaceGetNormalNum().WithMagnitude(1).Equals({ 0, 0, 1 }) &&
           fabs(e.FaceGetPointNum().z - 30) < 1e-6) {
            top = e.h;
        }
    }
    CHECK_TRUE(faces == 6);
    CHECK_TRUE(top.v != 0);
    SS.GW.ClearSelection();
    GraphicsWindow::Selection sel = {};
    sel.entity = top;
    SS.GW.selection.Add(&sel);
    Group::MenuGroup(Command::GROUP_WRKPL);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Group *sketch = SK.GetGroup(SS.GW.activeGroup);
    CHECK_TRUE(sketch->subtype == Group::Subtype::WORKPLANE_BY_FACE);
    hEntity wp = sketch->h.entity(0);
    CHECK_EQ_EPS(SK.GetEntity(wp)->WorkplaneGetOffset().z, 30);
    // The link raised by 7: the sketch's plane goes with it.
    SK.GetParam(hg.param(2))->val += 7;
    SS.MarkGroupDirty(hg);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_EQ_EPS(SK.GetEntity(wp)->WorkplaneGetOffset().z, 37);
    // Saved and opened again, the sketch is still on that face.
    Platform::Path saved = helper->GetAssetPath(__FILE__, "onface.slvs", "out");
    CHECK_TRUE(SS.SaveToFile(saved));
    CHECK_TRUE(SS.LoadFromFile(saved));
    SS.AfterNewFile();
    SS.kernelKind = KernelKind::OCCT;
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_TRUE(SK.entity.FindByIdNoOops(wp) != nullptr);
    CHECK_EQ_EPS(SK.GetEntity(wp)->WorkplaneGetOffset().z, 37);
    remove(saved.raw.c_str());
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
