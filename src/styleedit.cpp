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

// The light palette for the default styles; the dark one is SolveSpace's own (Defaults).
// Yellow hover and white lines vanish on white, so those change most.
static const struct {
    uint32_t  style;
    RgbaColor light;
} LightColors[] = {
    { Style::ACTIVE_GRP,   RGBf(0.10, 0.11, 0.13) },
    { Style::CONSTRUCTION, RGBf(0.10, 0.55, 0.10) },
    { Style::INACTIVE_GRP, RGBf(0.65, 0.45, 0.20) },
    { Style::DATUM,        RGBf(0.00, 0.55, 0.00) },
    { Style::SOLID_EDGE,   RGBf(0.25, 0.25, 0.25) },
    { Style::CONSTRAINT,   RGBf(0.75, 0.00, 0.75) },
    { Style::SELECTED,     RGBf(0.90, 0.00, 0.00) },
    { Style::HOVERED,      RGBf(0.95, 0.55, 0.00) },
    { Style::CONTOUR_FILL, RGBf(0.80, 0.92, 0.92) },
    { Style::NORMALS,      RGBf(0.00, 0.40, 0.40) },
    { Style::ANALYZE,      RGBf(0.00, 0.60, 0.70) },
    { Style::DRAW_ERROR,   RGBf(1.00, 0.00, 0.00) },
    { Style::DIM_SOLID,    RGBf(0.90, 0.90, 0.90) },
    { Style::HIDDEN_EDGE,  RGBf(0.55, 0.55, 0.55) },
    { Style::OUTLINE,      RGBf(0.20, 0.20, 0.20) },
};
static const RgbaColor DarkBackground = RGBi(0, 0, 0), LightBackground = RGBi(0xF4, 0xF5, 0xF7);

void Style::UseThemeColors(bool light) {
    Platform::SettingsRef settings = Platform::GetSettings();
    for(const Default *d = &Defaults[0]; d->h.v; d++) {
        for(const auto &lc : LightColors) {
            if(lc.style != d->h.v) continue;
            RgbaColor from = light ? d->color : lc.light, to = light ? lc.light : d->color;
            Style *s = Get(d->h);
            if(s->color.Equals(from)) s->color = to;
        }
    }
    RgbaColor fromBg = light ? DarkBackground : LightBackground;
    if(SS.backgroundColor.Equals(fromBg)) {
        SS.backgroundColor = light ? LightBackground : DarkBackground;
    }
    FreezeDefaultStyles(settings);
    settings->FreezeColor("BackgroundColor", SS.backgroundColor);
    SS.GW.persistentDirty = true;
    SS.GW.Invalidate();
}

std::string Style::DisplayName(hStyle hs) {
    static const std::pair<uint32_t, const char *> names[] = {
        { ACTIVE_GRP, "Active group lines" }, { CONSTRUCTION, "Construction lines" },
        { INACTIVE_GRP, "Inactive group lines" }, { DATUM, "Points" },
        { SOLID_EDGE, "Solid edges" }, { CONSTRAINT, "Constraints" },
        { SELECTED, "Selected" }, { HOVERED, "Hovered" }, { CONTOUR_FILL, "Contour fill" },
        { NORMALS, "Normals" }, { ANALYZE, "Analysis" }, { DRAW_ERROR, "Errors" },
        { DIM_SOLID, "Dimmed solids" }, { HIDDEN_EDGE, "Hidden edges" },
        { OUTLINE, "Outlines" },
    };
    for(auto &[h, name] : names) {
        if(hs.v == h) return name;
    }
    Style *s = Style::Get(hs);
    return s->name.empty() ? s->DescriptionString() : s->name;
}

}
