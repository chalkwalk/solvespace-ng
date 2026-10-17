//-----------------------------------------------------------------------------
// Icons: Lucide-style SVGs drawn into pixels with lunasvg, white on transparent, so the UI can
// tint them with the theme's text colour.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_UI_ICONRASTER_H
#define SOLVESPACE_UI_ICONRASTER_H

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace SolveSpace {
namespace Ui {

struct IconImage {
    int                  width = 0, height = 0;
    std::vector<uint8_t> rgba; // straight alpha, rows top to bottom
};

// Renders an SVG (stroke="currentColor") white on transparent at px x px; nothing if the SVG
// is broken or px is not positive.
std::optional<IconImage> RasterizeIcon(std::string_view svg, int px);

}
}

#endif
