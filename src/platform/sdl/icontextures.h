//-----------------------------------------------------------------------------
// GL textures of the NG icons (res/icons/ng), drawn white for tinting; cached per size and
// freed with the ImGui context.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_PLATFORM_SDL_ICONTEXTURES_H
#define SOLVESPACE_PLATFORM_SDL_ICONTEXTURES_H

#include <string>
#include <string_view>
#include "imgui.h"

namespace SolveSpace {
namespace Platform {

// The icon "icons/ng/<name>.svg" at px x px; 0 if it doesn't rasterize.
ImTextureID IconTexture(const std::string &name, int px);
// The same for SVG text, cached under `key`.
ImTextureID IconTextureFromSvg(const std::string &key, std::string_view svg, int px);

}
}

#endif
