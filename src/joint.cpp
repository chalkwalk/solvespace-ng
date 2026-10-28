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

bool Joint::CentreOf(hEntity he, Vector *p) {
    Entity *e = SK.entity.FindByIdNoOops(he);
    if(e == nullptr) return false;
    if(e->IsPoint()) {
        *p = e->PointGetNum();
    } else if(e->IsCircle() || e->type == Entity::Type::ARC_OF_CIRCLE) {
        *p = SK.GetEntity(e->point[0])->PointGetNum();
    } else {
        return false;
    }
    return true;
}

int Joint::Motions() const {
    switch(type) {
        case Type::CYLINDRICAL: return 2;
        case Type::PLANAR:
        case Type::BALL: return 3;
        case Type::GEAR: return 0;
        default: return 1;
    }
}

bool Joint::IsAngle(int dof) const {
    switch(type) {
        case Type::REVOLUTE:
        case Type::BALL: return true;
        case Type::CYLINDRICAL: return dof == 0;
        case Type::PLANAR: return dof == 2;
        default: return false;
    }
}

double Joint::Clamp(double v, int dof) const {
    return limited[dof] ? std::max(lo[dof], std::min(hi[dof], v)) : v;
}

Pose Joint::Motion() const {
    Pose   m;
    Vector p, d;
    if(type == Type::BALL) {
        if(!CentreOf(axisA, &p)) return m;
        // About x, then y, then z of the parent's frame.
        Quaternion qx = Quaternion::From(Vector::From(1, 0, 0), value[0] * PI / 180),
                   qy = Quaternion::From(Vector::From(0, 1, 0), value[1] * PI / 180),
                   qz = Quaternion::From(Vector::From(0, 0, 1), value[2] * PI / 180);
        m.q = qz.Times(qy.Times(qx));
        m.t = p.Minus(m.q.Rotate(p));
        return m;
    }
    if(type == Type::GEAR || !AxisOf(axisA, &p, &d)) return m;
    double turn = type == Type::REVOLUTE || type == Type::CYLINDRICAL ? value[0]
                  : type == Type::PLANAR                              ? value[2]
                                                                      : 0;
    if(turn != 0) {
        m.q = Quaternion::From(d, turn * PI / 180);
        m.t = p.Minus(m.q.Rotate(p));
    }
    switch(type) {
        case Type::SLIDER: m.t = m.t.Plus(d.ScaledBy(value[0])); break;
        case Type::CYLINDRICAL: m.t = m.t.Plus(d.ScaledBy(value[1])); break;
        case Type::PLANAR:
            m.t = m.t.Plus(d.Normal(0).ScaledBy(value[0])).Plus(d.Normal(1).ScaledBy(value[1]));
            break;
        default: break;
    }
    return m;
}

static Joint *ParentJoint(hPart hp) {
    for(Joint &j : SK.joint) {
        if(j.type != Joint::Type::GEAR && j.child == hp) return &j;
    }
    return nullptr;
}

// The joints coupled to hj's motion 0 by gears, hj first, each with its factor: its motion 0
// is the factor times hj's.
static std::vector<std::pair<hJoint, double>> Coupled(hJoint hj) {
    std::vector<std::pair<hJoint, double>> out = { { hj, 1 } };
    for(size_t i = 0; i < out.size(); i++) {
        for(const Joint &g : SK.joint) {
            if(g.type != Joint::Type::GEAR) continue;
            hJoint other;
            double k;
            if(g.driver == out[i].first) {
                other = g.driven;
                k     = g.ratio;
            } else if(g.driven == out[i].first && g.ratio != 0) {
                other = g.driver;
                k     = 1 / g.ratio;
            } else {
                continue;
            }
            if(SK.joint.FindByIdNoOops(other) == nullptr) continue;
            bool seen = false;
            for(const auto &c : out) seen = seen || c.first == other;
            if(!seen) out.push_back({ other, out[i].second * k });
        }
    }
    return out;
}

