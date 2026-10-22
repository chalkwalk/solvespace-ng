//-----------------------------------------------------------------------------
// The outliner panel. Rows come from BuildOutliner(); every change goes through a core method
// shared with the property browser, run after the frame.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "imgui.h"
#include "ui/outliner.h"
#include "ui/outlinermodel.h"
#include "ui/icons.h"
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
    // Columns as Fusion's browser: active, shown, degrees of freedom left, type and name.
    if(!ImGui::BeginTable("##outliner-table", 4, ImGuiTableFlags_SizingFixedFit)) {
        OutlinerPanel().End();
        return run;
    }
    ImGui::TableSetupColumn("##active", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("##shown", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("DOF", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Group", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();
    for(const OutlinerRow &r : BuildOutliner()) {
        hGroup hg = r.group;
        if(!r.partHeader.empty()) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(3);
            float iconSize = ImGui::GetFrameHeight() - 2;
            if(ImTextureID tex = Icon("insert-part", iconSize)) {
                ImGui::Image(tex, ImVec2(iconSize, iconSize));
                ImGui::SameLine();
            }
            ImGui::TextDisabled("%s", r.partHeader.c_str());
        }
        ImGui::PushID((int)hg.v);
        if(r.afterActive) {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.55f);
        }
        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        // Active radio; #references can't be activated.
        if(r.references) {
            ImGui::Dummy(ImVec2(radioWidth, radioWidth));
        } else if(ImGui::RadioButton(("##active:" + r.name).c_str(), r.active) && !r.active) {
            run = [hg] { SS.GW.ActivateGroup(hg); };
        }
        ImGui::TableNextColumn();

        // Shown; groups after the active one aren't generated, so their state can't change.
        bool shown = r.visible;
        ImGui::BeginDisabled(r.afterActive);
        if(ImGui::Checkbox(("##shown:" + r.name).c_str(), &shown)) {
            run = [hg] { SS.GW.ToggleGroupVisible(hg); };
        }
        ImGui::EndDisabled();
        ImGui::TableNextColumn();

        // Solve status; a click shows the problems.
        ImGui::PushStyleColor(ImGuiCol_Text, StatusColor(r.status));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        std::string badge = r.afterActive ? "-" : r.StatusText();
        if(ImGui::SmallButton((badge + "##status:" + r.name).c_str()) && !r.afterActive) {
            run = [] { ProblemsPanel().SetShown(true); };
        }
        ImGui::PopStyleColor(2);
        ImGui::TableNextColumn();
        // The group's type, as the tool shelf draws it.
        float iconSize = ImGui::GetFrameHeight() - 2;
        if(ImTextureID tex = Icon(r.icon, iconSize)) {
            ImGui::Image(tex, ImVec2(iconSize, iconSize));
            ImGui::SameLine();
        }

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
            if(ImGui::Selectable((r.displayName + "##name:" + r.name).c_str(), false,
                                 ImGuiSelectableFlags_AllowDoubleClick)) {
                if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !r.references) {
                    run = [hg] { SS.GW.ActivateGroup(hg); };
                } else {
                    // The group, not whatever is selected (which Properties would show).
                    run = [hg] {
                        SS.GW.ClearSelection();
                        ShowGroupInProperties(hg);
                    };
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
    ImGui::EndTable();
    OutlinerPanel().End();
    return run;
}

}
}
