// Hole groups (AGENTS.md section 6, M3d): holes at points sketched on a face, into the solid
// along the sketch's workplane (−normal), plain, counterbored or countersunk, through or to a
// depth, optionally with a modelled ISO metric thread. Derived entities per point: the centre
// on the face, the bottom centre, the axis and the seat plane.
#include "solvespace.h"

#include "harness.h"

#include "../features.h"

namespace {

// A sketch on the box's top face (normal +Z) with points at the given places; then the hole
// group made from them.
std::vector<hEntity> PointsOnTop(std::vector<Vector> at) {
    SS.GW.projRight = Vector::From(1, 0, 0);
    SS.GW.projUp    = Vector::From(0, 1, 0);
    Select({ Extrusion()->Remap(hRequest{ 5 }.entity(1), Group::REMAP_TOP) }); // (0, 0, 30)
    Group::MenuGroup(Command::GROUP_WRKPL);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    std::vector<hEntity> points;
    for(Vector p : at) {
        hRequest hr = SS.GW.AddRequest(Request::Type::DATUM_POINT, /*rememberForUndo=*/false);
        SS.GenerateAll(SolveSpaceUI::Generate::ALL);
        SK.GetEntity(hr.entity(0))->PointForceTo(p);
        points.push_back(hr.entity(0));
    }
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    return points;
}

Group *HoleAt(std::vector<Vector> at) {
    return MakeFeature(Command::GROUP_HOLE, PointsOnTop(at));
}

void Spec(Group *g, Group::HoleSpec spec) {
    SS.SetHoleSpec(g->h, spec);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
}

double ExactVolume() {
    MassProps mp;
    SS.MassProperties(&mp);
    return mp.volume;
}

Vector PointOf(hEntity he) {
    return SK.GetEntity(he)->PointGetNum();
}

}

TEST_CASE(hole_needs_points_in_a_workplane) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    CHECK_TRUE(MakeFeature(Command::GROUP_HOLE, {}) == nullptr);
    // A point of the solid is free in 3D.
    CHECK_TRUE(MakeFeature(Command::GROUP_HOLE,
                           { Extrusion()->Remap(hRequest{ 5 }.entity(1), Group::REMAP_TOP) }) ==
               nullptr);
}

TEST_CASE(hole_with_the_solvespace_kernel_is_refused) {
    CHECK_LOAD("box.slvs");
    CHECK_TRUE(HoleAt({ { 5, 10, 30 } }) == nullptr);
}

#if defined(HAVE_OCCT)
TEST_CASE(a_hole_declares_its_axis_and_seat) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    Group *g = HoleAt({ { 5, 10, 30 } });
    CHECK_TRUE(g != nullptr && g->type == Group::Type::HOLE && g->points.size() == 1);
    Group::HoleSpec spec = g->hole;
    spec.kind            = Group::HoleKind::COUNTERBORE;
    spec.diameter        = 4;
    spec.depth           = 0; // through
    spec.headDiameter    = 8;
    spec.headDepth       = 3;
    Spec(g, spec);
    hEntity p = g->points[0];
    CHECK_TRUE(PointOf(g->Remap(p, Group::REMAP_HOLE_TOP)).Equals({ 5, 10, 30 }));
    CHECK_TRUE(PointOf(g->Remap(p, Group::REMAP_HOLE_BOTTOM)).Equals({ 5, 10, 0 }));
    Entity *axis = SK.GetEntity(g->Remap(p, Group::REMAP_HOLE_AXIS));
    CHECK_TRUE(axis->type == Entity::Type::LINE_SEGMENT);
    Entity *seat = SK.GetEntity(g->Remap(p, Group::REMAP_HOLE_SEAT));
    CHECK_TRUE(seat->IsFace());
    CHECK_EQ_EPS(seat->FaceGetPointNum().z, 27);
    CHECK_TRUE(fabs(fabs(seat->FaceGetNormalNum().WithMagnitude(1).z) - 1) < 1e-9);
    // The point moves; so does the hole.
    SK.GetEntity(p)->PointForceTo({ 3, 4, 30 });
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_TRUE(PointOf(g->Remap(p, Group::REMAP_HOLE_BOTTOM)).Equals({ 3, 4, 0 }));
}

