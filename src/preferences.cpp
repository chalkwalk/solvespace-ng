//-----------------------------------------------------------------------------
// Application settings, shared by the property browser's configuration and view screens and
// the NG Preferences and View panels. Parsing and limits are the browser's.
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {

// A field holding a length; edited in the display units.
static double *LengthField(Setting setting) {
    switch(setting) {
        case Setting::MARGIN_LEFT:       return &SS.exportMargin.left;
        case Setting::MARGIN_RIGHT:      return &SS.exportMargin.right;
        case Setting::MARGIN_BOTTOM:     return &SS.exportMargin.bottom;
        case Setting::MARGIN_TOP:        return &SS.exportMargin.top;
        case Setting::CANVAS_WIDTH:      return &SS.exportCanvas.width;
        case Setting::CANVAS_HEIGHT:     return &SS.exportCanvas.height;
        case Setting::CANVAS_DX:         return &SS.exportCanvas.dx;
        case Setting::CANVAS_DY:         return &SS.exportCanvas.dy;
        case Setting::GCODE_DEPTH:       return &SS.gCode.depth;
        case Setting::GCODE_SAFE_HEIGHT: return &SS.gCode.safeHeight;
        case Setting::GCODE_FEED:        return &SS.gCode.feed;
        case Setting::GCODE_PLUNGE_FEED: return &SS.gCode.plungeFeed;
        default:                         return nullptr;
    }
}

std::string SolveSpaceUI::SettingText(Setting setting, int index) {
    if(double *mm = LengthField(setting)) return MmToString(*mm, true);
    switch(setting) {
        case Setting::MODEL_COLOR: {
            RgbaColor c = modelColor[index];
            return ssprintf("%.2f, %.2f, %.2f", c.redF(), c.greenF(), c.blueF());
        }
        case Setting::CHORD_TOLERANCE:             return ssprintf("%lg", chordTol);
        case Setting::MAX_SEGMENTS:                return ssprintf("%d", maxSegments);
        case Setting::EXPORT_CHORD_TOLERANCE:      return ssprintf("%lg", exportChordTol);
        case Setting::EXPORT_MAX_SEGMENTS:         return ssprintf("%d", exportMaxSegments);
        case Setting::GRID_SPACING:                return MmToString(gridSpacing, true);
        case Setting::DIGITS_AFTER_DECIMAL:        return ssprintf("%d", UnitDigitsAfterDecimal());
        case Setting::DIGITS_AFTER_DECIMAL_DEGREE: return ssprintf("%d", afterDecimalDegree);
        case Setting::EXPORT_SCALE:                return ssprintf("%.3f", (double)exportScale);
        case Setting::EXPORT_OFFSET:               return MmToString(exportOffset, true);
        case Setting::GCODE_PASSES:                return std::to_string(gCode.passes);
        case Setting::AUTOSAVE_INTERVAL:           return std::to_string(autosaveInterval);
        case Setting::FIND_CONSTRAINT_TIMEOUT:     return std::to_string(timeoutRedundantConstr);
        case Setting::ANIMATION_SPEED:             return std::to_string(animationSpeed);
        case Setting::LIGHT_INTENSITY:             return ssprintf("%.2f", lightIntensity[index]);
        case Setting::LIGHT_DIRECTION: {
            Vector d = lightDir[index];
            return ssprintf("%.2f, %.2f, %.2f", d.x, d.y, d.z);
        }
        case Setting::LIGHT_AMBIENT:    return ssprintf("%.2f", ambientIntensity);
        case Setting::DENSITY:          return ssprintf("%.3g", density);
        case Setting::CAMERA_TANGENT:   return ssprintf("%.3f", cameraTangent * 1000);
        case Setting::EXPLODE_DISTANCE: return MmToString(explodeDistance, true);
        case Setting::BACKGROUND_COLOR: {
            RgbaColor c = backgroundColor;
            return ssprintf("%.2f, %.2f, %.2f", c.redF(), c.greenF(), c.blueF());
        }
        default:                        return "";
    }
}

bool SolveSpaceUI::SetSetting(Setting setting, const std::string &s, int index) {
    if(double *mm = LengthField(setting)) {
        Expr *e = Expr::From(s, /*popUpError=*/true);
        if(!e) return false;
        *mm = (float)ExprToMm(e); // the browser rounds these to float
        return true;
    }
    switch(setting) {
        case Setting::MODEL_COLOR:
        case Setting::BACKGROUND_COLOR: {
            Vector rgb;
            if(sscanf(s.c_str(), "%lf, %lf, %lf", &rgb.x, &rgb.y, &rgb.z) != 3) {
                Error(_("Bad format: specify color as r, g, b"));
                return false;
            }
            rgb = rgb.ClampWithin(0, 1);
            if(setting == Setting::MODEL_COLOR) {
                modelColor[index] = RGBf(rgb.x, rgb.y, rgb.z);
            } else {
                backgroundColor = RGBf(rgb.x, rgb.y, rgb.z);
                GW.Invalidate();
            }
            return true;
        }
        case Setting::CHORD_TOLERANCE:
            chordTol = max(0.0, atof(s.c_str()));
            GenerateAll(Generate::ALL);
            return true;
        case Setting::EXPORT_CHORD_TOLERANCE:
            exportChordTol = max(0.0, atof(s.c_str()));
            return true;
        case Setting::MAX_SEGMENTS:
            maxSegments = min(1000, max(7, atoi(s.c_str())));
            GenerateAll(Generate::ALL);
            return true;
        case Setting::EXPORT_MAX_SEGMENTS:
            exportMaxSegments = min(1000, max(7, atoi(s.c_str())));
            return true;
        case Setting::GRID_SPACING:
            gridSpacing = (float)min(1e4, max(1e-3, StringToMm(s)));
            GW.Invalidate();
            return true;
        case Setting::DIGITS_AFTER_DECIMAL: {
            int v = atoi(s.c_str());
            if(v < 0 || v > 8) {
                Error(_("Specify between 0 and %d digits after the decimal."), 8);
                return false;
            }
            SetUnitDigitsAfterDecimal(v);
            GW.Invalidate();
            return true;
        }
        case Setting::DIGITS_AFTER_DECIMAL_DEGREE: {
            int v = atoi(s.c_str());
            if(v < 0 || v > 4) {
                Error(_("Specify between 0 and %d digits after the decimal."), 4);
                return false;
            }
            afterDecimalDegree = v;
            GW.Invalidate();
            return true;
        }
        case Setting::EXPORT_SCALE: {
            Expr *e = Expr::From(s, /*popUpError=*/true);
            if(!e) return false;
            double ev = e->Eval();
            if(fabs(ev) < 0.001 || IsReasonable(ev)) {
                Error(_("Export scale must not be zero!"));
                return false;
            }
            exportScale = (float)ev;
            return true;
        }
        case Setting::EXPORT_OFFSET: {
            Expr *e = Expr::From(s, /*popUpError=*/true);
            if(!e) return false;
            double ev = ExprToMm(e);
            if(IsReasonable(ev) || ev < 0) {
                Error(_("Cutter radius offset must not be negative!"));
                return false;
            }
            exportOffset = (float)ev;
            return true;
        }
        case Setting::GCODE_PASSES: {
            Expr *e = Expr::From(s, /*popUpError=*/true);
            if(e) gCode.passes = (int)(e->Eval());
            gCode.passes = max(1, min(1000, gCode.passes));
            return e != nullptr;
        }
        case Setting::AUTOSAVE_INTERVAL: {
            int interval = atoi(s.c_str());
            if(!interval) {
                Error(_("Bad format: specify interval in integral minutes"));
                return false;
            }
            if(interval < 1) {
                Error(_("Bad value: autosave interval should be positive"));
                return false;
            }
            autosaveInterval = interval;
            ScheduleAutosave();
            return true;
        }
        case Setting::FIND_CONSTRAINT_TIMEOUT: {
            int timeout = atoi(s.c_str());
            if(!timeout) return false;
            timeoutRedundantConstr = timeout >= 1 ? timeout : 1000;
            return true;
        }
        case Setting::ANIMATION_SPEED: {
            int speed      = atoi(s.c_str());
            animationSpeed = speed >= 0 ? speed : 800;
            return true;
        }
        case Setting::LIGHT_INTENSITY:
            lightIntensity[index] = min(1.0, max(0.0, atof(s.c_str())));
            GW.Invalidate();
            return true;
        case Setting::DENSITY:
            if(atof(s.c_str()) <= 0) {
                Error(_("The density must be greater than zero."));
                return false;
            }
            density = atof(s.c_str());
            return true;
        case Setting::LIGHT_AMBIENT:
            ambientIntensity = min(1.0, max(0.0, atof(s.c_str())));
            GW.Invalidate();
            return true;
        case Setting::LIGHT_DIRECTION: {
            double x, y, z;
            if(sscanf(s.c_str(), "%lf, %lf, %lf", &x, &y, &z) != 3) {
                Error(_("Bad format: specify coordinates as x, y, z"));
                return false;
            }
            lightDir[index] = Vector::From(x, y, z);
            GW.Invalidate();
            return true;
        }
        case Setting::CAMERA_TANGENT:
            cameraTangent = (min(2.0, max(0.0, atof(s.c_str())))) / 1000.0;
            GW.Invalidate();
            if(!usePerspectiveProj) {
                Message(_("The perspective factor will have no effect until you "
                          "enable View -> Use Perspective Projection."));
            }
            return true;
        case Setting::EXPLODE_DISTANCE:
            explodeDistance = min(1e4, max(-1e4, StringToMm(s)));
            MarkGroupDirty(GW.activeGroup, true);
            return true;
        default:
            return false;
    }
}

bool SolveSpaceUI::SettingOn(SettingToggle toggle) {
    switch(toggle) {
        case SettingToggle::USE_SI_PREFIXES:            return useSIPrefixes;
        case SettingToggle::ARC_DIM_DEFAULT_DIAMETER:   return arcDimDefaultDiameter;
        case SettingToggle::SHOW_FULL_FILE_PATH:        return showFullFilePath;
        case SettingToggle::FIX_EXPORT_COLORS:          return fixExportColors;
        case SettingToggle::EXPORT_BACKGROUND_COLOR:    return exportBackgroundColor;
        case SettingToggle::DRAW_BACK_FACES:            return drawBackFaces;
        case SettingToggle::TURNTABLE_NAV:              return turntableNav;
        case SettingToggle::CAMERA_NAV:                 return cameraNav;
        case SettingToggle::IMMEDIATELY_EDIT_DIMENSION: return immediatelyEditDimension;
        case SettingToggle::SHOW_CONTOUR_AREAS:         return showContourAreas;
        case SettingToggle::CHECK_CLOSED_CONTOUR:       return checkClosedContour;
        case SettingToggle::AUTOMATIC_LINE_CONSTRAINTS: return automaticLineConstraints;
        case SettingToggle::EXPORT_SHADED_TRIANGLES:    return exportShadedTriangles;
        case SettingToggle::EXPORT_PWL_CURVES:          return exportPwlCurves;
        case SettingToggle::EXPORT_CANVAS_SIZE_AUTO:    return exportCanvasSizeAuto;
        case SettingToggle::CONSTRAINT_STATE_COLORS:    return constraintStateColors;
    }
    return false;
}

void SolveSpaceUI::SetKernel(KernelKind kind) {
    if(kind == kernelKind || !KernelAvailable(kind)) return;
    FlushRegeneration();
    kernelKind = kind;
    Platform::GetSettings()->FreezeInt("Kernel", (int)kind);
    for(Group &g : SK.group) g.clean = false;
    GenerateAll(Generate::ALL);
    GW.Invalidate(/*clearPersistent=*/true);
    ScheduleShowTW();
}

void SolveSpaceUI::ToggleSetting(SettingToggle toggle) {
    switch(toggle) {
        case SettingToggle::USE_SI_PREFIXES:
            useSIPrefixes = !useSIPrefixes;
            GW.Invalidate();
            break;
        case SettingToggle::ARC_DIM_DEFAULT_DIAMETER:
            arcDimDefaultDiameter = !arcDimDefaultDiameter;
            break;
        case SettingToggle::SHOW_FULL_FILE_PATH:
            showFullFilePath = !showFullFilePath;
            UpdateWindowTitles();
            break;
        case SettingToggle::FIX_EXPORT_COLORS:
            fixExportColors = !fixExportColors;
            break;
        case SettingToggle::EXPORT_BACKGROUND_COLOR:
            exportBackgroundColor = !exportBackgroundColor;
            break;
        case SettingToggle::DRAW_BACK_FACES:
            drawBackFaces = !drawBackFaces;
            GW.Invalidate(/*clearPersistent=*/true);
            break;
        case SettingToggle::TURNTABLE_NAV:
            turntableNav = !turntableNav;
            // Turning it on: align the view so Z is vertical.
            if(turntableNav) GW.AnimateOnto(Quaternion::From({ -1, 0, 0 }, { 0, 0, 1 }), GW.offset);
            break;
        case SettingToggle::CAMERA_NAV:
            cameraNav = !cameraNav;
            break;
        case SettingToggle::IMMEDIATELY_EDIT_DIMENSION:
            immediatelyEditDimension = !immediatelyEditDimension;
            GW.Invalidate(/*clearPersistent=*/true);
            break;
        case SettingToggle::SHOW_CONTOUR_AREAS:
            showContourAreas = !showContourAreas;
            GW.Invalidate();
            break;
        case SettingToggle::CHECK_CLOSED_CONTOUR:
            checkClosedContour = !checkClosedContour;
            GW.Invalidate();
            break;
        case SettingToggle::AUTOMATIC_LINE_CONSTRAINTS:
            automaticLineConstraints = !automaticLineConstraints;
            GW.Invalidate();
            break;
        case SettingToggle::EXPORT_SHADED_TRIANGLES:
            exportShadedTriangles = !exportShadedTriangles;
            GW.Invalidate();
            break;
        case SettingToggle::EXPORT_PWL_CURVES:
            exportPwlCurves = !exportPwlCurves;
            GW.Invalidate();
            break;
        case SettingToggle::EXPORT_CANVAS_SIZE_AUTO:
            exportCanvasSizeAuto = !exportCanvasSizeAuto;
            GW.Invalidate();
            break;
        case SettingToggle::CONSTRAINT_STATE_COLORS:
            constraintStateColors = !constraintStateColors;
            GW.Invalidate(/*clearPersistent=*/true);
            break;
    }
}

}
