//-----------------------------------------------------------------------------
// The Properties panel. Ported from TextWindow::ShowGroupInfo; every change goes through the
// SolveSpaceUI method the property browser uses, run after the frame.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "imgui.h"
#include "ui/fields.h"
#include "ui/panels.h"
#include "ui/properties.h"
#include "ui/selectionmodel.h"
#include "ui/stylespanel.h"

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
        case Group::Type::FILLET:            return "round edges";
        case Group::Type::CHAMFER:           return "bevel edges";
        case Group::Type::SHELL:             return "hollow the solid";
        case Group::Type::DRAFT:             return "taper faces";
        case Group::Type::HOLE:              return "drill holes";
        case Group::Type::LOFT:              return "loft through profiles";
        case Group::Type::SWEEP:             return "sweep a profile along a path";
    }
    return "";
}

// What a feature refers to, as a row: the kind of entity and its group.
static std::string RefLabel(hEntity he) {
    Entity *e = SK.entity.FindByIdNoOops(he);
    if(e == nullptr) return "(gone)";
    const char *kind = "curve";
    if(e->IsFace()) {
        kind = "face";
    } else if(e->IsPoint()) {
        kind = "point";
    } else if(e->type == Entity::Type::LINE_SEGMENT) {
        kind = "line";
    } else if(e->type == Entity::Type::ARC_OF_CIRCLE) {
        kind = "arc";
    } else if(e->type == Entity::Type::CIRCLE) {
        kind = "circle";
    }
    Group *g = SK.group.FindByIdNoOops(e->group);
    return ssprintf("%s in %s", kind, g ? g->name.c_str() : "?");
}

