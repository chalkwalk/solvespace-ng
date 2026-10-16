//-----------------------------------------------------------------------------
// The status/HUD bar's content.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "ui/statusmodel.h"

namespace SolveSpace {
namespace Ui {

static void AddHint(StatusModel *m, const keymap::Keymap &km, const char *command,
                    const char *label) {
    auto chords = km.ChordsFor(command);
    if(chords.empty()) return;
    // Show the plainest binding (Esc rather than Alt+A).
    const keymap::Chord *best = &chords[0];
    for(const keymap::Chord &c : chords) {
        if(!c.ctrl && !c.alt && !c.shift) {
            best = &c;
            break;
        }
    }
    m->hints.push_back({ keymap::FormatChord(*best), label });
}

StatusModel BuildStatus(const keymap::Keymap &km, const std::string &modalTool,
                        const std::vector<KeyHint> &modalHints) {
    StatusModel m;
    if(!modalTool.empty()) {
        m.tool  = modalTool;
        m.hints = modalHints;
    } else if(SS.GW.pending.operation != GraphicsWindow::Pending::NONE) {
        if(SS.GW.pending.description) m.tool = SS.GW.pending.description;
        m.hints = modalHints;
        AddHint(&m, km, "unselect-all", "Cancel");
    } else {
        AddHint(&m, km, "ui.command-palette", "Commands");
        AddHint(&m, km, "ui.pie-add", "Add");
        AddHint(&m, km, "ui.pie-constrain", "Constrain");
        AddHint(&m, km, "ui.smart-dimension", "Dimension");
        AddHint(&m, km, "ui.grab", "Grab");
        AddHint(&m, km, "ui.toggle-sketch", "Sketch");
        AddHint(&m, km, "ui.pie-view", "Views");
    }
    if(Group *g = SK.group.FindByIdNoOops(SS.GW.activeGroup)) {
        m.group = g->name;
        m.dof   = g->solved.dof;
        switch(g->solved.how) {
            case SolveResult::OKAY:                     m.solve = "Solved"; break;
            case SolveResult::REDUNDANT_OKAY:           m.solve = "Redundant"; break;
            case SolveResult::DIDNT_CONVERGE:
            case SolveResult::REDUNDANT_DIDNT_CONVERGE: m.solve = "Didn't converge"; break;
            case SolveResult::TOO_MANY_UNKNOWNS:        m.solve = "Too many unknowns"; break;
        }
        m.solveProblem = g->solved.how != SolveResult::OKAY;
    }
    return m;
}

}
}
