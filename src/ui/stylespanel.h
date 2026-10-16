//-----------------------------------------------------------------------------
// The Styles panel: the property browser's list of styles and style screens.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_UI_STYLESPANEL_H
#define SOLVESPACE_UI_STYLESPANEL_H

#include <functional>
#include "solvespace.h"

namespace SolveSpace {
namespace Ui {

void ShowStyleInStyles(hStyle hs); // and show the panel
std::function<void()> DrawStyles(); // what the user changed, to run after the frame
void ResetStyles();

}
}

#endif
