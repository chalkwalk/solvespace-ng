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
static hJoint gShownJoint = {};
static bool   gShownFits  = false;
static std::map<uint32_t, std::string> gSweepText; // each joint's last clearance sweep

void ResetProperties() {
    gShownGroup = {};
    gShownJoint = {};
    gShownFits  = false;
    gSweepText.clear();
}

void ShowGroupInProperties(hGroup hg) {
    gShownGroup = hg;
    gShownJoint = {};
    gShownFits  = false;
    PropertiesPanel().SetShown(true);
}

void ShowJointInProperties(hJoint hj) {
    gShownJoint = hj;
    gShownFits  = false;
    PropertiesPanel().SetShown(true);
}

void ShowFitTableInProperties() {
    SS.GW.ClearSelection();
    gShownFits  = true;
    gShownJoint = {};
    PropertiesPanel().SetShown(true);
}

std::string JointSweepText(hJoint hj) {
    auto it = gSweepText.find(hj.v);
    return it == gSweepText.end() ? "" : it->second;
}

hJoint PropertiesJoint() {
    if(gShownJoint.v && SK.joint.FindByIdNoOops(gShownJoint)) return gShownJoint;
    return {};
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

// A fit joint's group: which side it is, its fit from the table, its style, depth and
// direction (each set on both groups of the pair).
static void DrawFitJoint(Group *g, std::function<void()> *run) {
    hGroup         hg   = g->h;
    Group::FitSpec spec = g->fit;
    Group         *other = SK.group.FindByIdNoOops(spec.partner);
    std::string    them  = other ? SS.PartName(other->PartOf()) : "?";
    ImGui::Spacing();
    if(spec.dowel) {
        ImGui::TextDisabled(_("a dowel's pocket; the other is in %s"), them.c_str());
    } else if(spec.female) {
        ImGui::TextDisabled(_("the pocket; the peg is on %s"), them.c_str());
    } else {
        ImGui::TextDisabled(_("the peg; the pocket is in %s"), them.c_str());
    }
    auto set = [hg, run](Group::FitSpec s) { *run = [hg, s] { SS.SetFitJoint(hg, s); }; };
    ImGui::TextUnformatted(_("fit"));
    for(const Fit &f : SS.FitTable()) {
        std::string label = ssprintf("%s (%s)##fit-kind-%s", f.name.c_str(),
                                     SS.MmToString(f.clearance).c_str(), f.name.c_str());
        if(ImGui::RadioButton(label.c_str(), spec.kind == f.h) && spec.kind != f.h) {
            Group::FitSpec s = spec;
            s.kind           = f.h;
            set(s);
        }
    }
    if(ImGui::Button(LabelId(_("fit table"), "##fit-table").c_str())) {
        *run = [] { ShowFitTableInProperties(); };
    }
    ImGui::TextUnformatted(_("style"));
    if(ImGui::RadioButton(LabelId(_("peg and pocket"), "##fit-peg").c_str(), !spec.dowel) &&
       spec.dowel) {
        Group::FitSpec s = spec;
        s.dowel          = 0;
        set(s);
    }
    if(ImGui::RadioButton(LabelId(_("dowel (a pocket in each)"), "##fit-dowel").c_str(),
                          spec.dowel != 0) &&
       !spec.dowel) {
        Group::FitSpec s = spec;
        s.dowel          = 1;
        set(s);
    }
    ImGui::TextUnformatted(spec.dowel ? _("dowel length") : _("depth"));
    ValueField("##fit-depth", SS.MmToString(spec.depth), run, [hg, spec](std::string text) {
        if(Expr *e = Expr::From(text, /*popUpError=*/true)) {
            Group::FitSpec s = spec;
            s.depth          = SS.ExprToMm(e);
            SS.SetFitJoint(hg, s);
        }
    }, SS.UnitName());
    bool flip = spec.flip != 0;
    if(ImGui::Checkbox(LabelId(_("other way"), "##fit-flip").c_str(), &flip)) {
        Group::FitSpec s = spec;
        s.flip           = flip;
        set(s);
    }
    if(!g->featureProblem.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_PlotLinesHovered));
        ImGui::TextWrapped("%s.", g->featureProblem.c_str());
        ImGui::PopStyleColor();
    }
}

