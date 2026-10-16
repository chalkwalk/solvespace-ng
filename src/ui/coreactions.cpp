//-----------------------------------------------------------------------------
// Core-backed ui.* actions.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "ui/coreactions.h"
#include "ui/uiactions.h"

namespace SolveSpace {
namespace Ui {

void RegisterCoreUiActions(bool animate) {
    UiActions &a = Actions();
    auto view = [&a, animate](const char *name, ViewPreset p) {
        a.Bind(name, [p, animate] { SS.GW.SnapToView(p, animate); });
    };
    view("ui.view-front",  ViewPreset::FRONT);
    view("ui.view-back",   ViewPreset::BACK);
    view("ui.view-right",  ViewPreset::RIGHT);
    view("ui.view-left",   ViewPreset::LEFT);
    view("ui.view-top",    ViewPreset::TOP);
    view("ui.view-bottom", ViewPreset::BOTTOM);
    a.Bind("ui.view-frame-selected", [] { SS.GW.FrameSelected(); });
    a.Bind("ui.hide",       [] { SS.GW.HideSelectedGroups(); });
    a.Bind("ui.unhide-all", [] { SS.GW.ShowAllGroups(); });
    a.Bind("ui.isolate",    [] { SS.GW.IsolateSelectedGroups(); });
}

}
}
