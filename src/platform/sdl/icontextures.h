//-----------------------------------------------------------------------------
// GL textures of the NG icons (res/icons/ng), drawn white for tinting; cached per size and
// freed with the ImGui context.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_PLATFORM_SDL_ICONTEXTURES_H
#define SOLVESPACE_PLATFORM_SDL_ICONTEXTURES_H

#include <string>
#include <string_view>
#include "imgui.h"
#include "ui/iconraster.h"

namespace SolveSpace {
namespace Platform {

// The icon "icons/ng/<name>.svg" at px x px in the palette's colours; 0 if it doesn't
// rasterize. Cached per size and palette, so a theme or style change draws it afresh.
ImTextureID IconTexture(const std::string &name, int px, const Ui::IconPalette &palette = {});
// The same for SVG text, cached under `key`.
ImTextureID IconTextureFromSvg(const std::string &key, std::string_view svg, int px,
                               const Ui::IconPalette &palette = {});

}
}

#endif
