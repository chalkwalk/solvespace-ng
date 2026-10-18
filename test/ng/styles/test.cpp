#include "solvespace.h"

#include "harness.h"

TEST_CASE(custom_style_values_and_undo) {
    CHECK_LOAD("line.slvs");
    hStyle hs = { Style::CreateCustomStyle() };
    CHECK_FALSE(Style::SetValue(hs, StyleValue::NAME, ""));
    CHECK_TRUE(Style::SetValue(hs, StyleValue::NAME, "hidden lines"));
    CHECK_EQ_STR(Style::Get(hs)->name, "hidden lines");
    CHECK_TRUE(Style::SetValue(hs, StyleValue::COLOR, "1, 0, 0"));
    CHECK_TRUE(Style::Get(hs)->color.red == 255 && Style::Get(hs)->color.blue == 0);
    CHECK_FALSE(Style::SetValue(hs, StyleValue::FILL_COLOR, "blue"));
    CHECK_TRUE(Style::SetValue(hs, StyleValue::TEXT_ANGLE, "370"));
    CHECK_EQ_EPS(Style::Get(hs)->textAngle, 10.0);
    SS.UndoUndo();
    CHECK_EQ_EPS(Style::Get(hs)->textAngle, 0.0);
}

TEST_CASE(style_width_units_switch_converts) {
    CHECK_LOAD("line.slvs");
    hStyle hs = { Style::CreateCustomStyle() };
    Style::ChangeOption(hs, StyleOption::WIDTH_IN_PIXELS);
    CHECK_TRUE(Style::SetValue(hs, StyleValue::WIDTH, "3"));
    CHECK_EQ_EPS(Style::Get(hs)->width, 3.0);
    CHECK_EQ_STR(Style::ValueText(hs, StyleValue::WIDTH), "3.00");
    Style::ChangeOption(hs, StyleOption::WIDTH_IN_MM);
    CHECK_TRUE(Style::Get(hs)->widthAs == Style::UnitsAs::MM);
    CHECK_EQ_EPS(Style::Get(hs)->width, 3.0 / SS.GW.scale);
}

TEST_CASE(style_toggles_alignment_stipple_and_delete) {
    CHECK_LOAD("line.slvs");
    hStyle hs = { Style::CreateCustomStyle() };
    Style::ChangeOption(hs, StyleOption::FILLED);
    CHECK_TRUE(Style::Get(hs)->filled);
    Style::ChangeOption(hs, StyleOption::ALIGN_RIGHT);
    Style::ChangeOption(hs, StyleOption::ALIGN_TOP);
    uint32_t origin = (uint32_t)Style::Get(hs)->textOrigin;
    CHECK_TRUE((origin & (uint32_t)Style::TextOrigin::RIGHT) &&
               (origin & (uint32_t)Style::TextOrigin::TOP));
    Style::SetStipple(hs, StipplePattern::DASH);
    CHECK_TRUE(Style::Get(hs)->stippleType == StipplePattern::DASH);
    Style::Delete(hs);
    CHECK_TRUE(SK.style.FindByIdNoOops(hs) == nullptr);
}

TEST_CASE(background_colour_is_a_setting) {
    CHECK_TRUE(SS.SetSetting(Setting::BACKGROUND_COLOR, "0.1, 0.2, 0.3"));
    CHECK_TRUE(SS.backgroundColor.green == (uint8_t)(0.2 * 255 + 0.5) ||
               SS.backgroundColor.green == (uint8_t)(0.2 * 255));
    CHECK_FALSE(SS.SetSetting(Setting::BACKGROUND_COLOR, "dark"));
}

static bool Same(RgbaColor a, RgbaColor b) {
    return a.Equals(b);
}

TEST_CASE(canvas_colours_follow_the_theme_unless_customised) {
    CHECK_LOAD("line.slvs");
    const hStyle active = { Style::ACTIVE_GRP }, hovered = { Style::HOVERED },
                 constraint = { Style::CONSTRAINT };
    RgbaColor stockActive = Style::Color(active), stockHovered = Style::Color(hovered);
    CHECK_TRUE(Same(SS.backgroundColor, RGBi(0, 0, 0)));
    Style::Get(constraint)->color = RGBi(0, 0, 255); // the user's own colour

    Style::UseThemeColors(/*light=*/true);
    CHECK_TRUE(SS.backgroundColor.red > 200 && SS.backgroundColor.green > 200);
    CHECK_TRUE(Style::Color(active).red < 80); // dark lines on a light background
    CHECK_FALSE(Same(Style::Color(hovered), stockHovered));
    CHECK_TRUE(Same(Style::Color(constraint), RGBi(0, 0, 255)));

    RgbaColor lightActive = Style::Color(active);
    Style::UseThemeColors(/*light=*/true); // already light: nothing moves
    CHECK_TRUE(Same(Style::Color(active), lightActive));

    Style::UseThemeColors(/*light=*/false);
    CHECK_TRUE(Same(SS.backgroundColor, RGBi(0, 0, 0)));
    CHECK_TRUE(Same(Style::Color(active), stockActive));
    CHECK_TRUE(Same(Style::Color(hovered), stockHovered));
    CHECK_TRUE(Same(Style::Color(constraint), RGBi(0, 0, 255)));
}

TEST_CASE(built_in_styles_have_readable_names) {
    // (P10) The built-in styles read as what they colour; a custom style shows its own name.
    CHECK_LOAD("line.slvs");
    CHECK_EQ_STR(Style::DisplayName(hStyle{ Style::ACTIVE_GRP }), "Active group lines");
    CHECK_EQ_STR(Style::DisplayName(hStyle{ Style::DATUM }), "Points");
    CHECK_EQ_STR(Style::DisplayName(hStyle{ Style::CONSTRAINT }), "Constraints");
    hStyle hs = { Style::CreateCustomStyle() };
    Style::SetValue(hs, StyleValue::NAME, "hidden lines");
    CHECK_EQ_STR(Style::DisplayName(hs), "hidden lines");
}
