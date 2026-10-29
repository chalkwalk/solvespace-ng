//-----------------------------------------------------------------------------
// The problems, following TextWindow::ShowGroupSolveInfo, the contour messages drawn by
// Group::DrawPolyError and the browser's boolean warning.
//-----------------------------------------------------------------------------
#include "ui/problemsmodel.h"

namespace SolveSpace {
namespace Ui {

static void AddSolveProblem(Group *g, std::vector<Problem> *out) {
    if(g->IsSolvedOkay()) return;
    Problem p;
    p.group     = g->h;
    p.groupName = g->name;
    p.timedOut  = g->solved.timeout;
    switch(g->solved.how) {
        case SolveResult::DIDNT_CONVERGE:
            p.kind   = Problem::Kind::UNSOLVABLE;
            p.title  = _("unsolvable constraints");
            p.detail = _("These constraints are incompatible. Edit > Undo may fix it.");
            break;
        case SolveResult::REDUNDANT_DIDNT_CONVERGE:
            p.kind   = Problem::Kind::UNSATISFIED;
            p.title  = _("unsolvable constraints");
            p.detail = _("These constraints are unsatisfied. Edit > Undo may fix it.");
            break;
        case SolveResult::REDUNDANT_OKAY:
            p.kind              = Problem::Kind::REDUNDANT;
            p.title             = _("redundant constraints");
            p.detail            = _("Remove any one of these to fix it, or allow redundant "
                                    "constraints in this group.");
            p.canAllowRedundant = true;
            break;
        case SolveResult::TOO_MANY_UNKNOWNS:
            p.kind   = Problem::Kind::TOO_MANY_UNKNOWNS;
            p.title  = _("too many unknowns in a single group");
            p.detail = _("Split the sketch into more groups.");
            break;
        default: return;
    }
    for(hConstraint hc : g->solved.remove) {
        if(SK.constraint.FindByIdNoOops(hc)) p.culprits.push_back(hc);
    }
    out->push_back(p);
}

static void AddContourProblem(Group *g, std::vector<Problem> *out) {
    // As the outliner's warning: a broken sketch, or a solid made from one.
    Group *sketch = g;
    if(g->type == Group::Type::EXTRUDE || g->type == Group::Type::LATHE) {
        sketch = SK.GetGroup(g->opA);
    } else if(g->type != Group::Type::DRAWING_WORKPLANE) {
        return;
    }
    if(!SS.checkClosedContour || sketch->polyError.how == PolyError::GOOD) return;
    Problem p;
    p.group     = g->h;
    p.groupName = g->name;
    p.kind      = Problem::Kind::CONTOUR;
    switch(sketch->polyError.how) {
        case PolyError::NOT_CLOSED:        p.title = _("not closed contour, or not all same style"); break;
        case PolyError::NOT_COPLANAR:      p.title = _("points not all coplanar"); break;
        case PolyError::SELF_INTERSECTING: p.title = _("contour is self-intersecting"); break;
        case PolyError::ZERO_LEN_EDGE:     p.title = _("zero-length edge"); break;
        case PolyError::GOOD:              break;
    }
    p.detail = sketch == g ? _("The sketch shows where.")
                           : ssprintf(_("In %s."), sketch->DescriptionString().c_str());
    out->push_back(p);
}

std::vector<Problem> BuildProblems() {
    std::vector<Problem> out;
    for(hGroup hg : SK.groupOrder) {
        Group *g = SK.GetGroup(hg);
        AddSolveProblem(g, &out);
        AddContourProblem(g, &out);
        if(g->booleanFailed) {
            Problem p;
            p.group          = hg;
            p.groupName      = g->name;
            p.kind           = Problem::Kind::BOOLEAN;
            p.title          = _("the Boolean operation failed");
            p.detail         = _("Forcing NURBS surfaces to a triangle mesh may fix it.");
            p.canForceToMesh = !g->forceToMesh;
            out.push_back(p);
        }
        if(!g->featureProblem.empty()) {
            Problem p;
            p.group     = hg;
            p.groupName = g->name;
            p.kind      = Problem::Kind::FEATURE;
            p.title     = g->featureProblem;
            out.push_back(p);
        }
        if(g->curvedEdges > 0 && (g->type == Group::Type::FILLET || g->type == Group::Type::CHAMFER)) {
            Problem p;
            p.group     = hg;
            p.groupName = g->name;
            p.kind      = Problem::Kind::NOTE;
            p.title     = ssprintf(g->curvedEdges == 1 ? _("%d edge has no lines to constrain")
                                                       : _("%d edges have no lines to constrain"),
                                   g->curvedEdges);
            p.detail    = _("It is curved, or a face beside it is: what it makes has no closed "
                            "form, so it can be snapped to but not constrained.");
            out.push_back(p);
        }
        if(!g->edgeMarks.empty() &&
           (g->type == Group::Type::FILLET || g->type == Group::Type::CHAMFER)) {
            int n = (int)g->edgeMarks.size();
            Problem p;
            p.group     = hg;
            p.groupName = g->name;
            p.kind      = Problem::Kind::NOTE;
            p.title     = ssprintf(n == 1 ? _("%d edge is held by where it is")
                                          : _("%d edges are held by where they are"), n);
            p.detail    = _("No sketch or feature line describes it, so it is found again by "
                            "its place: if the model changes there it may be lost, and nothing "
                            "can be constrained to it.");
            out.push_back(p);
        }
        if(hg == SS.GW.activeGroup) break;
    }
    return out;
}

}
}
