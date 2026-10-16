//-----------------------------------------------------------------------------
// Panel fields.
//-----------------------------------------------------------------------------
#include <map>
#include "imgui.h"
#include "ui/fields.h"

namespace SolveSpace {
namespace Ui {

void ValueField(const char *label, const std::string &value, std::function<void()> *run,
                std::function<void(std::string)> apply) {
    static std::map<ImGuiID, std::string> editing;
    ImGuiID     id   = ImGui::GetID(label);
    std::string text = editing.count(id) ? editing[id] : value;
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", text.c_str());
    ImGui::SetNextItemWidth(-FLT_MIN);
    bool entered = ImGui::InputText(label, buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue);
    if(ImGui::IsItemActive()) editing[id] = buf;
    if(entered || ImGui::IsItemDeactivatedAfterEdit()) {
        std::string s = buf;
        *run = [apply, s] { apply(s); };
        editing.erase(id);
    } else if(!ImGui::IsItemActive()) {
        editing.erase(id);
    }
}

}
}
