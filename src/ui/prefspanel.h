//-----------------------------------------------------------------------------
// The Preferences panel: the property browser's configuration screen, plus the NG settings.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_UI_PREFSPANEL_H
#define SOLVESPACE_UI_PREFSPANEL_H

#include <functional>

namespace SolveSpace {
namespace Ui {

std::function<void()> DrawPreferences(); // what the user changed, to run after the frame

}
}

#endif
