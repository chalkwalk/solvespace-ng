// Sketching on a face (after review 5): a new workplane group made from a face of the solid,
// any face entity (an extrusion's cap or side, a feature's derived face), that follows the face
// whenever the model changes. Its normal points to the viewer as the face was picked, its axes
// keep the view's.
#include "solvespace.h"

#include "harness.h"

#include "../features.h"

namespace {

// Looks from the direction toViewer with right as the screen's right.
void View(Vector right, Vector up) {
    SS.GW.projRight = right;
    SS.GW.projUp    = up;
}

Group *SketchOn(std::vector<hEntity> picked) {
    return MakeFeature(Command::GROUP_WRKPL, picked);
}

Entity *Workplane(Group *g) {
    return SK.GetEntity(g->h.entity(0));
}

hRequest LineIn(Vector a, Vector b) {
    hRequest hr = SS.GW.AddRequest(Request::Type::LINE_SEGMENT, /*rememberForUndo=*/false);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    SK.GetEntity(hr.entity(1))->PointForceTo(a);
    SK.GetEntity(hr.entity(2))->PointForceTo(b);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    return hr;
}

}

TEST_CASE(a_sketch_on_the_top_face_follows_it) {
    CHECK_LOAD("box.slvs");
    View({ 1, 0, 0 }, { 0, 1, 0 });
    Group *g = SketchOn({ Cap(Group::REMAP_TOP) });
    CHECK_TRUE(g != nullptr && g->type == Group::Type::DRAWING_WORKPLANE);
    CHECK_TRUE(g->subtype == Group::Subtype::WORKPLANE_BY_FACE);
    Entity *wp = Workplane(g);
    CHECK_TRUE(wp->Normal()->NormalN().Equals({ 0, 0, 1 }));
    CHECK_EQ_EPS(wp->WorkplaneGetOffset().z, 30);
    // Its axes keep the view's.
    CHECK_TRUE(wp->Normal()->NormalU().Equals({ 1, 0, 0 }));
    hRequest line = LineIn({ 2, 2, 30 }, { 8, 2, 30 });
    CHECK_EQ_EPS(SK.GetEntity(line.entity(1))->PointGetNum().z, 30);
    // Forty deep: the sketch moves up with the top.
    SK.GetParam(Extrusion()->h.param(2))->val = 20;
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_EQ_EPS(SK.GetEntity(line.entity(1))->PointGetNum().z, 40);
}

TEST_CASE(a_sketch_on_a_side_faces_out_of_the_solid) {
    CHECK_LOAD("box.slvs");
    View({ 1, 0, 0 }, { 0, 0, 1 }); // looking at the front, y = 0, from -Y
    Group *g = SketchOn({ SideFace(5) });
    CHECK_TRUE(g != nullptr);
    Entity *wp = Workplane(g);
    CHECK_TRUE(wp->Normal()->NormalN().Equals({ 0, -1, 0 }));
    CHECK_EQ_EPS(wp->WorkplaneGetOffset().y, 0);
}

TEST_CASE(a_sketch_on_a_face_and_a_point_starts_at_the_point) {
    CHECK_LOAD("box.slvs");
    View({ 1, 0, 0 }, { 0, 1, 0 });
    hEntity corner = Extrusion()->Remap(hRequest{ 6 }.entity(2), Group::REMAP_TOP); // (10, 20, 30)
    Group  *g      = SketchOn({ Cap(Group::REMAP_TOP), corner });
    CHECK_TRUE(g != nullptr);
    CHECK_TRUE(Workplane(g)->WorkplaneGetOffset().Equals({ 10, 20, 30 }));
}

TEST_CASE(a_sketch_on_a_face_survives_save_and_reload) {
    CHECK_LOAD("box.slvs");
    View({ 1, 0, 0 }, { 0, 1, 0 });
    Group *g  = SketchOn({ Cap(Group::REMAP_TOP) });
    hGroup hg = g->h;
    Platform::Path path = helper->GetAssetPath(__FILE__, "onface.slvs", "out");
    CHECK_TRUE(SS.SaveToFile(path));
    View({ 0, 1, 0 }, { 0, 0, 1 }); // a different view when loading changes nothing
    CHECK_TRUE(SS.LoadFromFile(path));
    SS.AfterNewFile();
    g = SK.GetGroup(hg);
    CHECK_TRUE(g->subtype == Group::Subtype::WORKPLANE_BY_FACE);
    CHECK_TRUE(Workplane(g)->Normal()->NormalN().Equals({ 0, 0, 1 }));
    CHECK_TRUE(Workplane(g)->Normal()->NormalU().Equals({ 1, 0, 0 }));
    remove(path.raw.c_str());
}

#if defined(HAVE_OCCT)
TEST_CASE(a_sketch_on_a_drafted_face_leans_with_it) {
    CHECK_LOAD("box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    Group *draft = MakeFeature(Command::GROUP_DRAFT, { SideFace(5) });
    Resize(draft, "5");
    View({ 1, 0, 0 }, { 0, 0, 1 });
    Group *g = SketchOn({ draft->Remap(SideFace(5), Group::REMAP_DRAFT_FACE) });
    CHECK_TRUE(g != nullptr);
    Vector want = Vector::From(0, -cos(5 * PI / 180), sin(5 * PI / 180));
    CHECK_TRUE(Workplane(g)->Normal()->NormalN().Equals(want, 1e-9));
}
#endif
