// The fit table and fit joints (AGENTS.md section 7, M5b): named clearances, tuned per printer,
// that fit joints take their gaps from. box.slvs: a 10 x 20 x 30 box (part 1).
#include "solvespace.h"

#include "harness.h"

#include "../features.h"

namespace {

std::string FileText(const Platform::Path &path) {
    std::string text;
    FILE *f = OpenFile(path, "rb");
    if(f == nullptr) return text;
    char buf[4096];
    size_t n;
    while((n = fread(buf, 1, sizeof(buf), f)) > 0) text.append(buf, n);
    fclose(f);
    return text;
}

}

TEST_CASE(a_new_document_has_the_default_fits) {
    CHECK_LOAD("box.slvs");
    std::vector<Fit> fits = SS.FitTable();
    CHECK_TRUE(fits.size() == 3);
    CHECK_EQ_STR(fits[0].name, "press");
    CHECK_EQ_EPS(fits[0].clearance, -0.1);
    CHECK_EQ_STR(fits[1].name, "slide");
    CHECK_EQ_EPS(fits[1].clearance, 0.2);
    CHECK_EQ_STR(fits[2].name, "loose");
    CHECK_EQ_EPS(fits[2].clearance, 0.4);
    CHECK_TRUE(SS.FindFit(fits[1].h) != nullptr);
    // A file that leaves them alone saves none of them.
    Platform::Path path = helper->GetAssetPath(__FILE__, "fits.slvs", "out");
    CHECK_TRUE(SS.SaveToFile(path));
    CHECK_TRUE(FileText(path).find("AddFit") == std::string::npos);
    remove(path.raw.c_str());
}

TEST_CASE(fits_are_edited_saved_and_undone) {
    CHECK_LOAD("box.slvs");
    hFit slide = SS.FitTable()[1].h;
    SS.SetFitClearance(slide, 0.25);
    CHECK_EQ_EPS(SS.FindFit(slide)->clearance, 0.25);
    hFit snug = SS.AddFit();
    CHECK_TRUE(SS.RenameFit(snug, "snug"));
    SS.SetFitClearance(snug, 0.05);
    CHECK_TRUE(SS.FitTable().size() == 4);
    CHECK_FALSE(SS.RenameFit(snug, "press")); // names are unique
    Platform::Path path = helper->GetAssetPath(__FILE__, "fits.slvs", "out");
    CHECK_TRUE(SS.SaveToFile(path));
    CHECK_TRUE(SS.LoadFromFile(path));
    SS.AfterNewFile();
    remove(path.raw.c_str());
    CHECK_TRUE(SS.FitTable().size() == 4);
    CHECK_EQ_EPS(SS.FindFit(slide)->clearance, 0.25);
    CHECK_EQ_STR(SS.FindFit(snug)->name, "snug");
    CHECK_EQ_EPS(SS.FindFit(snug)->clearance, 0.05);
    CHECK_TRUE(SS.DeleteFit(snug));
    CHECK_TRUE(SS.FindFit(snug) == nullptr);
    SS.UndoUndo();
    CHECK_TRUE(SS.FindFit(snug) != nullptr);
}

TEST_CASE(the_last_fit_stays) {
    CHECK_LOAD("box.slvs");
    std::vector<Fit> fits = SS.FitTable();
    CHECK_TRUE(SS.DeleteFit(fits[0].h));
    CHECK_TRUE(SS.DeleteFit(fits[1].h));
    CHECK_FALSE(SS.DeleteFit(fits[2].h));
    CHECK_TRUE(SS.FitTable().size() == 1);
}

#if defined(HAVE_OCCT)
namespace {

hRequest AddLine(Vector a, Vector b) {
    hRequest hr = SS.GW.AddRequest(Request::Type::LINE_SEGMENT, /*rememberForUndo=*/false);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    SK.GetEntity(hr.entity(1))->PointForceTo(a);
    SK.GetEntity(hr.entity(2))->PointForceTo(b);
    return hr;
}

void Rectangle(Vector a, Vector c) {
    Vector   p[4] = { a, { c.x, a.y, a.z }, c, { a.x, c.y, a.z } };
    hRequest side[4];
    for(int i = 0; i < 4; i++) side[i] = AddLine(p[i], p[(i + 1) % 4]);
    for(int i = 0; i < 4; i++) {
        Constraint::ConstrainCoincident(side[i].entity(2), side[(i + 1) % 4].entity(1));
    }
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
}

hEntity Circle(Vector centre, double r) {
    hRequest c = SS.GW.AddRequest(Request::Type::CIRCLE, /*rememberForUndo=*/false);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    SK.GetEntity(c.entity(1))->PointForceTo(centre);
    SK.GetEntity(c.entity(64))->DistanceForceTo(r);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    return c.entity(0);
}

double PartVolume(hPart hp) {
    MassProps mp;
    if(!SS.MassProperties(SS.PartSolid(hp), &mp)) return -1;
    return mp.volume;
}

// The box (part 1) and a 10 x 20 x 5 lid on it (part 2), and a sketch on the box's top with
// a circle of radius 2 at (5,10): the shared profile. The fit joint made from it, the box
// male; its male and female groups.
struct Joined {
    hPart  lid;
    hGroup male, female;
};

Joined BoxAndLid() {
    SS.SetKernel(KernelKind::OCCT);
    SS.GW.projRight = Vector::From(1, 0, 0);
    SS.GW.projUp    = Vector::From(0, 1, 0);
    Joined j;
    Select({ Cap(Group::REMAP_TOP) });
    j.lid = SS.NewPart();
    Rectangle({ 0, 0, 30 }, { 10, 20, 30 });
    Group::MenuGroup(Command::GROUP_EXTRUDE);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Group *e = SK.GetGroup(SS.GW.activeGroup);
    SK.GetParam(e->h.param(0))->val = 0;
    SK.GetParam(e->h.param(1))->val = 0;
    SK.GetParam(e->h.param(2))->val = 2.5; // 5 up
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    hEntity lidTop = SK.GetGroup(SS.GW.activeGroup)->Remap(Entity::NO_ENTITY, Group::REMAP_TOP);
    Select({ Cap(Group::REMAP_TOP) });
    Group::MenuGroup(Command::GROUP_WRKPL);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Circle({ 5, 10, 30 }, 2);
    Select({ Cap(Group::REMAP_TOP), lidTop }); // the male first
    int before = SK.groupOrder.n;
    Group::MenuGroup(Command::GROUP_FIT);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    if(SK.groupOrder.n != before + 2) return j;
    for(Group &g : SK.group) {
        if(g.type != Group::Type::FIT) continue;
        (g.fit.female ? j.female : j.male) = g.h;
    }
    return j;
}

void SetDepth(hGroup hg, double depth) {
    Group::FitSpec spec = SK.GetGroup(hg)->fit;
    spec.depth          = depth;
    SS.SetFitJoint(hg, spec);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
}

}

