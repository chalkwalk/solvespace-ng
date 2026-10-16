//-----------------------------------------------------------------------------
// The Problems panel.
//-----------------------------------------------------------------------------
#include <set>
#include "solvespace.h"
#include "imgui.h"
#include "ui/panels.h"
#include "ui/problems.h"
#include "ui/problemsmodel.h"
#include "ui/properties.h"

namespace SolveSpace {
namespace Ui {

static std::set<std::string> gKnown; // problems seen last frame

void ResetProblems() {
    gKnown.clear();
}

static void SelectConstraints(const std::vector<hConstraint> &culprits) {
    SS.GW.ClearSelection();
    for(hConstraint hc : culprits) {
        GraphicsWindow::Selection sel = {};
        sel.constraint = hc;
        SS.GW.selection.Add(&sel);
    }
    SS.GW.Invalidate();
    SS.ScheduleShowTW();
}

std::function<void()> DrawProblems() {
    std::function<void()> run;
    std::vector<Problem> problems = BuildProblems();
    // A problem that wasn't there last frame brings the panel up; not an open contour, which
    // is how every sketch looks while it is being drawn.
    std::set<std::string> now;
    for(const Problem &p : problems) {
        now.insert(p.Key());
        if(!gKnown.count(p.Key()) && p.kind != Problem::Kind::CONTOUR) {
            ProblemsPanel().SetShown(true);
        }
    }
    gKnown = now;

    if(!ProblemsPanel().Begin()) return run;
    if(problems.empty()) ImGui::TextDisabled("No problems.");
    for(size_t i = 0; i < problems.size(); i++) {
        const Problem &p = problems[i];
        ImGui::PushID((int)i);
        hGroup hg = p.group;
        if(ImGui::Selectable((p.groupName + "##group").c_str())) {
            run = [hg] { ShowGroupInProperties(hg); };
        }
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_PlotLinesHovered));
        ImGui::TextWrapped("%s", p.title.c_str());
        ImGui::PopStyleColor();
        ImGui::TextWrapped("%s", p.detail.c_str());
        if(!p.culprits.empty()) {
            std::vector<hConstraint> culprits = p.culprits;
            if(ImGui::SmallButton("select all")) run = [culprits] { SelectConstraints(culprits); };
            for(hConstraint hc : p.culprits) {
                Constraint *c = SK.constraint.FindByIdNoOops(hc);
                if(!c) continue;
                std::string label = "   " + c->DescriptionString() + "##culprit:" +
                                    std::to_string(hc.v);
                if(ImGui::Selectable(label.c_str())) run = [hc] { SelectConstraints({ hc }); };
            }
            if(p.timedOut) ImGui::TextDisabled("Some were left out: the search timed out.");
        }
        if(p.canAllowRedundant && ImGui::Button("allow redundant constraints")) {
            run = [hg] { SS.ChangeGroupOption(hg, GroupOption::ALLOW_REDUNDANT); };
        }
        if(p.canForceToMesh && ImGui::Button("force to triangle mesh")) {
            run = [hg] { SS.ChangeGroupOption(hg, GroupOption::FORCE_TO_MESH); };
        }
        ImGui::Separator();
        ImGui::PopID();
    }
    ProblemsPanel().End();
    return run;
}

}
}
