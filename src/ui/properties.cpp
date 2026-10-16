//-----------------------------------------------------------------------------
// The Properties panel. Ported from TextWindow::ShowGroupInfo; every change goes through the
// SolveSpaceUI method the property browser uses, run after the frame.
//-----------------------------------------------------------------------------
#include <map>
#include "solvespace.h"
#include "imgui.h"
#include "ui/panels.h"
#include "ui/properties.h"

namespace SolveSpace {
namespace Ui {

static hGroup gShownGroup = {};

void ResetProperties() {
    gShownGroup = {};
}

void ShowGroupInProperties(hGroup hg) {
    gShownGroup = hg;
    PropertiesPanel().SetShown(true);
}

hGroup PropertiesGroup() {
    if(gShownGroup.v && SK.group.FindByIdNoOops(gShownGroup)) return gShownGroup;
    return SS.GW.activeGroup;
}

// A text field for a value the model owns: it shows the model's value until it is edited,
// and Enter (or leaving the field after editing) passes the text to apply.
static void ValueField(const char *label, const std::string &value, std::function<void()> *run,
                       std::function<void(std::string)> apply) {
    static std::map<ImGuiID, std::string> editing;
    ImGuiID     id = ImGui::GetID(label);
    std::string text = editing.count(id) ? editing[id] : value;
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", text.c_str());
    ImGui::SetNextItemWidth(-FLT_MIN);
    bool entered = ImGui::InputText(label, buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue);
    if(ImGui::IsItemActive()) editing[id] = buf;
    if(entered || ImGui::IsItemDeactivatedAfterEdit()) {
        std::string s = buf;
        *run = [apply, s] { apply(s); };
        editing.erase(id);
    } else if(!ImGui::IsItemActive()) {
        editing.erase(id);
    }
}

static void Option(const char *label, bool on, hGroup hg, GroupOption option,
                   std::function<void()> *run) {
    bool v = on;
    if(ImGui::Checkbox(label, &v)) *run = [hg, option] { SS.ChangeGroupOption(hg, option); };
}

static void Radio(const char *label, bool on, std::function<void()> action,
                  std::function<void()> *run) {
    if(ImGui::RadioButton(label, on) && !on) *run = action;
}

static const char *TypeLine(const Group *g) {
    switch(g->type) {
        case Group::Type::DRAWING_3D:        return "sketch in 3d";
        case Group::Type::DRAWING_WORKPLANE: return "sketch in new workplane";
        case Group::Type::EXTRUDE:           return "extrude plane sketch";
        case Group::Type::LATHE:             return "lathe plane sketch";
        case Group::Type::REVOLVE:           return "revolve original sketch";
        case Group::Type::HELIX:             return "create helical extrusion";
        case Group::Type::ROTATE:            return "rotate original sketch";
        case Group::Type::TRANSLATE:         return "translate original sketch";
        case Group::Type::LINKED:            return "link geometry from file";
    }
    return "";
}

static void DrawGroup(Group *g, std::function<void()> *run) {
    hGroup hg   = g->h;
    using Type  = Group::Type;
    using Sub   = Group::Subtype;
    Type   type = g->type;

    ImGui::TextDisabled("Group");
    if(hg == Group::HGROUP_REFERENCES) {
        ImGui::TextUnformatted(g->name.c_str());
    } else {
        ValueField("##group-name", g->name, run, [hg](std::string s) { SS.RenameGroup(hg, s); });
    }
    ImGui::TextDisabled("%s", TypeLine(g));
    ImGui::Spacing();

    if(type == Type::EXTRUDE || type == Type::ROTATE || type == Type::TRANSLATE ||
       type == Type::REVOLVE || type == Type::HELIX) {
        bool one  = g->subtype == Sub::ONE_SIDED || g->subtype == Sub::ONE_SKEWED;
        bool two  = g->subtype == Sub::TWO_SIDED || g->subtype == Sub::TWO_SKEWED;
        bool skew = g->subtype == Sub::ONE_SKEWED || g->subtype == Sub::TWO_SKEWED;
        Radio("one-sided", one, [hg] { SS.ChangeGroupOption(hg, GroupOption::ONE_SIDED); }, run);
        ImGui::SameLine();
        Radio("two-sided", two, [hg] { SS.ChangeGroupOption(hg, GroupOption::TWO_SIDED); }, run);
        if(type == Type::EXTRUDE) {
            ImGui::SameLine();
            Option("skewed", skew, hg, GroupOption::SKEWED, run);
        }
    }
    if(type == Type::ROTATE || type == Type::TRANSLATE) {
        if(g->subtype == Sub::ONE_SIDED) {
            Radio("start with original", !g->skipFirst,
                  [hg] { SS.ChangeGroupOption(hg, GroupOption::START_WITH_ORIGINAL); }, run);
            ImGui::SameLine();
            Radio("start with copy #1", g->skipFirst,
                  [hg] { SS.ChangeGroupOption(hg, GroupOption::START_WITH_COPY); }, run);
        }
        ImGui::TextUnformatted("repeat (times)");
        ValueField("##repeat", std::to_string((int)g->valA), run,
                   [hg](std::string s) { SS.SetGroupRepeat(hg, s); });
    }
    if(type == Type::LINKED) {
        Platform::Path rel = g->linkFile.RelativeTo(SS.saveFile.Parent());
        ImGui::TextWrapped("%s", (rel.IsEmpty() ? g->linkFile : rel).raw.c_str());
        ImGui::TextUnformatted("scaled by");
        ValueField("##scale", ssprintf("%.3f", g->scale), run,
                   [hg](std::string s) { SS.SetGroupScale(hg, s); });
    }
    if(type == Type::HELIX) {
        bool fixed = g->valB != 0;
        ImGui::TextUnformatted("pitch (length per turn)");
        double pitch = fixed ? g->valB
                             : SK.GetParam(g->h.param(7))->val * PI / SK.GetParam(g->h.param(3))->val;
        ImGui::BeginDisabled(!fixed);
        ValueField("##pitch", SS.MmToString(pitch), run,
                   [hg](std::string s) { SS.SetHelixPitch(hg, s); });
        ImGui::EndDisabled();
        bool f = fixed;
        if(ImGui::Checkbox("fixed pitch", &f)) *run = [hg] { SS.ToggleHelixPitchFixed(hg); };
    }

    if(type == Type::EXTRUDE || type == Type::LATHE || type == Type::REVOLVE ||
       type == Type::LINKED || type == Type::HELIX) {
        using C = Group::CombineAs;
        ImGui::Spacing();
        ImGui::TextDisabled("solid model as");
        auto combine = [&](const char *label, C c) {
            Radio(label, g->meshCombine == c,
                  [hg, c] { SS.ChangeGroupOption(hg, GroupOption::COMBINE, c); }, run);
        };
        combine("union", C::UNION);
        ImGui::SameLine();
        combine("assemble", C::ASSEMBLE);
        combine("difference", C::DIFFERENCE);
        ImGui::SameLine();
        combine("intersection", C::INTERSECTION);
        if(type != Type::LINKED) {
            float rgb[3] = { g->color.redF(), g->color.greenF(), g->color.blueF() };
            if(ImGui::ColorEdit3("color", rgb, ImGuiColorEditFlags_NoInputs)) {
                // One undo step per edit, not per frame of a drag.
                bool first = ImGui::IsItemActivated() || !ImGui::IsItemActive();
                RgbaColor c = RgbaColor::FromFloat(rgb[0], rgb[1], rgb[2]);
                *run = [hg, c, first] { SS.SetGroupColor(hg, c, first); };
            } else if(ImGui::IsItemActivated()) {
                *run = [] { SS.UndoRemember(); };
            }
            ImGui::TextUnformatted("opacity");
            ValueField("##opacity", ssprintf("%.2f", g->color.alphaF()), run,
                       [hg](std::string s) { SS.SetGroupOpacity(hg, s); });
        }
        Option("suppress this group's solid model", g->suppress, hg,
               GroupOption::SUPPRESS_SOLID, run);
    }

    ImGui::Spacing();
    Option("show entities from this group", g->visible, hg, GroupOption::VISIBLE, run);
    if(!g->IsForcedToMeshBySource() && !g->IsTriangleMeshAssembly()) {
        Option("force NURBS surfaces to triangle mesh", g->forceToMesh, hg,
               GroupOption::FORCE_TO_MESH, run);
    } else {
        ImGui::TextDisabled("(model already forced to triangle mesh)");
    }
    Option("relax constraints and dimensions", g->relaxConstraints, hg,
           GroupOption::RELAX_CONSTRAINTS, run);
    Option("allow redundant constraints", g->allowRedundant, hg, GroupOption::ALLOW_REDUNDANT,
           run);
    Option("suppress dof calculation", g->suppressDofCalculation, hg, GroupOption::SUPPRESS_DOF,
           run);
    Option("treat all dimensions as reference", g->allDimsReference, hg,
           GroupOption::ALL_DIMS_REFERENCE, run);
    if(g->booleanFailed) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_PlotLinesHovered));
        ImGui::TextWrapped("The Boolean operation failed. It may be possible to fix the problem "
                           "by choosing 'force NURBS surfaces to triangle mesh'.");
        ImGui::PopStyleColor();
    }

    // What the group contains; a click selects it.
    ImGui::Spacing();
    int requests = 0;
    for(const Request &r : SK.request) requests += r.group == hg;
    if(ImGui::CollapsingHeader(ssprintf("Requests (%d)###requests", requests).c_str(),
                               ImGuiTreeNodeFlags_DefaultOpen)) {
        for(const Request &r : SK.request) {
            if(r.group != hg) continue;
            hRequest hr = r.h;
            std::string label = r.DescriptionString() + "##request:" + std::to_string(hr.v);
            if(ImGui::Selectable(label.c_str())) {
                *run = [hr] { TextWindow::ScreenSelectRequest(0, hr.v); };
            }
        }
    }
    int constraints = 0;
    for(const Constraint &c : SK.constraint) constraints += c.group == hg;
    if(ImGui::CollapsingHeader(ssprintf("Constraints (%d, %d DOF)###constraints", constraints,
                                        g->solved.dof).c_str(),
                               ImGuiTreeNodeFlags_DefaultOpen)) {
        for(const Constraint &c : SK.constraint) {
            if(c.group != hg) continue;
            hConstraint hc = c.h;
            std::string label = c.DescriptionString() + "##constraint:" + std::to_string(hc.v);
            if(ImGui::Selectable(label.c_str())) {
                *run = [hc] { TextWindow::ScreenSelectConstraint(0, hc.v); };
            }
        }
    }
}

std::function<void()> DrawProperties() {
    std::function<void()> run;
    if(!PropertiesPanel().Begin()) return run;
    if(Group *g = SK.group.FindByIdNoOops(PropertiesGroup())) DrawGroup(g, &run);
    PropertiesPanel().End();
    return run;
}

}
}
