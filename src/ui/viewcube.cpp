//-----------------------------------------------------------------------------
// The view cube.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "ui/viewcube.h"
#include "ui/viewcubemodel.h"

namespace SolveSpace {
namespace Ui {

static const double  CubeSize = 26; // half the side of a face seen square on
static const float   Box      = 100; // the area it may cover, for any orientation
static Point2d       gCenter;

Point2d ViewCubeCenter() {
    return gCenter;
}

double ViewCubeSize() {
    return CubeSize;
}

static ImVec2 V(Point2d p) {
    return ImVec2((float)p.x, (float)p.y);
}

// The top right corner of the viewport, left of any panel there.
static ImVec2 Placement() {
    ImGuiIO &io  = ImGui::GetIO();
    float    top = ImGui::GetFrameHeight() + 12;
    ImRect   box(io.DisplaySize.x - 12 - Box, top, io.DisplaySize.x - 12, top + Box);
    for(int pass = 0; pass < 8; pass++) {
        bool moved = false;
        for(ImGuiWindow *w : ImGui::GetCurrentContext()->Windows) {
            if(!w->WasActive || w->Hidden || w->ParentWindow ||
               (w->Flags & (ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_Popup |
                            ImGuiWindowFlags_NoBackground))) {
                continue; // (the pie and the cube itself have no background)
            }
            ImRect r = w->Rect();
            if(!r.Overlaps(box) || r.Min.y < 1) continue; // the menu bar sits above
            float shift = box.Max.x - (r.Min.x - 12);
            box.Min.x -= shift;
            box.Max.x -= shift;
            moved = true;
        }
        if(!moved) break;
    }
    return box.Min;
}

std::function<void()> DrawViewCube() {
    std::function<void()> run;
    ImVec2 at = Placement();
    ImGui::SetNextWindowPos(at);
    ImGui::SetNextWindowSize(ImVec2(Box, Box));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##viewcube-window", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav |
                 ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar();
    gCenter = Point2d::From(at.x + Box / 2, at.y + Box / 2);
    Vector right = SS.GW.projRight, up = SS.GW.projUp;

    ImGui::InvisibleButton("##viewcube", ImVec2(Box, Box));
    // See-through until the mouse is on it, so it hides little of the model (as FreeCAD's).
    bool  over    = ImGui::IsItemHovered() || ImGui::IsItemActive();
    float opacity = over ? 0.97f : 0.8f;
    ImGuiIO &io = ImGui::GetIO();
    Point2d  mouse = Point2d::From(io.MousePos.x, io.MousePos.y);
    std::optional<Vector> hovered;
    if(ImGui::IsItemHovered() || ImGui::IsItemActive()) {
        hovered = ViewCubePick(right, up, gCenter, CubeSize, mouse);
    }
    // Dragging the cube orbits, as a middle-button drag does; a click looks from what it hit.
    if(ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 3)) {
        double dx = io.MouseDelta.x, dy = io.MouseDelta.y;
        if(dx != 0 || dy != 0) run = [dx, dy] { SS.GW.OrbitBy(dx, dy); };
        hovered.reset();
    } else if(ImGui::IsItemDeactivated() && !ImGui::IsMouseDragPastThreshold(0, 3) && hovered) {
        Vector dir = *hovered;
        run = [dir] { SS.GW.SnapToDirection(dir, /*animate=*/true); };
    }

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
    ImGui::End();
    return run;
}

}
}
