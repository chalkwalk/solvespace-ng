//-----------------------------------------------------------------------------
// The Properties panel: the selection if there is one, else the settings of the group chosen
// in the outliner (else the active group). Replaces the property browser's group screen and its
// selection description.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_UI_PROPERTIES_H
#define SOLVESPACE_UI_PROPERTIES_H

#include <functional>
#include "solvespace.h"

namespace SolveSpace {
namespace Ui {

void   ShowGroupInProperties(hGroup hg); // and show the panel
void   ShowJointInProperties(hJoint hj); // a motion joint (M5a), and show the panel
hJoint PropertiesJoint();                // the joint the panel shows, if any
std::string JointSweepText(hJoint hj);   // the joint's last clearance sweep shown, or ""
hGroup PropertiesGroup();                // the group the panel shows now
// Draws the panel if shown. Returns what the user changed, to run after the frame.
std::function<void()> DrawProperties();
void ResetProperties();

}
}

#endif