// A gear or rack coupling between the joints moving the two picked entities' parts.
static hJoint MakeCoupling(const std::vector<hEntity> &picked) {
    hJoint joint[2] = {};
    for(size_t i = 0; i < 2 && picked.size() == 2; i++) {
        Joint *j = ParentJoint(SK.GetGroup(SK.GetEntity(picked[i])->group)->PartOf());
        if(j && (j->type == Joint::Type::REVOLUTE || j->type == Joint::Type::SLIDER ||
                 j->type == Joint::Type::CYLINDRICAL)) {
            joint[i] = j->h;
        }
    }
    if(!joint[0].v || !joint[1].v) {
        Error(_("Pick something on each of two parts that revolute, slider or cylindrical "
                "joints move: the gears' pitch circles, or a pinion's and its rack."));
        return {};
    }
    if(joint[0] == joint[1]) {
        Error(_("The two picks are moved by the same joint."));
        return {};
    }
    for(const auto &c : Coupled(joint[0])) {
        if(c.first == joint[1]) {
            Error(_("Those two joints are coupled already."));
            return {};
        }
    }
    // The ratio from the pitch circles: meshed gears turn opposite, a rack moves its
    // pinion's circumference per turn.
    Joint  *ja = SK.GetJoint(joint[0]), *jb = SK.GetJoint(joint[1]);
    Entity *ea = SK.GetEntity(picked[0]), *eb = SK.GetEntity(picked[1]);
    bool    ca = ea->IsCircle() || ea->type == Entity::Type::ARC_OF_CIRCLE;
    bool    cb = eb->IsCircle() || eb->type == Entity::Type::ARC_OF_CIRCLE;
    double  ratio = 1;
    if(ja->IsAngle(0) && jb->IsAngle(0) && ca && cb) {
        ratio = -ea->CircleGetRadiusNum() / eb->CircleGetRadiusNum();
    } else if(ja->IsAngle(0) && !jb->IsAngle(0) && ca) {
        ratio = ea->CircleGetRadiusNum() * PI / 180;
    } else if(!ja->IsAngle(0) && jb->IsAngle(0) && cb) {
        ratio = 180 / (PI * eb->CircleGetRadiusNum());
    }
    SS.UndoRemember();
    int n = 1;
    for(const Joint &j : SK.joint) n += j.type == Joint::Type::GEAR;
    Joint g  = {};
    g.type   = Joint::Type::GEAR;
    g.name   = ssprintf("Gear %d", n);
    g.driver = joint[0];
    g.driven = joint[1];
    g.axisA  = picked[0];
    g.axisB  = picked[1];
    g.ratio  = ratio;
    SK.joint.AddAndAssignId(&g);
    // The driven joint falls into step with its driver.
    SS.SetJointValue(g.driver, SK.GetJoint(g.driver)->value[0], /*remember=*/false);
    SS.GW.ClearSelection();
    SS.GW.Invalidate();
    SS.ScheduleShowTW();
    return g.h;
}

