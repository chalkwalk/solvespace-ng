//-----------------------------------------------------------------------------
// The View panel (from TextWindow::ShowEditView) and the tool panels (from the Paste
// Transformed, Step Dimension and Tangent Arc screens). A tool panel is shown while the
// property browser's screen for that tool is the current one, and drives the browser's own
// state and handlers, so the tool behaves exactly as there.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "imgui.h"
#include "ui/fields.h"
#include "ui/panels.h"
#include "ui/toolpanels.h"

namespace SolveSpace {
namespace Ui {

using Screen = TextWindow::Screen;
using Edit   = TextWindow::Edit;

// Applies text as the browser's edit field for that meaning would.
static void BrowserEdit(Edit meaning, const std::string &text) {
    SS.TW.edit.meaning = meaning;
    SS.TW.EditControlDone(text);
}

static void Field(const char *label, const char *id, const std::string &value, Edit meaning,
                  std::function<void()> *run) {
    ImGui::TextUnformatted(label);
    ValueField(id, value, run, [meaning](std::string s) { BrowserEdit(meaning, s); });
}

static void SettingField(const char *label, const char *id, Setting s, int index,
                         std::function<void()> *run) {
    ImGui::TextUnformatted(label);
    ValueField(id, SS.SettingText(s, index), run,
               [s, index](std::string text) { SS.SetSetting(s, text, index); });
}

static std::string Xyz(Vector v, bool lengths) {
    if(!lengths) return ssprintf("%.3f, %.3f, %.3f", v.x, v.y, v.z);
    return SS.MmToString(v.x, true) + ", " + SS.MmToString(v.y, true) + ", " +
           SS.MmToString(v.z, true);
}

static void DrawView(std::function<void()> *run) {
    if(!ViewPanel().Begin()) return;
    GraphicsWindow &gw = SS.GW;
    ImGui::TextUnformatted(ssprintf("scale (px per %s)", SS.UnitName()).c_str());
    ValueField("##view-scale", ssprintf("%.3f", gw.scale * SS.MmPerUnit()), run,
               [](std::string s) { SS.GW.SetViewScale(s); });
    if(ImGui::Button("set to full scale")) {
        *run = [] { TextWindow::ScreenChangeViewToFullScale(0, 0); };
    }
    ImGui::TextUnformatted("origin (the centre of the screen)");
    ValueField("##view-origin", Xyz(gw.offset.ScaledBy(-1), true), run,
               [](std::string s) { SS.GW.SetViewOrigin(s); });
    ImGui::TextUnformatted("projection: right");
    ValueField("##view-right", Xyz(gw.projRight, false), run,
               [](std::string s) { SS.GW.SetViewProjection(/*right=*/true, s); });
    ImGui::TextUnformatted("projection: up");
    ValueField("##view-up", Xyz(gw.projUp, false), run,
               [](std::string s) { SS.GW.SetViewProjection(/*right=*/false, s); });
    ImGui::TextDisabled("out (%.3f, %.3f, %.3f)", gw.projRight.Cross(gw.projUp).x,
                        gw.projRight.Cross(gw.projUp).y, gw.projRight.Cross(gw.projUp).z);
    SettingField("perspective factor (0 for parallel)", "##perspective", Setting::CAMERA_TANGENT,
                 0, run);
    for(int i = 0; i < 2; i++) {
        SettingField(ssprintf("light %d direction", i).c_str(),
                     ssprintf("##light-direction:%d", i).c_str(), Setting::LIGHT_DIRECTION, i,
                     run);
        SettingField(ssprintf("light %d intensity", i).c_str(),
                     ssprintf("##light-intensity:%d", i).c_str(), Setting::LIGHT_INTENSITY, i,
                     run);
    }
    SettingField("ambient lighting", "##ambient", Setting::LIGHT_AMBIENT, 0, run);
    SettingField("explode distance", "##explode-distance", Setting::EXPLODE_DISTANCE, 0, run);
    ViewPanel().End();
}

// The tool panels close (back to the browser's home screen) with their close button.
static bool BeginTool(Panel &panel, Screen screen) {
    bool active = SS.TW.shown.screen == screen;
    panel.SetShown(active);
    if(!active) return false;
    bool open = panel.Begin();
    if(!panel.Shown()) {
        // Closed with the title bar's button.
        if(open) panel.End();
        TextWindow::ScreenHome(0, 0);
        return false;
    }
    return open;
}

static void DrawPaste(std::function<void()> *run) {
    if(!BeginTool(PastePanel(), Screen::PASTE_TRANSFORMED)) return;
    auto &paste = SS.TW.shown.paste;
    Field("repeat (times)", "##paste-times", std::to_string(paste.times),
          Edit::PASTE_TIMES_REPEATED, run);
    Field("rotate (degrees)", "##paste-angle", ssprintf("%.3f", paste.theta * 180 / PI),
          Edit::PASTE_ANGLE, run);
    ImGui::TextUnformatted("about point");
    ImGui::TextDisabled("%s", Xyz(paste.origin, true).c_str());
    if(ImGui::Button("use the selected point")) {
        *run = [] { TextWindow::ScreenPasteTransformed('o', 0); };
    }
    ImGui::TextUnformatted("translate");
    ImGui::TextDisabled("%s", Xyz(paste.trans, true).c_str());
    if(ImGui::Button("use the two selected points")) {
        *run = [] { TextWindow::ScreenPasteTransformed('t', 0); };
    }
    Field("scale", "##paste-scale", ssprintf("%.3f", fabs(paste.scale)), Edit::PASTE_SCALE, run);
    bool mirror = paste.scale < 0;
    if(ImGui::Checkbox("mirror", &mirror)) {
        *run = [] { TextWindow::ScreenChangePasteTransformed('f', 0); };
    }
    if(ImGui::Button("paste transformed now")) {
        *run = [] { TextWindow::ScreenPasteTransformed('g', 0); };
    }
    PastePanel().End();
}

static void DrawStepDimension(std::function<void()> *run) {
    if(!BeginTool(StepDimensionPanel(), Screen::STEP_DIMENSION)) return;
    if(Constraint *c = SK.constraint.FindByIdNoOops(SS.TW.shown.constraint)) {
        auto &step = SS.TW.stepDim;
        ImGui::TextUnformatted(c->DescriptionString().c_str());
        auto value = [&](double v) {
            return step.isDistance ? SS.MmToString(v, true) : ssprintf("%.3f", v);
        };
        ImGui::TextDisabled("start %s", value(c->valA).c_str());
        Field("finish", "##step-finish", value(step.finish), Edit::STEP_DIM_FINISH, run);
        Field("steps", "##step-steps", std::to_string(step.steps), Edit::STEP_DIM_STEPS, run);
        if(ImGui::Button("step dimension now")) {
            *run = [] { TextWindow::ScreenStepDimGo(0, 0); };
        }
    }
    StepDimensionPanel().End();
}

static void DrawTangentArc(std::function<void()> *run) {
    if(!BeginTool(TangentArcPanel(), Screen::TANGENT_ARC)) return;
    bool automatic = !SS.tangentArcManual;
    if(ImGui::Checkbox("choose radius automatically", &automatic)) {
        *run = [] { TextWindow::ScreenChangeTangentArc('a', 0); };
    }
    ImGui::BeginDisabled(automatic);
    Field("radius", "##tangent-radius", SS.MmToString(SS.tangentArcRadius, true),
          Edit::TANGENT_ARC_RADIUS, run);
    ImGui::EndDisabled();
    bool modify = SS.tangentArcModify;
    if(ImGui::Checkbox("modify original entities", &modify)) {
        *run = [] { TextWindow::ScreenChangeTangentArc('m', 0); };
    }
    ImGui::TextWrapped("To make a tangent arc at a point, select the point and choose Sketch > "
                       "Tangent Arc at Point.");
    TangentArcPanel().End();
}

std::function<void()> DrawToolPanels() {
    std::function<void()> run;
    DrawView(&run);
    DrawPaste(&run);
    DrawStepDimension(&run);
    DrawTangentArc(&run);
    return run;
}

}
}
