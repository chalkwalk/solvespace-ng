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
