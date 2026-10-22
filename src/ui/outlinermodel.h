//-----------------------------------------------------------------------------
// What the outliner shows: one row per group, in solve order, as the property browser's home
// screen lists them.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_UI_OUTLINERMODEL_H
#define SOLVESPACE_UI_OUTLINERMODEL_H

#include <string>
#include <vector>
#include "solvespace.h"

namespace SolveSpace {
namespace Ui {

struct OutlinerRow {
    enum class Status { OK, DOF, WARNING, ERROR };

    hGroup      group;
    std::string name;
    std::string displayName; // "Origin" for #references (Fusion's name), else the name
    std::string icon;        // the shelf icon for the group's type (res/icons/ng)
    Group::Type type;
    bool        references;  // #references: always first, can't be activated
    bool        active, visible, suppressed;
    bool        afterActive; // not solved while an earlier group is active; shown dimmed
    Status      status;
    int         dof;
    std::string partHeader;  // the part's name where the part changes (files with parts)

    std::string StatusText() const; // degrees of freedom left (0 when fully constrained), "!" or "ERR"
};

std::vector<OutlinerRow> BuildOutliner();

}
}

#endif
