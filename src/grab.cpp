//-----------------------------------------------------------------------------
// G: grab the selection and move it under the solver without holding a mouse button, with
// X/Y/Z axis locks and a typed distance. Confirm with a click or Enter; Esc or right-click
// cancels and restores the sketch exactly.
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {

Vector GraphicsWindow::GrabAxisVector(int axis) {
    if(LockedInWorkplane()) {
        EntityBase *n = SK.GetEntity(ActiveWorkplane())->Normal();
        switch(axis) {
            case 0: return n->NormalU();
            case 1: return n->NormalV();
            case 2: return n->NormalN();
        }
    }
    switch(axis) {
        case 0: return Vector::From(1, 0, 0);
        case 1: return Vector::From(0, 1, 0);
        case 2: return Vector::From(0, 0, 1);
    }
    ssassert(false, "Unexpected grab axis");
}

bool GraphicsWindow::StartGrab(Point2d mouse) {
    if(pending.operation != Pending::NONE) return false;
    // Grabbing what is under the mouse, like a mouse drag, leaves nothing selected after.
    grab.byHover = selection.IsEmpty();
    SelectHoveredIfNothingSelected();
    pending.points.Clear();
    // A face of a part a joint moves: the grab turns or slides the joint (M5a), rather than
    // dragging the face (which would change the solid).
    grab.joint = {};
    for(const Selection &s : selection) {
        Entity *e = s.entity.v ? SK.entity.FindByIdNoOops(s.entity) : nullptr;
        if(e == nullptr || !e->IsFace()) continue;
        hPart hp = SK.GetGroup(e->group)->PartOf();
        for(const Joint &j : SK.joint) {
            if(j.child == hp) grab.joint = j.h;
        }
        if(grab.joint.v) break;
    }
    // What a mouse drag could move: draggable entities in the active group or earlier ones.
    int activeOrder = SK.GetGroup(activeGroup)->order;
    for(const Selection &s : selection) {
        if(!s.entity.v || grab.joint.v) continue;
        Entity *e = SK.GetEntity(s.entity);
        if(e->type == Entity::Type::WORKPLANE || !e->CanBeDragged()) continue;
        if(SK.GetGroup(e->group)->order > activeOrder) continue;
        StartDraggingByEntity(s.entity);
    }
    if(pending.points.n == 0 && !grab.joint.v) {
        if(grab.byHover) ClearSelection();
        return false;
    }

    grab.wasUnsaved = SS.unsaved;
    // Like UndoRemember, but redo survives until the grab is confirmed: a cancelled grab
    // changes nothing.
    SS.PushFromCurrentOnto(&SS.undo);
    SS.UndoEnableMenus();
    grab.undoCnt    = SS.undo.cnt;
    grab.undoWrite  = SS.undo.write;
    grab.startMouse = mouse;
    grab.lastMouse  = mouse;
    grab.baseMouse  = mouse;
    grab.baseDelta  = {};
    grab.freeDelta  = {};
    RecordGrabView();
    grab.axis       = -1;
    grab.typedMm.reset();
    grab.start.clear();
    for(hEntity h : pending.points) grab.start.push_back({ h, SK.GetEntity(h)->PointGetNum() });
    pending.operation   = Pending::GRABBING;
    pending.description = _("grab: move the mouse; X/Y/Z lock an axis; type a distance");
    if(grab.joint.v) {
        Joint *j = SK.GetJoint(grab.joint);
        for(int i = 0; i < Joint::MAX_MOTIONS; i++) grab.jointStart[i] = j->value[i];
        grab.jointDof    = -1;
        grab.jointTurned = 0;
        grab.jointFrom   = {};
        grab.jointRadial = GrabJointRadial(mouse);
        switch(j->type) {
            case Joint::Type::REVOLUTE:
                pending.description = _("grab: turn the joint with the mouse; type an angle");
                break;
            case Joint::Type::SLIDER:
                pending.description = _("grab: slide the joint with the mouse; type a distance");
                break;
            default:
                pending.description =
                    _("grab: move the joint with the mouse; X/Y/Z pick a motion; type a value");
                break;
        }
    }
    return true;
}

int GraphicsWindow::GrabJointDof() {
    Joint *j = SK.joint.FindByIdNoOops(grab.joint);
    if(j == nullptr) return 0;
    if(grab.jointDof >= 0) return grab.jointDof;
    return j->type == Joint::Type::PLANAR || j->type == Joint::Type::BALL ? -1 : 0;
}

bool GraphicsWindow::GrabJointAxis(Vector *p, Vector *d) {
    Joint *j = SK.joint.FindByIdNoOops(grab.joint);
    if(j == nullptr) return false;
    int dof = GrabJointDof();
    if(j->type == Joint::Type::BALL) {
        if(!Joint::CentreOf(j->axisA, p)) return false;
        *d = Vector::From(dof == 0, dof == 1, dof == 2);
    } else {
        if(!Joint::AxisOf(j->axisA, p, d)) return false;
        if(j->type == Joint::Type::PLANAR && (dof == 0 || dof == 1)) *d = d->Normal(dof);
    }
    Pose parent = SS.PartPose(j->parent);
    *p          = parent.Apply(*p);
    *d          = parent.q.Rotate(*d);
    return true;
}

Vector GraphicsWindow::GrabJointRadial(Point2d mouse) {
    Vector p, d;
    if(!GrabJointAxis(&p, &d)) return {};
    // The point under the mouse in the view plane through the axis point, then along the
    // view direction onto the plane across the axis (unless the axis lies in the view).
    Point2d a = ProjectPoint(p);
    Vector  q = p.Plus(projRight.ScaledBy((mouse.x - a.x) / scale))
                 .Plus(projUp.ScaledBy((mouse.y - a.y) / scale));
    Vector  n = projRight.Cross(projUp);
    if(fabs(n.Dot(d)) > 0.05) q = q.Plus(n.ScaledBy(p.Minus(q).Dot(d) / n.Dot(d)));
    Vector r = q.Minus(p);
    return r.Minus(d.ScaledBy(r.Dot(d)));
}

void GraphicsWindow::RecordGrabView() {
    grab.viewScale  = scale;
    grab.viewOffset = offset;
    grab.viewRight  = projRight;
    grab.viewUp     = projUp;
}

void GraphicsWindow::GrabMouseMoved(Point2d mouse) {
    if(pending.operation != Pending::GRABBING) return;
    if(scale != grab.viewScale || !offset.EqualsExactly(grab.viewOffset) ||
       !projRight.EqualsExactly(grab.viewRight) || !projUp.EqualsExactly(grab.viewUp)) {
        grab.baseDelta = grab.freeDelta;
        grab.baseMouse = mouse;
        RecordGrabView();
    }
    grab.freeDelta = grab.baseDelta.Plus(
        projRight.ScaledBy((mouse.x - grab.baseMouse.x) / scale)
            .Plus(projUp.ScaledBy((mouse.y - grab.baseMouse.y) / scale)));
    Vector delta;
    if(grab.axis >= 0 && grab.typedMm) {
        delta = GrabAxisVector(grab.axis).ScaledBy(*grab.typedMm);
    } else {
        delta = grab.freeDelta;
        if(grab.axis >= 0) {
            Vector ax = GrabAxisVector(grab.axis);
            delta     = ax.ScaledBy(delta.Dot(ax));
        }
    }
    if(grab.joint.v) {
        Joint *j = SK.joint.FindByIdNoOops(grab.joint);
        Vector p, d;
        if(j == nullptr || !GrabJointAxis(&p, &d)) return;
        int    dof = GrabJointDof();
        double value[Joint::MAX_MOTIONS];
        for(int i = 0; i < Joint::MAX_MOTIONS; i++) value[i] = grab.jointStart[i];
        Vector moved = grab.freeDelta.Minus(grab.jointFrom);
        Vector n     = projRight.Cross(projUp);
        if(grab.typedMm) {
            value[std::max(dof, 0)] += *grab.typedMm;
        } else if(dof < 0 && j->type == Joint::Type::PLANAR) {
            // In the plane, under the mouse (along the view direction onto it).
            Vector pn;
            Joint::AxisOf(j->axisA, &p, &pn);
            Pose parent = SS.PartPose(j->parent);
            Vector u = parent.q.Rotate(pn.Normal(0)), v = parent.q.Rotate(pn.Normal(1));
            pn       = parent.q.Rotate(pn);
            if(fabs(n.Dot(pn)) > 0.05) moved = moved.Minus(n.ScaledBy(moved.Dot(pn) / n.Dot(pn)));
            value[0] += moved.Dot(u);
            value[1] += moved.Dot(v);
        } else if(dof < 0) {
            // A trackball: the part turns as if the mouse pushed its near side, half a degree
            // a pixel, after its start pose; then back to angles about x, y and z.
            Pose       parent = SS.PartPose(j->parent);
            Vector     axis   = parent.q.Inverse().Rotate(n.Cross(moved));
            double     angle  = moved.Magnitude() * scale * 0.5 * PI / 180;
            Quaternion q      = Quaternion::IDENTITY;
            for(int i = 2; i >= 0; i--) {
                q = q.Times(Quaternion::From(Vector::From(i == 0, i == 1, i == 2),
                                             grab.jointStart[i] * PI / 180));
            }
            if(axis.Magnitude() > LENGTH_EPS) {
                q = Quaternion::From(axis.WithMagnitude(1), angle).Times(q);
            }
            Vector cx = q.Rotate(Vector::From(1, 0, 0)), cy = q.Rotate(Vector::From(0, 1, 0)),
                   cz = q.Rotate(Vector::From(0, 0, 1));
            value[0]  = atan2(cy.z, cz.z) * 180 / PI;
            value[1]  = asin(std::max(-1.0, std::min(1.0, -cx.z))) * 180 / PI;
            value[2]  = atan2(cx.y, cx.x) * 180 / PI;
        } else if(j->IsAngle(dof)) {
            // The angle the mouse has gone round the axis, added up move by move.
            Vector r0 = grab.jointRadial, r1 = GrabJointRadial(mouse);
            if(r0.Magnitude() > LENGTH_EPS && r1.Magnitude() > LENGTH_EPS) {
                grab.jointTurned += atan2(d.Dot(r0.Cross(r1)), r0.Dot(r1)) * 180 / PI;
                grab.jointRadial = r1;
            }
            value[dof] += grab.jointTurned;
        } else {
            // Along the axis as it shows on screen, one to one with the mouse.
            Vector dv = d.Minus(n.ScaledBy(d.Dot(n) / n.MagSquared()));
            if(dv.MagSquared() > 0.05 * 0.05) value[dof] += moved.Dot(dv) / dv.MagSquared();
        }
        for(int i = 0; i < j->Motions(); i++) {
            SS.SetJointValue(grab.joint, value[i], /*remember=*/false, i);
        }
        grab.lastMouse = mouse;
        Invalidate();
        return;
    }
    for(auto &[h, p0] : grab.start) {
        SK.GetEntity(h)->PointForceTo(p0.Plus(delta));
        SS.MarkGroupDirtyByEntity(h);
    }
    grab.lastMouse = mouse;
    Invalidate();
}

void GraphicsWindow::GrabSetAxis(int axis) {
    if(Joint *j = grab.joint.v ? SK.joint.FindByIdNoOops(grab.joint) : nullptr) {
        // A joint's motion: picked again, back to the joint's own. It starts afresh.
        if(axis >= j->Motions()) return;
        grab.jointDof    = grab.jointDof == axis ? -1 : axis;
        grab.jointTurned = 0;
        grab.jointFrom   = grab.freeDelta;
        grab.jointRadial = GrabJointRadial(grab.lastMouse);
        GrabMouseMoved(grab.lastMouse);
        return;
    }
    // Points drawn in a workplane stay in it: locking to its normal would freeze the grab.
    if(axis == 2 && LockedInWorkplane()) return;
    grab.axis = grab.axis == axis ? -1 : axis;
    GrabMouseMoved(grab.lastMouse);
}

void GraphicsWindow::GrabSetTyped(std::optional<double> mm) {
    grab.typedMm = mm;
    GrabMouseMoved(grab.lastMouse);
}

void GraphicsWindow::GrabConfirm() {
    if(pending.operation != Pending::GRABBING) return;
    SS.unsaved = true;
    SS.UndoClearStack(&SS.redo);
    SS.UndoEnableMenus();
    ClearPending();
    grab.start.clear();
    if(grab.byHover) ClearSelection();
    Invalidate();
}

void GraphicsWindow::GrabCancel() {
    if(pending.operation != Pending::GRABBING) return;
    ClearPending();
    grab.start.clear();
    if(grab.byHover) ClearSelection();
    // If something else recorded an undo step meanwhile, undoing would revert that step
    // instead of the grab; keep everything as it is (as a confirm would).
    if(SS.undo.cnt != grab.undoCnt || SS.undo.write != grab.undoWrite) {
        Invalidate();
        return;
    }
    // StartGrab remembered the state just before the grab; return to it, leaving redo alone.
    SS.PopOntoCurrentFrom(&SS.undo);
    SS.UndoEnableMenus();
    SS.unsaved = grab.wasUnsaved;
}

} // namespace SolveSpace
