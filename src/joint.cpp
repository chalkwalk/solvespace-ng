//-----------------------------------------------------------------------------
// Motion joints (AGENTS.md section 7, M5a): each turns or slides its child part relative to its
// parent, about or along an axis both share in the design pose. A part's pose is its joints'
// motions composed from the first part; parts move as a mesh transform, never rebuilt.
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {

bool Joint::AxisOf(hEntity he, Vector *p, Vector *d) {
    Entity *e = SK.entity.FindByIdNoOops(he);
    if(e == nullptr) return false;
    if(e->type == Entity::Type::LINE_SEGMENT) {
        *p = SK.GetEntity(e->point[0])->PointGetNum();
        *d = SK.GetEntity(e->point[1])->PointGetNum().Minus(*p);
    } else if(e->IsCircle() || e->type == Entity::Type::ARC_OF_CIRCLE) {
        *p = SK.GetEntity(e->point[0])->PointGetNum();
        *d = e->Normal()->NormalN();
    } else if(e->IsFace()) {
        *p = e->FaceGetPointNum();
        *d = e->FaceGetNormalNum();
    } else {
        return false;
    }
    if(d->Magnitude() < LENGTH_EPS) return false;
    *d = d->WithMagnitude(1);
    return true;
}

double Joint::Clamp(double v) const {
    return limited ? std::max(lo, std::min(hi, v)) : v;
}

Pose Joint::Motion() const {
    Pose   m;
    Vector p, d;
    if(!AxisOf(axisA, &p, &d)) return m;
    if(type == Type::REVOLUTE) {
        m.q = Quaternion::From(d, value * PI / 180);
        m.t = p.Minus(m.q.Rotate(p));
    } else {
        m.t = d.ScaledBy(value);
    }
    return m;
}

static Joint *ParentJoint(hPart hp) {
    for(Joint &j : SK.joint) {
        if(j.child == hp) return &j;
    }
    return nullptr;
}

hJoint SolveSpaceUI::MakeJoint(Joint::Type type) {
    std::vector<hEntity> picked;
    for(const GraphicsWindow::Selection &s : GW.selection) {
        if(s.entity.v) picked.push_back(s.entity);
    }
    Vector pa, da, pb, db;
    if(picked.size() != 2 || !Joint::AxisOf(picked[0], &pa, &da) ||
       !Joint::AxisOf(picked[1], &pb, &db)) {
        Error(_("Select two axes for the joint, one on each part: line segments, circles, "
                "arcs or faces."));
        return {};
    }
    hPart part[2];
    for(int i = 0; i < 2; i++) part[i] = SK.GetGroup(SK.GetEntity(picked[i])->group)->PartOf();
    if(part[0] == part[1]) {
        Error(_("The two axes are on the same part; a joint moves one part against another."));
        return {};
    }
    // The parent is the part earlier in part order.
    std::vector<hPart> order = PartOrder();
    int ia = (int)(std::find(order.begin(), order.end(), part[0]) - order.begin());
    int ib = (int)(std::find(order.begin(), order.end(), part[1]) - order.begin());
    if(ib < ia) {
        std::swap(picked[0], picked[1]);
        std::swap(part[0], part[1]);
        std::swap(pa, pb);
        std::swap(da, db);
    }
    if(da.Cross(db).Magnitude() > 1e-6) {
        Error(_("The two axes aren't parallel."));
        return {};
    }
    if(type == Joint::Type::REVOLUTE && pb.Minus(pa).Cross(da).Magnitude() > 1e-4) {
        Error(_("The two axes aren't the same line; a revolute joint turns about one axis."));
        return {};
    }
    if(ParentJoint(part[1]) != nullptr) {
        Error(_("Part '%s' already has a joint to move it."), PartName(part[1]).c_str());
        return {};
    }
    UndoRemember();
    int n = 1;
    for(const Joint &j : SK.joint) n += j.type == type;
    Joint j  = {};
    j.type   = type;
    j.name   = ssprintf(type == Joint::Type::REVOLUTE ? "Revolute %d" : "Slider %d", n);
    j.parent = part[0];
    j.child  = part[1];
    j.axisA  = picked[0];
    j.axisB  = picked[1];
    SK.joint.AddAndAssignId(&j);
    GW.ClearSelection();
    GW.Invalidate();
    ScheduleShowTW();
    return j.h;
}

Pose SolveSpaceUI::PartPose(hPart hp) {
    Pose pose;
    // Up the tree to the first part: each joint's motion after its parent's pose.
    for(int depth = 0; depth <= SK.joint.n; depth++) {
        Joint *j = ParentJoint(hp);
        if(j == nullptr) break;
        pose = j->Motion().After(pose);
        hp   = j->parent;
    }
    return pose;
}

void SolveSpaceUI::SetJointValue(hJoint hj, double value, bool remember) {
    Joint *j = SK.joint.FindByIdNoOops(hj);
    if(j == nullptr) return;
    value = j->Clamp(value);
    if(value == j->value) return;
    if(remember) {
        UndoRemember();
        j = SK.GetJoint(hj);
    }
    j->value = value;
    unsaved  = true;
    GW.Invalidate();
    ScheduleShowTW();
}

void SolveSpaceUI::SetJointLimits(hJoint hj, bool limited, double lo, double hi) {
    if(SK.joint.FindByIdNoOops(hj) == nullptr) return;
    if(limited && lo > hi) std::swap(lo, hi);
    UndoRemember();
    Joint *j   = SK.GetJoint(hj);
    j->limited = limited;
    j->lo      = lo;
    j->hi      = hi;
    j->value   = j->Clamp(j->value);
    GW.Invalidate();
    ScheduleShowTW();
}

void SolveSpaceUI::DeleteJoint(hJoint hj) {
    if(SK.joint.FindByIdNoOops(hj) == nullptr) return;
    UndoRemember();
    SK.joint.RemoveById(hj);
    GW.Invalidate();
    ScheduleShowTW();
}

bool SolveSpaceUI::RenameJoint(hJoint hj, const std::string &name) {
    if(name.empty() || SK.joint.FindByIdNoOops(hj) == nullptr) return false;
    UndoRemember();
    SK.GetJoint(hj)->name = name;
    return true;
}

bool SolveSpaceUI::ParseJointValue(hJoint hj, const std::string &text, double *value) {
    Joint *j = SK.joint.FindByIdNoOops(hj);
    Expr  *e = Expr::From(text, /*popUpError=*/true);
    if(j == nullptr || e == nullptr) return false;
    *value = j->type == Joint::Type::REVOLUTE ? e->Eval() : ExprToMm(e);
    return true;
}

std::string SolveSpaceUI::JointValueString(hJoint hj, double value) {
    Joint *j = SK.joint.FindByIdNoOops(hj);
    if(j && j->type == Joint::Type::REVOLUTE) return ssprintf("%.2f", value);
    return MmToString(value);
}

void SolveSpaceUI::DesignPose() {
    bool moved = false;
    for(const Joint &j : SK.joint) moved = moved || j.value != 0;
    if(!moved) return;
    UndoRemember();
    for(Joint &j : SK.joint) j.value = 0;
    GW.Invalidate();
    ScheduleShowTW();
}

void SolveSpaceUI::PruneJoints() {
    SK.joint.ClearTags();
    bool any = false;
    for(Joint &j : SK.joint) {
        bool gone = !SK.entity.FindByIdNoOops(j.axisA) || !SK.entity.FindByIdNoOops(j.axisB);
        for(hPart hp : { j.parent, j.child }) {
            if(hp != Part::FIRST && !SK.part.FindByIdNoOops(hp)) gone = true;
        }
        if(gone) {
            j.tag = 1;
            any   = true;
        }
    }
    if(any) SK.joint.RemoveTagged();
}

// The parts a joint moves: its child and the parts under it.
static bool MovedBy(const Joint &j, hPart hp) {
    for(int depth = 0; depth <= SK.joint.n; depth++) {
        if(hp == j.child) return true;
        Joint *up = ParentJoint(hp);
        if(up == nullptr) return false;
        hp = up->parent;
    }
    return false;
}

SolveSpaceUI::ClearanceSweep SolveSpaceUI::SweepClearance(hJoint hj, int samples) {
    ClearanceSweep sweep;
    FlushRegeneration();
    Joint *j = SK.joint.FindByIdNoOops(hj);
    if(j == nullptr) return sweep;
    bool   revolute = j->type == Joint::Type::REVOLUTE;
    double lo = j->lo, hi = j->hi;
    if(!j->limited) {
        if(!revolute) {
            sweep.text = _("Set the slider's limits first: the sweep runs between them.");
            return sweep;
        }
        lo = 0;
        hi = 360;
    }
    std::vector<Group *> moving, still;
    for(hPart hp : PartOrder()) {
        Group *g = PartSolid(hp);
        if(g == nullptr || !g->runningShape) continue;
        (MovedBy(*j, hp) ? moving : still).push_back(g);
    }
    if(moving.empty() || still.empty()) {
        sweep.text = _("The joint moves no solid past another; there is nothing to sweep.");
        return sweep;
    }
    double kept = j->value;
    // The smallest distance between the moving and the still parts at a value.
    auto gapAt = [&](double v, double *gap) {
        SK.GetJoint(hj)->value = v;
        *gap = VERY_POSITIVE;
        for(Group *m : moving) {
            Pose pm = PartPose(m->PartOf());
            for(Group *s : still) {
                Pose   ps = PartPose(s->PartOf());
                double d;
                if(!KernelMinDistance(m->runningShape, pm.q, pm.t, s->runningShape, ps.q, ps.t,
                                      &d)) {
                    return false;
                }
                *gap = std::min(*gap, d);
            }
        }
        return true;
    };
    const double touch = 1e-6;
    int    n    = std::max(2, samples);
    double step = (hi - lo) / (revolute && !j->limited ? n : n - 1);
    double best = VERY_POSITIVE, bestAt = lo;
    bool   inside = false;
    for(int i = 0; i < n; i++) {
        double v = lo + step * i, d;
        if(!gapAt(v, &d)) {
            SK.GetJoint(hj)->value = kept;
            sweep.text = _("The clearance sweep needs the Open CASCADE kernel's solids.");
            return sweep;
        }
        if(d < best) {
            best   = d;
            bestAt = v;
        }
        if(d < touch) {
            if(!inside) sweep.collisions.push_back({ v, v });
            sweep.collisions.back().second = v;
        }
        inside = d < touch;
    }
    // Refine the smallest gap between its neighbouring samples (golden section).
    if(best >= touch) {
        double a = bestAt - step, b = bestAt + step;
        if(j->limited) {
            a = std::max(a, lo);
            b = std::min(b, hi);
        }
        const double r = (sqrt(5.0) - 1) / 2;
        double c = b - r * (b - a), e = a + r * (b - a), fc, fe;
        gapAt(c, &fc);
        gapAt(e, &fe);
        for(int k = 0; k < 30 && b - a > 1e-6; k++) {
            if(fc < fe) {
                b = e, e = c, fe = fc;
                c = b - r * (b - a);
                gapAt(c, &fc);
            } else {
                a = c, c = e, fc = fe;
                e = a + r * (b - a);
                gapAt(e, &fe);
            }
        }
        double v = fc < fe ? c : e, d = std::min(fc, fe);
        if(d < best) {
            best   = d;
            bestAt = v;
        }
    }
    SK.GetJoint(hj)->value = kept;

    auto valueText = [&](double v) {
        if(revolute) return ssprintf("%.1f°", v);
        return MmToString(v) + " mm";
    };
    sweep.ok  = true;
    sweep.gap = best;
    sweep.at  = bestAt;
    if(sweep.collisions.empty()) {
        sweep.text = ssprintf(_("Minimum gap %s mm at %s."), MmToString(best).c_str(),
                              valueText(bestAt).c_str());
    } else {
        sweep.text = _("The parts touch or overlap");
        for(size_t i = 0; i < sweep.collisions.size(); i++) {
            const auto &c = sweep.collisions[i];
            sweep.text += i == 0 ? " " : ", ";
            sweep.text += c.first == c.second
                              ? ssprintf(_("at %s"), valueText(c.first).c_str())
                              : ssprintf(_("from %s to %s"), valueText(c.first).c_str(),
                                         valueText(c.second).c_str());
        }
        sweep.text += ".";
    }
    return sweep;
}

} // namespace SolveSpace
