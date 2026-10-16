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
    Group::Type type;
    bool        references;  // #references: always first, can't be activated
    bool        active, visible, suppressed;
    bool        afterActive; // not solved while an earlier group is active; shown dimmed
    Status      status;
    int         dof;

    std::string StatusText() const; // "ok", the DOF count, "!" or "ERR"
};

std::vector<OutlinerRow> BuildOutliner();

}
}

#endif
