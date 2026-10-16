//-----------------------------------------------------------------------------
// Typed values while drawing become real constraints: "25 Tab 30 Enter" on a line gives a
// length and an angle constraint, then drawing continues as after a click.
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {

// Where a label goes, as MenuConstrain places it: beside a distance, inside an angle's arc.
static Vector LabelOffset(Constraint::Type type, hEntity ptA, hEntity ptB, hEntity entityA,
                          hEntity entityB) {
    double size = 50 / SS.GW.scale;
    if(type == Constraint::Type::PT_PT_DISTANCE) {
        Vector n = SS.GW.projRight.Cross(SS.GW.projUp);
        Vector d = SK.GetEntity(ptA)->PointGetNum().Minus(SK.GetEntity(ptB)->PointGetNum());
        return n.Cross(d).WithMagnitude(size);
    }
    if(type == Constraint::Type::ANGLE) {
        Vector bisector = SK.GetEntity(entityA)->VectorGetNum().WithMagnitude(1).Plus(
                          SK.GetEntity(entityB)->VectorGetNum().WithMagnitude(1));
        return bisector.WithMagnitude(size);
    }
    return {};
}

static hConstraint AddValued(Constraint::Type type, hEntity ptA, hEntity ptB, hEntity entityA,
                             hEntity entityB, double val) {
    Constraint c = {};
    c.disp.offset = LabelOffset(type, ptA, ptB, entityA, entityB);
    c.group      = SS.GW.activeGroup;
    c.workplane  = SS.GW.ActiveWorkplane();
    c.type       = type;
    c.ptA        = ptA;
    c.ptB        = ptB;
    c.entityA    = entityA;
    c.entityB    = entityB;
    c.valA       = val;
    return Constraint::AddConstraint(&c, /*rememberForUndo=*/false);
}

GraphicsWindow::TypedKind GraphicsWindow::TypedInputKind() {
    switch(pending.operation) {
        case Pending::DRAGGING_NEW_LINE_POINT: return TypedKind::LINE;
        case Pending::DRAGGING_NEW_RADIUS:     return TypedKind::CIRCLE;
        case Pending::DRAGGING_NEW_POINT:
            if(pending.command == Command::RECTANGLE && pending.requests.n == 4) {
                return TypedKind::RECTANGLE;
            }
            return TypedKind::NONE;
        default: return TypedKind::NONE;
    }
}

bool GraphicsWindow::CommitTypedLine(std::optional<double> lengthMm,
                                     std::optional<double> angleDeg) {
    if(TypedInputKind() != TypedKind::LINE || (!lengthMm && !angleDeg)) return false;
    hRequest hr     = pending.request;
    hEntity  startP = hr.entity(1), endP = hr.entity(2);
    Vector   start  = SK.GetEntity(startP)->PointGetNum();
    Vector   cur    = SK.GetEntity(endP)->PointGetNum().Minus(start);
    bool     inWp   = LockedInWorkplane();
    Vector   u = projRight, v = projUp;
    if(inWp) {
        EntityBase *n = SK.GetEntity(ActiveWorkplane())->Normal();
        u = n->NormalU();
        v = n->NormalV();
    }
    double len = lengthMm ? *lengthMm : cur.Magnitude();
    if(len < LENGTH_EPS) return false;
    Vector dir = cur.Magnitude() > LENGTH_EPS ? cur.WithMagnitude(1) : u;
    double a   = 0;
    if(angleDeg && inWp) {
        a   = fmod(fmod(*angleDeg, 360) + 360, 360);
        dir = u.ScaledBy(cos(a * PI / 180)).Plus(v.ScaledBy(sin(a * PI / 180)));
    }
    Vector end = start.Plus(dir.ScaledBy(len));
    SK.GetEntity(endP)->PointForceTo(end);

    if(lengthMm) {
        AddValued(Constraint::Type::PT_PT_DISTANCE, startP, endP, Entity::NO_ENTITY,
                  Entity::NO_ENTITY, len);
    }
    if(angleDeg && inWp) {
        if(fabs(remainder(a, 180)) < 1e-9) {
            Constraint::Constrain(Constraint::Type::HORIZONTAL, Entity::NO_ENTITY,
                                  Entity::NO_ENTITY, hr.entity(0));
        } else if(fabs(remainder(a - 90, 180)) < 1e-9) {
            Constraint::Constrain(Constraint::Type::VERTICAL, Entity::NO_ENTITY,
                                  Entity::NO_ENTITY, hr.entity(0));
        } else {
            // No line-to-axis angle constraint exists: measure against a horizontal
            // construction reference line from the start point.
            hRequest ref = AddRequest(Request::Type::LINE_SEGMENT, /*rememberForUndo=*/false);
            SK.GetRequest(ref)->construction = true;
            SK.GetEntity(ref.entity(1))->PointForceTo(start);
            SK.GetEntity(ref.entity(2))->PointForceTo(start.Plus(u.ScaledBy(len)));
            Constraint::ConstrainCoincident(ref.entity(1), startP);
            Constraint::Constrain(Constraint::Type::HORIZONTAL, Entity::NO_ENTITY,
                                  Entity::NO_ENTITY, ref.entity(0));
            AddValued(Constraint::Type::ANGLE, Entity::NO_ENTITY, Entity::NO_ENTITY,
                      ref.entity(0), hr.entity(0), a <= 180 ? a : 360 - a);
        }
    }
    SS.MarkGroupDirty(activeGroup);
    ContinueLineChain(end);
    Invalidate();
    return true;
}

bool GraphicsWindow::CommitTypedCircle(double diameterMm) {
    if(TypedInputKind() != TypedKind::CIRCLE || diameterMm <= LENGTH_EPS) return false;
    Entity *circle = SK.GetEntity(pending.circle);
    SK.GetEntity(circle->distance)->DistanceForceTo(diameterMm / 2);
    AddValued(Constraint::Type::DIAMETER, Entity::NO_ENTITY, Entity::NO_ENTITY, pending.circle,
              Entity::NO_ENTITY, diameterMm);
    SS.MarkGroupDirty(activeGroup);
    ClearPending();
    Invalidate();
    return true;
}

bool GraphicsWindow::CommitTypedRectangle(std::optional<double> widthMm,
                                          std::optional<double> heightMm) {
    if(TypedInputKind() != TypedKind::RECTANGLE || (!widthMm && !heightMm)) return false;
    hRequest lns[4];
    for(int i = 0; i < 4; i++) lns[i] = pending.requests[i];
    EntityBase *n = SK.GetEntity(ActiveWorkplane())->Normal();
    Vector      u = n->NormalU(), v = n->NormalV();
    // MouseLeftDown's rectangle case: lns[2].entity(1) is the fixed corner and pending.point
    // (lns[1].entity(2)) the dragged one.
    Vector fixed = SK.GetEntity(lns[2].entity(1))->PointGetNum();
    Vector cur   = SK.GetEntity(pending.point)->PointGetNum().Minus(fixed);
    double du = cur.Dot(u), dv = cur.Dot(v);
    if(widthMm)  du = (du < 0 ? -1 : 1) * *widthMm;
    if(heightMm) dv = (dv < 0 ? -1 : 1) * *heightMm;
    SK.GetEntity(pending.point)->PointForceTo(fixed.Plus(u.ScaledBy(du)).Plus(v.ScaledBy(dv)));
    // Lines with odd index are horizontal (along u), even ones vertical.
    if(widthMm) {
        AddValued(Constraint::Type::PT_PT_DISTANCE, lns[1].entity(1), lns[1].entity(2),
                  Entity::NO_ENTITY, Entity::NO_ENTITY, *widthMm);
    }
    if(heightMm) {
        AddValued(Constraint::Type::PT_PT_DISTANCE, lns[0].entity(1), lns[0].entity(2),
                  Entity::NO_ENTITY, Entity::NO_ENTITY, *heightMm);
    }
    SS.MarkGroupDirty(activeGroup);
    ClearPending();
    Invalidate();
    return true;
}

} // namespace SolveSpace
