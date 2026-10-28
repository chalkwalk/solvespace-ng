//-----------------------------------------------------------------------------
// The Styles panel. Ported from TextWindow::ShowListOfStyles and ShowStyleInfo; every change
// goes through the Style methods the browser uses.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "imgui.h"
#include "ui/fields.h"
#include "ui/panels.h"
#include "ui/stylespanel.h"

namespace SolveSpace {
namespace Ui {

static hStyle gShown = {};

void ResetStyles() {
    gShown = {};
}

void ShowStyleInStyles(hStyle hs) {
    gShown = hs;
    StylesPanel().SetShown(true);
}

// A colour field; *text gets the new colour as "r, g, b", *first whether a gesture started.
static bool ColourEdit(const char *label, RgbaColor c, std::string *text, bool *first) {
    float now[3] = { c.redF(), c.greenF(), c.blueF() }, rgb[3];
    if(!ColourField(label, now, rgb, first)) return false;
    *text = ssprintf("%f, %f, %f", rgb[0], rgb[1], rgb[2]);
    return true;
}

static void Value(const char *label, const char *id, hStyle hs, StyleValue v,
                  std::function<void()> *run) {
    ImGui::TextUnformatted(label);
    ValueField(id, Style::ValueText(hs, v), run,
               [hs, v](std::string text) { Style::SetValue(hs, v, text); });
}

static void Radio(const char *label, bool on, hStyle hs, StyleOption o,
                  std::function<void()> *run) {
    if(ImGui::RadioButton(label, on) && !on) *run = [hs, o] { Style::ChangeOption(hs, o); };
}

static void Check(const char *label, bool on, hStyle hs, StyleOption o,
                  std::function<void()> *run) {
    bool v = on;
    if(ImGui::Checkbox(label, &v)) *run = [hs, o] { Style::ChangeOption(hs, o); };
}

static void DrawStyle(Style *s, std::function<void()> *run) {
    hStyle hs     = s->h;
    bool   custom = hs.v >= Style::FIRST_CUSTOM;
    using U       = Style::UnitsAs;

    ImGui::TextDisabled(_("Style"));
    if(custom) {
        Value(_("name"), "##style-name", hs, StyleValue::NAME, run);
        if(ImGui::Button(_("delete style"))) {
            *run = [hs] {
                Style::Delete(hs);
                gShown = {};
            };
        }
    } else {
        ImGui::TextUnformatted(Style::DisplayName(hs).c_str());
    }

    ImGui::SeparatorText(_("line stroke"));
    std::string text;
    bool        first;
    if(ColourEdit(_("stroke color"), s->color, &text, &first)) {
        *run = [hs, text, first] { Style::SetValue(hs, StyleValue::COLOR, text, first); };
    }
    Value(_("width"), "##width", hs, StyleValue::WIDTH, run);
    Value(_("stipple width"), "##stipple-width", hs, StyleValue::STIPPLE_WIDTH, run);
    if(custom) {
        Radio(LabelId(_("pixels"), "##width").c_str(), s->widthAs == U::PIXELS, hs,
              StyleOption::WIDTH_IN_PIXELS, run);
        ImGui::SameLine();
        Radio((std::string(SS.UnitName()) + "##width").c_str(), s->widthAs == U::MM, hs,
              StyleOption::WIDTH_IN_MM, run);
    } else {
        ImGui::TextDisabled(_("in pixels"));
    }
    static const char *patterns[] = { N_("continuous"), N_("short dash"), N_("dash"),
                                      N_("long dash"), N_("dash dot"), N_("dash dot dot"),
                                      N_("dot"), N_("freehand"), N_("zigzag") };
    ImGui::TextDisabled(_("stipple type"));
    int current = (int)s->stippleType;
    for(int i = 0; i <= (int)StipplePattern::LAST; i++) {
        if(i % 3 != 0) ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0 + 120.0f * (i % 3));
        if(ImGui::RadioButton(_(patterns[i]), i == current) && i != current) {
            *run = [hs, i] { Style::SetStipple(hs, (StipplePattern)i); };
        }
    }