// A split's group: which half it keeps, and the dowels across the cut (set on both halves).
static void DrawSplit(Group *g, std::function<void()> *run) {
    hGroup         hg    = g->h;
    Group::FitSpec spec  = g->fit;
    double         dia   = g->valA;
    Group         *other = SK.group.FindByIdNoOops(spec.partner);
    ImGui::Spacing();
    ImGui::TextDisabled(_("%s; the other half is %s"),
                        spec.female ? _("the half in front") : _("the half behind"),
                        other ? SS.PartName(other->PartOf()).c_str() : "?");
    auto set = [hg, dia, run](Group::FitSpec s, double d) {
        *run = [hg, s, d] { SS.SetSplit(hg, s, d); };
    };
    bool flip = spec.flip != 0;
    if(ImGui::Checkbox(LabelId(_("swap the halves"), "##split-flip").c_str(), &flip)) {
        Group::FitSpec s = spec;
        s.flip           = flip;
        set(s, dia);
    }
    bool dowels = spec.dowel != 0;
    if(ImGui::Checkbox(LabelId(_("dowels across the cut"), "##split-dowels").c_str(), &dowels)) {
        Group::FitSpec s = spec;
        s.dowel          = dowels;
        set(s, dia);
    }
    if(spec.dowel) {
        auto mm = [hg, spec, dia](bool length) {
            return [hg, spec, dia, length](std::string text) {
                Expr *e = Expr::From(text, /*popUpError=*/true);
                if(e == nullptr) return;
                Group::FitSpec s = spec;
                double         d = dia;
                (length ? s.depth : d) = SS.ExprToMm(e);
                SS.SetSplit(hg, s, d);
            };
        };
        ImGui::TextUnformatted(_("diameter"));
        ValueField("##split-diameter", SS.MmToString(dia), run, mm(false), SS.UnitName());
        ImGui::TextUnformatted(_("length"));
        ValueField("##split-length", SS.MmToString(spec.depth), run, mm(true), SS.UnitName());
        ImGui::TextUnformatted(_("fit"));
        for(const Fit &f : SS.FitTable()) {
            std::string label = ssprintf("%s (%s)##split-kind-%s", f.name.c_str(),
                                         SS.MmToString(f.clearance).c_str(), f.name.c_str());
            if(ImGui::RadioButton(label.c_str(), spec.kind == f.h) && spec.kind != f.h) {
                Group::FitSpec s = spec;
                s.kind           = f.h;
                set(s, dia);
            }
        }
    }
    if(!g->featureProblem.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_PlotLinesHovered));
        ImGui::TextWrapped("%s.", g->featureProblem.c_str());
        ImGui::PopStyleColor();
    }
}

static const char *TypeLine(const Group *g) {
    switch(g->type) {
        case Group::Type::DRAWING_3D:        return _("sketch in 3d");
        case Group::Type::DRAWING_WORKPLANE: return _("sketch in new workplane");
        case Group::Type::EXTRUDE:           return _("extrude plane sketch");
        case Group::Type::LATHE:             return _("lathe plane sketch");
        case Group::Type::REVOLVE:           return _("revolve original sketch");
        case Group::Type::HELIX:             return _("create helical extrusion");
        case Group::Type::ROTATE:            return _("rotate original sketch");
        case Group::Type::TRANSLATE:         return _("translate original sketch");
        case Group::Type::LINKED:            return _("link geometry from file");
        case Group::Type::FILLET:            return _("round edges");
        case Group::Type::CHAMFER:           return _("bevel edges");
        case Group::Type::SHELL:             return _("hollow the solid");
        case Group::Type::DRAFT:             return _("taper faces");
        case Group::Type::HOLE:              return _("drill holes");
        case Group::Type::LOFT:              return _("loft through profiles");
        case Group::Type::SWEEP:             return _("sweep a profile along a path");
        case Group::Type::FIT:               return _("fit joint");
        case Group::Type::SPLIT:             return _("split the part");
    }
    return "";
}

