//-----------------------------------------------------------------------------
// The view cube in the bottom left of the viewport, with the reference axes coming out of it:
// click a face, edge or corner to look from there; drag it to orbit.
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
// Where the reference axes start: the cube's -X -Y -Z corner, on screen (window coordinates).
Point2d ViewCubeAxesOrigin();
// Whether a mouse event at (x, y) is the cube's: over one of its faces, or during a press
// that started on one. Everything else around it (the axes) belongs to the canvas.
// A canvas press in progress (a marquee, say) keeps the mouse even over the cube.
bool ViewCubeTakesMouseAt(double x, double y, bool canvasPressed);

}
}

#endif
