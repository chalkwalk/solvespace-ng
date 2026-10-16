//-----------------------------------------------------------------------------
// The Problems panel: failed solves, redundant constraints, broken contours and failed
// booleans, each with its culprits one click away. It opens itself when a new problem appears.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_UI_PROBLEMS_H
#define SOLVESPACE_UI_PROBLEMS_H

#include <functional>

namespace SolveSpace {
namespace Ui {

std::function<void()> DrawProblems(); // what the user asked for, to run after the frame
void ResetProblems();

}
}

#endif
