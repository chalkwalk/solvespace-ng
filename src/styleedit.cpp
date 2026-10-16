//-----------------------------------------------------------------------------
// Editing styles, shared by the property browser's style screens and the NG Styles panel.
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {

void Style::ChangeOption(hStyle hs, StyleOption option) {
    SS.UndoRemember();
    Style   *s     = Style::Get(hs);
    uint32_t align = (uint32_t)s->textOrigin;
    const uint32_t L = (uint32_t)TextOrigin::LEFT, R = (uint32_t)TextOrigin::RIGHT,
                   B = (uint32_t)TextOrigin::BOT, T = (uint32_t)TextOrigin::TOP;
    switch(option) {
        // Switching units converts the values at the current zoom.
        case StyleOption::WIDTH_IN_MM:
            if(s->widthAs != UnitsAs::MM) {
                s->widthAs = UnitsAs::MM;
                s->width /= SS.GW.scale;
                s->stippleScale /= SS.GW.scale;
            }
            break;
        case StyleOption::WIDTH_IN_PIXELS:
            if(s->widthAs != UnitsAs::PIXELS) {
                s->widthAs = UnitsAs::PIXELS;
                s->width *= SS.GW.scale;
                s->stippleScale *= SS.GW.scale;
            }
            break;
        case StyleOption::TEXT_HEIGHT_IN_MM:
            if(s->textHeightAs != UnitsAs::MM) {
                s->textHeightAs = UnitsAs::MM;
                s->textHeight /= SS.GW.scale;
            }
            break;
        case StyleOption::TEXT_HEIGHT_IN_PIXELS:
            if(s->textHeightAs != UnitsAs::PIXELS) {
                s->textHeightAs = UnitsAs::PIXELS;
                s->textHeight *= SS.GW.scale;
            }
            break;
        case StyleOption::EXPORTABLE: s->exportable = !s->exportable; break;
        case StyleOption::VISIBLE:    s->visible = !s->visible; break;
        case StyleOption::FILLED:     s->filled = !s->filled; break;
        case StyleOption::ALIGN_LEFT:   align = (align | L) & ~R; break;
        case StyleOption::ALIGN_CENTER: align = align & ~L & ~R; break;
        case StyleOption::ALIGN_RIGHT:  align = (align & ~L) | R; break;
        case StyleOption::ALIGN_BOTTOM: align = (align | B) & ~T; break;
        case StyleOption::ALIGN_MIDDLE: align = align & ~B & ~T; break;
        case StyleOption::ALIGN_TOP:    align = (align & ~B) | T; break;
    }
    s->textOrigin = (TextOrigin)align;
    SS.GW.Invalidate(/*clearPersistent=*/true);
}

std::string Style::ValueText(hStyle hs, StyleValue value) {
    Style *s = Style::Get(hs);
    auto length = [](double v, UnitsAs units) {
        return units == UnitsAs::PIXELS ? ssprintf("%.2f", v) : SS.MmToString(v, true);
    };
    auto colour = [](RgbaColor c) {
        return ssprintf("%.2f, %.2f, %.2f", c.redF(), c.greenF(), c.blueF());
    };
    switch(value) {
        case StyleValue::NAME:          return s->name;
        case StyleValue::COLOR:         return colour(s->color);
        case StyleValue::FILL_COLOR:    return colour(s->fillColor);
        case StyleValue::WIDTH:         return length(s->width, s->widthAs);
        case StyleValue::STIPPLE_WIDTH: return length(s->stippleScale, s->widthAs);
        case StyleValue::TEXT_HEIGHT:   return length(s->textHeight, s->textHeightAs);
        case StyleValue::TEXT_ANGLE:    return ssprintf("%.2f", s->textAngle);
    }
    return "";
}

bool Style::SetValue(hStyle hs, StyleValue value, const std::string &str, bool rememberUndo) {
    switch(value) {
        case StyleValue::NAME:
            if(str.empty()) {
                Error(_("Style name cannot be empty"));
                return false;
            }
            SS.UndoRemember();
            Style::Get(hs)->name = str;
            break;
        case StyleValue::COLOR:
        case StyleValue::FILL_COLOR: {
            Vector rgb;
            if(sscanf(str.c_str(), "%lf, %lf, %lf", &rgb.x, &rgb.y, &rgb.z) != 3) {
                Error(_("Bad format: specify color as r, g, b"));
                return false;
            }
            rgb = rgb.ClampWithin(0, 1);
            if(rememberUndo) SS.UndoRemember();
            Style *s = Style::Get(hs);
            (value == StyleValue::COLOR ? s->color : s->fillColor) = RGBf(rgb.x, rgb.y, rgb.z);
            break;
        }
        case StyleValue::WIDTH:
        case StyleValue::STIPPLE_WIDTH:
        case StyleValue::TEXT_HEIGHT: {
            SS.UndoRemember();
            Style  *s     = Style::Get(hs);
            UnitsAs units = value == StyleValue::TEXT_HEIGHT ? s->textHeightAs : s->widthAs;
            double  v     = units == UnitsAs::MM ? SS.StringToMm(str) : atof(str.c_str());
            v = max(0.0, v);
            if(value == StyleValue::TEXT_HEIGHT) {
                s->textHeight = v;
            } else if(value == StyleValue::STIPPLE_WIDTH) {
                s->stippleScale = v;
            } else {
                s->width = v;
            }
            break;
        }
        case StyleValue::TEXT_ANGLE:
            SS.UndoRemember();
            Style::Get(hs)->textAngle = WRAP_SYMMETRIC(atof(str.c_str()), 360);
            break;
    }
    SS.GW.persistentDirty = true;
    return true;
}

void Style::SetStipple(hStyle hs, StipplePattern pattern) {
    Style::Get(hs)->stippleType = pattern;
    SS.GW.persistentDirty = true;
}

void Style::Delete(hStyle hs) {
    SS.UndoRemember();
    // Recreated automatically if something still uses it.
    if(SK.style.FindByIdNoOops(hs)) SK.style.RemoveById(hs);
    SS.GW.Invalidate();
}

}
