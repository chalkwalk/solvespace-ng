//-----------------------------------------------------------------------------
// The view cube in the top right of the viewport: click a face, edge or corner to look from
// there; drag it to orbit.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_UI_VIEWCUBE_H
#define SOLVESPACE_UI_VIEWCUBE_H

#include <functional>
#include "solvespace.h"

namespace SolveSpace {
namespace Ui {

// Draws the cube. Returns what the user asked for, to run after the frame.
std::function<void()> DrawViewCube();
// Where it was drawn last frame (window coordinates) and its half-size, for ViewCubeFaces.
Point2d ViewCubeCenter();
double  ViewCubeSize();

}
}

#endif
