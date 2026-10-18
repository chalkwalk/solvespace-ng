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

// The colours icons are drawn in, as 0xRRGGBB. Icons mirror the drawing area, so the canvas
// parts take its line styles' colours; each has a placeholder colour in the SVG.
struct IconPalette {
    uint32_t text         = 0xffffff; // currentColor: UI chrome (arrows, axes of motion)
    uint32_t entity       = 0xffffff; // #e00001: sketch lines (Style ACTIVE_GRP)
    uint32_t point        = 0x00cc00; // #e00002: points (Style DATUM)
    uint32_t constraint   = 0xff19ff; // #e00003: constraint marks (Style CONSTRAINT)
    uint32_t construction = 0x19b219; // #e00004: construction lines (Style CONSTRUCTION)
    uint32_t solid        = 0x4f8ef7; // #e00005 / #e00006 / #e00007: lit, mid, shaded faces
};

// Renders an SVG at px x px in the palette's colours; nothing if the SVG is broken or px is
// not positive.
std::optional<IconImage> RasterizeIcon(std::string_view svg, int px,
                                       const IconPalette &palette = {});

}
}

#endif