hJoint SolveSpaceUI::MakeJoint(Joint::Type type) {
    std::vector<hEntity> picked;
    for(const GraphicsWindow::Selection &s : GW.selection) {
        if(s.entity.v) picked.push_back(s.entity);
    }
    if(type == Joint::Type::GEAR) return MakeCoupling(picked);
    bool   ball = type == Joint::Type::BALL;
    Vector pa, da, pb, db;
    if(ball) {
        if(picked.size() != 2 || !Joint::CentreOf(picked[0], &pa) ||
           !Joint::CentreOf(picked[1], &pb)) {
            Error(_("Select the ball joint's centre on each part: points, circles or arcs."));
            return {};
        }
    } else if(picked.size() != 2 || !Joint::AxisOf(picked[0], &pa, &da) ||
              !Joint::AxisOf(picked[1], &pb, &db)) {
        Error(type == Joint::Type::PLANAR
                  ? _("Select the joint's plane on each part: faces, or lines along its "
                      "normal.")
                  : _("Select two axes for the joint, one on each part: line segments, "
                      "circles, arcs or faces."));
        return {};
    }
    hPart part[2];
    for(int i = 0; i < 2; i++) part[i] = SK.GetGroup(SK.GetEntity(picked[i])->group)->PartOf();
    if(part[0] == part[1]) {
        Error(_("The two picks are on the same part; a joint moves one part against "
                "another."));
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
    if(ball) {
        if(!pa.Equals(pb, 1e-4)) {
            Error(_("The two centres aren't the same point; a ball joint turns about one "
                    "centre."));
            return {};
        }
    } else if(da.Cross(db).Magnitude() > 1e-6) {
        Error(type == Joint::Type::PLANAR ? _("The two planes aren't parallel.")
                                          : _("The two axes aren't parallel."));
        return {};
    }
    if((type == Joint::Type::REVOLUTE || type == Joint::Type::CYLINDRICAL) &&
       pb.Minus(pa).Cross(da).Magnitude() > 1e-4) {
        Error(_("The two axes aren't the same line; the joint turns about one axis."));
        return {};
    }
    if(ParentJoint(part[1]) != nullptr) {
        Error(_("Part '%s' already has a joint to move it."), PartName(part[1]).c_str());
        return {};
    }
    UndoRemember();
    int n = 1;
    for(const Joint &j : SK.joint) n += j.type == type;
    const char *kind = "Revolute";
    switch(type) {
        case Joint::Type::SLIDER: kind = "Slider"; break;
        case Joint::Type::CYLINDRICAL: kind = "Cylindrical"; break;
        case Joint::Type::PLANAR: kind = "Planar"; break;
        case Joint::Type::BALL: kind = "Ball"; break;
        default: break;
    }
    Joint j  = {};
    j.type   = type;
    j.name   = ssprintf("%s %d", kind, n);
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

void SolveSpaceUI::SetJointValue(hJoint hj, double value, bool remember, int dof) {
    Joint *j = SK.joint.FindByIdNoOops(hj);
    if(j == nullptr || dof < 0 || dof >= j->Motions()) return;
    // Motion 0 moves the joints geared to it too: within every one's limits.
    std::vector<std::pair<hJoint, double>> moved = { { hj, 1 } };
    if(dof == 0) moved = Coupled(hj);
    double lo = VERY_NEGATIVE, hi = VERY_POSITIVE;
    for(const auto &[h, k] : moved) {
        Joint *c = SK.GetJoint(h);
        if(!c->limited[dof] || k == 0) continue;
        double a = c->lo[dof] / k, b = c->hi[dof] / k;
        lo = std::max(lo, std::min(a, b));
        hi = std::min(hi, std::max(a, b));
    }
    if(lo > hi) return; // no value suits them all
    value       = std::max(lo, std::min(hi, value));
    bool change = false;
    for(const auto &[h, k] : moved) change = change || SK.GetJoint(h)->value[dof] != k * value;
    if(!change) return;
    if(remember) UndoRemember();
    for(const auto &[h, k] : moved) SK.GetJoint(h)->value[dof] = k * value;
    unsaved = true;
    GW.Invalidate();
    ScheduleShowTW();
}

void SolveSpaceUI::SetJointRatio(hJoint hj, double ratio) {
    Joint *g = SK.joint.FindByIdNoOops(hj);
    if(g == nullptr || g->type != Joint::Type::GEAR || ratio == 0 || ratio == g->ratio) return;
    UndoRemember();
    g        = SK.GetJoint(hj);
    g->ratio = ratio;
    if(Joint *d = SK.joint.FindByIdNoOops(g->driver)) {
        SetJointValue(d->h, d->value[0], /*remember=*/false);
    }
    unsaved = true;
}

void SolveSpaceUI::SetJointLimits(hJoint hj, bool limited, double lo, double hi, int dof) {
    Joint *j = SK.joint.FindByIdNoOops(hj);
    if(j == nullptr || dof < 0 || dof >= j->Motions()) return;
    if(limited && lo > hi) std::swap(lo, hi);
    UndoRemember();
    j               = SK.GetJoint(hj);
    j->limited[dof] = limited;
    j->lo[dof]      = lo;
    j->hi[dof]      = hi;
    j->value[dof]   = j->Clamp(j->value[dof], dof);
    GW.Invalidate();
    ScheduleShowTW();
}

void SolveSpaceUI::DeleteJoint(hJoint hj) {
    if(SK.joint.FindByIdNoOops(hj) == nullptr) return;
    UndoRemember();
    SK.joint.RemoveById(hj);
    PruneJoints(); // its couplings
    GW.Invalidate();
    ScheduleShowTW();
}

bool SolveSpaceUI::RenameJoint(hJoint hj, const std::string &name) {
    if(name.empty() || SK.joint.FindByIdNoOops(hj) == nullptr) return false;
    UndoRemember();
    SK.GetJoint(hj)->name = name;
    return true;
}

bool SolveSpaceUI::ParseJointValue(hJoint hj, const std::string &text, double *value,
                                   int dof) {
    Joint *j = SK.joint.FindByIdNoOops(hj);
    Expr  *e = Expr::From(text, /*popUpError=*/true);
    if(j == nullptr || e == nullptr) return false;
    *value = j->IsAngle(dof) ? e->Eval() : ExprToMm(e);
    return true;
}

std::string SolveSpaceUI::JointValueString(hJoint hj, double value, int dof) {
    Joint *j = SK.joint.FindByIdNoOops(hj);
    if(j && j->IsAngle(dof)) return ssprintf("%.2f", value);
    return MmToString(value);
}

void SolveSpaceUI::DesignPose() {
    bool moved = false;
    for(const Joint &j : SK.joint) {
        for(double v : j.value) moved = moved || v != 0;
    }
    if(!moved) return;
    UndoRemember();
    for(Joint &j : SK.joint) {
        for(double &v : j.value) v = 0;
    }
    GW.Invalidate();
    ScheduleShowTW();
}

void SolveSpaceUI::PruneJoints() {
    SK.joint.ClearTags();
    bool any = false;
    for(Joint &j : SK.joint) {
        bool gone = !SK.entity.FindByIdNoOops(j.axisA) || !SK.entity.FindByIdNoOops(j.axisB);
        if(j.type == Joint::Type::GEAR) {
            // A coupling goes with either joint (removed now, or below).
            for(hJoint h : { j.driver, j.driven }) {
                Joint *c = SK.joint.FindByIdNoOops(h);
                if(c == nullptr || !SK.entity.FindByIdNoOops(c->axisA) ||
                   !SK.entity.FindByIdNoOops(c->axisB)) {
                    gone = true;
                }
            }
        }
        for(hPart hp : { j.parent, j.child }) {
            if(j.type != Joint::Type::GEAR && hp != Part::FIRST && !SK.part.FindByIdNoOops(hp)) {
                gone = true;
            }
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

void SolveSpaceUI::PlayJoint(hJoint hj, int dof, int64_t now) {
    Joint *j = SK.joint.FindByIdNoOops(hj);
    if(j == nullptr || dof < 0 || dof >= j->Motions()) return;
    UndoRemember();
    jointPlay = { hj, dof, now, j->value[dof], j->value[dof] };
    if(!jointPlayTimer) jointPlayTimer = Platform::CreateTimer();
    jointPlayTimer->onTimeout = [this] {
        StepJointAnimation(GetMilliseconds());
        if(jointPlay.joint.v != 0) jointPlayTimer->RunAfterNextFrame();
    };
    jointPlayTimer->RunAfterNextFrame();
    ScheduleShowTW();
}

void SolveSpaceUI::StopJoint() {
    jointPlay = {};
    ScheduleShowTW();
}

bool SolveSpaceUI::PlayingJoint(hJoint hj, int dof) {
    return jointPlay.joint.v != 0 && jointPlay.joint == hj && jointPlay.dof == dof;
}

void SolveSpaceUI::StepJointAnimation(int64_t now) {
    if(jointPlay.joint.v == 0) return;
    int    dof = jointPlay.dof;
    Joint *j   = SK.joint.FindByIdNoOops(jointPlay.joint);
    // Gone, or moved by something else (typed, scrubbed, undone): stop.
    if(j == nullptr || j->value[dof] != jointPlay.last) {
        StopJoint();
        return;
    }
    double t = (now - jointPlay.t0) / 1000.0, v;
    if(j->IsAngle(dof) && !j->limited[dof]) {
        v = fmod(jointPlay.v0 + 90 * t + 180, 360);
        if(v < 0) v += 360;
        v -= 180;
    } else {
        // As the scrub slider's range.
        double lo = j->limited[dof] ? j->lo[dof] : (j->IsAngle(dof) ? -180 : -50);
        double hi = j->limited[dof] ? j->hi[dof] : (j->IsAngle(dof) ? 180 : 50);
        double span = hi - lo;
        if(span <= 0) {
            StopJoint();
            return;
        }
        // Up from where it was, reflected at each end; from the top it starts down.
        double x = fmod(std::max(0.0, jointPlay.v0 - lo) + span / 2 * t, 2 * span);
        v        = lo + (x <= span ? x : 2 * span - x);
    }
    SetJointValue(jointPlay.joint, v, /*remember=*/false, dof);
    jointPlay.last = SK.GetJoint(jointPlay.joint)->value[dof];
}

SolveSpaceUI::ClearanceSweep SolveSpaceUI::SweepClearance(hJoint hj, int samples, int dof) {
    ClearanceSweep sweep;
    FlushRegeneration();
    Joint *j = SK.joint.FindByIdNoOops(hj);
    if(j == nullptr || dof < 0 || dof >= j->Motions()) return sweep;
    bool   revolute = j->IsAngle(dof), limited = j->limited[dof];
    double lo = j->lo[dof], hi = j->hi[dof];
    if(!limited) {
        if(!revolute) {
            sweep.text = _("Set the slide's limits first: the sweep runs between them.");
            return sweep;
        }
        lo = 0;
        hi = 360;
    }
    // Motion 0 turns the joints geared to it too.
    std::vector<std::pair<hJoint, double>> coupled = { { hj, 1 } };
    if(dof == 0) coupled = Coupled(hj);
    std::vector<Group *> moving, still;
    for(hPart hp : PartOrder()) {
        Group *g = PartSolid(hp);
        if(g == nullptr || !g->runningShape) continue;
        bool moves = false;
        for(const auto &c : coupled) moves = moves || MovedBy(*SK.GetJoint(c.first), hp);
        (moves ? moving : still).push_back(g);
    }
    if(moving.empty() || still.empty()) {
        sweep.text = _("The joint moves no solid past another; there is nothing to sweep.");
        return sweep;
    }
    std::vector<double> kept;
    for(const auto &c : coupled) kept.push_back(SK.GetJoint(c.first)->value[dof]);
    auto restore = [&] {
        for(size_t i = 0; i < coupled.size(); i++) {
            SK.GetJoint(coupled[i].first)->value[dof] = kept[i];
        }
    };
    // The smallest distance between the moving and the still parts at a value.
    auto gapAt = [&](double v, double *gap) {
        for(const auto &[h, k] : coupled) SK.GetJoint(h)->value[dof] = k * v;
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
    double step = (hi - lo) / (revolute && !limited ? n : n - 1);
    double best = VERY_POSITIVE, bestAt = lo;
    bool   inside = false;
    for(int i = 0; i < n; i++) {
        double v = lo + step * i, d;
        if(!gapAt(v, &d)) {
            restore();
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
        if(limited) {
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
    restore();

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