    if(custom) {
        ImGui::SeparatorText(_("contour fill"));
        if(ColourEdit(_("fill color"), s->fillColor, &text, &first)) {
            *run = [hs, text, first] {
                Style::SetValue(hs, StyleValue::FILL_COLOR, text, first);
            };
        }
        Check(_("contours are filled"), s->filled, hs, StyleOption::FILLED, run);
    }

    ImGui::SeparatorText(_("text"));
    Value(_("height"), "##text-height", hs, StyleValue::TEXT_HEIGHT, run);
    if(custom) {
        Radio(LabelId(_("pixels"), "##text").c_str(), s->textHeightAs == U::PIXELS, hs,
              StyleOption::TEXT_HEIGHT_IN_PIXELS, run);
        ImGui::SameLine();
        Radio((std::string(SS.UnitName()) + "##text").c_str(), s->textHeightAs == U::MM, hs,
              StyleOption::TEXT_HEIGHT_IN_MM, run);
        Value(_("angle"), "##text-angle", hs, StyleValue::TEXT_ANGLE, run);
        uint32_t o = (uint32_t)s->textOrigin;
        uint32_t L = (uint32_t)Style::TextOrigin::LEFT, R = (uint32_t)Style::TextOrigin::RIGHT,
                 B = (uint32_t)Style::TextOrigin::BOT, T = (uint32_t)Style::TextOrigin::TOP;
        ImGui::TextDisabled(_("comment alignment"));
        Radio(_("left"), o & L, hs, StyleOption::ALIGN_LEFT, run);
        ImGui::SameLine();
        Radio(_("center"), !(o & (L | R)), hs, StyleOption::ALIGN_CENTER, run);
        ImGui::SameLine();
        Radio(_("right"), o & R, hs, StyleOption::ALIGN_RIGHT, run);
        Radio(_("bottom"), o & B, hs, StyleOption::ALIGN_BOTTOM, run);
        ImGui::SameLine();
        Radio(_("middle"), !(o & (B | T)), hs, StyleOption::ALIGN_MIDDLE, run);
        ImGui::SameLine();
        Radio(_("top"), o & T, hs, StyleOption::ALIGN_TOP, run);
    } else {
        ImGui::TextDisabled(_("in pixels"));
    }

    ImGui::Spacing();
    if(custom) Check(_("show these objects on screen"), s->visible, hs, StyleOption::VISIBLE, run);
    Check(_("export these objects"), s->exportable, hs, StyleOption::EXPORTABLE, run);
    if(custom) {
        SS.GW.GroupSelection();
        if(SS.GW.gs.stylables > 0 && ImGui::Button(_("assign the selection to this style"))) {
            *run = [hs] { Style::AssignSelectionToStyle(hs.v); };
        }
        ImGui::TextDisabled(_("Or right-click lines and curves on the drawing."));
    }
}

std::function<void()> DrawStyles() {
    std::function<void()> run;
    if(!StylesPanel().Begin()) return run;
    for(Style &s : SK.style) {
        hStyle hs = s.h;
        ImGui::PushID((int)hs.v);
        float rgb[3] = { s.color.redF(), s.color.greenF(), s.color.blueF() };
        ImGui::ColorButton("##swatch", ImVec4(rgb[0], rgb[1], rgb[2], 1),
                           ImGuiColorEditFlags_NoTooltip, ImVec2(16, 16));
        ImGui::SameLine();
        std::string label = Style::DisplayName(hs) + "##style:" + std::to_string(hs.v);
        if(ImGui::Selectable(label.c_str(), hs == gShown)) run = [hs] { gShown = hs; };
        ImGui::PopID();
    }
    if(ImGui::Button(_("create a new custom style"))) {
        run = [] { gShown = hStyle{ Style::CreateCustomStyle() }; };
    }
    ImGui::SameLine();
    if(ImGui::Button(_("load factory defaults"))) {
        run = [] { TextWindow::ScreenLoadFactoryDefaultStyles(0, 0); };
    }
    std::string text;
    bool        first;
    if(ColourEdit(_("background color"), SS.backgroundColor, &text, &first)) {
        run = [text] { SS.SetSetting(Setting::BACKGROUND_COLOR, text); };
    }
    if(gShown.v) {
        if(Style *s = SK.style.FindByIdNoOops(gShown)) {
            ImGui::Separator();
            DrawStyle(s, &run);
        }
    }
    StylesPanel().End();
    return run;
}

}
}
