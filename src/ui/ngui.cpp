//-----------------------------------------------------------------------------
// Per-frame entry points of the SolveSpace NG user interface layer.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "imgui.h"
#include "commandnames.h"
#include "ui/drawtool.h"
#include "ui/grabtool.h"
#include "ui/outliner.h"
#include "ui/prefspanel.h"
#include "ui/problems.h"
#include "ui/stylespanel.h"
#include "ui/toolpanels.h"
#include "ui/properties.h"
#include "ui/panels.h"
#include "ui/ngui.h"
#include "ui/palettewidget.h"
#include "ui/piemenu.h"
#include "ui/theme.h"
#include "ui/uiactions.h"

namespace SolveSpace {
namespace Ui {

static CommandPalette       gPalette;
static std::string          gModalTool;
static std::vector<KeyHint> gModalHints;
static bool                 gHideBrowser = false;

void SetModalStatus(std::string tool, std::vector<KeyHint> hints) {
    gModalTool  = std::move(tool);
    gModalHints = std::move(hints);
}

// A bar along the bottom edge, floating over the viewport like the menu bar.
static void DrawStatusBar(const keymap::Keymap &keymap) {
    StatusModel m   = BuildStatus(keymap, gModalTool, gModalHints);
    ImGuiIO    &io  = ImGui::GetIO();
    ImGuiStyle &st  = ImGui::GetStyle();
    float       h   = ImGui::GetFrameHeight() + 2 * 6.0f;
    ImGui::SetNextWindowPos(ImVec2(0, io.DisplaySize.y - h));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, h));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 6));
    ImGui::Begin("##statusbar", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing |
                 ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav |
                 ImGuiWindowFlags_NoInputs); // the canvas keeps hover, clicks and grabs here
    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImGui::AlignTextToFramePadding();
    if(!m.tool.empty()) {
        ImGui::TextUnformatted(m.tool.c_str());
        ImGui::SameLine(0, 24);
    }
    for(const KeyHint &hint : m.hints) {
        ImGui::PushFont(MonoFont(), 0.0f);
        ImVec2 ks = ImGui::CalcTextSize(hint.keys.c_str());
        ImVec2 p  = ImGui::GetCursorScreenPos();
        float  y  = p.y + (ImGui::GetFrameHeight() - ks.y) / 2;
        dl->AddRectFilled(ImVec2(p.x, y - 2), ImVec2(p.x + ks.x + 10, y + ks.y + 2),
                          ImGui::GetColorU32(ImGuiCol_FrameBg), st.FrameRounding);
        dl->AddRect(ImVec2(p.x, y - 2), ImVec2(p.x + ks.x + 10, y + ks.y + 2),
                    ImGui::GetColorU32(ImGuiCol_Border), st.FrameRounding);
        dl->AddText(ImVec2(p.x + 5, y), ImGui::GetColorU32(ImGuiCol_Text), hint.keys.c_str());
        ImGui::PopFont();
        ImGui::Dummy(ImVec2(ks.x + 10, ImGui::GetFrameHeight()));
        ImGui::SameLine(0, 6);
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", hint.label.c_str());
        ImGui::SameLine(0, 16);
    }
    // Right side: group, DOF, solver state.
    std::string dof   = m.dof >= 0 ? "DOF " + std::to_string(m.dof) : "";
    float       right = ImGui::CalcTextSize(m.group.c_str()).x + 16 +
                        ImGui::CalcTextSize(m.solve.c_str()).x + 16;
    ImGui::PushFont(MonoFont(), 0.0f);
    right += ImGui::CalcTextSize(dof.c_str()).x;
    ImGui::PopFont();
    ImGui::SameLine(std::max(ImGui::GetCursorPosX(), io.DisplaySize.x - right - 12));
    ImGui::TextDisabled("%s", m.group.c_str());
    ImGui::SameLine(0, 16);
    ImGui::PushFont(MonoFont(), 0.0f);
    ImGui::TextUnformatted(dof.c_str());
    ImGui::PopFont();
    ImGui::SameLine(0, 16);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(
        m.solveProblem ? ImGuiCol_PlotLinesHovered : ImGuiCol_PlotHistogram));
    ImGui::TextUnformatted(m.solve.c_str());
    ImGui::PopStyleColor();
    ImGui::End();
    ImGui::PopStyleVar(2);
}

static std::string MenuLabelFor(Command c) {
    std::string l = GraphicsWindow::MenuLabel(c);
    return l.empty() ? CommandName(c) : Platform::StripMnemonic(l);
}

