// Draft groups (AGENTS.md section 6, M3c): an extrusion's side faces tapered about its sketch
// plane along its direction; each drafted face is a derived entity, Remap(face,
// REMAP_DRAFT_FACE), through the face's base line with its normal turned by the angle.
#include "solvespace.h"

#include "harness.h"

#include "../features.h"

namespace {

std::vector<hEntity> Sides() {
    return { SideFace(5), SideFace(6), SideFace(7), SideFace(8) };
}

// The box's sides drafted by deg along +Z about z = 0: each side moves in by z tan(deg), a
// prismatoid of height 30.
double Drafted(double deg) {
    double d  = 30 * tan(deg * PI / 180);
    double a1 = 10 * 20, am = (10 - d) * (20 - d), a2 = (10 - 2 * d) * (20 - 2 * d);
    return 30.0 / 6 * (a1 + 4 * am + a2);
}

}

TEST_CASE(draft_takes_only_the_sides_of_an_extrusion) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    CHECK_TRUE(MakeFeature(Command::GROUP_DRAFT, { Cap(Group::REMAP_TOP) }) == nullptr);
    CHECK_TRUE(MakeFeature(Command::GROUP_DRAFT, {}) == nullptr);
}

#if defined(HAVE_OCCT)
TEST_CASE(draft_tapers_the_four_sides) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    Group *g = MakeFeature(Command::GROUP_DRAFT, Sides());
    CHECK_TRUE(g != nullptr && g->type == Group::Type::DRAFT && g->faces.size() == 4);
    Resize(g, "5");
    CHECK_TRUE(g->featureProblem.empty());
    CHECK_TRUE(fabs(Volume(g) - Drafted(5)) < 0.005 * Drafted(5));
}

TEST_CASE(negative_draft_widens) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    Group *g = MakeFeature(Command::GROUP_DRAFT, Sides());
    Resize(g, "-5");
    CHECK_TRUE(fabs(Volume(g) - Drafted(-5)) < 0.005 * Drafted(-5));
}

TEST_CASE(a_drafted_face_is_declared_and_named) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    Group *g = MakeFeature(Command::GROUP_DRAFT, { SideFace(5) }); // the front, y = 0
    Resize(g, "5");
    hEntity drafted = g->Remap(SideFace(5), Group::REMAP_DRAFT_FACE);
    Entity *f       = SK.entity.FindByIdNoOops(drafted);
    CHECK_TRUE(f != nullptr && f->IsFace());
    Vector n = f->FaceGetNormalNum().WithMagnitude(1);
    Vector want = Vector::From(0, -cos(5 * PI / 180), sin(5 * PI / 180));
    CHECK_TRUE(n.Equals(want, 1e-9) || n.Equals(want.ScaledBy(-1), 1e-9));
    CHECK_TRUE(fabs(f->FaceGetPointNum().Dot(want)) < 1e-9); // through the bottom front line
    CHECK_TRUE(FaceNames(g).count(drafted.v) == 1);
}

TEST_CASE(a_point_on_a_drafted_face_follows_a_change_upstream) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    Group *g = MakeFeature(Command::GROUP_DRAFT, { SideFace(5) });
    Resize(g, "5");
    hEntity pt = PointInPlane(g->Remap(SideFace(5), Group::REMAP_DRAFT_FACE));
    MoveFront(-5);
    Vector want = Vector::From(0, -cos(5 * PI / 180), sin(5 * PI / 180));
    CHECK_TRUE(fabs(SK.GetEntity(pt)->PointGetNum().Minus({ 0, -5, 0 }).Dot(want)) < 1e-6);
}
#endif

#if defined(HAVE_OCCT)
// The angle is a solver parameter (its tangent), free, so pulling the drafted face's top edge
// sets it (owner's feedback after M4a); Equal between two drafts makes their angles match.
TEST_CASE(a_drafts_top_edge_sets_its_angle) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    hGroup hg = MakeFeature(Command::GROUP_DRAFT, { SideFace(5) })->h;
    CHECK_TRUE(SK.GetGroup(hg)->solved.dof == 1);
    // The top edge: the front's bottom line raised 30, in by 30 tan(2 degrees).
    Entity *top = SK.GetEntity(SK.GetGroup(hg)->Remap(SideFace(5), Group::REMAP_DRAFT_TOP));
    Vector  a   = SK.GetEntity(top->point[0])->PointGetNum();
    CHECK_EQ_EPS(a.z, 30);
    CHECK_EQ_EPS(a.y, 30 * tan(2 * PI / 180));
    SK.GetEntity(top->point[0])->PointForceTo({ a.x, 3, 30 });
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_EQ_EPS(SK.GetParam(SK.GetGroup(hg)->h.param(0))->val, 0.1);
    MassProps mp;
    CHECK_TRUE(SS.MassProperties(&mp));
    CHECK_TRUE(fabs(mp.volume - (6000 - 0.5 * 30 * 3 * 10)) < 1e-6);
    // Held, it shows in degrees.
    SS.HoldFeatureSize(hg, true);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    for(Constraint &c : SK.constraint) {
        if(c.group == hg && c.type == Constraint::Type::FEATURE_SIZE) {
            CHECK_TRUE(c.other);
            CHECK_TRUE(c.Label().find("°") != std::string::npos);
            CHECK_EQ_EPS(c.valA, atan(0.1) * 180 / PI);
        }
    }
}

TEST_CASE(two_drafts_share_an_angle_when_made_equal) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    hGroup front = MakeFeature(Command::GROUP_DRAFT, { SideFace(5) })->h;
    SS.SetFeatureSize(front, "6");
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    hGroup back = MakeFeature(Command::GROUP_DRAFT, { SideFace(7) })->h;
    SS.SetFeatureSize(back, "3");
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    // Pick a handle of each, and Equal.
    Select({ SK.GetGroup(front)->Remap(SideFace(5), Group::REMAP_DRAFT_TOP),
             SK.GetGroup(back)->Remap(SideFace(7), Group::REMAP_DRAFT_TOP) });
    Constraint::MenuConstrain(Command::EQUAL);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    double a = SK.GetParam(SK.GetGroup(front)->h.param(0))->val;
    double b = SK.GetParam(SK.GetGroup(back)->h.param(0))->val;
    CHECK_EQ_EPS(a, tan(6 * PI / 180));
    CHECK_EQ_EPS(b, a);
    CHECK_TRUE(SK.GetGroup(back)->solved.dof == 0);
}
#endif
