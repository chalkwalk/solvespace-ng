#include "solvespace.h"

#include "harness.h"

TEST_CASE(numeric_settings_parse_clamp_and_refuse) {
    SS.SetSetting(Setting::MAX_SEGMENTS, "3");
    CHECK_TRUE(SS.maxSegments == 7); // clamped, as the browser does
    SS.SetSetting(Setting::MAX_SEGMENTS, "64");
    CHECK_TRUE(SS.maxSegments == 64);
    CHECK_EQ_STR(SS.SettingText(Setting::MAX_SEGMENTS), "64");
    CHECK_FALSE(SS.SetSetting(Setting::DIGITS_AFTER_DECIMAL, "12"));
    CHECK_TRUE(SS.SetSetting(Setting::DIGITS_AFTER_DECIMAL, "3"));
    CHECK_TRUE(SS.UnitDigitsAfterDecimal() == 3);
    CHECK_FALSE(SS.SetSetting(Setting::EXPORT_OFFSET, "-1"));
    CHECK_TRUE(SS.SetSetting(Setting::EXPORT_OFFSET, "0.5"));
    CHECK_EQ_EPS(SS.exportOffset, 0.5);
    CHECK_TRUE(SS.SetSetting(Setting::CANVAS_WIDTH, "2*100"));
    CHECK_EQ_EPS(SS.exportCanvas.width, 200);
    CHECK_FALSE(SS.SetSetting(Setting::AUTOSAVE_INTERVAL, "soon"));
    CHECK_TRUE(SS.SetSetting(Setting::AUTOSAVE_INTERVAL, "7"));
    CHECK_TRUE(SS.autosaveInterval == 7);
}

TEST_CASE(model_colours_take_r_g_b) {
    CHECK_FALSE(SS.SetSetting(Setting::MODEL_COLOR, "red", 2));
    CHECK_TRUE(SS.SetSetting(Setting::MODEL_COLOR, "1, 0, 0.5", 2));
    CHECK_TRUE(SS.modelColor[2].red == 255 && SS.modelColor[2].green == 0);
}

TEST_CASE(toggles_flip_and_report) {
    bool before = SS.SettingOn(SettingToggle::USE_SI_PREFIXES);
    SS.ToggleSetting(SettingToggle::USE_SI_PREFIXES);
    CHECK_TRUE(SS.useSIPrefixes == !before);
    CHECK_TRUE(SS.SettingOn(SettingToggle::USE_SI_PREFIXES) == !before);
    SS.ToggleSetting(SettingToggle::EXPORT_CANVAS_SIZE_AUTO);
    SS.ToggleSetting(SettingToggle::EXPORT_CANVAS_SIZE_AUTO);
    CHECK_TRUE(SS.SettingOn(SettingToggle::CHECK_CLOSED_CONTOUR) == SS.checkClosedContour);
}
