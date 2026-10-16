//-----------------------------------------------------------------------------
// A radial (pie) menu drawn over the viewport.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "imgui.h"
#include "pie.h"
#include "ui/piemenu.h"

namespace SolveSpace {
namespace Ui {

PieMenu &Pie() {
    static PieMenu pie;
    return pie;
}

void PieMenu::Open(std::string t, std::vector<PieItem> it) {
    title       = std::move(t);
    items       = std::move(it);
    ImVec2 m    = ImGui::GetIO().MousePos;
    if(!ImGui::IsMousePosValid(&m)) m = ImVec2(ImGui::GetIO().DisplaySize.x / 2,
                                               ImGui::GetIO().DisplaySize.y / 2);
    cx          = m.x;
    cy          = m.y;
    highlighted = -1;
    picked      = -1;
    cancelled   = false;
    open        = true;
}

bool PieMenu::HandleChord(const keymap::Chord &c) {
    if(!open) return false;
    pie::KeyResult r = pie::KeyAction(c.key, c.ctrl || c.alt || c.shift, (int)items.size());
    switch(r.kind) {
        case pie::KeyKind::PICK:    picked = r.index; break;
        case pie::KeyKind::CONFIRM: picked = highlighted; break;
        case pie::KeyKind::CANCEL:  cancelled = true; break;
        case pie::KeyKind::NONE:    break;
    }
    return true;
}

std::string PieMenu::Draw() {
    if(!open) return "";
    ImGuiIO &io = ImGui::GetIO();
    // A transparent full-viewport window: it owns the mouse while the pie is open, so clicks
    // don't reach the canvas (the backend checks WantCaptureMouse).
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("##pie", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove);
    ImGui::InvisibleButton("##pie-capture", io.DisplaySize,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

    const float radius = 110.0f, dead = 24.0f;
    highlighted = pie::SectorAt(io.MousePos.x - cx, io.MousePos.y - cy, (int)items.size(), dead);
    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImGuiStyle &st = ImGui::GetStyle();
    dl->AddCircleFilled(ImVec2(cx, cy), dead, ImGui::GetColorU32(ImGuiCol_PopupBg));
    dl->AddCircle(ImVec2(cx, cy), dead, ImGui::GetColorU32(ImGuiCol_Border), 0, 2.0f);
    if(highlighted >= 0) {
        float dx, dy;
        pie::ItemDirection(highlighted, (int)items.size(), &dx, &dy);
        dl->AddLine(ImVec2(cx + dx * dead * 0.4f, cy + dy * dead * 0.4f),
                    ImVec2(cx + dx * dead, cy + dy * dead),
                    ImGui::GetColorU32(ImGuiCol_ButtonHovered), 4.0f);
    }
    ImVec2 ts = ImGui::CalcTextSize(title.c_str());
    dl->AddText(ImVec2(cx - ts.x / 2, cy - dead - ts.y - 6), ImGui::GetColorU32(ImGuiCol_Text),
                title.c_str());
    if(items.empty()) {
        const char *none = "Nothing applies to the selection";
        ImVec2 ns = ImGui::CalcTextSize(none);
        dl->AddText(ImVec2(cx - ns.x / 2, cy + dead + 6),
                    ImGui::GetColorU32(ImGuiCol_TextDisabled), none);
    }
    for(int i = 0; i < (int)items.size(); i++) {
        float dx, dy;
        pie::ItemDirection(i, (int)items.size(), &dx, &dy);
        std::string text = std::to_string(i + 1) + "  " + items[i].label;
        ImVec2 sz = ImGui::CalcTextSize(text.c_str());
        ImVec2 pad(st.FramePadding.x * 2, st.FramePadding.y * 2);
        ImVec2 c(cx + dx * radius, cy + dy * radius);
        // Push labels outward so they never overlap the centre on the left/right.
        c.x += dx * sz.x / 2;
        ImVec2 a(c.x - sz.x / 2 - pad.x, c.y - sz.y / 2 - pad.y);
        ImVec2 b(c.x + sz.x / 2 + pad.x, c.y + sz.y / 2 + pad.y);
        ImGuiCol bg = i == highlighted ? ImGuiCol_ButtonHovered : ImGuiCol_PopupBg;
        dl->AddRectFilled(a, b, ImGui::GetColorU32(bg), st.FrameRounding);
        dl->AddRect(a, b, ImGui::GetColorU32(ImGuiCol_Border), st.FrameRounding);
        dl->AddText(ImVec2(a.x + pad.x, a.y + pad.y),
                    ImGui::GetColorU32(items[i].enabled ? ImGuiCol_Text : ImGuiCol_TextDisabled),
                    text.c_str());
    }
    if(ImGui::IsMouseClicked(ImGuiMouseButton_Left)) picked = highlighted;
    if(ImGui::IsMouseClicked(ImGuiMouseButton_Right)) cancelled = true;
    // An empty pie only explains itself; any click or confirm closes it.
    if(items.empty() && (picked >= 0 || ImGui::IsMouseClicked(ImGuiMouseButton_Left))) {
        cancelled = true;
    }
    ImGui::End();

    std::string result;
    if(picked >= 0 && picked < (int)items.size() && items[picked].enabled) {
        result = items[picked].commandName;
        open   = false;
    } else if(cancelled) {
        open = false;
    }
    picked = -1;
    return result;
}

}
}
