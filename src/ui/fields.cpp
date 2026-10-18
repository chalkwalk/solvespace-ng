//-----------------------------------------------------------------------------
// Panel fields.
//-----------------------------------------------------------------------------
#include <map>
#include "imgui.h"
#include "imgui_internal.h"
#include "ui/fields.h"

namespace SolveSpace {
namespace Ui {

void ValueField(const char *label, const std::string &value, std::function<void()> *run,
                std::function<void(std::string)> apply, const char *suffix) {
    static std::map<ImGuiID, std::string> editing;
    ImGuiID     id   = ImGui::GetID(label);
    std::string text = editing.count(id) ? editing[id] : value;
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", text.c_str());
    bool hasSuffix = suffix && *suffix;
    ImGui::SetNextItemWidth(hasSuffix ? -(ImGui::CalcTextSize(suffix).x +
                                          ImGui::GetStyle().ItemSpacing.x)
                                      : -FLT_MIN);
    bool entered = ImGui::InputText(label, buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue);
    bool active  = ImGui::IsItemActive();
    bool edited  = ImGui::IsItemDeactivatedAfterEdit();
    if(hasSuffix) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", suffix);
    }
    if(active) editing[id] = buf;
    if(entered || edited) {
        std::string s = buf;
        if(s != value) *run = [apply, s] { apply(s); };
        editing.erase(id);
    } else if(!active) {
        editing.erase(id);
    }
}

bool ColourField(const char *label, const float current[3], float rgb[3], bool *gestureStart) {
    static ImGuiID gesture = 0; // the field the current click or drag in a picker belongs to
    if(!ImGui::IsAnyItemActive()) gesture = 0;

    ImGui::PushID(label);
    ImVec4 c(current[0], current[1], current[2], 1);
    ImGui::ColorButton("##swatch", c, ImGuiColorEditFlags_NoTooltip,
                       ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()));
    bool open = ImGui::IsItemClicked();
    ImGui::SameLine();
    // A real button with the label, so the field can be found (ColorButton reports none).
    if(ImGui::Button(label)) open = true;
    if(open) ImGui::OpenPopup("picker");
    bool changed = false;
    if(ImGui::BeginPopup("picker")) {
        float v[3] = { current[0], current[1], current[2] };
        // Colours are stored with 8 bits a channel: the picker's own rounding is no change
        // (it would otherwise "change" the colour on every frame it is open).
        auto byte  = [](float f) { return (int)(255.1f * f); }; // as RgbaColor::FromFloat
        bool moved = ImGui::ColorPicker3("##picker", v, ImGuiColorEditFlags_NoSidePreview) &&
                     (byte(v[0]) != byte(current[0]) || byte(v[1]) != byte(current[1]) ||
                      byte(v[2]) != byte(current[2]));
        if(moved) {
            ImGuiID id    = ImGui::GetID("picker");
            *gestureStart = gesture != id;
            gesture       = id;
            rgb[0]        = v[0];
            rgb[1]        = v[1];
            rgb[2]        = v[2];
            changed       = true;
        }
        ImGui::EndPopup();
    }
    ImGui::PopID();
    return changed;
}

}
}
