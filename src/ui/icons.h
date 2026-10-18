//-----------------------------------------------------------------------------
// NG's icons (res/icons/ng) in the current colours: the drawing's line styles and the theme
// (iconraster.h), drawn at the framebuffer's resolution. Shared by the shelf, Outliner and pies.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_UI_ICONS_H
#define SOLVESPACE_UI_ICONS_H

#include <string>
#include "imgui.h"
#include "ui/iconraster.h"

namespace SolveSpace {
namespace Ui {

IconPalette CurrentIconPalette();
// The icon `size` logical pixels square; 0 if it doesn't rasterize.
ImTextureID Icon(const std::string &name, float size);

}
}

#endif