// A feature's edges, faces, centres or profiles (review 5): a click selects one, x removes it.
static void RefList(const char *title, hGroup hg, const std::vector<hEntity> &refs,
                    std::function<void()> *run) {
    ImGui::TextDisabled("%s", title);
    float remove = ImGui::GetFrameHeight();
    for(hEntity he : refs) {
        std::string id = ssprintf("%08x", he.v);
        std::string label = RefLabel(he) + "##ref-" + id;
        if(ImGui::Selectable(label.c_str(), SS.GW.IsSelected(he), 0,
                             ImVec2(ImGui::GetContentRegionAvail().x - remove - 4, 0))) {
            *run = [he] {
                SS.GW.ClearSelection();
                GraphicsWindow::Selection sel = {};
                sel.entity = he;
                SS.GW.selection.Add(&sel);
                SS.GW.Invalidate();
            };
        }
        ImGui::SameLine();
        if(ImGui::SmallButton(("x##remove-" + id).c_str())) {
            *run = [hg, he] { SS.RemoveGroupRef(hg, he); };
        }
    }
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

    if(type == Type::HOLE) {
        using F             = SolveSpaceUI::HoleField;
        const HoleSpec spec = g->hole;
        auto kind = [&](const char *label, HoleKind k) {
            Radio(label, spec.kind == k,
                  [hg, spec, k] {
                      HoleSpec s = spec;
                      s.kind     = k;
                      SS.SetHoleSpec(hg, s);
                  },
                  run);
        };
        kind("plain", HoleKind::SIMPLE);
        ImGui::SameLine();
        kind("counterbore", HoleKind::COUNTERBORE);
        ImGui::SameLine();
        kind("countersink", HoleKind::COUNTERSINK);
        auto field = [&](const char *label, const char *id, std::string now, F f) {
            ImGui::TextUnformatted(label);
            ValueField(id, now, run, [hg, f](std::string s) { SS.SetHoleField(hg, f, s); });
        };
        field("thread (M4, M4x0.5 or none)", "##hole-thread", SS.HoleThreadText(spec),
              F::THREAD);
        if(spec.thread <= 0) {
            field("diameter", "##hole-diameter", SS.MmToString(spec.diameter), F::DIAMETER);
        }
        field("depth (blank: through all)", "##hole-depth",
              spec.depth > 0 ? SS.MmToString(spec.depth) : "", F::DEPTH);
        if(spec.kind != HoleKind::SIMPLE) {
            field(spec.kind == HoleKind::COUNTERBORE ? "counterbore diameter"
                                                     : "countersink diameter",
                  "##hole-head-diameter", SS.MmToString(spec.headDiameter), F::HEAD_DIAMETER);
        }
        if(spec.kind == HoleKind::COUNTERBORE) {
            field("counterbore depth", "##hole-head-depth", SS.MmToString(spec.headDepth),
                  F::HEAD_DEPTH);
        } else if(spec.kind == HoleKind::COUNTERSINK) {
            field("countersink angle (degrees)", "##hole-head-angle",
                  ssprintf("%g", spec.headAngle), F::HEAD_ANGLE);
        }
        field("print clearance (added to every diameter)", "##hole-clearance",
              SS.MmToString(spec.clearance), F::CLEARANCE);
        bool flip = spec.flip != 0;
        if(ImGui::Checkbox("drill the other way", &flip)) {
            *run = [hg, spec] {
                HoleSpec s = spec;
                s.flip     = s.flip ? 0 : 1;
                SS.SetHoleSpec(hg, s);
            };
        }
        RefList("hole centres", hg, g->points, run);
        if(!g->featureProblem.empty()) {
            std::string problem = g->featureProblem + ".";
            problem[0]          = (char)toupper(problem[0]);
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ImGui::GetStyleColorVec4(ImGuiCol_PlotLinesHovered));
            ImGui::TextWrapped("%s", problem.c_str());
            ImGui::PopStyleColor();
        }
    }

    if(type == Type::FILLET || type == Type::CHAMFER || type == Type::SHELL ||
       type == Type::DRAFT) {
        const char *label = type == Type::FILLET    ? "radius"
                            : type == Type::CHAMFER ? "distance"
                            : type == Type::SHELL   ? "wall thickness"
                                                    : "angle (degrees)";
        ImGui::TextUnformatted(label);
        std::string size = type == Type::DRAFT ? ssprintf("%.2f", g->valA) : SS.MmToString(g->valA);
        ValueField("##feature-size", size, run,
                   [hg](std::string s) { SS.SetFeatureSize(hg, s); });
        if(type == Type::SHELL) {
            using S   = Group::ShellSide;
            auto side = [&](const char *label, S s) {
                Radio(label, g->shellSide == s, [hg, s] { SS.SetShellSide(hg, s); }, run);
            };
            side("inside", S::INSIDE);
            ImGui::SameLine();
            side("outside", S::OUTSIDE);
            ImGui::SameLine();
            side("both sides", S::BOTH);
        }
        if(type == Type::FILLET || type == Type::CHAMFER) {
            RefList("edges", hg, g->edges, run);
        } else if(type == Type::SHELL) {
            RefList(g->faces.empty() ? "no face open (a sealed hollow)" : "faces left open", hg,
                    g->faces, run);
        } else {
            RefList("faces", hg, g->faces, run);
        }
    }

    if(type == Type::LOFT || type == Type::SWEEP) {
        if(type == Type::LOFT) {
            RefList("profiles, in order", hg, g->profiles, run);
        } else {
            RefList("profile, then the path", hg, g->profiles, run);
        }
        if(!g->featureProblem.empty()) {
            std::string problem = g->featureProblem + ".";
            problem[0]          = (char)toupper(problem[0]);
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ImGui::GetStyleColorVec4(ImGuiCol_PlotLinesHovered));
            ImGui::TextWrapped("%s", problem.c_str());
            ImGui::PopStyleColor();
        }
    }

    if(type == Type::EXTRUDE || type == Type::LATHE || type == Type::REVOLVE ||
       type == Type::LINKED || type == Type::HELIX || type == Type::LOFT ||
       type == Type::SWEEP) {
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
            float now[3] = { g->color.redF(), g->color.greenF(), g->color.blueF() }, rgb[3];
            bool  first;
            if(ColourField("color", now, rgb, &first)) {
                // One undo step per click or drag, not per frame of it.
                RgbaColor c = RgbaColor::FromFloat(rgb[0], rgb[1], rgb[2]);
                *run = [hg, c, first] { SS.SetGroupColor(hg, c, first); };
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

// A row's link: what clicking it shows.
static std::function<void()> LinkAction(const SelectionRow &r) {
    if(r.entity.v) return [he = r.entity] { TextWindow::ScreenSelectEntity(0, he.v); };
    if(r.constraint.v) {
        return [hc = r.constraint] { TextWindow::ScreenSelectConstraint(0, hc.v); };
    }
    if(r.group.v) {
        return [hg = r.group] {
            SS.GW.ClearSelection();
            ShowGroupInProperties(hg);
        };
    }
    if(r.style.v) return [hs = r.style] { ShowStyleInStyles(hs); };
    return {};
}

static void DrawRows(const std::vector<SelectionRow> &rows, const char *idPrefix,
                     std::function<void()> *run) {
    if(!ImGui::BeginTable(idPrefix, 2, ImGuiTableFlags_SizingStretchProp)) return;
    for(size_t i = 0; i < rows.size(); i++) {
        const SelectionRow &r = rows[i];
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextDisabled("%s", r.label.c_str());
        ImGui::TableNextColumn();
        std::function<void()> link = LinkAction(r);
        if(link) {
            std::string label = r.value + "##" + idPrefix + ":" + std::to_string(i);
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_CheckMark));
            if(ImGui::Selectable(label.c_str())) *run = link;
            ImGui::PopStyleColor();
        } else {
            ImGui::TextWrapped("%s", r.value.c_str());
        }
    }
    ImGui::EndTable();
}

