// M4's "done when" (AGENTS.md section 8): an enclosure and lid co-designed in one file export
// two printable parts. box.slvs: a 10 x 20 x 30 box.
#include "solvespace.h"

#include "harness.h"

#include "../features.h"

#if defined(HAVE_OCCT)
namespace {

hRequest AddLine(Vector a, Vector b) {
    hRequest hr = SS.GW.AddRequest(Request::Type::LINE_SEGMENT, /*rememberForUndo=*/false);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    SK.GetEntity(hr.entity(1))->PointForceTo(a);
    SK.GetEntity(hr.entity(2))->PointForceTo(b);
    return hr;
}

// A binary STL's triangles: their count, signed volume, and edges used other than twice.
struct StlInfo {
    int    triangles = 0;
    double volume    = 0;
    int    open      = 0;
};

StlInfo ReadStl(const Platform::Path &path) {
    StlInfo info;
    FILE *f = fopen(path.raw.c_str(), "rb");
    if(!f) return info;
    char     header[80];
    uint32_t n = 0;
    if(fread(header, 1, 80, f) != 80 || fread(&n, 4, 1, f) != 1) {
        fclose(f);
        return info;
    }
    std::map<std::pair<std::tuple<float, float, float>, std::tuple<float, float, float>>, int> uses;
    for(uint32_t i = 0; i < n; i++) {
        float d[12];
        uint16_t attr;
        if(fread(d, 4, 12, f) != 12 || fread(&attr, 2, 1, f) != 1) break;
        Vector v[3];
        for(int k = 0; k < 3; k++) v[k] = Vector::From(d[3 + 3 * k], d[4 + 3 * k], d[5 + 3 * k]);
        info.volume += v[0].Dot(v[1].Cross(v[2])) / 6;
        info.triangles++;
        for(int k = 0; k < 3; k++) {
            auto a = std::make_tuple(d[3 + 3 * k], d[4 + 3 * k], d[5 + 3 * k]);
            int  j = (k + 1) % 3;
            auto b = std::make_tuple(d[3 + 3 * j], d[4 + 3 * j], d[5 + 3 * j]);
            uses[{ std::min(a, b), std::max(a, b) }]++;
        }
    }
    fclose(f);
    for(auto &u : uses) info.open += u.second != 2;
    return info;
}

// The enclosure (the box hollowed, its top open) and its lid, a part sketched on its top.
void MakeEnclosureAndLid(Test::Helper *helper) {
    CHECK_LOAD("box.slvs");
    SS.SetKernel(KernelKind::OCCT);
    // The enclosure: the box hollowed, its top open, walls 1.5.
    hEntity top = Cap(Group::REMAP_TOP);
    Group *shell = MakeFeature(Command::GROUP_SHELL, { top });
    Resize(shell, "1.5");
    SS.RenamePart(Part::FIRST, "enclosure");
    // The lid: a new part sketched on the enclosure's top, its outline, 2 thick.
    SS.GW.projRight = Vector::From(1, 0, 0);
    SS.GW.projUp    = Vector::From(0, 1, 0);
    Select({ top });
    hPart lid = SS.NewPart();
    SS.RenamePart(lid, "lid");
    Vector p[4] = { { 0, 0, 30 }, { 10, 0, 30 }, { 10, 20, 30 }, { 0, 20, 30 } };
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
    SK.GetParam(e->h.param(2))->val = 1;
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
}

}

TEST_CASE(an_enclosure_and_lid_export_as_two_printable_parts) {
    MakeEnclosureAndLid(helper);
    CHECK_TRUE(SS.PartOrder().size() == 2);
    Platform::Path base = helper->GetAssetPath(__FILE__, "case.stl", "out");
    std::vector<Platform::Path> written = SS.ExportParts(base);
    CHECK_TRUE(written.size() == 2);
    if(written.size() != 2) return;
    CHECK_TRUE(written[0].FileName().find("enclosure") != std::string::npos);
    CHECK_TRUE(written[1].FileName().find("lid") != std::string::npos);
    StlInfo enclosure = ReadStl(written[0]), cover = ReadStl(written[1]);
    CHECK_TRUE(enclosure.triangles > 0 && enclosure.open == 0);
    CHECK_TRUE(cover.triangles > 0 && cover.open == 0);
    CHECK_TRUE(fabs(enclosure.volume - (6000 - 7 * 17 * 28.5)) < 0.01);
    CHECK_TRUE(fabs(cover.volume - 10 * 20 * 2) < 0.01);
    for(const Platform::Path &w : written) remove(w.raw.c_str());

    // STEP too: the exact B-reps.
    base = helper->GetAssetPath(__FILE__, "case.step", "out");
    written = SS.ExportParts(base);
    CHECK_TRUE(written.size() == 2);
    for(const Platform::Path &w : written) remove(w.raw.c_str());
}

// The whole assembly in one STEP file: a body per part, each a product named after its part.
TEST_CASE(the_assembly_exports_as_one_step_with_a_body_per_part) {
    MakeEnclosureAndLid(helper);
    CHECK_TRUE(SS.PartOrder().size() == 2);
    Platform::Path path = helper->GetAssetPath(__FILE__, "assembly.step", "out");
    CHECK_TRUE(SS.ExportAssemblyStep(path));
    std::string step;
    CHECK_TRUE(Platform::ReadFile(path, &step));
    remove(path.raw.c_str());
    int bodies = 0;
    for(size_t at = step.find("MANIFOLD_SOLID_BREP"); at != std::string::npos;
        at = step.find("MANIFOLD_SOLID_BREP", at + 1)) {
        bodies++;
    }
    CHECK_TRUE(bodies == 2);
    CHECK_TRUE(step.find("PRODUCT('enclosure'") != std::string::npos);
    CHECK_TRUE(step.find("PRODUCT('lid'") != std::string::npos);
}
#endif
