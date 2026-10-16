//-----------------------------------------------------------------------------
// The outliner panel: the document's groups, to activate, show or hide, rename, isolate and
// delete them.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_UI_OUTLINER_H
#define SOLVESPACE_UI_OUTLINER_H

#include <functional>

namespace SolveSpace {
namespace Ui {

// Draws the panel if shown. Returns what the user asked for, to run after the frame.
std::function<void()> DrawOutliner();
void ResetOutliner(); // forget an unfinished rename (a new document or main window)

}
}

#endif
