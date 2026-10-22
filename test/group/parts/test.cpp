// Parts (AGENTS.md section 7, M4a): a part tags groups; each part has its own solid, carried
// from its own previous group; groups reference anything earlier, whatever its part.
// box.slvs: a 10 x 20 x 30 box (part 1, the only one).
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

// A rectangle in the active workplane group, closed.
void Rectangle(Vector a, Vector c) {
    Vector p[4] = { a, { c.x, a.y, a.z }, c, { a.x, c.y, a.z } };
    hRequest side[4];
    for(int i = 0; i < 4; i++) side[i] = AddLine(p[i], p[(i + 1) % 4]);
    for(int i = 0; i < 4; i++) {
        Constraint::ConstrainCoincident(side[i].entity(2), side[(i + 1) % 4].entity(1));
    }
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
}

double SolidVolume(Group *g) {
    return g ? Volume(g) : 0;
}

// Part 2: a new part sketched on the box's top, a 4 x 6 rectangle extruded 5 up.
hPart LidOnTop() {
    SS.GW.projRight = Vector::From(1, 0, 0);
    SS.GW.projUp    = Vector::From(0, 1, 0);
    Select({ Cap(Group::REMAP_TOP) });
    hPart lid = SS.NewPart();
    Rectangle({ 2, 2, 30 }, { 6, 8, 30 });
    Group::MenuGroup(Command::GROUP_EXTRUDE);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Group *e = SK.GetGroup(SS.GW.activeGroup);
    SK.GetParam(e->h.param(0))->val = 0;
    SK.GetParam(e->h.param(1))->val = 0;
    SK.GetParam(e->h.param(2))->val = 2.5;
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    return lid;
}

}

TEST_CASE(a_file_starts_with_one_part) {
    CHECK_LOAD("box.slvs");
    CHECK_TRUE(SK.part.IsEmpty()); // the first part is implicit until there is a second
    CHECK_TRUE(Extrusion()->PartOf() == Part::FIRST);
}

TEST_CASE(a_new_part_keeps_its_own_solid) {
    CHECK_LOAD("box.slvs");
    hGroup box = Extrusion()->h;
    hPart  lid = LidOnTop();
    CHECK_TRUE(SK.part.n == 2);
    CHECK_TRUE(SK.GetGroup(SS.GW.activeGroup)->PartOf() == lid);
    CHECK_TRUE(SK.GetGroup(box)->PartOf() == Part::FIRST);
    // The lid sits on the box but is its own solid; the box is unchanged.
    CHECK_TRUE(fabs(SolidVolume(SS.PartSolid(lid)) - 4 * 6 * 5) < 1e-6);
    CHECK_TRUE(fabs(SolidVolume(SS.PartSolid(Part::FIRST)) - 6000) < 1e-6);
    CHECK_TRUE(SS.PartSolid(Part::FIRST)->h == box);
}

TEST_CASE(a_part_follows_what_it_references_in_another) {
    CHECK_LOAD("box.slvs");
    hPart lid = LidOnTop();
    // The box forty deep: the lid, sketched on its top, moves up with it.
    SK.GetParam(Extrusion()->h.param(2))->val = 20;
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Group *g = SS.PartSolid(lid);
    SMesh  m = {};
    m.MakeFromCopyOf(&g->runningMesh);
    g->runningShell.TriangulateInto(&m);
    double zmin = VERY_POSITIVE;
    for(const STriangle &t : m.l) {
        for(const Vector &v : t.vertices) zmin = std::min(zmin, v.z);
    }
    m.Clear();
    CHECK_EQ_EPS(zmin, 40);
}

TEST_CASE(parts_survive_save_and_reload) {
    CHECK_LOAD("box.slvs");
    hPart lid = LidOnTop();
    SS.RenamePart(lid, "lid");
    Platform::Path path = helper->GetAssetPath(__FILE__, "parts.slvs", "out");
    CHECK_TRUE(SS.SaveToFile(path));
    CHECK_TRUE(SS.LoadFromFile(path));
    SS.AfterNewFile();
    CHECK_TRUE(SK.part.n == 2);
    CHECK_EQ_STR(SK.GetPart(lid)->name, "lid");
    CHECK_TRUE(fabs(SolidVolume(SS.PartSolid(lid)) - 120) < 1e-6);
    CHECK_TRUE(fabs(SolidVolume(SS.PartSolid(Part::FIRST)) - 6000) < 1e-6);
    remove(path.raw.c_str());
}

TEST_CASE(undo_takes_back_a_new_part) {
    CHECK_LOAD("box.slvs");
    Select({ Cap(Group::REMAP_TOP) });
    SS.NewPart();
    CHECK_TRUE(SK.part.n == 2);
    SS.UndoUndo();
    CHECK_TRUE(SK.part.IsEmpty());
    CHECK_TRUE(SK.GetGroup(SS.GW.activeGroup)->PartOf() == Part::FIRST);
}
