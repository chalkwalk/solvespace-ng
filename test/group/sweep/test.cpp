// Sweep groups (AGENTS.md section 6, M3e): a closed profile swept along a path of connected
// curves. Picked as a curve of the profile sketch, then the path's curves. sketch.slvs: a
// workplane sketch in XY (group 2) with one line, made construction here.
#include "solvespace.h"

#include "harness.h"

#include "../features.h"

namespace {

// A circle of radius r at the origin in the XY sketch.
hEntity Circle(double r) {
    for(Request &req : SK.request) {
        if(req.type == Request::Type::LINE_SEGMENT) req.construction = true;
    }
    SS.kernelKind = KernelKind::OCCT;
    hRequest c = SS.GW.AddRequest(Request::Type::CIRCLE, /*rememberForUndo=*/false);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    SK.GetEntity(c.entity(1))->PointForceTo({ 0, 0, 0 });
    SK.GetEntity(c.entity(64))->DistanceForceTo(r);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    return c.entity(0);
}

hEntity Line3D(Vector a, Vector b) {
    hRequest hr = SS.GW.AddRequest(Request::Type::LINE_SEGMENT, /*rememberForUndo=*/false);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    SK.GetEntity(hr.entity(1))->PointForceTo(a);
    SK.GetEntity(hr.entity(2))->PointForceTo(b);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    return hr.entity(0);
}

// A quarter arc in the ZX plane from the origin up to (10, 0, 10), about (10, 0, 0).
hEntity QuarterArc() {
    Select({ Request::HREQUEST_REFERENCE_ZX.entity(0) });
    Group::MenuGroup(Command::GROUP_WRKPL);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    hRequest hr = SS.GW.AddRequest(Request::Type::ARC_OF_CIRCLE, /*rememberForUndo=*/false);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    SK.GetEntity(hr.entity(1))->PointForceTo({ 10, 0, 0 });
    SK.GetEntity(hr.entity(2))->PointForceTo({ 0, 0, 0 });
    SK.GetEntity(hr.entity(3))->PointForceTo({ 10, 0, 10 });
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    return hr.entity(0);
}

double ExactVolume() {
    MassProps mp;
    SS.MassProperties(&mp);
    return mp.volume;
}

}

TEST_CASE(sweep_needs_a_profile_and_a_connected_path) {
    CHECK_LOAD("sketch.slvs");
    hEntity c = Circle(1);
    Group::MenuGroup(Command::GROUP_3D);
    hEntity a = Line3D({ 0, 0, 0 }, { 0, 0, 10 }), b = Line3D({ 0, 5, 20 }, { 0, 5, 30 });
    CHECK_TRUE(MakeFeature(Command::GROUP_SWEEP, { c }) == nullptr);
    CHECK_TRUE(MakeFeature(Command::GROUP_SWEEP, { c, a, b }) == nullptr); // not connected
    CHECK_TRUE(MakeFeature(Command::GROUP_SWEEP, { a, c }) == nullptr);    // no profile first
}

#if defined(HAVE_OCCT)
TEST_CASE(a_circle_swept_along_a_line_is_a_cylinder) {
    CHECK_LOAD("sketch.slvs");
    hEntity c = Circle(1);
    Group::MenuGroup(Command::GROUP_3D);
    hEntity path = Line3D({ 0, 0, 0 }, { 0, 0, 10 });
    Group  *g    = MakeFeature(Command::GROUP_SWEEP, { c, path });
    CHECK_TRUE(g != nullptr && g->type == Group::Type::SWEEP && g->profiles.size() == 2);
    CHECK_TRUE(g->featureProblem.empty());
    CHECK_TRUE(fabs(ExactVolume() - PI * 10) < 1e-6);
}

TEST_CASE(a_circle_swept_along_a_quarter_arc_obeys_pappus) {
    CHECK_LOAD("sketch.slvs");
    hEntity c    = Circle(1);
    hEntity path = QuarterArc();
    Group  *g    = MakeFeature(Command::GROUP_SWEEP, { c, path });
    CHECK_TRUE(g != nullptr && g->featureProblem.empty());
    CHECK_TRUE(fabs(ExactVolume() - PI * (PI * 10 / 2)) < 1e-3);
}

TEST_CASE(a_sweep_along_two_lines_given_out_of_order) {
    CHECK_LOAD("sketch.slvs");
    hEntity c = Circle(1);
    Group::MenuGroup(Command::GROUP_3D);
    hEntity second = Line3D({ 0, 0, 10 }, { 0, 10, 10 }), first = Line3D({ 0, 0, 0 }, { 0, 0, 10 });
    Group *g = MakeFeature(Command::GROUP_SWEEP, { c, second, first });
    CHECK_TRUE(g != nullptr && g->featureProblem.empty());
    double v = ExactVolume();
    CHECK_TRUE(v > PI * 18 && v < PI * 21);
}

TEST_CASE(a_sweep_declares_its_end_faces) {
    CHECK_LOAD("sketch.slvs");
    hEntity c = Circle(1);
    Group::MenuGroup(Command::GROUP_3D);
    hEntity path = Line3D({ 0, 0, 10 }, { 0, 0, 0 }); // drawn from the far end
    Group  *g    = MakeFeature(Command::GROUP_SWEEP, { c, path });
    Entity *start = SK.GetEntity(g->Remap(c, Group::REMAP_SWEEP_START));
    Entity *end   = SK.GetEntity(g->Remap(c, Group::REMAP_SWEEP_END));
    CHECK_TRUE(start->FaceGetPointNum().Equals({ 0, 0, 0 }));
    CHECK_TRUE(start->FaceGetNormalNum().WithMagnitude(1).Equals({ 0, 0, -1 }));
    CHECK_TRUE(end->FaceGetPointNum().Equals({ 0, 0, 10 }));
    CHECK_TRUE(end->FaceGetNormalNum().WithMagnitude(1).Equals({ 0, 0, 1 }));
    std::set<uint32_t> names = FaceNames(g);
    CHECK_TRUE(names.count(g->Remap(c, Group::REMAP_SWEEP_START).v) == 1);
    CHECK_TRUE(names.count(g->Remap(c, Group::REMAP_SWEEP_END).v) == 1);
}
#endif
