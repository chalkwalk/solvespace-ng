//-----------------------------------------------------------------------------
// The G grab tool.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "ui/grabtool.h"

namespace SolveSpace {
namespace Ui {

GrabTool &Grab() {
    static GrabTool tool;
    return tool;
}

bool GrabTool::Start() {
    field.Clear();
    return SS.GW.StartGrab(SS.GW.currentMousePosition);
}

bool GrabTool::Active() const {
    return SS.GW.pending.operation == GraphicsWindow::Pending::GRABBING;
}

bool GrabTool::HandleChord(const keymap::Chord &c) {
    if(!Active() || c.ctrl) return false;
    if(!c.alt && !c.shift && (c.key == "x" || c.key == "y" || c.key == "z")) {
        SS.GW.GrabSetAxis(c.key[0] - 'x');
        return true;
    }
    if(field.HandleChord(c)) {
        SS.GW.GrabSetTyped(field.ValueMm(0));
        return true;
    }
    if(c.key == "Enter" || c.key == "NumEnter") {
        SS.GW.GrabConfirm();
        return true;
    }
    if(c.key == "Esc") {
        SS.GW.GrabCancel();
        return true;
    }
    return true; // swallow everything else while grabbing
}

std::string GrabTool::Description() const {
    std::string d    = _("Grab");
    int         axis = SS.GW.grab.axis;
    if(axis >= 0) d += std::string(" \xC2\xB7 ") + "XYZ"[axis];
    if(!field.Text(0).empty()) {
        d += " \xC2\xB7 " + field.Text(0);
        if(axis < 0) d += std::string(" ") + _("(lock an axis to use a typed distance)");
    }
    return d;
}

std::vector<KeyHint> GrabTool::Hints() const {
    return { { SS.GW.LockedInWorkplane() ? "X Y" : "X Y Z", _("Axis") }, { "0-9", _("Distance") },
             { "Enter", _("Confirm") }, { "Esc", _("Cancel") } };
}

}
}
