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
    UndoRemember();
    g->valA = mm;
    MarkGroupDirty(hg);
    return true;
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