// What a feature refers to, as a row: the kind of entity and its group.
static std::string RefLabel(hEntity he) {
    Entity *e = SK.entity.FindByIdNoOops(he);
    if(e == nullptr) return _("(gone)");
    const char *kind = _("curve");
    if(e->IsFace()) {
        kind = _("face");
    } else if(e->IsPoint()) {
        kind = _("point");
    } else if(e->type == Entity::Type::LINE_SEGMENT) {
        kind = _("line");
    } else if(e->type == Entity::Type::ARC_OF_CIRCLE) {
        kind = _("arc");
    } else if(e->type == Entity::Type::CIRCLE) {
        kind = _("circle");
    }
    Group *g = SK.group.FindByIdNoOops(e->group);
    return ssprintf(_("%s in %s"), kind, g ? g->name.c_str() : "?");
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

    ImGui::TextDisabled(_("Group"));
    if(hg == Group::HGROUP_REFERENCES) {
        ImGui::TextUnformatted(g->name.c_str());
    } else {
        ValueField("##group-name", g->name, run, [hg](std::string s) { SS.RenameGroup(hg, s); });
    }
    ImGui::TextDisabled("%s", TypeLine(g));
    if(!SK.part.IsEmpty() && hg != Group::HGROUP_REFERENCES) {
        hPart hp = g->PartOf();
        ImGui::TextUnformatted(_("part"));
        ValueField("##part-name", SS.PartName(hp), run,
                   [hp](std::string s) { SS.RenamePart(hp, s); });
    }
    if(type == Type::LINKED && hg != Group::HGROUP_REFERENCES) {
        // An older file's link sharing its part with other groups can have its own (M4b).
        bool shared = false;
        for(Group &other : SK.group) {
            if(other.h != hg && other.h != Group::HGROUP_REFERENCES &&
               other.PartOf() == g->PartOf()) {
                shared = true;
            }
        }
        if(shared && ImGui::Button(_("own part"))) *run = [hg] { SS.MoveToNewPart(hg); };
    }
    if(hg != Group::HGROUP_REFERENCES && g->HasRunningSolid()) {
        // The part's material (M4b), for its mass in the parts list; default: Preferences'.
        hPart  hp  = g->PartOf();
        Part  *p   = SK.part.FindByIdNoOops(hp);
        double own = p ? p->density : 0;
        ImGui::TextUnformatted(_("material"));
        Radio(LabelId(_("default"), "##part-material-default").c_str(), own == 0,
              [hp] { SS.SetPartMaterial(hp, 0); }, run);
        int i = 1;
        for(const SolveSpaceUI::Material &m : SolveSpaceUI::Materials()) {
            if(i++ % 3 != 0) ImGui::SameLine();
            double      d     = m.density;
            std::string label = std::string(m.name) + "##part-material-" + m.name;
            Radio(label.c_str(), own == d, [hp, d] { SS.SetPartMaterial(hp, d); }, run);
        }
    }
    ImGui::Spacing();

    if(type == Type::EXTRUDE || type == Type::ROTATE || type == Type::TRANSLATE ||
       type == Type::REVOLVE || type == Type::HELIX) {
        bool one  = g->subtype == Sub::ONE_SIDED || g->subtype == Sub::ONE_SKEWED;
        bool two  = g->subtype == Sub::TWO_SIDED || g->subtype == Sub::TWO_SKEWED;
        bool skew = g->subtype == Sub::ONE_SKEWED || g->subtype == Sub::TWO_SKEWED;
        Radio(_("one-sided"), one, [hg] { SS.ChangeGroupOption(hg, GroupOption::ONE_SIDED); }, run);
        ImGui::SameLine();
        Radio(_("two-sided"), two, [hg] { SS.ChangeGroupOption(hg, GroupOption::TWO_SIDED); }, run);
        if(type == Type::EXTRUDE) {
            ImGui::SameLine();
            Option(_("skewed"), skew, hg, GroupOption::SKEWED, run);
        }
    }
    if(type == Type::ROTATE || type == Type::TRANSLATE) {
        if(g->subtype == Sub::ONE_SIDED) {
            Radio(_("start with original"), !g->skipFirst,
                  [hg] { SS.ChangeGroupOption(hg, GroupOption::START_WITH_ORIGINAL); }, run);
            ImGui::SameLine();
            Radio(_("start with copy #1"), g->skipFirst,
                  [hg] { SS.ChangeGroupOption(hg, GroupOption::START_WITH_COPY); }, run);
        }
        ImGui::TextUnformatted(_("repeat (times)"));
        ValueField("##repeat", std::to_string((int)g->valA), run,
                   [hg](std::string s) { SS.SetGroupRepeat(hg, s); });
    }
    if(type == Type::LINKED) {
        Platform::Path rel = g->linkFile.RelativeTo(SS.saveFile.Parent());
        ImGui::TextWrapped("%s", (rel.IsEmpty() ? g->linkFile : rel).raw.c_str());
        ImGui::TextUnformatted(_("scaled by"));
        ValueField("##scale", ssprintf("%.3f", g->scale), run,
                   [hg](std::string s) { SS.SetGroupScale(hg, s); });
    }
    if(type == Type::HELIX) {
        bool fixed = g->valB != 0;
        ImGui::TextUnformatted(_("pitch (length per turn)"));
        double pitch = fixed ? g->valB
                             : SK.GetParam(g->h.param(7))->val * PI / SK.GetParam(g->h.param(3))->val;
        ImGui::BeginDisabled(!fixed);
        ValueField("##pitch", SS.MmToString(pitch), run,
                   [hg](std::string s) { SS.SetHelixPitch(hg, s); });
        ImGui::EndDisabled();
        bool f = fixed;
        if(ImGui::Checkbox(_("fixed pitch"), &f)) *run = [hg] { SS.ToggleHelixPitchFixed(hg); };
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
        kind(_("plain"), HoleKind::SIMPLE);
        ImGui::SameLine();
        kind(_("counterbore"), HoleKind::COUNTERBORE);
        ImGui::SameLine();
        kind(_("countersink"), HoleKind::COUNTERSINK);
        auto field = [&](const char *label, const char *id, std::string now, F f) {
            ImGui::TextUnformatted(label);
            ValueField(id, now, run, [hg, f](std::string s) { SS.SetHoleField(hg, f, s); });
        };
        field(_("thread (M4, M4x0.5 or none)"), "##hole-thread", SS.HoleThreadText(spec),
              F::THREAD);
        if(spec.thread <= 0) {
            field(_("diameter"), "##hole-diameter", SS.MmToString(spec.diameter), F::DIAMETER);
        }
        field(_("depth (blank: through all)"), "##hole-depth",
              spec.depth > 0 ? SS.MmToString(spec.depth) : "", F::DEPTH);
        if(spec.kind != HoleKind::SIMPLE) {
            field(spec.kind == HoleKind::COUNTERBORE ? _("counterbore diameter")
                                                     : _("countersink diameter"),
                  "##hole-head-diameter", SS.MmToString(spec.headDiameter), F::HEAD_DIAMETER);
        }
        if(spec.kind == HoleKind::COUNTERBORE) {
            field(_("counterbore depth"), "##hole-head-depth", SS.MmToString(spec.headDepth),
                  F::HEAD_DEPTH);
        } else if(spec.kind == HoleKind::COUNTERSINK) {
            field(_("countersink angle (degrees)"), "##hole-head-angle",
                  ssprintf("%g", spec.headAngle), F::HEAD_ANGLE);
        }
        field(_("print clearance (added to every diameter)"), "##hole-clearance",
              SS.MmToString(spec.clearance), F::CLEARANCE);
        bool flip = spec.flip != 0;
        if(ImGui::Checkbox(_("drill the other way"), &flip)) {
            *run = [hg, spec] {
                HoleSpec s = spec;
                s.flip     = s.flip ? 0 : 1;
                SS.SetHoleSpec(hg, s);
            };
        }
        RefList(_("hole centres"), hg, g->points, run);
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
        const char *label = type == Type::FILLET    ? _("radius")
                            : type == Type::CHAMFER ? _("distance")
                            : type == Type::SHELL   ? _("wall thickness")
                                                    : _("angle (degrees)");
        ImGui::TextUnformatted(label);
        // The solved size: what its dimension holds, or what constraints and drags set.
        std::string size = type == Type::DRAFT ? ssprintf("%.2f", atan(g->FeatureSize()) * 180 / PI)
                                               : SS.MmToString(g->FeatureSize());
        ValueField("##feature-size", size, run,
                   [hg](std::string s) { SS.SetFeatureSize(hg, s); });
        // Free, the size follows its handles and constraints; held, a dimension fixes it.
        bool held = false;
        for(const Constraint &c : SK.constraint) {
            held = held || (c.group == hg && c.type == Constraint::Type::FEATURE_SIZE);
        }
        bool hold = held;
        if(ImGui::Checkbox(_("hold this size (a dimension)"), &hold)) {
            *run = [hg, hold] { SS.HoldFeatureSize(hg, hold); };
        }
        if(type == Type::SHELL) {
            using S   = Group::ShellSide;
            auto side = [&](const char *label, S s) {
                Radio(label, g->shellSide == s, [hg, s] { SS.SetShellSide(hg, s); }, run);
            };
            side(_("inside"), S::INSIDE);
            ImGui::SameLine();
            side(_("outside"), S::OUTSIDE);
            ImGui::SameLine();
            side(_("both sides"), S::BOTH);
        }
        if(type == Type::FILLET || type == Type::CHAMFER) {
            if(!g->edges.empty()) RefList(_("edges"), hg, g->edges, run);
            if(!g->faces.empty()) {
                RefList(_("faces"), hg, g->faces, run);
                bool outline = g->featureOutline != 0;
                if(ImGui::Checkbox(_("only the outline of these faces"), &outline)) {
                    *run = [hg, outline] { SS.SetFeatureOutline(hg, outline); };
                }
            }
        } else if(type == Type::SHELL) {
            RefList(g->faces.empty() ? _("no face open (a sealed hollow)") : _("faces left open"),
                    hg, g->faces, run);
        } else {
            RefList(_("faces"), hg, g->faces, run);
        }
    }

    if(type == Type::LOFT || type == Type::SWEEP) {
        if(type == Type::LOFT) {
            RefList(_("profiles, in order"), hg, g->profiles, run);
        } else {
            RefList(_("profile, then the path"), hg, g->profiles, run);
            ImGui::TextUnformatted(_("round the path's corners (radius; 0: sharp)"));
            ValueField("##sweep-corners", SS.MmToString(g->valB), run,
                       [hg](std::string s) { SS.SetSweepCorners(hg, s); });
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

    if(type == Type::FIT) DrawFitJoint(g, run);
    if(type == Type::SPLIT) DrawSplit(g, run);

    if(type == Type::EXTRUDE || type == Type::LATHE || type == Type::REVOLVE ||
       type == Type::LINKED || type == Type::HELIX || type == Type::LOFT ||
       type == Type::SWEEP) {
        using C = Group::CombineAs;
        ImGui::Spacing();
        ImGui::TextDisabled(_("solid model as"));
        auto combine = [&](const char *label, C c) {
            Radio(label, g->meshCombine == c,
                  [hg, c] { SS.ChangeGroupOption(hg, GroupOption::COMBINE, c); }, run);
        };
        combine(_("union"), C::UNION);
        ImGui::SameLine();
        combine(_("assemble"), C::ASSEMBLE);
        combine(_("difference"), C::DIFFERENCE);
        ImGui::SameLine();
        combine(_("intersection"), C::INTERSECTION);
        if(type != Type::LINKED) {
            float now[3] = { g->color.redF(), g->color.greenF(), g->color.blueF() }, rgb[3];
            bool  first;
            if(ColourField(_("color"), now, rgb, &first)) {
                // One undo step per click or drag, not per frame of it.
                RgbaColor c = RgbaColor::FromFloat(rgb[0], rgb[1], rgb[2]);
                *run = [hg, c, first] { SS.SetGroupColor(hg, c, first); };
            }
            ImGui::TextUnformatted(_("opacity"));
            ValueField("##opacity", ssprintf("%.2f", g->color.alphaF()), run,
                       [hg](std::string s) { SS.SetGroupOpacity(hg, s); });
        }
        Option(_("suppress this group's solid model"), g->suppress, hg,
               GroupOption::SUPPRESS_SOLID, run);
    }

    ImGui::Spacing();
    Option(_("show entities from this group"), g->visible, hg, GroupOption::VISIBLE, run);
    if(!g->IsForcedToMeshBySource() && !g->IsTriangleMeshAssembly()) {
        Option(_("force NURBS surfaces to triangle mesh"), g->forceToMesh, hg,
               GroupOption::FORCE_TO_MESH, run);
    } else {
        ImGui::TextDisabled(_("(model already forced to triangle mesh)"));
    }
    Option(_("relax constraints and dimensions"), g->relaxConstraints, hg,
           GroupOption::RELAX_CONSTRAINTS, run);
    Option(_("allow redundant constraints"), g->allowRedundant, hg, GroupOption::ALLOW_REDUNDANT,
           run);
    Option(_("suppress dof calculation"), g->suppressDofCalculation, hg, GroupOption::SUPPRESS_DOF,
           run);
    Option(_("treat all dimensions as reference"), g->allDimsReference, hg,
           GroupOption::ALL_DIMS_REFERENCE, run);
    if(g->booleanFailed) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_PlotLinesHovered));
        ImGui::TextWrapped(_("The Boolean operation failed. It may be possible to fix the problem "
                             "by choosing 'force NURBS surfaces to triangle mesh'."));
        ImGui::PopStyleColor();
    }

    // What the group contains; a click selects it.
    ImGui::Spacing();
    int requests = 0;
    for(const Request &r : SK.request) requests += r.group == hg;
    if(ImGui::CollapsingHeader((ssprintf(_("Requests (%d)"), requests) + "###requests").c_str(),
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
    if(ImGui::CollapsingHeader((ssprintf(_("Constraints (%d, %d DOF)"), constraints,
                                         g->solved.dof) + "###constraints").c_str(),
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
    ImGui::TextDisabled(_("Selection"));
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
            if(ImGui::Checkbox(_("reference"), &reference)) {
                *run = [hc] { TextWindow::ScreenConstraintToggleReference(0, hc.v); };
            }
            if(m.isDiameter) {
                bool radius = m.showAsRadius;
                if(ImGui::Checkbox(_("use radius"), &radius)) {
                    *run = [hc] { TextWindow::ScreenConstraintShowAsRadius(0, hc.v); };
                }
            }
        }
    }
    if(m.ttf.v) {
        hRequest hr = m.ttf;
        ImGui::TextUnformatted(_("text"));
        ValueField("##ttf-text", m.ttfText, run,
                   [hr](std::string s) { SS.SetTtfText(hr, s); });
        bool kerning = m.ttfKerning;
        if(ImGui::Checkbox(_("apply kerning"), &kerning)) {
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
    if(m.styleAssigned && ImGui::Button(_("remove assigned style"))) {
        *run = [] { TextWindow::ScreenAssignSelectionToStyle(0, 0); };
    }
    if(ImGui::Button(_("unselect all"))) {
        *run = [] { GraphicsWindow::MenuEdit(Command::UNSELECT_ALL); };
    }
}

// A motion joint: its name, what it joins, each motion's value (typed or scrubbed), limits and
// clearance sweep.
static void DrawJoint(Joint *j, std::function<void()> *run) {
    hJoint hj = j->h;
    ImGui::TextDisabled(_("Joint"));
    ValueField("##joint-name", j->name, run, [hj](std::string s) { SS.RenameJoint(hj, s); });
    static const char *const kinds[] = { "", N_("revolute"), N_("slider"), N_("cylindrical"),
                                         N_("planar"), N_("ball"), N_("gear coupling") };
    if(j->type == Joint::Type::GEAR) {
        Joint *a = SK.joint.FindByIdNoOops(j->driver), *b = SK.joint.FindByIdNoOops(j->driven);
        ImGui::TextDisabled(_("%s: %s drives %s"), _(kinds[(int)j->type]),
                            a ? a->name.c_str() : "?",
                            b ? b->name.c_str() : "?");
        ImGui::Spacing();
        ImGui::TextUnformatted(_("ratio"));
        ValueField("##joint-ratio", ssprintf("%.6g", j->ratio), run, [hj](std::string s) {
            if(Expr *e = Expr::From(s, /*popUpError=*/true)) SS.SetJointRatio(hj, e->Eval());
        });
    } else {
        ImGui::TextDisabled(_("%s: %s holds %s"), _(kinds[(int)j->type]),
                            SS.PartName(j->parent).c_str(), SS.PartName(j->child).c_str());
    }
    static const char *const names[][Joint::MAX_MOTIONS] = {
        {},
        { N_("angle") },
        { N_("distance") },
        { N_("angle"), N_("distance") },
        { N_("along u"), N_("along v"), N_("angle") },
        { N_("about x"), N_("about y"), N_("about z") },
        {},
    };
    for(int dof = 0; dof < j->Motions(); dof++) {
        bool        angle = j->IsAngle(dof);
        const char *unit  = angle ? "°" : SS.UnitName();
        std::string id    = dof == 0 ? "" : std::to_string(dof);
        auto parsed = [hj, dof](std::function<void(double)> use) {
            return [hj, dof, use](std::string s) {
                double v;
                if(SS.ParseJointValue(hj, s, &v, dof)) use(v);
            };
        };
        ImGui::Spacing();
        ImGui::TextUnformatted(_(names[(int)j->type][dof]));
        ValueField(("##joint-value" + id).c_str(), SS.JointValueString(hj, j->value[dof], dof), run,
                   parsed([hj, dof](double v) { SS.SetJointValue(hj, v, true, dof); }), unit);
        // Scrubbing: one undo step per drag of the slider.
        float lo = (float)(j->limited[dof] ? j->lo[dof] : (angle ? -180 : -50));
        float hi = (float)(j->limited[dof] ? j->hi[dof] : (angle ? 180 : 50));
        float v  = (float)j->value[dof];
        static bool scrubbed = false; // this drag of a slider has made its undo step
        ImGui::SetNextItemWidth(-1);
        bool changed = ImGui::SliderFloat(("##joint-scrub" + id).c_str(), &v, lo, hi,
                                          angle ? "%.1f°" : "%.2f");
        if(ImGui::IsItemActivated()) scrubbed = false;
        if(changed) {
            bool   first = !scrubbed;
            double value = v;
            scrubbed     = true;
            *run = [hj, value, first, dof] { SS.SetJointValue(hj, value, first, dof); };
        }
        bool   limited = j->limited[dof];
        double jlo = j->lo[dof], jhi = j->hi[dof];
        if(ImGui::Checkbox((LabelId(_("limits"), "##joint-limited") + id).c_str(), &limited)) {
            // Turned on: from the design pose to a quarter turn, or 10 along.
            double a = j->limited[dof] ? jlo : 0, b = j->limited[dof] ? jhi : (angle ? 90 : 10);
            *run = [hj, limited, a, b, dof] { SS.SetJointLimits(hj, limited, a, b, dof); };
        }
        if(j->limited[dof]) {
            ImGui::TextUnformatted(_("from"));
            ValueField(("##joint-lo" + id).c_str(), SS.JointValueString(hj, jlo, dof), run,
                       parsed([hj, jhi, dof](double x) {
                           SS.SetJointLimits(hj, true, x, jhi, dof);
                       }),
                       unit);
            ImGui::TextUnformatted(_("to"));
            ValueField(("##joint-hi" + id).c_str(), SS.JointValueString(hj, jhi, dof), run,
                       parsed([hj, jlo, dof](double x) {
                           SS.SetJointLimits(hj, true, jlo, x, dof);
                       }),
                       unit);
        }
        if(ImGui::Button((LabelId(_("sweep clearance"), "##joint-sweep") + id).c_str())) {
            *run = [hj, dof] { gSweepText[hj.v] = SS.SweepClearance(hj, 72, dof).text; };
        }
    }
    auto it = gSweepText.find(hj.v);
    if(it != gSweepText.end()) ImGui::TextWrapped("%s", it->second.c_str());
    ImGui::Spacing();
    if(ImGui::Button(LabelId(_("design pose"), "##joint-design-pose").c_str())) {
        *run = [] { SS.DesignPose(); };
    }
    ImGui::SameLine();
    if(ImGui::Button(LabelId(_("delete"), "##joint-delete").c_str())) {
        *run = [hj] { SS.DeleteJoint(hj); };
    }
}

// The fit table: each fit's name and clearance, editable; add and delete.
static void DrawFitTable(std::function<void()> *run) {
    ImGui::TextDisabled(_("Fit table"));
    ImGui::TextWrapped(_("Clearances for fit joints, tuned to your printer: each side's gap "
                         "(negative presses)."));
    for(const Fit &f : SS.FitTable()) {
        hFit        hf = f.h;
        std::string id = std::to_string(hf.v);
        ImGui::Spacing();
        ValueField(("##fit-name-" + id).c_str(), f.name, run,
                   [hf](std::string s) { SS.RenameFit(hf, s); });
        ValueField(("##fit-clearance-" + id).c_str(), SS.MmToString(f.clearance), run,
                   [hf](std::string s) {
                       if(Expr *e = Expr::From(s, /*popUpError=*/true)) {
                           SS.SetFitClearance(hf, SS.ExprToMm(e));
                       }
                   },
                   SS.UnitName());
        if(ImGui::Button((LabelId(_("delete"), "##fit-delete-") + id).c_str())) {
            *run = [hf] { SS.DeleteFit(hf); };
        }
    }
    ImGui::Spacing();
    if(ImGui::Button(LabelId(_("add a fit"), "##fit-add").c_str())) *run = [] { SS.AddFit(); };
}

std::function<void()> DrawProperties() {
    std::function<void()> run;
    if(!PropertiesPanel().Begin()) return run;
    SelectionModel selection = BuildSelection();
    if(!selection.empty) {
        DrawSelection(selection, &run);
    } else if(Joint *j = SK.joint.FindByIdNoOops(PropertiesJoint())) {
        DrawJoint(j, &run);
    } else if(gShownFits) {
        DrawFitTable(&run);
    } else if(Group *g = SK.group.FindByIdNoOops(PropertiesGroup())) {
        DrawGroup(g, &run);
    }
    PropertiesPanel().End();
    return run;
}

}
}
