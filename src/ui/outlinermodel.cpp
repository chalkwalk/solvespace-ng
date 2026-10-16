//-----------------------------------------------------------------------------
// The outliner's rows; the status rules follow TextWindow::ShowListOfGroups.
//-----------------------------------------------------------------------------
#include "ui/outlinermodel.h"

namespace SolveSpace {
namespace Ui {

std::string OutlinerRow::StatusText() const {
    switch(status) {
        case Status::OK:      return "ok";
        case Status::DOF:     return dof > 999 ? "###" : std::to_string(dof);
        case Status::WARNING: return "!";
        case Status::ERROR:   return "ERR";
    }
    return "";
}

std::vector<OutlinerRow> BuildOutliner() {
    std::vector<OutlinerRow> rows;
    bool afterActive = false;
    for(hGroup hg : SK.groupOrder) {
        Group *g = SK.GetGroup(hg);
        // A sketch whose contour is broken, or a solid made from one.
        bool warn = (g->type == Group::Type::DRAWING_WORKPLANE &&
                     g->polyError.how != PolyError::GOOD) ||
                    ((g->type == Group::Type::EXTRUDE || g->type == Group::Type::LATHE) &&
                     SK.GetGroup(g->opA)->polyError.how != PolyError::GOOD);
        OutlinerRow r = {};
        r.group       = hg;
        r.name        = g->name;
        r.type        = g->type;
        r.references  = hg == Group::HGROUP_REFERENCES;
        r.active      = hg == SS.GW.activeGroup;
        r.visible     = g->visible;
        r.suppressed  = g->suppress;
        r.afterActive = afterActive;
        r.dof         = g->solved.dof;
        if(!g->IsSolvedOkay()) {
            r.status = OutlinerRow::Status::ERROR;
        } else if(warn && SS.checkClosedContour) {
            r.status = OutlinerRow::Status::WARNING;
        } else if(r.dof > 0) {
            r.status = OutlinerRow::Status::DOF;
        } else {
            r.status = OutlinerRow::Status::OK;
        }
        rows.push_back(r);
        if(r.active) afterActive = true;
    }
    return rows;
}

}
}
