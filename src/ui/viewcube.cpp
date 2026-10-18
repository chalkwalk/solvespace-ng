//-----------------------------------------------------------------------------
// The view cube, in the bottom left of the viewport with SolveSpace's reference X/Y/Z axes
// coming out of it (decided with the owner, 2026-10-17: where SolveSpace has always drawn its
// axes). The axes are the canvas's own entities, still selectable for constraints; the cube
// takes the mouse only over its faces, so clicks beside it reach them.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "ui/viewcube.h"
#include "ui/viewcubemodel.h"

namespace SolveSpace {
namespace Ui {

static const double CubeSize   = 24;  // half the side of a face seen square on
static const double AxisLength = 62;  // from the cube's centre, past its corners
static const float  Box        = 150; // the area cube, axes and labels may cover

static Point2d gCenter;
static bool    gPressed = false, gDragging = false;
static Point2d gPressAt;

Point2d ViewCubeCenter() {
    return gCenter;
}

double ViewCubeSize() {
    return CubeSize;
}

bool ViewCubeTakesMouseAt(double x, double y, bool canvasPressed) {
    if(gPressed) return true; // a press on the cube keeps the mouse until it is released
    if(canvasPressed) return false;
    return ViewCubePick(SS.GW.projRight, SS.GW.projUp, gCenter, CubeSize, Point2d::From(x, y))
        .has_value();
}

static ImVec2 V(Point2d p) {
    return ImVec2((float)p.x, (float)p.y);
}

// The bottom left corner of the viewport, above the status bar, right of any panel there.
static ImVec2 Placement() {
    ImGuiIO &io     = ImGui::GetIO();
    float    bottom = io.DisplaySize.y - 12;
    if(ImGuiWindow *status = ImGui::FindWindowByName("##statusbar")) {
        if(status->WasActive) bottom = status->Pos.y - 8;
    }
    ImRect box(12, bottom - Box, 12 + Box, bottom);
    for(int pass = 0; pass < 8; pass++) {
        bool moved = false;
        for(ImGuiWindow *w : ImGui::GetCurrentContext()->Windows) {
            if(!w->WasActive || w->Hidden || w->ParentWindow ||
               (w->Flags & (ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_Popup |
                            ImGuiWindowFlags_NoBackground))) {
                continue; // (the pie and the cube itself have no background)
            }
            ImRect r = w->Rect();
            if(!r.Overlaps(box) || r.Min.y < 1 || strcmp(w->Name, "##statusbar") == 0) continue;
            float shift = (r.Max.x + 12) - box.Min.x;
            box.Min.x += shift;
            box.Max.x += shift;
            moved = true;
        }
        if(!moved) break;
    }
    // No free spot (a small window with both side panels): stay in the corner, on screen.
    if(box.Max.x > io.DisplaySize.x) return ImVec2(12, bottom - Box);
    return box.Min;
}

std::function<void()> DrawViewCube() {
    std::function<void()> run;
    ImGuiIO &io = ImGui::GetIO();
    ImVec2   at = Placement();
    // No inputs: the cube's faces take the mouse (ViewCubeTakesMouseAt), the rest of the area
    // belongs to the canvas, where the axes are.
    ImGui::SetNextWindowPos(at);
    ImGui::SetNextWindowSize(ImVec2(Box, Box));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##viewcube-window", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav |
                 ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing |
                 ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar();
    Point2d center = Point2d::From(at.x + Box / 2, at.y + Box / 2);
    if(!center.Equals(gCenter)) {
        gCenter = center;
        // The canvas draws the axes from here (pixels from the view centre, y up).
        SS.GW.referenceAxesAnchor =
            Point2d::From(center.x - io.DisplaySize.x / 2, io.DisplaySize.y / 2 - center.y);
        SS.GW.referenceAxesLength = AxisLength;
        SS.GW.Invalidate(/*clearPersistent=*/true);
    }
    // The cube's area, findable by the UI tests as "viewcube" (the window takes no input).
    ImGui::InvisibleButton("##viewcube", ImVec2(Box, Box));

    Vector  right = SS.GW.projRight, up = SS.GW.projUp;
    Point2d mouse = Point2d::From(io.MousePos.x, io.MousePos.y);
    bool    overOtherUi = ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);
    std::optional<Vector> hovered;
    if(!overOtherUi || gPressed) hovered = ViewCubePick(right, up, gCenter, CubeSize, mouse);
    // Press on a face, then drag to orbit (as a middle-button drag) or release to look from
    // what was pressed.
    if(ImGui::IsMouseClicked(ImGuiMouseButton_Left) && hovered && !overOtherUi) {
        gPressed  = true;
        gDragging = false;
        gPressAt  = mouse;
    }
    if(gPressed) {
        if(!gDragging && mouse.DistanceTo(gPressAt) > 3) gDragging = true;
        if(gDragging) {
            double dx = io.MouseDelta.x, dy = io.MouseDelta.y;
            if(dx != 0 || dy != 0) run = [dx, dy] { SS.GW.OrbitBy(dx, dy); };
            hovered.reset();
        }
        if(ImGui::IsMouseReleased(ImGuiMouseButton_Left) || !io.MouseDown[0]) {
            if(!gDragging && hovered) {
                Vector dir = *hovered;
                run = [dir] { SS.GW.SnapToDirection(dir, /*animate=*/true); };
            }
            gPressed = gDragging = false;
        }
    }

