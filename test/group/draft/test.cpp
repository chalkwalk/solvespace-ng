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
