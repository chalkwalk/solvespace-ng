//-----------------------------------------------------------------------------
// The Preferences panel. Ported from TextWindow::ShowConfiguration; every change goes through
// SolveSpaceUI::SetSetting / ToggleSetting, as the browser's do.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "imgui.h"
#include "ui/fields.h"
#include "ui/panels.h"
#include "ui/prefspanel.h"
#include "ui/theme.h"
#include "ui/uiactions.h"

namespace SolveSpace {
namespace Ui {

static void Toggle(const char *label, SettingToggle t, std::function<void()> *run) {
    bool on = SS.SettingOn(t);
    if(ImGui::Checkbox(label, &on)) *run = [t] { SS.ToggleSetting(t); };
}

// A labelled field; the id is the label without spaces, e.g. "##chord-tolerance".
static void Field(const char *label, const char *id, Setting s, std::function<void()> *run,
                  int index = 0) {
    ImGui::TextUnformatted(label);
    ValueField(id, SS.SettingText(s, index), run,
               [s, index](std::string text) { SS.SetSetting(s, text, index); });
}

static void Display(std::function<void()> *run) {
    ImGui::TextDisabled(_("model colours"));
    for(int i = 0; i < SS.MODEL_COLORS; i++) {
        RgbaColor c      = SS.modelColor[i];
        float     now[3] = { c.redF(), c.greenF(), c.blueF() }, rgb[3];
        bool      first;
        if(ColourField(std::to_string(i + 1).c_str(), now, rgb, &first)) {
            std::string text = ssprintf("%f, %f, %f", rgb[0], rgb[1], rgb[2]);
            *run = [i, text] { SS.SetSetting(Setting::MODEL_COLOR, text, i); };
        }
        if((i + 1) % 4 != 0 && i + 1 < SS.MODEL_COLORS) ImGui::SameLine();
    }
    Field(_("chord tolerance (%)"), "##chord-tolerance", Setting::CHORD_TOLERANCE, run);
    ImGui::TextDisabled(_("%g mm, %d triangles"), SS.chordTolCalculated,
                        SK.GetGroup(SS.GW.activeGroup)->displayMesh.l.n);
    Field(_("max piecewise linear segments"), "##max-segments", Setting::MAX_SEGMENTS, run);
    Field(_("snap grid spacing"), "##grid-spacing", Setting::GRID_SPACING, run);
    Field(_("digits after the decimal point (distances)"), "##digits",
          Setting::DIGITS_AFTER_DECIMAL, run);
    ImGui::TextDisabled(_("e.g. %s"), SS.MmToString(SS.StringToMm("1.23456789")).c_str());
    Field(_("digits after the decimal point (angles)"), "##digits-angle",
          Setting::DIGITS_AFTER_DECIMAL_DEGREE, run);
    ImGui::TextDisabled(_("e.g. %s"), SS.DegreeToString(1.23456789).c_str());
    Toggle(_("use SI prefixes for distances"), SettingToggle::USE_SI_PREFIXES, run);
    Toggle(_("draw triangle back faces in red"), SettingToggle::DRAW_BACK_FACES, run);
    Toggle(_("check sketch for closed contour"), SettingToggle::CHECK_CLOSED_CONTOUR, run);
    Toggle(_("show areas of closed contours"), SettingToggle::SHOW_CONTOUR_AREAS, run);
    Toggle(_("enable automatic line constraints"), SettingToggle::AUTOMATIC_LINE_CONSTRAINTS, run);
    Toggle(_("edit newly added dimensions"), SettingToggle::IMMEDIATELY_EDIT_DIMENSION, run);
    Toggle(_("arc default is diameter"), SettingToggle::ARC_DIM_DEFAULT_DIAMETER, run);
    Toggle(_("display the full path in the title bar"), SettingToggle::SHOW_FULL_FILE_PATH, run);
}

static void Navigation(std::function<void()> *run) {
    Toggle(_("use camera mouse navigation"), SettingToggle::CAMERA_NAV, run);
    Toggle(_("use turntable mouse navigation"), SettingToggle::TURNTABLE_NAV, run);
    Field(_("animation speed (ms; 0 to disable)"), "##animation-speed", Setting::ANIMATION_SPEED,
          run);
}

static void Export(std::function<void()> *run) {
    Field(_("chord tolerance (mm)"), "##export-chord-tolerance", Setting::EXPORT_CHORD_TOLERANCE,
          run);
    Field(_("max piecewise linear segments"), "##export-max-segments", Setting::EXPORT_MAX_SEGMENTS,
          run);
    Field(_("scale factor 1: (1:1 = mm, 1:25.4 = inch)"), "##export-scale", Setting::EXPORT_SCALE,
          run);
    Field(_("cutter radius offset (0 = none)"), "##export-offset", Setting::EXPORT_OFFSET, run);
    Toggle(_("export shaded 2d triangles"), SettingToggle::EXPORT_SHADED_TRIANGLES, run);
    if(fabs(SS.exportOffset) > LENGTH_EPS) {
        ImGui::TextDisabled(_("curves as piecewise linear (the cutter radius is not zero)"));
    } else {
        Toggle(_("export curves as piecewise linear"), SettingToggle::EXPORT_PWL_CURVES, run);
    }
    Toggle(_("fix white exported lines"), SettingToggle::FIX_EXPORT_COLORS, run);
    Toggle(_("export background color"), SettingToggle::EXPORT_BACKGROUND_COLOR, run);
    Toggle(_("lay parts flat on export"), SettingToggle::EXPORT_LAY_FLAT, run);

    ImGui::TextDisabled(_("canvas size"));
    bool autoSize = SS.exportCanvasSizeAuto;
    if(ImGui::RadioButton(_("fixed"), !autoSize) && autoSize) {
        *run = [] { SS.ToggleSetting(SettingToggle::EXPORT_CANVAS_SIZE_AUTO); };
    }
    ImGui::SameLine();
    if(ImGui::RadioButton(_("auto (margins)"), autoSize) && !autoSize) {
        *run = [] { SS.ToggleSetting(SettingToggle::EXPORT_CANVAS_SIZE_AUTO); };
    }
    if(autoSize) {
        Field(_("left"), "##margin-left", Setting::MARGIN_LEFT, run);
        Field(_("right"), "##margin-right", Setting::MARGIN_RIGHT, run);
        Field(_("bottom"), "##margin-bottom", Setting::MARGIN_BOTTOM, run);
        Field(_("top"), "##margin-top", Setting::MARGIN_TOP, run);
    } else {
        Field(_("width"), "##canvas-width", Setting::CANVAS_WIDTH, run);
        Field(_("height"), "##canvas-height", Setting::CANVAS_HEIGHT, run);
        Field(_("offset x"), "##canvas-dx", Setting::CANVAS_DX, run);
        Field(_("offset y"), "##canvas-dy", Setting::CANVAS_DY, run);
    }

    ImGui::TextDisabled(_("g code"));
    Field(_("depth"), "##gcode-depth", Setting::GCODE_DEPTH, run);
    Field(_("passes"), "##gcode-passes", Setting::GCODE_PASSES, run);
    Field(_("feed"), "##gcode-feed", Setting::GCODE_FEED, run);
    Field(_("plunge feed"), "##gcode-plunge-feed", Setting::GCODE_PLUNGE_FEED, run);
}

static void Ng(std::function<void()> *run) {
    bool light = LoadThemeChoice() == ThemeChoice::LIGHT;
    if(ImGui::RadioButton(_("dark theme"), !light) && light) *run = [] { ToggleTheme(); };
    ImGui::SameLine();
    if(ImGui::RadioButton(_("light theme"), light) && !light) *run = [] { ToggleTheme(); };
    std::string preset = Platform::GetSettings()->ThawString("NG_KeymapPreset", "modern");
    for(const char *p : { "modern", "classic" }) {
        std::string label = std::string(p) == "modern" ? _("modern keys") : _("classic keys");
        if(ImGui::RadioButton(label.c_str(), preset == p) && preset != p) {
            std::string chosen = p;
            *run = [chosen] {
                Platform::GetSettings()->FreezeString("NG_KeymapPreset", chosen);
                Actions().Run("ui.reload-keymap");
            };
        }
        if(std::string(p) == "modern") ImGui::SameLine();
    }
    // Under-constrained as the Constraints style, fully constrained quieter, culprits red.
    Toggle(_("colour constraints by state"), SettingToggle::CONSTRAINT_STATE_COLORS, run);
    // A studio light that turns with the view, for reading a part's shape (#19).
    Toggle(_("matcap shading"), SettingToggle::MATCAP_SHADING, run);
    // The material mass properties weigh: a common filament, or any density.
    ImGui::TextDisabled(_("material for mass properties"));
    for(const SolveSpaceUI::Material &m : SolveSpaceUI::Materials()) {
        bool on = SolveSpaceUI::MaterialFor(SS.density) == &m;
        if(ImGui::RadioButton(m.name, on) && !on) {
            double d = m.density;
            *run     = [d] { SS.SetSetting(Setting::DENSITY, ssprintf("%g", d)); };
        }
        if(&m != &SolveSpaceUI::Materials().back() && &m != &SolveSpaceUI::Materials()[3]) {
            ImGui::SameLine();
        }
    }
    Field(_("density (g/cm³)"), "##density", Setting::DENSITY, run);
}

std::function<void()> DrawPreferences() {
    std::function<void()> run;
    if(!PreferencesPanel().Begin()) return run;
    // NG's own settings (theme, keys) first (owner review 2026-10-17, P11).
    if(ImGui::CollapsingHeader("SolveSpace NG", ImGuiTreeNodeFlags_DefaultOpen)) Ng(&run);
    if(ImGui::CollapsingHeader(_("Display"), ImGuiTreeNodeFlags_DefaultOpen)) Display(&run);
    if(ImGui::CollapsingHeader(_("Navigation"))) Navigation(&run);
    if(ImGui::CollapsingHeader(_("Export"))) Export(&run);
    if(ImGui::CollapsingHeader(_("Solver and files"))) {
        Field(_("redundant constraint timeout (ms)"), "##redundant-timeout",
              Setting::FIND_CONSTRAINT_TIMEOUT, &run);
        Field(_("autosave interval (minutes)"), "##autosave-interval", Setting::AUTOSAVE_INTERVAL,
              &run);
    }
    if(ImGui::CollapsingHeader(_("About"))) {
        const char *vendor = "", *renderer = "", *version = "";
        if(SS.GW.canvas) SS.GW.canvas->GetIdent(&vendor, &renderer, &version);
        ImGui::TextDisabled(_("gl vendor   %s"), vendor);
        ImGui::TextDisabled(_("gl renderer %s"), renderer);
        ImGui::TextDisabled(_("gl version  %s"), version);
    }
    PreferencesPanel().End();
    return run;
}

}
}
