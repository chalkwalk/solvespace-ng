//-----------------------------------------------------------------------------
// Editing a dimension's value or a comment's text, shared by the on-canvas editor and the NG
// Properties panel.
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {

std::string SolveSpaceUI::ConstraintEditText(hConstraint hc) {
    Constraint *c = SK.GetConstraint(hc);
    if(c->type == Constraint::Type::COMMENT) return c->comment;

    double value = fabs(c->valA);
    // Shown as a radius: edited as a radius.
    if(c->type == Constraint::Type::DIAMETER && c->other) value /= 2;

    std::string text;
    // The usual number of digits first.
    if(c->type == Constraint::Type::LENGTH_RATIO ||
       c->type == Constraint::Type::ARC_ARC_LEN_RATIO ||
       c->type == Constraint::Type::ARC_LINE_LEN_RATIO) {
        text = ssprintf("%.3f", value);
    } else if(c->type == Constraint::Type::ANGLE) {
        text = DegreeToString(value);
    } else {
        text = MmToString(value, true);
        value /= MmPerUnit();
    }
    // If that doesn't represent it exactly, as many digits as it takes, up to 10.
    int digits = 0;
    while(fabs(std::stod(text) - value) > 1e-10) {
        text = ssprintf("%.*f", digits, value);
        digits++;
    }
    return text;
}

bool SolveSpaceUI::SetConstraintValue(hConstraint hc, const std::string &text) {
    Constraint *c = SK.GetConstraint(hc);
    if(c->type == Constraint::Type::COMMENT) {
        UndoRemember();
        c->comment = text;
        return true;
    }

    Expr *e = Expr::From(text, /*popUpError=*/true);
    if(!e) return false;
    UndoRemember();
    c = SK.GetConstraint(hc);
    switch(c->type) {
        case Constraint::Type::PROJ_PT_DISTANCE:
        case Constraint::Type::PT_LINE_DISTANCE:
        case Constraint::Type::PT_FACE_DISTANCE:
        case Constraint::Type::PT_PLANE_DISTANCE:
        case Constraint::Type::LENGTH_DIFFERENCE:
        case Constraint::Type::ARC_ARC_DIFFERENCE:
        case Constraint::Type::ARC_LINE_DIFFERENCE: {
            // A signed distance internally, though the sign isn't shown: typing a negative
            // value flips it.
            bool wasNeg = (c->valA < 0);
            c->valA     = wasNeg ? -ExprToMm(e) : ExprToMm(e);
            break;
        }
        case Constraint::Type::ANGLE:
        case Constraint::Type::LENGTH_RATIO:
        case Constraint::Type::ARC_ARC_LEN_RATIO:
        case Constraint::Type::ARC_LINE_LEN_RATIO:
            // No unit conversion, and always positive.
            c->valA = fabs(e->Eval());
            break;
        case Constraint::Type::DIAMETER:
            c->valA = fabs(ExprToMm(e));
            // Edited as a radius when shown as one.
            if(c->other) c->valA *= 2;
            break;
        default:
            // Always positive, with the unit conversion.
            c->valA = fabs(ExprToMm(e));
            break;
    }
    MarkGroupDirty(c->group);
    return true;
}

void SolveSpaceUI::SetTtfText(hRequest hr, const std::string &text) {
    Request *r = SK.request.FindByIdNoOops(hr);
    if(!r) return;
    UndoRemember();
    r = SK.request.FindByIdNoOops(hr);
    r->str = text;
    MarkGroupDirty(r->group);
}

}
