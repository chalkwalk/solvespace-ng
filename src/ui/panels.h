//-----------------------------------------------------------------------------
// Panels float over the viewport (AGENTS.md section 5). Each remembers whether it is shown and
// where it is through Platform::Settings ("NG_Panel_<name>_*").
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_UI_PANELS_H
#define SOLVESPACE_UI_PANELS_H

#include <string>

namespace SolveSpace {
namespace Ui {

// RIGHT_TOP and RIGHT_BELOW stack two panels on the right (the Outliner over Properties, as
// Blender and SketchUp; owner review 4, layout c).
enum class PanelEdge { LEFT, RIGHT, RIGHT_TOP, RIGHT_BELOW, BOTTOM_RIGHT, CENTER, TOP };

class Panel {
public:
    // A panel that doesn't remember being shown (a tool's) is shown only while its tool is.
    Panel(std::string name, std::string title, PanelEdge edge, float width, bool shownByDefault,
          bool remembersShown = true);

    void Load(); // the shown flag from the settings; call when a main window is made
    bool autoSize = false; // sized by its contents (the tool shelf)
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
    bool        shownByDefault, shown, remembersShown;
    float       savedX = -1, savedY = -1, savedW = -1, savedH = -1;
};

Panel &OutlinerPanel();
Panel &PropertiesPanel();
Panel &ProblemsPanel();
Panel &PreferencesPanel();
Panel &StylesPanel();
Panel &ViewPanel();
Panel &PastePanel();         // shown while the browser's Paste Transformed screen is
Panel &StepDimensionPanel(); // likewise
Panel &TangentArcPanel();    // likewise
Panel &ShelfPanel();         // the tool shelf; shown while SS.showToolbar
Panel &ShortcutsPanel();     // Help > Keyboard Shortcuts

}
}

#endif
