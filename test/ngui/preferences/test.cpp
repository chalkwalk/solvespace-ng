// The Preferences panel: SolveSpace's configuration screen and the NG settings.
#include "../uitest.h"
#include "ui/properties.h"
#include "harness.h"

using namespace SolveSpace::UiTest;

static bool PreferencesShown() {
    return Driver::FindItem("Preferences").has_value();
}

TEST_CASE(configuration_command_and_key_open_preferences) {
    CHECK_LOAD("line.slvs");
    CHECK_FALSE(PreferencesShown());
    CHECK_TRUE(Driver::ClickMenu({ "Edit", "Preferences..." }));
    CHECK_TRUE(PreferencesShown());
    CHECK_FALSE(SS.GW.showTextWindow); // not the old browser
    CHECK_TRUE(ClosePanel("Preferences"));
    Driver::Key("Ctrl+,");
    CHECK_TRUE(PreferencesShown());
}

TEST_CASE(toggles_and_fields_change_settings) {
    CHECK_LOAD("line.slvs");
    Driver::Key("Ctrl+,");
    bool si = SS.useSIPrefixes;
    // Below the SolveSpace NG section, which comes first.
    CHECK_TRUE(RevealIn("Preferences", "use SI prefixes for distances"));
    CHECK_TRUE(Driver::ClickItem("use SI prefixes for distances"));
    CHECK_TRUE(SS.useSIPrefixes == !si);
    CHECK_TRUE(RevealIn("Preferences", "max-segments"));
    CHECK_TRUE(EditField("max-segments", "64"));
    CHECK_TRUE(SS.maxSegments == 64);
    CHECK_TRUE(RevealIn("Preferences", "digits"));
    CHECK_TRUE(EditField("digits", "3"));
    CHECK_TRUE(SS.UnitDigitsAfterDecimal() == 3);
}

TEST_CASE(a_bad_value_is_refused_with_a_message) {
    CHECK_LOAD("line.slvs");
    Driver::Key("Ctrl+,");
    int digits = SS.UnitDigitsAfterDecimal();
    CHECK_TRUE(RevealIn("Preferences", "digits"));
    CHECK_TRUE(EditField("digits", "12"));
    CHECK_TRUE(SS.UnitDigitsAfterDecimal() == digits);
    CHECK_TRUE(Driver::Dialogs().size() == 1);
}

TEST_CASE(export_section_opens_and_takes_lengths) {
    CHECK_LOAD("line.slvs");
    Driver::Key("Ctrl+,");
    CHECK_FALSE(Driver::FindItem("export-offset").has_value()); // collapsed
    CHECK_TRUE(Driver::ClickItem("Display")); // fold the long first section
    CHECK_TRUE(Driver::ClickItem("Export"));
    CHECK_TRUE(RevealIn("Preferences", "export-offset"));
    CHECK_TRUE(EditField("export-offset", "0.25"));
    CHECK_EQ_EPS(SS.exportOffset, 0.25);
    CHECK_TRUE(Driver::FindItem("curves as piecewise linear (the cutter radius is not zero)")
                   .has_value() ||
               !Driver::FindItem("export curves as piecewise linear").has_value());
}

TEST_CASE(classic_keys_take_effect_at_once) {
    CHECK_LOAD("line.slvs");
    Driver::Key("Ctrl+,");
    CHECK_TRUE(RevealIn("Preferences", "classic keys"));
    CHECK_TRUE(Driver::ClickItem("classic keys"));
    Driver::Move(600, 650); // off the panel, onto the canvas
    Driver::Key("S");       // Classic: Line Segment
    CHECK_TRUE(SS.GW.pending.command == Command::LINE_SEGMENT);
}

TEST_CASE(context_menu_group_properties_opens_properties) {
    CHECK_LOAD("line.slvs");
    Driver::ClickMenu({ "New Group", "Sketch in 3D" });
    hRequest line = *LastRequest(Request::Type::LINE_SEGMENT);
    hGroup   sketch = SK.GetRequest(line)->group;
    Point2d  m = AtMiddle(line);
    ClickAt(m);
    CHECK_TRUE(ContextMenu(m, "Group Properties"));
    CHECK_TRUE(Ui::PropertiesGroup() == sketch);
    CHECK_FALSE(SS.GW.showTextWindow);
}



TEST_CASE(the_ng_section_comes_first) {
    // (P11) Theme and keys are what people look for: visible without scrolling.
    CHECK_LOAD("line.slvs");
    Driver::Key("Ctrl+,");
    std::optional<Driver::Item> light = Driver::FindItem("light theme");
    CHECK_TRUE(light && light->fullyVisible);
}

TEST_CASE(constraint_state_colours_are_on_and_can_be_turned_off) {
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(SS.constraintStateColors); // NG's default
    Driver::Key("Ctrl+,");
    std::optional<Driver::Item> box = Driver::FindItem("colour constraints by state");
    CHECK_TRUE(box && box->checked);
    CHECK_TRUE(Driver::ClickItem("colour constraints by state"));
    CHECK_FALSE(SS.constraintStateColors);
}

TEST_CASE(ng_makes_solids_with_occt_only) {
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(SS.kernelKind == KernelKind::OCCT);
    CHECK_TRUE(Platform::GetSettings()->ThawInt("Kernel", -1) == 1);
    Driver::Key("Ctrl+,");
    CHECK_TRUE(RevealIn("Preferences", "PETG"));
    CHECK_FALSE(RevealIn("Preferences", "SolveSpace kernel"));
    CHECK_FALSE(RevealIn("Preferences", "Open CASCADE kernel"));
}

TEST_CASE(ng_picks_a_material_for_mass_properties) {
    CHECK_LOAD("line.slvs");
    Driver::Key("Ctrl+,");
    CHECK_TRUE(RevealIn("Preferences", "PETG"));
    CHECK_TRUE(Driver::ClickItem("PETG"));
    CHECK_EQ_EPS(SS.density, 1.27);
    CHECK_TRUE(Platform::GetSettings()->ThawFloat("Density", 0) == 1.27);
}
