// Parts carry a material, and the document lists its parts (AGENTS.md section 7, M4b: "a
// simple parts list (BOM)"). box.slvs: a 10 x 20 x 30 box.
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

// Part 2: a 4 x 6 x 5 block sketched on the box's top.
hPart LidOnTop() {
    SS.GW.projRight = Vector::From(1, 0, 0);
    SS.GW.projUp    = Vector::From(0, 1, 0);
    Select({ Cap(Group::REMAP_TOP) });
    hPart  lid  = SS.NewPart();
    Vector p[4] = { { 2, 2, 30 }, { 6, 2, 30 }, { 6, 8, 30 }, { 2, 8, 30 } };
    hRequest side[4];
    for(int i = 0; i < 4; i++) side[i] = AddLine(p[i], p[(i + 1) % 4]);
    for(int i = 0; i < 4; i++) {
        Constraint::ConstrainCoincident(side[i].entity(2), side[(i + 1) % 4].entity(1));
    }
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
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

TEST_CASE(a_part_takes_the_document_density_until_given_a_material) {
    CHECK_LOAD("box.slvs");
    SS.density = 1.24;
    CHECK_EQ_EPS(SS.PartDensity(Part::FIRST), 1.24);
    SS.SetPartMaterial(Part::FIRST, 1.27);
    CHECK_EQ_EPS(SS.PartDensity(Part::FIRST), 1.27);
    SS.UndoUndo();
    CHECK_EQ_EPS(SS.PartDensity(Part::FIRST), 1.24);
}

TEST_CASE(a_part_material_survives_save_and_reload) {
    CHECK_LOAD("box.slvs");
    SS.SetPartMaterial(Part::FIRST, 1.04);
    Platform::Path path = helper->GetAssetPath(__FILE__, "material.slvs", "out");
    CHECK_TRUE(SS.SaveToFile(path));
    CHECK_TRUE(SS.LoadFromFile(path));
    SS.AfterNewFile();
    CHECK_EQ_EPS(SS.PartDensity(Part::FIRST), 1.04);
    remove(path.raw.c_str());
}

TEST_CASE(the_parts_list_gives_each_part_its_volume_mass_and_size) {
    CHECK_LOAD("box.slvs");
    SS.density = 1.24;
    hPart lid  = LidOnTop();
    SS.RenamePart(Part::FIRST, "box");
    SS.RenamePart(lid, "lid");
    SS.SetPartMaterial(lid, 1.27);
    std::vector<SolveSpaceUI::PartRow> rows = SS.PartsList();
    CHECK_TRUE(rows.size() == 2);
    CHECK_EQ_STR(rows[0].name, "box");
    CHECK_EQ_STR(rows[0].material, "PLA");
    CHECK_TRUE(fabs(rows[0].volume - 6000) < 1e-3);
    CHECK_TRUE(fabs(rows[0].mass - 6000 / 1000.0 * 1.24) < 1e-3);
    CHECK_TRUE(rows[0].size.Equals({ 10, 20, 30 }, 1e-6));
    CHECK_EQ_STR(rows[1].name, "lid");
    CHECK_EQ_STR(rows[1].material, "PETG");
    CHECK_TRUE(fabs(rows[1].volume - 120) < 1e-3);
    CHECK_TRUE(rows[1].size.Equals({ 4, 6, 5 }, 1e-6));
}

TEST_CASE(the_parts_list_exports_as_csv) {
    CHECK_LOAD("box.slvs");
    SS.density = 1.24;
    hPart lid  = LidOnTop();
    SS.RenamePart(lid, "lid, top");
    Platform::Path path = helper->GetAssetPath(__FILE__, "parts.csv", "out");
    CHECK_TRUE(SS.ExportPartsList(path));
    std::string csv;
    CHECK_TRUE(ReadFile(path, &csv));
    CHECK_TRUE(csv.find("part,source,material,density_g_cm3,volume_mm3,mass_g,size_x_mm,"
                        "size_y_mm,size_z_mm\n") == 0);
    CHECK_TRUE(csv.find("\nPart 1,,PLA,1.24,6000.000,7.440,10.000,20.000,30.000\n") !=
               std::string::npos);
    CHECK_TRUE(csv.find("\n\"lid, top\",,PLA,1.24,120.000,0.149,4.000,6.000,5.000\n") !=
               std::string::npos);
    remove(path.raw.c_str());
}
