//-----------------------------------------------------------------------------
// Icons drawn into pixels with lunasvg.
//-----------------------------------------------------------------------------
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <lunasvg.h>
#include "ui/iconraster.h"

namespace SolveSpace {
namespace Ui {

static std::string Hex(uint32_t rgb) {
    char buf[8];
    snprintf(buf, sizeof(buf), "#%06x", rgb & 0xffffff);
    return buf;
}

// A colour mixed towards white (t > 0) or black (t < 0).
static uint32_t Shade(uint32_t rgb, double t) {
    uint32_t out = 0;
    for(int shift = 0; shift <= 16; shift += 8) {
        double c = (rgb >> shift) & 0xff;
        c        = t > 0 ? c + (255 - c) * t : c * (1 + t);
        out |= (uint32_t)std::min(255.0, std::max(0.0, c + 0.5)) << shift;
    }
    return out;
}

std::optional<IconImage> RasterizeIcon(std::string_view svg, int px,
                                       const IconPalette &palette) {
    if(px <= 0) return std::nullopt;
    std::string text(svg);
    const std::pair<std::string, uint32_t> colours[] = {
        { "currentColor", palette.text },
        { "#e00001", palette.entity },
        { "#e00002", palette.point },
        { "#e00003", palette.constraint },
        { "#e00004", palette.construction },
        { "#e00005", Shade(palette.solid, 0.35) },
        { "#e00006", palette.solid },
        { "#e00007", Shade(palette.solid, -0.35) },
    };
    for(const auto &[placeholder, rgb] : colours) {
        std::string value = Hex(rgb);
        for(size_t at; (at = text.find(placeholder)) != std::string::npos;) {
            text.replace(at, placeholder.size(), value);
        }
    }
    std::unique_ptr<lunasvg::Document> doc = lunasvg::Document::loadFromData(text);
    if(!doc) return std::nullopt;
    lunasvg::Bitmap bmp = doc->renderToBitmap(px, px, 0x00000000);
    if(bmp.isNull()) return std::nullopt;
    bmp.convertToRGBA(); // lunasvg renders premultiplied BGRA
    IconImage im;
    im.width  = bmp.width();
    im.height = bmp.height();
    im.rgba.resize((size_t)im.width * im.height * 4);
    for(int y = 0; y < im.height; y++) {
        memcpy(&im.rgba[(size_t)y * im.width * 4], bmp.data() + (size_t)y * bmp.stride(),
               (size_t)im.width * 4);
    }
    return im;
}

}
}
