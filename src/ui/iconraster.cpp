//-----------------------------------------------------------------------------
// Icons drawn into pixels with lunasvg.
//-----------------------------------------------------------------------------
#include <cstring>
#include <string>
#include <lunasvg.h>
#include "ui/iconraster.h"

namespace SolveSpace {
namespace Ui {

std::optional<IconImage> RasterizeIcon(std::string_view svg, int px) {
    if(px <= 0) return std::nullopt;
    // Lucide strokes with currentColor; draw white so the UI can tint it with any colour.
    std::string text(svg);
    const std::string current = "currentColor";
    for(size_t at; (at = text.find(current)) != std::string::npos;) {
        text.replace(at, current.size(), "#ffffff");
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
