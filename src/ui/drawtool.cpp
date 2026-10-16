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
        case TypedKind::LINE:      fields = TypedFields({ "length", "angle" }); break;
        case TypedKind::CIRCLE:    fields = TypedFields({ "diameter" }); break;
        case TypedKind::RECTANGLE: fields = TypedFields({ "width", "height" }); break;
        case TypedKind::NONE:      fields = TypedFields(std::vector<std::string>{}); break;
    }
}

bool DrawTool::Active() {
    Sync();
    return lastKind != TypedKind::NONE;
}

bool DrawTool::HandleChord(const keymap::Chord &c) {
    if(!Active() || c.ctrl || c.alt) return false;
    if(fields.HandleChord(c)) return true;
    if(c.key == "Esc") {
        if(!fields.Any()) return false;
        fields.Clear();
        return true;
    }
    if(c.key != "Enter" && c.key != "NumEnter") return false;
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
    static const char *names[] = { "", "Line", "Circle", "Rectangle" };
    std::string        d       = names[(int)lastKind];
    for(int i = 0; i < (int)fields.Count(); i++) {
        d += " \xC2\xB7 " + fields.Label(i) + " " + fields.Text(i);
        if(i == fields.Active()) d += "_";
    }
    return d;
}

std::vector<KeyHint> DrawTool::Hints() {
    return { { "0-9", "Value" }, { "Tab", "Next field" }, { "Enter", "Apply" },
             { "Esc", "Clear / cancel" } };
}

}
}
