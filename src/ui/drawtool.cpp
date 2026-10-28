//-----------------------------------------------------------------------------
// Typed values while drawing.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "ui/drawtool.h"

namespace SolveSpace {
namespace Ui {

using TypedKind = GraphicsWindow::TypedKind;

DrawTool &Draw() {
    static DrawTool tool;
    return tool;
}

void DrawTool::Sync() {
    TypedKind kind   = SS.GW.TypedInputKind();
    uint32_t  target = kind == TypedKind::CIRCLE ? SS.GW.pending.circle.v
                                                 : SS.GW.pending.request.v;
    if(kind == lastKind && target == lastTarget) return;
    lastKind   = kind;
    lastTarget = target;
    switch(kind) {
        // An angle needs a plane to be measured in.
        case TypedKind::LINE:
            fields = SS.GW.LockedInWorkplane() ? TypedFields({ _("length"), _("angle") })
                                               : TypedFields({ _("length") });
            break;
        case TypedKind::CIRCLE:    fields = TypedFields({ _("diameter") }); break;
        case TypedKind::RECTANGLE: fields = TypedFields({ _("width"), _("height") }); break;
        case TypedKind::NONE:      fields = TypedFields(std::vector<std::string>{}); break;
    }
}

bool DrawTool::Active() {
    Sync();
    return lastKind != TypedKind::NONE;
}

bool DrawTool::HandleChord(const keymap::Chord &c) {
    if(!Active() || c.ctrl || c.alt) return false;
    bool enter = (c.key == "Enter" || c.key == "NumEnter");
    if(lastKind == TypedKind::LINE && !fields.Any() && !c.shift) {
        // With nothing typed, Backspace steps back one segment and Enter ends the chain.
        if(c.key == "Backspace") return SS.GW.RemoveLastLineSegment();
        if(enter) {
            SS.GW.CancelPendingNew();
            return true;
        }
    }
    if(fields.HandleChord(c)) return true;
    if(c.key == "Esc") {
        if(!fields.Any()) return false;
        fields.Clear();
        return true;
    }
    if(!enter) return false;
    if(!fields.Any()) return false;
    bool ok = false;
    switch(lastKind) {
        case TypedKind::LINE:
            ok = SS.GW.CommitTypedLine(fields.ValueMm(0), fields.ValueRaw(1));
            break;
        case TypedKind::CIRCLE:
            if(auto d = fields.ValueMm(0)) ok = SS.GW.CommitTypedCircle(*d);
            break;
        case TypedKind::RECTANGLE:
            ok = SS.GW.CommitTypedRectangle(fields.ValueMm(0), fields.ValueMm(1));
            break;
        case TypedKind::NONE: break;
    }
    if(ok) {
        lastKind = TypedKind::NONE; // force fresh fields on the next Sync()
        Sync();
    }
    return true; // a bad value keeps the fields for correction
}

std::string DrawTool::Description() {
    if(!Active()) return "";
    static const char *names[] = { "", N_("Line"), N_("Circle"), N_("Rectangle") };
    std::string        d       = lastKind == TypedKind::NONE ? "" : _(names[(int)lastKind]);
    for(int i = 0; i < (int)fields.Count(); i++) {
        d += " \xC2\xB7 " + fields.Label(i) + " " + fields.Text(i);
        if(i == fields.Active()) d += "_";
    }
    return d;
}

std::vector<KeyHint> DrawTool::Hints() {
    if(lastKind == TypedKind::LINE && !fields.Any()) {
        return { { "0-9", _("Value") }, { "Backspace", _("Remove last segment") },
                 { "Enter / Esc / RMB", _("Finish") } };
    }
    return { { "0-9", _("Value") }, { "Tab", _("Next field") }, { "Enter", _("Apply") },
             { "Esc", _("Clear / cancel") } };
}

}
}
