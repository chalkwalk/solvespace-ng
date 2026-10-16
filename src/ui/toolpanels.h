//-----------------------------------------------------------------------------
// The View panel, and the panels of the tools that need settings before they run: Paste
// Transformed, Step Dimension and Tangent Arc.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_UI_TOOLPANELS_H
#define SOLVESPACE_UI_TOOLPANELS_H

#include <functional>

namespace SolveSpace {
namespace Ui {

std::function<void()> DrawToolPanels(); // what the user asked for, to run after the frame

}
}

#endif