void InitNgUi() {
    // A new main window starts with no palette, pie or modal tool state.
    gPalette = CommandPalette{};
    Pie()    = PieMenu{};
    Grab()   = GrabTool{};
    Draw()   = DrawTool{};
    SetModalStatus("", {});
    ResetOutliner();
    ResetProperties();
    ResetProblems();
    ResetStyles();
    OutlinerPanel().Load();
    PropertiesPanel().Load();
    ProblemsPanel().Load();
    PreferencesPanel().Load();
    StylesPanel().Load();
    ViewPanel().Load();
    // Browser screens that have a panel open the panel.
    SS.GW.showScreenElsewhere = [] { return ShowScreenInPanel(); };
    gHideBrowser = true;
    SS.GW.browserIsPrimary = false;
    ApplyTheme(LoadThemeChoice());
    Actions().Bind("ui.command-palette", [] { gPalette.Open(); });
    Actions().Bind("ui.toggle-theme", [] { ToggleTheme(); });
    Actions().Bind("ui.toggle-outliner", [] { OutlinerPanel().Toggle(); });
    Actions().Bind("ui.toggle-properties", [] { PropertiesPanel().Toggle(); });
    Actions().Bind("ui.toggle-problems", [] { ProblemsPanel().Toggle(); });
    Actions().Bind("ui.pie-add", [] {
        Pie().Open("Add", {
            { "Line", "line-segment" }, { "Rectangle", "rectangle" }, { "Circle", "circle" },
            { "Arc", "arc" }, { "Spline", "cubic-spline" }, { "Point", "datum-point" },
            { "Text", "ttf-text" }, { "Workplane", "workplane" } });
    });
    Actions().Bind("ui.pie-constrain", [] {
        SS.GW.SelectHoveredIfNothingSelected();
        SS.GW.GroupSelection();
        std::vector<PieItem> items;
        for(Command c : Constraint::PieCommands()) {
            if(items.size() == 8) break;
            if(!Constraint::AppliesToSelection(c)) continue;
            items.push_back({ MenuLabelFor(c), CommandName(c) });
        }
        Pie().Open("Constrain", std::move(items));
    });
    Actions().Bind("ui.pie-view", [] {
        Pie().Open("View", {
            { "Top", "ui.view-top" }, { "Right", "ui.view-right" },
            { "Isometric", "nearest-iso" }, { "Front", "ui.view-front" },
            { "Bottom", "ui.view-bottom" }, { "Left", "ui.view-left" },
            { "Frame Selected", "ui.view-frame-selected" }, { "Back", "ui.view-back" } });
    });
}

bool ShowScreenInPanel() {
    using Screen = TextWindow::Screen;
    switch(SS.TW.shown.screen) {
        case Screen::LIST_OF_GROUPS:   OutlinerPanel().SetShown(true); return true;
        case Screen::CONFIGURATION:    PreferencesPanel().SetShown(true); return true;
        case Screen::GROUP_INFO:       ShowGroupInProperties(SS.TW.shown.group); return true;
        case Screen::GROUP_SOLVE_INFO: ProblemsPanel().SetShown(true); return true;
        case Screen::LIST_OF_STYLES:   StylesPanel().SetShown(true); return true;
        case Screen::STYLE_INFO:       ShowStyleInStyles(SS.TW.shown.style); return true;
        case Screen::EDIT_VIEW:        ViewPanel().SetShown(true); return true;
        // Their panels show themselves while the screen is current.
        case Screen::PASTE_TRANSFORMED:
        case Screen::STEP_DIMENSION:
        case Screen::TANGENT_ARC:      return true;
        default:                       return false;
    }
}

void HandleUiAction(const std::string &action) {
    if(!Actions().Run(action)) {
        dbp("UI action '%s' is unknown, unbound or disabled", action.c_str());
    }
}

std::function<void()> DrawNgUi(Platform::MenuBarSdl *menuBar, const keymap::Keymap &keymap) {
    if(Grab().Active()) {
        SetModalStatus(Grab().Description(), Grab().Hints());
    } else if(Draw().Active()) {
        SetModalStatus(Draw().Description(), Draw().Hints());
    } else {
        SetModalStatus("", {});
    }
    DrawStatusBar(keymap);
    std::function<void()> run = DrawOutliner();
    if(auto changed = DrawProperties()) run = changed;
    if(auto changed = DrawProblems()) run = changed;
    if(auto changed = DrawPreferences()) run = changed;
    if(auto changed = DrawStyles()) run = changed;
    if(auto changed = DrawToolPanels()) run = changed;
    if(gHideBrowser) {
        // The Properties panel replaces the property browser, which starts hidden; View >
        // Show Property Browser brings it back.
        gHideBrowser = false;
        run = [] {
            SS.GW.showTextWindow = false;
            SS.GW.EnsureValidActives(); // syncs the window and the View menu's check mark
        };
    }
    if(Pie().IsOpen()) {
        std::string name = Pie().Draw();
        if(!name.empty()) return [name] { RunNamedCommand(name); };
    }
    if(menuBar && gPalette.IsOpen()) {
        if(auto chosen = gPalette.Draw(BuildPaletteEntries(Platform::FlattenMenuBar(*menuBar)),
                                       keymap)) {
            return chosen;
        }
    }
    return run;
}

bool ModalKey(const keymap::Chord &c) {
    if(Pie().IsOpen()) return Pie().HandleChord(c);
    if(Grab().Active()) return Grab().HandleChord(c);
    if(Draw().Active()) return Draw().HandleChord(c);
    return false;
}

bool CapturesText() {
    return gPalette.IsOpen();
}

}
}
