//-----------------------------------------------------------------------------
// The outliner panel. Rows come from BuildOutliner(); every change goes through a core method
// shared with the property browser, run after the frame.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "imgui.h"
#include "ui/outliner.h"
#include "ui/outlinermodel.h"
#include "ui/panels.h"
#include "ui/properties.h"

namespace SolveSpace {
namespace Ui {

static hGroup gRenaming        = {};
static bool   gRenameFocus     = false;
static char   gRenameText[256] = {};

void ResetOutliner() {
    gRenaming = {};
}

static ImVec4 StatusColor(OutlinerRow::Status s) {
    switch(s) {
        case OutlinerRow::Status::OK:      return ImGui::GetStyleColorVec4(ImGuiCol_PlotHistogram);
        case OutlinerRow::Status::DOF:     return ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
        case OutlinerRow::Status::WARNING:
        case OutlinerRow::Status::ERROR:   return ImGui::GetStyleColorVec4(ImGuiCol_PlotLinesHovered);
    }
    return ImGui::GetStyleColorVec4(ImGuiCol_Text);
}

std::function<void()> DrawOutliner() {
    std::function<void()> run;
    if(!OutlinerPanel().Begin()) return run;
    // A rename in progress ends if its group went away (undo, file reload).
    if(gRenaming.v && !SK.group.FindByIdNoOops(gRenaming)) gRenaming = {};

    float radioWidth = ImGui::GetFrameHeight();
    for(const OutlinerRow &r : BuildOutliner()) {
        hGroup hg = r.group;
        ImGui::PushID((int)hg.v);
        if(r.afterActive) {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.55f);
        }

        // Active radio; #references can't be activated.
        if(r.references) {
            ImGui::Dummy(ImVec2(radioWidth, radioWidth));
        } else if(ImGui::RadioButton(("##active:" + r.name).c_str(), r.active) && !r.active) {
            run = [hg] { SS.GW.ActivateGroup(hg); };
        }
        ImGui::SameLine();

        // Shown; groups after the active one aren't generated, so their state can't change.
        bool shown = r.visible;
        ImGui::BeginDisabled(r.afterActive);
        if(ImGui::Checkbox(("##shown:" + r.name).c_str(), &shown)) {
            run = [hg] { SS.GW.ToggleGroupVisible(hg); };
        }
        ImGui::EndDisabled();
        ImGui::SameLine();

        // Solve status; the details for now come from the browser's solve screen.
        ImGui::PushStyleColor(ImGuiCol_Text, StatusColor(r.status));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        std::string badge = r.afterActive ? "-" : r.StatusText();
        if(ImGui::SmallButton((badge + "##status:" + r.name).c_str()) && !r.afterActive) {
            // Until the Problems panel exists, the browser's solve screen has the details.
            run = [hg] { ShowInPropertyBrowser(&TextWindow::ScreenHowGroupSolved, hg.v); };
        }
        ImGui::PopStyleColor(2);
        ImGui::SameLine();

        if(gRenaming == hg) {
            if(gRenameFocus) {
                ImGui::SetKeyboardFocusHere();
                gRenameFocus = false;
            }
            ImGui::SetNextItemWidth(-1);
            if(ImGui::InputText(("##rename:" + r.name).c_str(), gRenameText, sizeof(gRenameText),
                                ImGuiInputTextFlags_EnterReturnsTrue |
                                ImGuiInputTextFlags_AutoSelectAll)) {
                std::string name = gRenameText;
                run       = [hg, name] { SS.RenameGroup(hg, name); };
                gRenaming = {};
            } else if(ImGui::IsItemDeactivated()) {
                gRenaming = {}; // Esc or a click elsewhere
            }
        } else {
            if(ImGui::Selectable((r.name + "##name:" + r.name).c_str(), false,
                                 ImGuiSelectableFlags_AllowDoubleClick)) {
                if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !r.references) {
                    run = [hg] { SS.GW.ActivateGroup(hg); };
                } else {
                    run = [hg] { ShowGroupInProperties(hg); };
                }
            }
            if(ImGui::BeginPopupContextItem("##context")) {
                if(ImGui::MenuItem("Rename", nullptr, false, !r.references)) {
                    gRenaming    = hg;
                    gRenameFocus = true;
                    snprintf(gRenameText, sizeof(gRenameText), "%s", r.name.c_str());
                }
                if(ImGui::MenuItem("Isolate")) {
                    run = [hg] { SS.GW.IsolateGroups({ hg }); };
                }
                if(ImGui::MenuItem("Delete", nullptr, false, !r.references)) {
                    run = [hg] { SS.GW.DeleteGroup(hg); };
                }
                ImGui::EndPopup();
            }
        }

        if(r.afterActive) ImGui::PopStyleVar();
        ImGui::PopID();
    }
    OutlinerPanel().End();
    return run;
}

}
}
