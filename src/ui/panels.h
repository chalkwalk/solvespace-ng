//-----------------------------------------------------------------------------
// Panels float over the viewport (AGENTS.md section 5). Each remembers whether it is shown and
// where it is through Platform::Settings ("NG_Panel_<name>_*").
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_UI_PANELS_H
#define SOLVESPACE_UI_PANELS_H

#include <string>

namespace SolveSpace {
namespace Ui {

enum class PanelEdge { LEFT, RIGHT };

class Panel {
public:
    Panel(std::string name, std::string title, PanelEdge edge, float width, bool shownByDefault);

    void Load(); // the shown flag from the settings; call when a main window is made
    bool Shown() const { return shown; }
    void SetShown(bool s);
    void Toggle() { SetShown(!shown); }

    // Begins the panel's window; false if it is hidden or collapsed (then skip its contents).
    // Call End() whenever Begin() was called and the panel is shown.
    bool Begin();
    void End();

private:
    std::string name, title;
    PanelEdge   edge;
    float       width;
    bool        shownByDefault, shown;
    float       savedX = -1, savedY = -1, savedW = -1, savedH = -1;
};

Panel &OutlinerPanel();

}
}

#endif