static void DrawSelection(const SelectionModel &m, std::function<void()> *run) {
    ImGui::TextDisabled("Selection");
    ImGui::TextUnformatted(m.title.c_str());
    if(m.constraint.v) {
        hConstraint hc = m.constraint;
        auto setValue  = [hc](std::string s) { SS.SetConstraintValue(hc, s); };
        if(m.isComment) {
            ValueField("##comment", m.valueText, run, setValue);
        } else if(m.hasValue) {
            ImGui::BeginDisabled(m.reference); // a reference dimension only measures
            ValueField("##value", m.valueText, run, setValue, m.unit.c_str());
            ImGui::EndDisabled();
            bool reference = m.reference;
            if(ImGui::Checkbox("reference", &reference)) {
                *run = [hc] { TextWindow::ScreenConstraintToggleReference(0, hc.v); };
            }
            if(m.isDiameter) {
                bool radius = m.showAsRadius;
                if(ImGui::Checkbox("use radius", &radius)) {
                    *run = [hc] { TextWindow::ScreenConstraintShowAsRadius(0, hc.v); };
                }
            }
        }
    }
    if(m.ttf.v) {
        hRequest hr = m.ttf;
        ImGui::TextUnformatted("text");
        ValueField("##ttf-text", m.ttfText, run,
                   [hr](std::string s) { SS.SetTtfText(hr, s); });
        bool kerning = m.ttfKerning;
        if(ImGui::Checkbox("apply kerning", &kerning)) {
            *run = [hr] { TextWindow::ScreenToggleTtfKerning(0, hr.v); };
        }
        SS.fonts.LoadAll();
        std::string current;
        for(TtfFont &f : SS.fonts.l) {
            if(f.FontFileBaseName() == m.ttfFont) current = f.name;
        }
        ImGui::SetNextItemWidth(-FLT_MIN);
        if(ImGui::BeginCombo("##font", current.c_str())) {
            for(int i = 0; i < SS.fonts.l.n; i++) {
                TtfFont &f = SS.fonts.l[i];
                std::string label = f.name + "##font:" + std::to_string(i);
                if(ImGui::Selectable(label.c_str(), f.FontFileBaseName() == m.ttfFont)) {
                    *run = [i] { TextWindow::ScreenSetTtfFont(0, (uint32_t)i); };
                }
            }
            ImGui::EndCombo();
        }
    }
    ImGui::Spacing();
    DrawRows(m.rows, "row", run);
    for(size_t i = 0; i < m.sections.size(); i++) {
        const SelectionSection &s = m.sections[i];
        std::string header = s.title + "###section:" + std::to_string(i);
        if(ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            DrawRows(s.rows, ("section" + std::to_string(i)).c_str(), run);
        }
    }
    ImGui::Spacing();
    if(m.styleAssigned && ImGui::Button("remove assigned style")) {
        *run = [] { TextWindow::ScreenAssignSelectionToStyle(0, 0); };
    }
    if(ImGui::Button("unselect all")) {
        *run = [] { GraphicsWindow::MenuEdit(Command::UNSELECT_ALL); };
    }
}

std::function<void()> DrawProperties() {
    std::function<void()> run;
    if(!PropertiesPanel().Begin()) return run;
    SelectionModel selection = BuildSelection();
    if(!selection.empty) {
        DrawSelection(selection, &run);
    } else if(Group *g = SK.group.FindByIdNoOops(PropertiesGroup())) {
        DrawGroup(g, &run);
    }
    PropertiesPanel().End();
    return run;
}

}
}