TEST_CASE(the_hole_spec_survives_save_and_reload) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    Group *g = HoleAt({ { 5, 10, 30 }, { 5, 5, 30 } });
    Group::HoleSpec spec = g->hole;
    spec.kind            = Group::HoleKind::COUNTERSINK;
    spec.diameter        = 3.5;
    spec.depth           = 12;
    spec.headDiameter    = 7;
    spec.headAngle       = 82;
    spec.thread          = 4;
    spec.pitch           = 0.7;
    spec.flip            = true;
    Spec(g, spec);
    hGroup hg = g->h;
    Platform::Path path = helper->GetAssetPath(__FILE__, "holes.slvs", "out");
    CHECK_TRUE(SS.SaveToFile(path));
    CHECK_TRUE(SS.LoadFromFile(path));
    SS.AfterNewFile();
    g = SK.GetGroup(hg);
    CHECK_TRUE(g->points.size() == 2);
    CHECK_TRUE(g->hole.kind == Group::HoleKind::COUNTERSINK);
    CHECK_EQ_EPS(g->hole.diameter, 3.5);
    CHECK_EQ_EPS(g->hole.depth, 12);
    CHECK_EQ_EPS(g->hole.headDiameter, 7);
    CHECK_EQ_EPS(g->hole.headAngle, 82);
    CHECK_EQ_EPS(g->hole.thread, 4);
    CHECK_EQ_EPS(g->hole.pitch, 0.7);
    CHECK_TRUE(g->hole.flip);
    remove(path.raw.c_str());
}

TEST_CASE(holes_cut_the_solid) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    Group *g = HoleAt({ { 5, 10, 30 } });
    Group::HoleSpec spec = g->hole;
    spec.diameter        = 4;
    spec.depth           = 0;
    Spec(g, spec);
    CHECK_TRUE(g->featureProblem.empty());
    CHECK_TRUE(fabs(ExactVolume() - (6000 - PI * 4 * 30)) < 1e-6);

    spec.depth = 10; // blind
    Spec(g, spec);
    CHECK_TRUE(fabs(ExactVolume() - (6000 - PI * 4 * 10)) < 1e-6);

    spec.depth        = 0;
    spec.kind         = Group::HoleKind::COUNTERBORE;
    spec.headDiameter = 8;
    spec.headDepth    = 3;
    Spec(g, spec);
    CHECK_TRUE(fabs(ExactVolume() - (6000 - PI * 4 * 30 - PI * (16 - 4) * 3)) < 1e-6);

    spec.kind      = Group::HoleKind::COUNTERSINK;
    spec.headAngle = 90; // from 8 to 4 over 2 deep
    Spec(g, spec);
    double cone = PI * 2 / 3 * (16 + 8 + 4) - PI * 4 * 2;
    CHECK_TRUE(fabs(ExactVolume() - (6000 - PI * 4 * 30 - cone)) < 1e-6);
    // A countersink meets the bore with no flat seat, so no face takes the seat's name.
    CHECK_TRUE(FaceNames(g).count(g->Remap(g->points[0], Group::REMAP_HOLE_SEAT).v) == 0);
}

TEST_CASE(a_counterbore_seat_is_named) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    Group *g = HoleAt({ { 5, 10, 30 } });
    Group::HoleSpec spec = g->hole;
    spec.kind            = Group::HoleKind::COUNTERBORE;
    spec.diameter        = 4;
    spec.headDiameter    = 8;
    spec.headDepth       = 3;
    Spec(g, spec);
    CHECK_TRUE(FaceNames(g).count(g->Remap(g->points[0], Group::REMAP_HOLE_SEAT).v) == 1);
    // In the solid's colour, as the rest of the hole.
    for(const STriangle &t : g->RunningMesh().l) {
        if(t.meta.face == g->Remap(g->points[0], Group::REMAP_HOLE_SEAT).v) {
            CHECK_TRUE(t.meta.color.Equals(g->color));
            break;
        }
    }
}

