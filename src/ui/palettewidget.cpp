//-----------------------------------------------------------------------------
// The command palette: fuzzy search over every menu item.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "fuzzy.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "ui/palettewidget.h"

namespace SolveSpace {
namespace Ui {

void CommandPalette::Open() {
    open        = true;
    framesOpen  = 0;
    fieldActive = false;
    query[0]   = '\0';
    selected   = 0;
}

bool CommandPalette::IsOpen() const {
    return open;
}

std::function<void()> CommandPalette::Draw(const std::vector<PaletteEntry> &entries,
                                           const keymap::Keymap &keymap) {
    if(!open) return {};
    ImGuiIO &io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.2f),
                            ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(std::min(640.0f, io.DisplaySize.x - 32), 0));
    ImGui::Begin("##palette", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_AlwaysAutoResize);
    // Not while another item (a row being clicked) is active: taking focus would cancel it.
    if(!fieldActive && !ImGui::IsAnyItemActive()) {
        // Characters typed right after opening arrive before the field can take focus; keep them.
        std::string typed;
        for(ImWchar c : io.InputQueueCharacters) {
            if(c >= 0x20 && c < 0x7f) typed += (char)c;
        }
        io.InputQueueCharacters.resize(0);
        strncat(query, typed.c_str(), sizeof(query) - strlen(query) - 1);
        ImGui::SetKeyboardFocusHere();
    }
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##query", "Type a command...", query, sizeof(query));
    bool justActivated = !fieldActive && ImGui::IsItemActive();
    fieldActive = ImGui::IsItemActive();
    if(justActivated) {
        // Focusing selects the field's text; put the cursor after what was already typed.
        if(ImGuiInputTextState *state = ImGui::GetInputTextState(ImGui::GetItemID())) {
            int end = (int)strlen(query);
            state->SetSelection(end, end);
        }
    }

    std::vector<std::string> labels;
    for(const auto &e : entries) {
        labels.push_back(e.keywords.empty() ? e.label : e.label + " (" + e.keywords + ")");
    }
    std::vector<size_t> ranked = palette::Rank(query, labels, 12);
    if(selected >= (int)ranked.size()) selected = std::max(0, (int)ranked.size() - 1);
    if(ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
        selected = std::min(selected + 1, std::max(0, (int)ranked.size() - 1));
    }
    if(ImGui::IsKeyPressed(ImGuiKey_UpArrow)) selected = std::max(selected - 1, 0);

    int activate = -1;
    for(int i = 0; i < (int)ranked.size(); i++) {
        const PaletteEntry &e = entries[ranked[i]];
        std::string shortcut;
        for(const std::string &name : { e.commandName, e.keyAlias }) {
            if(name.empty() || !shortcut.empty()) continue;
            auto chords = keymap.ChordsFor(name);
            if(!chords.empty()) shortcut = keymap::FormatChord(chords[0]);
        }
        ImGui::PushID(i);
        ImGui::BeginDisabled(!e.enabled);
        float lineRight = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
        if(ImGui::Selectable(e.label.c_str(), i == selected)) activate = i;
        if(!shortcut.empty()) {
            ImGui::SameLine(lineRight - ImGui::CalcTextSize(shortcut.c_str()).x);
            ImGui::TextDisabled("%s", shortcut.c_str());
        }
        ImGui::EndDisabled();
        ImGui::PopID();
    }
    if(ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
        activate = selected;
    }
    // Give the window a couple of frames to take focus before treating "unfocused" as dismissal.
    bool close = ImGui::IsKeyPressed(ImGuiKey_Escape) ||
                 (framesOpen > 1 &&
                  !ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows));
    ImGui::End();
    framesOpen++;

    if(activate >= 0 && activate < (int)ranked.size()) {
        const PaletteEntry &e = entries[ranked[activate]];
        open = false;
        if(e.enabled) return e.run;
    } else if(close) {
        open = false;
    }
    return {};
}

}
}