    // See-through until the mouse is on it, so it hides little of the model (as FreeCAD's).
    float opacity = (hovered || gPressed) ? 0.97f : 0.8f;
    ImDrawList *dl     = ImGui::GetWindowDrawList();
    ImVec4      face   = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
    ImVec4      text   = ImGui::GetStyleColorVec4(ImGuiCol_Text);
    // Edges in the text colour, so the cube reads in either theme.
    ImU32       border = ImGui::GetColorU32(ImGuiCol_Text, 0.45f * opacity);
    face.w = opacity;
    for(const CubeFace &f : ViewCubeFaces(right, up, gCenter, CubeSize)) {
        if(!f.visible) continue;
        // Faces turned away from the viewer are drawn a little darker.
        float  shade = 0.6f + 0.4f * (float)f.facing;
        ImVec4 c(face.x * shade, face.y * shade, face.z * shade, face.w);
        ImVec2 q[4] = { V(f.corners[0]), V(f.corners[1]), V(f.corners[2]), V(f.corners[3]) };
        dl->AddConvexPolyFilled(q, 4, ImGui::GetColorU32(c));
        dl->AddPolyline(q, 4, border, 1.5f, ImDrawFlags_Closed);
        if(f.facing > 0.4) {
            ImVec2 mid((q[0].x + q[2].x) / 2, (q[0].y + q[2].y) / 2);
            ImVec2 ts = ImGui::CalcTextSize(f.label.c_str());
            ImVec4 t  = text;
            t.w       = std::min(1.0f, (float)f.facing * 1.4f) * opacity;
            dl->AddText(ImVec2(mid.x - ts.x / 2, mid.y - ts.y / 2), ImGui::GetColorU32(t),
                        f.label.c_str());
        }
    }
    if(hovered) {
        ImU32 hi = ImGui::GetColorU32(ImGuiCol_ButtonHovered, 0.85f);
        for(const auto &quad : ViewCubeRegion(right, up, gCenter, CubeSize, *hovered)) {
            ImVec2 q[4] = { V(quad[0]), V(quad[1]), V(quad[2]), V(quad[3]) };
            dl->AddConvexPolyFilled(q, 4, hi);
        }
    }
    // Name the axes at their tips, in the canvas's axis colours.
    const struct { const char *name; Vector dir; ImU32 colour; } axes[] = {
        { "X", { 1, 0, 0 }, IM_COL32(230, 60, 60, 255) },
        { "Y", { 0, 1, 0 }, IM_COL32(40, 180, 60, 255) },
        { "Z", { 0, 0, 1 }, IM_COL32(70, 120, 255, 255) },
    };
    for(const auto &a : axes) {
        double sx = a.dir.Dot(right), sy = -a.dir.Dot(up);
        double len = sqrt(sx * sx + sy * sy);
        if(len < 0.25) continue; // pointing at or away from the viewer: no room for a label
        double  d   = AxisLength + 9;
        ImVec2  ts  = ImGui::CalcTextSize(a.name);
        ImVec2  pos((float)(gCenter.x + sx * d - ts.x / 2), (float)(gCenter.y + sy * d - ts.y / 2));
        dl->AddText(pos, a.colour, a.name);
    }
    ImGui::End();
    return run;
}

}
}
