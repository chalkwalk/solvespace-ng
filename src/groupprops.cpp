//-----------------------------------------------------------------------------
// Group settings, shared by the property browser and the NG Properties panel.
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {

void SolveSpaceUI::ChangeGroupOption(hGroup hg, GroupOption option, Group::CombineAs combine) {
    UndoRemember();
    Group *g = SK.GetGroup(hg);
    using Subtype = Group::Subtype;
    switch(option) {
        case GroupOption::ONE_SIDED:
            if(g->subtype == Subtype::TWO_SIDED) g->subtype = Subtype::ONE_SIDED;
            if(g->subtype == Subtype::TWO_SKEWED) g->subtype = Subtype::ONE_SKEWED;
            break;
        case GroupOption::TWO_SIDED:
            if(g->subtype == Subtype::ONE_SIDED) g->subtype = Subtype::TWO_SIDED;
            if(g->subtype == Subtype::ONE_SKEWED) g->subtype = Subtype::TWO_SKEWED;
            break;
        case GroupOption::SKEWED:
            switch(g->subtype) {
                case Subtype::ONE_SIDED:  g->subtype = Subtype::ONE_SKEWED; break;
                case Subtype::TWO_SIDED:  g->subtype = Subtype::TWO_SKEWED; break;
                case Subtype::ONE_SKEWED: g->subtype = Subtype::ONE_SIDED; break;
                case Subtype::TWO_SKEWED: g->subtype = Subtype::TWO_SIDED; break;
                default: break;
            }
            break;
        case GroupOption::START_WITH_COPY:     g->skipFirst = true; break;
        case GroupOption::START_WITH_ORIGINAL: g->skipFirst = false; break;
        case GroupOption::COMBINE: {
            using C = Group::CombineAs;
            // A new extrusion points for a union; with no constraints yet, flip it when
            // switching between adding (union, assemble) and removing (difference,
            // intersection) material, to save doing it by hand.
            auto removes = [](C c) { return c == C::DIFFERENCE || c == C::INTERSECTION; };
            if(g->type == Group::Type::EXTRUDE && g->meshCombine != combine &&
               g->GetNumConstraints() == 0 && removes(g->meshCombine) != removes(combine)) {
                g->ExtrusionForceVectorTo(g->ExtrusionGetVector().Negated());
            }
            g->meshCombine = combine;
            break;
        }
        case GroupOption::SUPPRESS_SOLID:     g->suppress = !g->suppress; break;
        case GroupOption::VISIBLE:            g->visible = !g->visible; break;
        case GroupOption::FORCE_TO_MESH:      g->forceToMesh = !g->forceToMesh; break;
        case GroupOption::RELAX_CONSTRAINTS:  g->relaxConstraints = !g->relaxConstraints; break;
        case GroupOption::ALLOW_REDUNDANT:    g->allowRedundant = !g->allowRedundant; break;
        case GroupOption::SUPPRESS_DOF:
            g->suppressDofCalculation = !g->suppressDofCalculation;
            break;
        case GroupOption::ALL_DIMS_REFERENCE: g->allDimsReference = !g->allDimsReference; break;
    }
    MarkGroupDirty(g->h);
    GW.ClearSuper();
}

bool SolveSpaceUI::SetGroupRepeat(hGroup hg, const std::string &expr) {
    Expr *e = Expr::From(expr, /*popUpError=*/true);
    if(!e) return false;
    double ev = e->Eval();
    if((int)ev < 1) {
        Error(_("Can't repeat fewer than 1 time."));
        return false;
    }
    if((int)ev > 999) {
        Error(_("Can't repeat more than 999 times."));
        return false;
    }
    UndoRemember();
    Group *g = SK.GetGroup(hg);
    g->valA  = ev;
    if(g->type == Group::Type::ROTATE && g->GetNumConstraints() == 0) {
        // With no constraints, space the copies evenly over a turn as the numerical guess.
        // A constrained group keeps its guess, which would otherwise break convergence.
        double copies = g->skipFirst ? (ev + 1) : ev;
        SK.GetParam(g->h.param(3))->val = PI / (2 * copies);
    }
    MarkGroupDirty(g->h);
    return true;
}

bool SolveSpaceUI::SetGroupScale(hGroup hg, const std::string &expr) {
    Expr *e = Expr::From(expr, /*popUpError=*/true);
    if(!e) return false;
    double ev = e->Eval();
    if(fabs(ev) < 1e-6) {
        Error(_("Scale cannot be zero."));
        return false;
    }
    SK.GetGroup(hg)->scale = ev;
    MarkGroupDirty(hg);
    return true;
}

bool SolveSpaceUI::SetHelixPitch(hGroup hg, const std::string &expr) {
    Expr *e = Expr::From(expr, /*popUpError=*/true);
    if(!e) return false;
    SK.GetGroup(hg)->valB = e->Eval() * MmPerUnit(); // stored in valB
    MarkGroupDirty(hg);
    return true;
}

bool SolveSpaceUI::SetFeatureSize(hGroup hg, const std::string &expr) {
    Expr *e = Expr::From(expr, /*popUpError=*/true);
    if(!e) return false;
    Group *g = SK.GetGroup(hg);
    if(g->type == Group::Type::DRAFT) {
        // An angle in degrees: positive narrows along the pull, negative widens.
        double deg = e->Eval();
        if(fabs(deg) < 1e-6 || fabs(deg) >= 89) {
            Error(_("The draft angle must be between -89 and 89 degrees, and not zero."));
            return false;
        }
        UndoRemember();
        g->valA = deg;
        MarkGroupDirty(hg);
        return true;
    }
    double mm = e->Eval() * MmPerUnit();
    if(mm <= LENGTH_EPS) {
        Error(_("The radius or distance must be greater than zero."));
        return false;
    }
    // The size's dimension, when it holds the size, is what changes (M3h).
    for(Constraint &c : SK.constraint) {
        if(c.group != hg || c.type != Constraint::Type::FEATURE_SIZE || c.reference) continue;
        hConstraint hc = c.h;
        UndoRemember();
        SK.GetConstraint(hc)->valA = mm;
        g       = SK.GetGroup(hg);
        g->valA = mm;
        MarkGroupDirty(hg);
        return true;
    }
    UndoRemember();
    g       = SK.GetGroup(hg);
    g->valA = mm;
    if(Param *p = SK.param.FindByIdNoOops(g->h.param(0))) p->val = mm;
    MarkGroupDirty(hg);
    return true;
}

bool SolveSpaceUI::RemoveGroupRef(hGroup hg, hEntity he) {
    auto lists = [](Group *g) {
        return std::vector<std::vector<hEntity> *>{ &g->edges, &g->faces, &g->points,
                                                    &g->profiles };
    };
    Group *g = SK.GetGroup(hg);
    for(size_t i = 0; i < 4; i++) {
        std::vector<hEntity> *refs = lists(g)[i];
        auto it = std::find(refs->begin(), refs->end(), he);
        if(it == refs->end()) continue;
        size_t least = 1;
        if(g->type == Group::Type::SHELL) least = 0;
        if(g->type == Group::Type::LOFT || g->type == Group::Type::SWEEP) least = 2;
        if(g->type == Group::Type::SWEEP && it == refs->begin()) least = refs->size(); // profile
        if(refs->size() <= least) {
            Error(_("This feature needs it; delete the group instead."));
            return false;
        }
        UndoRemember();
        refs = lists(SK.GetGroup(hg))[i];
        refs->erase(std::find(refs->begin(), refs->end(), he));
        MarkGroupDirty(hg);
        return true;
    }
    return false;
}

void SolveSpaceUI::SetShellSide(hGroup hg, ShellSide side) {
    UndoRemember();
    SK.GetGroup(hg)->shellSide = side;
    MarkGroupDirty(hg);
}

bool SolveSpaceUI::SetHoleSpec(hGroup hg, const HoleSpec &spec) {
    const char *bad = nullptr;
    if(spec.thread > 0 && (spec.pitch <= 0 || spec.Bore() <= 0)) {
        bad = _("A thread needs a pitch smaller than its diameter.");
    } else if(spec.thread <= 0 && spec.diameter <= LENGTH_EPS) {
        bad = _("The hole's diameter must be greater than zero.");
    } else if(spec.clearance < 0 || spec.Bore() <= 0) {
        bad = _("The print clearance must be zero or more.");
    } else if(spec.depth < 0) {
        bad = _("The hole's depth must be zero (through all) or more.");
    } else if(spec.kind != HoleKind::SIMPLE && spec.Head() <= spec.Bore()) {
        bad = _("The head's diameter must be greater than the hole's.");
    } else if(spec.kind == HoleKind::COUNTERBORE && spec.headDepth <= LENGTH_EPS) {
        bad = _("The counterbore's depth must be greater than zero.");
    } else if(spec.kind == HoleKind::COUNTERSINK &&
              (spec.headAngle <= 0 || spec.headAngle >= 180)) {
        bad = _("The countersink's angle must be between 0 and 180 degrees.");
    }
    if(bad) {
        Error("%s", bad);
        return false;
    }
    UndoRemember();
    SK.GetGroup(hg)->hole = spec;
    MarkGroupDirty(hg);
    return true;
}

// ISO 261 metric coarse pitches.
static double CoarsePitch(double nominal) {
    static const double table[][2] = {
        { 1, 0.25 },  { 1.2, 0.25 }, { 1.6, 0.35 }, { 2, 0.4 },  { 2.5, 0.45 }, { 3, 0.5 },
        { 3.5, 0.6 }, { 4, 0.7 },    { 5, 0.8 },    { 6, 1 },    { 8, 1.25 },   { 10, 1.5 },
        { 12, 1.75 }, { 14, 2 },     { 16, 2 },     { 18, 2.5 }, { 20, 2.5 },   { 22, 2.5 },
        { 24, 3 },    { 27, 3 },     { 30, 3.5 },   { 33, 3.5 }, { 36, 4 },
    };
    for(const auto &row : table) {
        if(fabs(row[0] - nominal) < 1e-9) return row[1];
    }
    return 0;
}

std::string SolveSpaceUI::HoleThreadText(const HoleSpec &spec) {
    if(spec.thread <= 0) return "none";
    std::string text = "M" + ssprintf("%g", spec.thread);
    if(fabs(CoarsePitch(spec.thread) - spec.pitch) > 1e-9) text += ssprintf("x%g", spec.pitch);
    return text;
}

bool SolveSpaceUI::SetHoleField(hGroup hg, HoleField field, const std::string &text) {
    HoleSpec spec = SK.GetGroup(hg)->hole;
    if(field == HoleField::THREAD) {
        std::string t = text;
        t.erase(std::remove(t.begin(), t.end(), ' '), t.end());
        if(t.empty() || t == "none" || t == "0") {
            spec.thread = spec.pitch = 0;
        } else {
            double nominal = 0, pitch = 0;
            if(toupper(t[0]) == 'M') t = t.substr(1);
            int n = sscanf(t.c_str(), "%lf%*[xX]%lf", &nominal, &pitch);
            if(n < 1 || nominal <= 0) {
                Error(_("Write the thread as M4, M4x0.5, or none."));
                return false;
            }
            if(n < 2) pitch = CoarsePitch(nominal);
            if(pitch <= 0) {
                Error(_("M%g has no standard coarse pitch; write it as M%gx<pitch>."), nominal,
                      nominal);
                return false;
            }
            spec.thread = nominal;
            spec.pitch  = pitch;
        }
        return SetHoleSpec(hg, spec);
    }
    double value = 0;
    if(!(field == HoleField::DEPTH && (text.empty() || text == "through"))) {
        Expr *e = Expr::From(text, /*popUpError=*/true);
        if(!e) return false;
        value = e->Eval();
    }
    switch(field) {
        case HoleField::DIAMETER:      spec.diameter     = value * MmPerUnit(); break;
        case HoleField::DEPTH:         spec.depth        = value * MmPerUnit(); break;
        case HoleField::HEAD_DIAMETER: spec.headDiameter = value * MmPerUnit(); break;
        case HoleField::HEAD_DEPTH:    spec.headDepth    = value * MmPerUnit(); break;
        case HoleField::HEAD_ANGLE:    spec.headAngle    = value; break;
        case HoleField::CLEARANCE:     spec.clearance    = value * MmPerUnit(); break;
        case HoleField::THREAD:        break;
    }
    return SetHoleSpec(hg, spec);
}

void SolveSpaceUI::ToggleHelixPitchFixed(hGroup hg) {
    Group *g = SK.GetGroup(hg);
    if(g->valB == 0.0) {
        g->valB = SK.GetParam(g->h.param(7))->val * PI / SK.GetParam(g->h.param(3))->val;
    } else {
        g->valB = 0.0;
    }
    GW.Invalidate();
}

void SolveSpaceUI::SetGroupColor(hGroup hg, RgbaColor rgb, bool rememberUndo) {
    if(rememberUndo) UndoRemember();
    Group *g = SK.GetGroup(hg);
    g->color = RgbaColor::FromFloat(rgb.redF(), rgb.greenF(), rgb.blueF(), g->color.alphaF());
    MarkGroupDirty(hg);
    GW.ClearSuper();
}

bool SolveSpaceUI::SetGroupOpacity(hGroup hg, const std::string &expr) {
    Expr *e = Expr::From(expr, /*popUpError=*/true);
    if(!e) return false;
    double alpha = e->Eval();
    if(alpha < 0 || alpha > 1) {
        Error(_("Opacity must be between zero and one."));
        return false;
    }
    SK.GetGroup(hg)->color.alpha = (int)(255.1f * alpha);
    MarkGroupDirty(hg);
    GW.ClearSuper();
    return true;
}

}
