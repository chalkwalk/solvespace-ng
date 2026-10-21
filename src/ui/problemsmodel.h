//-----------------------------------------------------------------------------
// What the Problems panel lists: failed solves with their culprit constraints, redundant
// constraints, broken contours and failed booleans, for the groups that are solved now.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_UI_PROBLEMSMODEL_H
#define SOLVESPACE_UI_PROBLEMSMODEL_H

#include <string>
#include <vector>
#include "solvespace.h"

namespace SolveSpace {
namespace Ui {

struct Problem {
    enum class Kind { UNSOLVABLE, UNSATISFIED, REDUNDANT, TOO_MANY_UNKNOWNS, CONTOUR, BOOLEAN, FEATURE };

    hGroup                   group;
    std::string              groupName;
    Kind                     kind;
    std::string              title;  // "unsolvable constraints", "contour is self-intersecting"
    std::string              detail; // what to do about it
    std::vector<hConstraint> culprits;
    bool                     timedOut          = false; // the culprit search was cut short
    bool                     canAllowRedundant = false;
    bool                     canForceToMesh    = false;

    // Identifies the problem across frames (to notice new ones).
    std::string Key() const { return std::to_string(group.v) + ":" + std::to_string((int)kind); }
};

// Groups up to and including the active one; later ones are not solved.
std::vector<Problem> BuildProblems();

}
}

#endif
