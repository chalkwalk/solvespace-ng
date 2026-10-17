//-----------------------------------------------------------------------------
// The view cube (top right of the viewport, as in Fusion 360, Onshape and FreeCAD): the cube
// drawn for the current view, and which face, edge or corner is under a point. A face looks
// straight at that side; an edge or a corner looks between its faces.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_UI_VIEWCUBEMODEL_H
#define SOLVESPACE_UI_VIEWCUBEMODEL_H

#include <array>
#include <optional>
#include <string>
#include <vector>
#include "solvespace.h"

namespace SolveSpace {
namespace Ui {

struct CubeFace {
    std::string            label;   // "Front", "Top", ...
    Vector                 normal;  // out of the cube; Front is -Y, Top is +Z (Z up)
    std::array<Point2d, 4> corners; // on screen, y down
    bool                   visible; // facing the viewer
    double                 facing;  // normal . (towards the viewer), for shading
};

// The cube spans center +/- size on screen for a face seen square on.
std::vector<CubeFace> ViewCubeFaces(Vector right, Vector up, Point2d center, double size);

// The direction to look from (towards the viewer, not normalized: a face normal, or the sum of
// two or three for an edge or a corner) for the cube part under p; nothing off the cube.
std::optional<Vector> ViewCubePick(Vector right, Vector up, Point2d center, double size,
                                   Point2d p);

// The part of each visible face that belongs to a picked direction, for highlighting.
std::vector<std::array<Point2d, 4>> ViewCubeRegion(Vector right, Vector up, Point2d center,
                                                   double size, Vector pick);

}
}

#endif
