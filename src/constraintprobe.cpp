//-----------------------------------------------------------------------------
// Which constraints fit the current selection: smart dimension and the constrain pie.
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {

Command GraphicsWindow::SmartDimensionCommand() {
    if((gs.vectors == 2 || gs.vectors == 3 || gs.vectors == 4) && gs.n == gs.vectors) {
        return Command::ANGLE;
    }
    return Command::DISTANCE_DIA;
}

void GraphicsWindow::SmartDimension() {
    SelectHoveredIfNothingSelected();
    GroupSelection();
    Constraint::MenuConstrain(SmartDimensionCommand());
}

} // namespace SolveSpace

namespace SolveSpace {

// Mirrors the selection checks of each case in Constraint::MenuConstrain (constraint.cpp).
bool Constraint::AppliesToSelection(Command id) {
    auto const &gs          = SS.GW.gs;
    bool        inWorkplane = SS.GW.LockedInWorkplane();
    switch(id) {
        case Command::DISTANCE_DIA:
        case Command::REF_DISTANCE:
            return (gs.points == 2 && gs.n == 2) ||
                   (gs.lineSegments == 1 && gs.n == 1) ||
                   (gs.vectors == 1 && gs.points == 2 && gs.n == 3) ||
                   (gs.workplanes == 1 && gs.points == 1 && gs.n == 2) ||
                   (gs.lineSegments == 1 && gs.points == 1 && gs.n == 2) ||
                   (gs.faces == 1 && gs.points == 1 && gs.n == 2) ||
                   (gs.circlesOrArcs == 1 && gs.n == 1);
        case Command::ON_ENTITY:
            return (gs.points >= 2 && gs.points == gs.n) ||
                   (gs.points == 1 && gs.workplanes == 1 && gs.n == 2) ||
                   (gs.points == 1 && gs.lineSegments == 1 && gs.n == 2) ||
                   (gs.points == 1 && gs.circlesOrArcs == 1 && gs.n == 2) ||
                   (gs.points == 1 && gs.faces >= 1 && gs.n == gs.points + gs.faces);
        case Command::EQUAL:
            return (gs.lineSegments >= 2 && gs.lineSegments == gs.n) ||
                   (gs.lineSegments == 2 && gs.points == 2 && gs.n == 4) ||
                   (gs.lineSegments == 1 && gs.points == 2 && gs.n == 3) ||
                   (gs.lineSegments == 2 && gs.points == 1 && gs.n == 3) ||
                   (gs.circlesOrArcs >= 2 && gs.circlesOrArcs == gs.n) ||
                   (gs.arcs == 1 && gs.lineSegments == 1 && gs.n == 2);
        case Command::RATIO:
        case Command::DIFFERENCE:
            return (gs.lineSegments == 2 && gs.n == 2) || (gs.arcs == 2 && gs.n == 2) ||
                   (gs.lineSegments == 1 && gs.arcs == 1 && gs.n == 2);
        case Command::AT_MIDPOINT:
            return (gs.lineSegments == 1 && gs.points == 1 && gs.n == 2) ||
                   (gs.lineSegments == 1 && gs.workplanes == 1 && gs.n == 2);
        case Command::SYMMETRIC: {
            bool ok = (gs.points == 2 && ((gs.workplanes == 1 && gs.n == 3) || gs.n == 2)) ||
                      (gs.lineSegments == 1 && ((gs.workplanes == 1 && gs.n == 2) || gs.n == 1)) ||
                      (inWorkplane && gs.lineSegments == 2 && gs.n == 2) ||
                      (inWorkplane && gs.lineSegments == 1 && gs.points == 2 && gs.n == 3);
            // Two points or one line with no symmetry entity become horizontal/vertical
            // symmetry, which needs a workplane.
            bool needsWorkplane = (gs.points == 2 && gs.n == 2) ||
                                  (gs.lineSegments == 1 && gs.n == 1);
            return ok && (!needsWorkplane || SS.GW.ActiveWorkplane() != Entity::FREE_IN_3D);
        }
        case Command::VERTICAL:
        case Command::HORIZONTAL:
            if(SS.GW.ActiveWorkplane() == Entity::FREE_IN_3D) return false;
            return (gs.lineSegments > 0 && gs.lineSegments == gs.n) ||
                   (gs.points >= 2 && gs.n == gs.points);
        case Command::ORIENTED_SAME:
            return gs.anyNormals == 2 && gs.n == 2;
        case Command::OTHER_ANGLE:
        case Command::REFERENCE:
            return gs.constraints == 1 && gs.n == 0;
        case Command::ANGLE:
        case Command::REF_ANGLE:
            return (gs.vectors == 3 && gs.n == 3) || (gs.vectors == 4 && gs.n == 4) ||
                   (gs.vectors == 2 && gs.n == 2);
        case Command::PARALLEL:
            return (gs.faces == 2 && gs.n == 2) || (gs.vectors > 1 && gs.vectors == gs.n) ||
                   (gs.lineSegments == 1 && gs.arcs == 1 &&
                    (gs.n == 2 || (gs.points == 1 && gs.n == 3))) ||
                   (gs.lineSegments == 1 && gs.cubics == 1 &&
                    (gs.n == 2 || (gs.points == 1 && gs.n == 3))) ||
                   (inWorkplane && gs.cubics + gs.arcs == 2 &&
                    (gs.n == 2 || (gs.points == 2 && gs.n == 4)));
        case Command::PERPENDICULAR:
            return (gs.faces == 2 && gs.n == 2) || (gs.vectors == 2 && gs.n == 2);
        case Command::WHERE_DRAGGED:
            return gs.points == 1 && gs.n == 1;
        case Command::COMMENT:
            return true;
        default:
            return false;
    }
}

const std::vector<Command> &Constraint::PieCommands() {
    static const std::vector<Command> list = {
        Command::ON_ENTITY, Command::HORIZONTAL, Command::VERTICAL, Command::PARALLEL,
        Command::PERPENDICULAR, Command::EQUAL, Command::AT_MIDPOINT, Command::SYMMETRIC,
        Command::DISTANCE_DIA, Command::ANGLE, Command::RATIO, Command::DIFFERENCE,
        Command::ORIENTED_SAME, Command::WHERE_DRAGGED, Command::REFERENCE, Command::OTHER_ANGLE };
    return list;
}

} // namespace SolveSpace