// The male part gets the profile as a peg; the female a pocket wider by the clearance all
// round, and deeper by it.
TEST_CASE(a_fit_joint_makes_a_peg_and_a_pocket_with_the_clearance) {
    CHECK_LOAD("box.slvs");
    Joined j = BoxAndLid();
    CHECK_TRUE(j.male.v && j.female.v);
    Group *male = SK.GetGroup(j.male), *female = SK.GetGroup(j.female);
    CHECK_TRUE(male->PartOf() == Part::FIRST && female->PartOf() == j.lid);
    CHECK_TRUE(male->fit.partner == j.female && female->fit.partner == j.male);
    CHECK_TRUE(male->fit.kind == SS.FitTable()[1].h); // slide, 0.2
    SetDepth(j.male, 3);
    CHECK_EQ_EPS(SK.GetGroup(j.female)->fit.depth, 3);
    CHECK_TRUE(fabs(PartVolume(Part::FIRST) - (6000 + PI * 4 * 3)) < 1e-6);
    // The pocket's bottom edge is rounded by the clearance (a little less cut).
    double pocket = PI * 2.2 * 2.2 * 3.2;
    CHECK_TRUE(fabs(PartVolume(j.lid) - (1000 - pocket)) < 0.15);
    // A press fit: the pocket narrower, as deep as the peg.
    Group::FitSpec spec = SK.GetGroup(j.male)->fit;
    spec.kind           = SS.FitTable()[0].h;
    SS.SetFitJoint(j.male, spec);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_TRUE(SK.GetGroup(j.female)->fit.kind == SS.FitTable()[0].h);
    CHECK_TRUE(fabs(PartVolume(j.lid) - (1000 - PI * 1.9 * 1.9 * 3)) < 1e-3);
    // Retuning the table refits it.
    SS.SetFitClearance(SS.FitTable()[0].h, -0.2);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_TRUE(fabs(PartVolume(j.lid) - (1000 - PI * 1.8 * 1.8 * 3)) < 1e-3);
    // The fit in use can't be deleted.
    CHECK_FALSE(SS.DeleteFit(SS.FitTable()[0].h));
}

// A dowel joint: a pocket in each part, half the pin's length each side, for a separate pin.
TEST_CASE(a_dowel_joint_pockets_both_parts) {
    CHECK_LOAD("box.slvs");
    Joined j = BoxAndLid();
    Group::FitSpec spec = SK.GetGroup(j.male)->fit;
    spec.dowel          = 1;
    spec.depth          = 6;
    SS.SetFitJoint(j.female, spec);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    double pocket = PI * 2.2 * 2.2 * 3.2;
    CHECK_TRUE(fabs(PartVolume(Part::FIRST) - (6000 - pocket)) < 0.15);
    CHECK_TRUE(fabs(PartVolume(j.lid) - (1000 - pocket)) < 0.15);
    // Saved and loaded.
    Platform::Path path = helper->GetAssetPath(__FILE__, "dowel.slvs", "out");
    CHECK_TRUE(SS.SaveToFile(path));
    CHECK_TRUE(SS.LoadFromFile(path));
    SS.AfterNewFile();
    remove(path.raw.c_str());
    SS.SetKernel(KernelKind::OCCT);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_TRUE(SK.GetGroup(j.male)->fit.dowel && SK.GetGroup(j.male)->fit.partner == j.female);
    CHECK_TRUE(fabs(PartVolume(j.lid) - (1000 - pocket)) < 0.15);
    // Deleting one group of the pair deletes both.
    SS.GW.activeGroup = SK.groupOrder[0];
    SS.GW.DeleteGroup(j.male);
    CHECK_TRUE(SK.group.FindByIdNoOops(j.female) == nullptr);
}

// The male first, the female second: one pick on each of two parts.
TEST_CASE(a_fit_joint_needs_a_closed_profile_and_two_parts) {
    CHECK_LOAD("box.slvs");
    SS.SetKernel(KernelKind::OCCT);
    Select({ Cap(Group::REMAP_TOP) });
    Group::MenuGroup(Command::GROUP_WRKPL);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Circle({ 5, 10, 30 }, 2);
    int before = SK.groupOrder.n;
    Select({ Cap(Group::REMAP_TOP), Cap(Group::REMAP_BOTTOM) }); // one part
    Group::MenuGroup(Command::GROUP_FIT);
    CHECK_TRUE(SK.groupOrder.n == before);
}
#endif