TEST_CASE(two_points_make_two_holes) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    Group *g = HoleAt({ { 5, 5, 30 }, { 5, 15, 30 } });
    Group::HoleSpec spec = g->hole;
    spec.diameter        = 4;
    Spec(g, spec);
    CHECK_TRUE(fabs(ExactVolume() - (6000 - 2 * PI * 4 * 30)) < 1e-6);
}

TEST_CASE(a_hole_off_the_solid_says_so) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    Group *g = HoleAt({ { 50, 50, 30 } });
    CHECK_TRUE(fabs(ExactVolume() - 6000) < 1e-6);
    CHECK_FALSE(g->featureProblem.empty());
}

TEST_CASE(a_flipped_hole_goes_the_other_way) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    Group *g = HoleAt({ { 5, 10, 30 } });
    Group::HoleSpec spec = g->hole;
    spec.flip            = true; // up, out of the box
    Spec(g, spec);
    CHECK_TRUE(fabs(ExactVolume() - 6000) < 1e-6);
}

TEST_CASE(an_m4_thread_is_modelled) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    Group *g = HoleAt({ { 5, 10, 30 } });
    Group::HoleSpec spec = g->hole;
    spec.thread          = 4;
    spec.pitch           = 0.7;
    spec.depth           = 10;
    Spec(g, spec);
    CHECK_TRUE(g->featureProblem.empty());
    double minor = 4 - 1.0825 * 0.7;
    double removed = 6000 - ExactVolume();
    CHECK_TRUE(removed > PI * minor * minor / 4 * 10 + 1);
    CHECK_TRUE(removed < PI * 4 * 10);
}
TEST_CASE(hole_fields_take_what_is_typed) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    Group *g  = HoleAt({ { 5, 10, 30 } });
    hGroup hg = g->h;
    using F   = SolveSpaceUI::HoleField;
    CHECK_TRUE(SS.SetHoleField(hg, F::THREAD, "M3"));
    CHECK_EQ_EPS(SK.GetGroup(hg)->hole.pitch, 0.5);
    CHECK_EQ_STR(SS.HoleThreadText(SK.GetGroup(hg)->hole), "M3");
    CHECK_TRUE(SS.SetHoleField(hg, F::THREAD, "m4 x 0.5"));
    CHECK_EQ_STR(SS.HoleThreadText(SK.GetGroup(hg)->hole), "M4x0.5");
    CHECK_FALSE(SS.SetHoleField(hg, F::THREAD, "M7")); // no coarse pitch
    CHECK_TRUE(SS.SetHoleField(hg, F::THREAD, "none"));
    CHECK_EQ_STR(SS.HoleThreadText(SK.GetGroup(hg)->hole), "none");
    CHECK_TRUE(SS.SetHoleField(hg, F::DEPTH, "12"));
    CHECK_EQ_EPS(SK.GetGroup(hg)->hole.depth, 12);
    CHECK_TRUE(SS.SetHoleField(hg, F::DEPTH, ""));
    CHECK_EQ_EPS(SK.GetGroup(hg)->hole.depth, 0);
    CHECK_FALSE(SS.SetHoleField(hg, F::DIAMETER, "-1"));
}
// Review 5: a print clearance widens the bore, the head and the thread.
TEST_CASE(a_print_clearance_widens_the_hole) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    Group *g = HoleAt({ { 5, 10, 30 } });
    CHECK_TRUE(SS.SetHoleField(g->h, SolveSpaceUI::HoleField::DIAMETER, "4"));
    CHECK_TRUE(SS.SetHoleField(g->h, SolveSpaceUI::HoleField::CLEARANCE, "0.2"));
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_EQ_EPS(g->hole.clearance, 0.2);
    CHECK_TRUE(fabs(ExactVolume() - (6000 - PI * 2.1 * 2.1 * 30)) < 1e-6);
    CHECK_FALSE(SS.SetHoleField(g->h, SolveSpaceUI::HoleField::CLEARANCE, "-5"));
}
#endif
