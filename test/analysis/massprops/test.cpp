// Mass properties (AGENTS.md section 6, M3c): volume, surface area, mass and centre of mass of
// the active group's solid, exact from the B-rep with Open CASCADE, else from the mesh.
#include "solvespace.h"

#include "harness.h"

static bool IsTheBox(const MassProps &mp, double tolerance) {
    return fabs(mp.volume - 6000) < tolerance * 6000 && fabs(mp.area - 2200) < tolerance * 2200 &&
           mp.centre.Equals({ 5, 10, 15 }, tolerance * 30);
}

TEST_CASE(mass_properties_of_a_box_from_the_mesh) {
    CHECK_LOAD("../../group/fillet/box.slvs");
    MassProps mp;
    CHECK_TRUE(SS.MassProperties(&mp));
    CHECK_TRUE(IsTheBox(mp, 1e-6));
    SS.density = 1.24;
    std::string text = SS.MassPropertiesText();
    CHECK_TRUE(text.find("7.44 g") != std::string::npos);
    CHECK_TRUE(SS.SetSetting(Setting::DENSITY, "7.85")); // steel
    CHECK_TRUE(SS.MassPropertiesText().find("47.10 g") != std::string::npos);
    CHECK_FALSE(SS.SetSetting(Setting::DENSITY, "0"));
}

TEST_CASE(mass_properties_without_a_solid) {
    CHECK_LOAD("../../group/fillet/box.slvs");
    SS.GW.activeGroup = SK.groupOrder[1]; // the sketch
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    MassProps mp;
    CHECK_FALSE(SS.MassProperties(&mp));
    CHECK_TRUE(SS.MassPropertiesText().find("no solid") != std::string::npos);
}

#if defined(HAVE_OCCT)
TEST_CASE(mass_properties_of_a_box_from_the_b_rep) {
    CHECK_LOAD("../../group/fillet/box.slvs");
    SS.kernelKind = KernelKind::OCCT;
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    MassProps mp;
    CHECK_TRUE(SS.MassProperties(&mp));
    CHECK_TRUE(mp.exact);
    CHECK_TRUE(IsTheBox(mp, 1e-9));
}
#endif

// Review 5: common printing materials by name; mass properties name the one in use.
TEST_CASE(materials_are_named_by_their_density) {
    CHECK_TRUE(!SolveSpaceUI::Materials().empty());
    CHECK_EQ_STR(SolveSpaceUI::Materials()[0].name, "PLA");
    CHECK_LOAD("../../group/fillet/box.slvs");
    CHECK_TRUE(SS.SetSetting(Setting::DENSITY, "1.27"));
    CHECK_TRUE(SS.MassPropertiesText().find("PETG") != std::string::npos);
    CHECK_TRUE(SS.SetSetting(Setting::DENSITY, "2.7")); // aluminium: not a printing material
    CHECK_TRUE(SS.MassPropertiesText().find("PETG") == std::string::npos);
}
