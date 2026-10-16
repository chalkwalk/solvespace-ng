//-----------------------------------------------------------------------------
// For the configuration screen, setup items that are not specific to the
// file being edited right now.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#if defined(_OPENMP)
#include <omp.h>
#endif

namespace SolveSpace {

void TextWindow::ScreenChangeColor(int link, uint32_t v) {
    SS.TW.ShowEditControlWithColorPicker(13, SS.modelColor[v]);

    SS.TW.edit.meaning = Edit::COLOR;
    SS.TW.edit.i = v;
}

void TextWindow::ScreenChangeChordTolerance(int link, uint32_t v) {
    SS.TW.ShowEditControl(3, ssprintf("%lg", SS.chordTol));
    SS.TW.edit.meaning = Edit::CHORD_TOLERANCE;
    SS.TW.edit.i = 0;
}

void TextWindow::ScreenChangeMaxSegments(int link, uint32_t v) {
    SS.TW.ShowEditControl(3, ssprintf("%d", SS.maxSegments));
    SS.TW.edit.meaning = Edit::MAX_SEGMENTS;
    SS.TW.edit.i = 0;
}

void TextWindow::ScreenChangeExportChordTolerance(int link, uint32_t v) {
    SS.TW.ShowEditControl(3, ssprintf("%lg", SS.exportChordTol));
    SS.TW.edit.meaning = Edit::CHORD_TOLERANCE;
    SS.TW.edit.i = 1;
}

void TextWindow::ScreenChangeExportMaxSegments(int link, uint32_t v) {
    SS.TW.ShowEditControl(3, ssprintf("%d", SS.exportMaxSegments));
    SS.TW.edit.meaning = Edit::MAX_SEGMENTS;
    SS.TW.edit.i = 1;
}

void TextWindow::ScreenChangeGridSpacing(int link, uint32_t v) {
    SS.TW.ShowEditControl(3, SS.MmToString(SS.gridSpacing, true));
    SS.TW.edit.meaning = Edit::GRID_SPACING;
}

void TextWindow::ScreenChangeDigitsAfterDecimal(int link, uint32_t v) {
    SS.TW.ShowEditControl(14, ssprintf("%d", SS.UnitDigitsAfterDecimal()));
    SS.TW.edit.meaning = Edit::DIGITS_AFTER_DECIMAL;
}

void TextWindow::ScreenChangeDigitsAfterDecimalDegree(int link, uint32_t v) {
    SS.TW.ShowEditControl(14, ssprintf("%d", SS.afterDecimalDegree));
    SS.TW.edit.meaning = Edit::DIGITS_AFTER_DECIMAL_DEGREE;
}

void TextWindow::ScreenChangeUseSIPrefixes(int link, uint32_t v) {
    SS.ToggleSetting(SettingToggle::USE_SI_PREFIXES);
}

void TextWindow::ScreenChangeExportScale(int link, uint32_t v) {
    SS.TW.ShowEditControl(5, ssprintf("%.3f", (double)SS.exportScale));
    SS.TW.edit.meaning = Edit::EXPORT_SCALE;
}

void TextWindow::ScreenChangeExportOffset(int link, uint32_t v) {
    SS.TW.ShowEditControl(3, SS.MmToString(SS.exportOffset, true));
    SS.TW.edit.meaning = Edit::EXPORT_OFFSET;
}

void TextWindow::ScreenChangeArcDimDefault(int link, uint32_t v) {
    SS.ToggleSetting(SettingToggle::ARC_DIM_DEFAULT_DIAMETER);
}

void TextWindow::ScreenChangeShowFullFilePath(int link, uint32_t v) {
    SS.ToggleSetting(SettingToggle::SHOW_FULL_FILE_PATH);
}

void TextWindow::ScreenChangeFixExportColors(int link, uint32_t v) {
    SS.ToggleSetting(SettingToggle::FIX_EXPORT_COLORS);
}

void TextWindow::ScreenChangeExportBackgroundColor(int link, uint32_t v) {
    SS.ToggleSetting(SettingToggle::EXPORT_BACKGROUND_COLOR);
}

void TextWindow::ScreenChangeBackFaces(int link, uint32_t v) {
    SS.ToggleSetting(SettingToggle::DRAW_BACK_FACES);
}

void TextWindow::ScreenChangeTurntableNav(int link, uint32_t v) {
    SS.ToggleSetting(SettingToggle::TURNTABLE_NAV);
}

void TextWindow::ScreenChangeCameraNav(int link, uint32_t v) {
    SS.ToggleSetting(SettingToggle::CAMERA_NAV);
}

void TextWindow::ScreenChangeImmediatelyEditDimension(int link, uint32_t v) {
    SS.ToggleSetting(SettingToggle::IMMEDIATELY_EDIT_DIMENSION);
}

void TextWindow::ScreenChangeShowContourAreas(int link, uint32_t v) {
    SS.ToggleSetting(SettingToggle::SHOW_CONTOUR_AREAS);
}

void TextWindow::ScreenChangeCheckClosedContour(int link, uint32_t v) {
    SS.ToggleSetting(SettingToggle::CHECK_CLOSED_CONTOUR);
}

void TextWindow::ScreenChangeAutomaticLineConstraints(int link, uint32_t v) {
    SS.ToggleSetting(SettingToggle::AUTOMATIC_LINE_CONSTRAINTS);
}

void TextWindow::ScreenChangeShadedTriangles(int link, uint32_t v) {
    SS.ToggleSetting(SettingToggle::EXPORT_SHADED_TRIANGLES);
}

void TextWindow::ScreenChangePwlCurves(int link, uint32_t v) {
    SS.ToggleSetting(SettingToggle::EXPORT_PWL_CURVES);
}

void TextWindow::ScreenChangeCanvasSizeAuto(int link, uint32_t v) {
    if(SS.exportCanvasSizeAuto != (link == 't')) {
        SS.ToggleSetting(SettingToggle::EXPORT_CANVAS_SIZE_AUTO);
    }
}

void TextWindow::ScreenChangeCanvasSize(int link, uint32_t v) {
    double d;
    switch(v) {
        case  0: d = SS.exportMargin.left;      break;
        case  1: d = SS.exportMargin.right;     break;
        case  2: d = SS.exportMargin.bottom;    break;
        case  3: d = SS.exportMargin.top;       break;

        case 10: d = SS.exportCanvas.width;     break;
        case 11: d = SS.exportCanvas.height;    break;
        case 12: d = SS.exportCanvas.dx;        break;
        case 13: d = SS.exportCanvas.dy;        break;

        default: return;
    }
    int col = 13;
    if(v < 10) col = 11;
    SS.TW.ShowEditControl(col, SS.MmToString(d, true));
    SS.TW.edit.meaning = Edit::CANVAS_SIZE;
    SS.TW.edit.i = v;
}

void TextWindow::ScreenChangeGCodeParameter(int link, uint32_t v) {
    std::string buf;
    switch(link) {
        case 'd':
            SS.TW.edit.meaning = Edit::G_CODE_DEPTH;
            buf += SS.MmToString(SS.gCode.depth, true);
            break;

        case 'h':
            SS.TW.edit.meaning = Edit::G_CODE_SAFE_HEIGHT;
            buf += SS.MmToString(SS.gCode.safeHeight, true);
            break;

        case 's':
            SS.TW.edit.meaning = Edit::G_CODE_PASSES;
            buf += std::to_string(SS.gCode.passes);
            break;

        case 'F':
            SS.TW.edit.meaning = Edit::G_CODE_FEED;
            buf += SS.MmToString(SS.gCode.feed, true);
            break;

        case 'P':
            SS.TW.edit.meaning = Edit::G_CODE_PLUNGE_FEED;
            buf += SS.MmToString(SS.gCode.plungeFeed, true);
            break;
    }
    SS.TW.ShowEditControl(14, buf);
}

void TextWindow::ScreenChangeAutosaveInterval(int link, uint32_t v) {
    SS.TW.ShowEditControl(3, std::to_string(SS.autosaveInterval));
    SS.TW.edit.meaning = Edit::AUTOSAVE_INTERVAL;
}

void TextWindow::ScreenChangeFindConstraintTimeout(int link, uint32_t v) {
    SS.TW.ShowEditControl(3, std::to_string(SS.timeoutRedundantConstr));
    SS.TW.edit.meaning = Edit::FIND_CONSTRAINT_TIMEOUT;
}

void TextWindow::ScreenChangeAnimationSpeed(int link, uint32_t v) {
    SS.TW.ShowEditControl(3, std::to_string(SS.animationSpeed));
    SS.TW.edit.meaning = Edit::ANIMATION_SPEED;
}

void TextWindow::ShowConfiguration() {
    int i;
    Printf(true, "%Ft user color (r, g, b)");

    for(i = 0; i < SS.MODEL_COLORS; i++) {
        Printf(false, "%Bp   #%d:  %Bz  %Bp  (%@, %@, %@) %f%D%Ll%Fl[change]%E",
            (i & 1) ? 'd' : 'a',
            i, &SS.modelColor[i],
            (i & 1) ? 'd' : 'a',
            SS.modelColor[i].redF(),
            SS.modelColor[i].greenF(),
            SS.modelColor[i].blueF(),
            &ScreenChangeColor, i);
    }

    Printf(false, "");
    Printf(false, "%Ft chord tolerance (in percents)%E");
    Printf(false, "%Ba   %@ %% %Fl%Ll%f%D[change]%E; %@ mm, %d triangles",
        SS.chordTol,
        &ScreenChangeChordTolerance, 0, SS.chordTolCalculated,
        SK.GetGroup(SS.GW.activeGroup)->displayMesh.l.n);
    Printf(false, "%Ft max piecewise linear segments%E");
    Printf(false, "%Ba   %d %Fl%Ll%f[change]%E",
        SS.maxSegments,
        &ScreenChangeMaxSegments);

    Printf(false, "");
    Printf(false, "%Ft export chord tolerance (in mm)%E");
    Printf(false, "%Ba   %@ %Fl%Ll%f%D[change]%E",
        SS.exportChordTol,
        &ScreenChangeExportChordTolerance, 0);
    Printf(false, "%Ft export max piecewise linear segments%E");
    Printf(false, "%Ba   %d %Fl%Ll%f[change]%E",
        SS.exportMaxSegments,
        &ScreenChangeExportMaxSegments);

    Printf(false, "%Ft snap grid spacing%E");
    Printf(false, "%Ba   %s %Fl%Ll%f%D[change]%E",
        SS.MmToString(SS.gridSpacing).c_str(),
        &ScreenChangeGridSpacing, 0);

    Printf(false, "");
    Printf(false, "%Ft digits after decimal point to show%E");
    Printf(false, "%Ba%Ft   distances: %Fd%d %Fl%Ll%f%D[change]%E (e.g. '%s')",
        SS.UnitDigitsAfterDecimal(),
        &ScreenChangeDigitsAfterDecimal, 0,
        SS.MmToString(SS.StringToMm("1.23456789")).c_str());
    Printf(false, "%Bd%Ft   angles:    %Fd%d %Fl%Ll%f%D[change]%E (e.g. '%s')",
        SS.afterDecimalDegree,
        &ScreenChangeDigitsAfterDecimalDegree, 0,
        SS.DegreeToString(1.23456789).c_str());
    Printf(false, "  %Fd%f%Ll%s  use SI prefixes for distances%E",
        &ScreenChangeUseSIPrefixes,
        SS.useSIPrefixes ? CHECK_TRUE : CHECK_FALSE);

    Printf(false, "");
    Printf(false, "%Ft export scale factor (1:1=mm, 1:25.4=inch)");
    Printf(false, "%Ba   1:%# %Fl%Ll%f%D[change]%E",
        (double)SS.exportScale,
        &ScreenChangeExportScale, 0);
    Printf(false, "%Ft cutter radius offset (0=no offset) ");
    Printf(false, "%Ba   %s %Fl%Ll%f%D[change]%E",
        SS.MmToString(SS.exportOffset).c_str(),
        &ScreenChangeExportOffset, 0);

    Printf(false, "");
    Printf(false, "  %Fd%f%Ll%s  export shaded 2d triangles%E",
        &ScreenChangeShadedTriangles,
        SS.exportShadedTriangles ? CHECK_TRUE : CHECK_FALSE);
    if(fabs(SS.exportOffset) > LENGTH_EPS) {
        Printf(false, "  %Fd%s  curves as piecewise linear%E "
                      "(since cutter radius is not zero)", CHECK_TRUE);
    } else {
        Printf(false, "  %Fd%f%Ll%s  export curves as piecewise linear%E",
            &ScreenChangePwlCurves,
            SS.exportPwlCurves ? CHECK_TRUE : CHECK_FALSE);
    }
    Printf(false, "  %Fd%f%Ll%s  fix white exported lines%E",
        &ScreenChangeFixExportColors,
        SS.fixExportColors ? CHECK_TRUE : CHECK_FALSE);
    Printf(false, "  %Fd%f%Ll%s  export background color%E",
        &ScreenChangeExportBackgroundColor,
        SS.exportBackgroundColor ? CHECK_TRUE : CHECK_FALSE);

    Printf(false, "");
    Printf(false, "%Ft export canvas size:  "
                  "%f%Fd%Lf%s fixed%E  "
                  "%f%Fd%Lt%s auto%E",
        &ScreenChangeCanvasSizeAuto,
        !SS.exportCanvasSizeAuto ? RADIO_TRUE : RADIO_FALSE,
        &ScreenChangeCanvasSizeAuto,
        SS.exportCanvasSizeAuto ? RADIO_TRUE : RADIO_FALSE);

    if(SS.exportCanvasSizeAuto) {
        Printf(false, "%Ft (by margins around exported geometry)");
        Printf(false, "%Ba%Ft   left:   %Fd%s %Fl%Ll%f%D[change]%E",
            SS.MmToString(SS.exportMargin.left).c_str(), &ScreenChangeCanvasSize, 0);
        Printf(false, "%Bd%Ft   right:  %Fd%s %Fl%Ll%f%D[change]%E",
            SS.MmToString(SS.exportMargin.right).c_str(), &ScreenChangeCanvasSize, 1);
        Printf(false, "%Ba%Ft   bottom: %Fd%s %Fl%Ll%f%D[change]%E",
            SS.MmToString(SS.exportMargin.bottom).c_str(), &ScreenChangeCanvasSize, 2);
        Printf(false, "%Bd%Ft   top:    %Fd%s %Fl%Ll%f%D[change]%E",
            SS.MmToString(SS.exportMargin.top).c_str(), &ScreenChangeCanvasSize, 3);
    } else {
        Printf(false, "%Ft (by absolute dimensions and offsets)");
        Printf(false, "%Ba%Ft   width:    %Fd%s %Fl%Ll%f%D[change]%E",
            SS.MmToString(SS.exportCanvas.width).c_str(), &ScreenChangeCanvasSize, 10);
        Printf(false, "%Bd%Ft   height:   %Fd%s %Fl%Ll%f%D[change]%E",
            SS.MmToString(SS.exportCanvas.height).c_str(), &ScreenChangeCanvasSize, 11);
        Printf(false, "%Ba%Ft   offset x: %Fd%s %Fl%Ll%f%D[change]%E",
            SS.MmToString(SS.exportCanvas.dx).c_str(), &ScreenChangeCanvasSize, 12);
        Printf(false, "%Bd%Ft   offset y: %Fd%s %Fl%Ll%f%D[change]%E",
            SS.MmToString(SS.exportCanvas.dy).c_str(), &ScreenChangeCanvasSize, 13);
    }

    Printf(false, "");
    Printf(false, "%Ft exported g code parameters");
    Printf(false, "%Ba%Ft   depth:     %Fd%s %Fl%Ld%f[change]%E",
        SS.MmToString(SS.gCode.depth).c_str(), &ScreenChangeGCodeParameter);
    Printf(false, "%Bd%Ft   passes:    %Fd%d %Fl%Ls%f[change]%E",
        SS.gCode.passes, &ScreenChangeGCodeParameter);
    Printf(false, "%Ba%Ft   feed:      %Fd%s %Fl%LF%f[change]%E",
        SS.MmToString(SS.gCode.feed).c_str(), &ScreenChangeGCodeParameter);
    Printf(false, "%Bd%Ft   plunge fd: %Fd%s %Fl%LP%f[change]%E",
        SS.MmToString(SS.gCode.plungeFeed).c_str(), &ScreenChangeGCodeParameter);

    Printf(false, "");
    Printf(false, "  %Fd%f%Ll%s  draw triangle back faces in red%E",
        &ScreenChangeBackFaces,
        SS.drawBackFaces ? CHECK_TRUE : CHECK_FALSE);
    Printf(false, "  %Fd%f%Ll%s  check sketch for closed contour%E",
        &ScreenChangeCheckClosedContour,
        SS.checkClosedContour ? CHECK_TRUE : CHECK_FALSE);
    Printf(false, "  %Fd%f%Ll%s  show areas of closed contours%E",
        &ScreenChangeShowContourAreas,
        SS.showContourAreas ? CHECK_TRUE : CHECK_FALSE);
    Printf(false, "  %Fd%f%Ll%s  enable automatic line constraints%E",
        &ScreenChangeAutomaticLineConstraints,
        SS.automaticLineConstraints ? CHECK_TRUE : CHECK_FALSE);
    Printf(false, "  %Fd%f%Ll%s  use camera mouse navigation%E", &ScreenChangeCameraNav,
        SS.cameraNav ? CHECK_TRUE : CHECK_FALSE);
    Printf(false, "  %Fd%f%Ll%s  use turntable mouse navigation%E", &ScreenChangeTurntableNav,
        SS.turntableNav ? CHECK_TRUE : CHECK_FALSE);
    Printf(false, "  %Fd%f%Ll%s  edit newly added dimensions%E",
        &ScreenChangeImmediatelyEditDimension,
        SS.immediatelyEditDimension ? CHECK_TRUE : CHECK_FALSE);
    Printf(false, "  %Fd%f%Ll%s  arc default is diameter%E",
        &ScreenChangeArcDimDefault,
        SS.arcDimDefaultDiameter ? CHECK_TRUE : CHECK_FALSE);
    Printf(false, "  %Fd%f%Ll%s  display the full path in the title bar%E",
           &ScreenChangeShowFullFilePath, SS.showFullFilePath ? CHECK_TRUE : CHECK_FALSE);
    Printf(false, "");
    Printf(false, "%Ft autosave interval (in minutes)%E");
    Printf(false, "%Ba   %d %Fl%Ll%f[change]%E",
        SS.autosaveInterval, &ScreenChangeAutosaveInterval);
    Printf(false, "");
    Printf(false, "%Ft redundant constraint timeout (in ms)%E");
    Printf(false, "%Ba   %d %Fl%Ll%f[change]%E",
        SS.timeoutRedundantConstr, &ScreenChangeFindConstraintTimeout);
    Printf(false, "");
    Printf(false, "%Ft animation speed (in ms; 0 to disable)%E");
    Printf(false, "%Ba   %d %Fl%Ll%f[change]%E",
        SS.animationSpeed, &ScreenChangeAnimationSpeed);

    if(canvas) {
        const char *gl_vendor, *gl_renderer, *gl_version;
        canvas->GetIdent(&gl_vendor, &gl_renderer, &gl_version);
        Printf(false, "");
        Printf(false, " %Ftgl vendor   %E%s", gl_vendor);
        Printf(false, " %Ft   renderer %E%s", gl_renderer);
        Printf(false, " %Ft   version  %E%s", gl_version);
    }

    #if defined(_OPENMP)
        Printf(false, " %FtOpenMP enabled");
        Printf(false, " %Ft   threads  %E%d", omp_get_max_threads());
    #endif
}

bool TextWindow::EditControlDoneForConfiguration(const std::string &s) {
    Setting setting;
    int     index = 0;
    switch(edit.meaning) {
        case Edit::LIGHT_INTENSITY:   setting = Setting::LIGHT_INTENSITY; index = edit.i; break;
        case Edit::LIGHT_AMBIENT:     setting = Setting::LIGHT_AMBIENT; break;
        case Edit::LIGHT_DIRECTION:   setting = Setting::LIGHT_DIRECTION; index = edit.i; break;
        case Edit::COLOR:             setting = Setting::MODEL_COLOR; index = edit.i; break;
        case Edit::CHORD_TOLERANCE:
            setting = edit.i == 0 ? Setting::CHORD_TOLERANCE : Setting::EXPORT_CHORD_TOLERANCE;
            break;
        case Edit::MAX_SEGMENTS:
            setting = edit.i == 0 ? Setting::MAX_SEGMENTS : Setting::EXPORT_MAX_SEGMENTS;
            break;
        case Edit::CAMERA_TANGENT:    setting = Setting::CAMERA_TANGENT; break;
        case Edit::GRID_SPACING:      setting = Setting::GRID_SPACING; break;
        case Edit::EXPLODE_DISTANCE:  setting = Setting::EXPLODE_DISTANCE; break;
        case Edit::DIGITS_AFTER_DECIMAL: setting = Setting::DIGITS_AFTER_DECIMAL; break;
        case Edit::DIGITS_AFTER_DECIMAL_DEGREE:
            setting = Setting::DIGITS_AFTER_DECIMAL_DEGREE;
            break;
        case Edit::EXPORT_SCALE:      setting = Setting::EXPORT_SCALE; break;
        case Edit::EXPORT_OFFSET:     setting = Setting::EXPORT_OFFSET; break;
        case Edit::CANVAS_SIZE: {
            static const Setting canvas[] = {
                Setting::MARGIN_LEFT, Setting::MARGIN_RIGHT, Setting::MARGIN_BOTTOM,
                Setting::MARGIN_TOP, Setting::CANVAS_WIDTH, Setting::CANVAS_HEIGHT,
                Setting::CANVAS_DX, Setting::CANVAS_DY };
            if(edit.i >= 0 && edit.i < 4) {
                setting = canvas[edit.i];
            } else if(edit.i >= 10 && edit.i < 14) {
                setting = canvas[edit.i - 6];
            } else {
                return true;
            }
            break;
        }
        case Edit::G_CODE_DEPTH:       setting = Setting::GCODE_DEPTH; break;
        case Edit::G_CODE_SAFE_HEIGHT: setting = Setting::GCODE_SAFE_HEIGHT; break;
        case Edit::G_CODE_PASSES:      setting = Setting::GCODE_PASSES; break;
        case Edit::G_CODE_FEED:        setting = Setting::GCODE_FEED; break;
        case Edit::G_CODE_PLUNGE_FEED: setting = Setting::GCODE_PLUNGE_FEED; break;
        case Edit::AUTOSAVE_INTERVAL:  setting = Setting::AUTOSAVE_INTERVAL; break;
        case Edit::FIND_CONSTRAINT_TIMEOUT: setting = Setting::FIND_CONSTRAINT_TIMEOUT; break;
        case Edit::ANIMATION_SPEED:    setting = Setting::ANIMATION_SPEED; break;
        default: return false;
    }
    SS.SetSetting(setting, s, index);
    return true;
}

} // namespace SolveSpace
