//-----------------------------------------------------------------------------
// UI fonts and the dark/light ImGui theme, built from design tokens.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_UI_THEME_H
#define SOLVESPACE_UI_THEME_H

struct ImFont;

namespace SolveSpace {
namespace Ui {

enum class ThemeChoice { DARK, LIGHT };

ThemeChoice LoadThemeChoice();       // setting "NG_Theme", default DARK
void ApplyTheme(ThemeChoice choice); // loads fonts once, sets ImGuiStyle colours/metrics
void ToggleTheme();                  // flips, applies, persists
ImFont *MonoFont();                  // JetBrains Mono, for numbers

}
}

#endif
