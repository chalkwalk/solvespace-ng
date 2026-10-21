//-----------------------------------------------------------------------------
// The outliner's rows; the status rules follow TextWindow::ShowListOfGroups.
//-----------------------------------------------------------------------------
#include "ui/outlinermodel.h"

namespace SolveSpace {
namespace Ui {

std::string OutlinerRow::StatusText() const {
    switch(status) {
        case Status::OK:      return "0";
        case Status::DOF:     return dof > 999 ? "###" : std::to_string(dof);
        case Status::WARNING: return "!";
        case Status::ERROR:   return "ERR";
    }
    return "";
}

static const char *IconFor(Group::Type t) {
    switch(t) {
        case Group::Type::DRAWING_3D:        return "sketch-3d";
        case Group::Type::DRAWING_WORKPLANE: return "new-sketch";
        case Group::Type::EXTRUDE:           return "extrude";
        case Group::Type::LATHE:             return "lathe";
        case Group::Type::REVOLVE:           return "revolve";
        case Group::Type::HELIX:             return "helix";
        case Group::Type::ROTATE:            return "circular-pattern";
        case Group::Type::TRANSLATE:         return "linear-pattern";
        case Group::Type::LINKED:            return "insert-part";
        case Group::Type::FILLET:            return "fillet";
        case Group::Type::CHAMFER:           return "chamfer";
    }
    return "isometric";
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
        r.displayName = r.references ? "Origin" : g->name;
        r.icon        = r.references ? "origin" : IconFor(g->type);
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
