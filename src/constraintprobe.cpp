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
