//-----------------------------------------------------------------------------
// The command palette: fuzzy search over every menu item.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "commandnames.h"
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

static std::string ShortcutFor(const Platform::MenuItemSdl &item, const keymap::Keymap &keymap) {
    if(!item.commandId) return "";
    const char *name = CommandName((Command)item.commandId);
    if(!name) return "";
    auto chords = keymap.ChordsFor(name);
    return chords.empty() ? "" : keymap::FormatChord(chords[0]);
}

std::shared_ptr<Platform::MenuItemSdl> CommandPalette::Draw(
    const std::vector<Platform::FlatMenuItem> &items, const keymap::Keymap &keymap) {
    if(!open) return nullptr;
    ImGuiIO &io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.2f),
                            ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(std::min(640.0f, io.DisplaySize.x - 32), 0));
    ImGui::Begin("##palette", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_AlwaysAutoResize);
    if(!fieldActive) {
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
    for(const auto &it : items) labels.push_back(it.path);
    std::vector<size_t> ranked = palette::Rank(query, labels, 12);
    if(selected >= (int)ranked.size()) selected = std::max(0, (int)ranked.size() - 1);
    if(ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
        selected = std::min(selected + 1, std::max(0, (int)ranked.size() - 1));
    }
    if(ImGui::IsKeyPressed(ImGuiKey_UpArrow)) selected = std::max(selected - 1, 0);

    int activate = -1;
    for(int i = 0; i < (int)ranked.size(); i++) {
        const Platform::FlatMenuItem &fi = items[ranked[i]];
        std::string shortcut = ShortcutFor(*fi.item, keymap);
        ImGui::PushID(i);
        ImGui::BeginDisabled(!fi.item->enabled);
        float lineRight = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
        if(ImGui::Selectable(fi.path.c_str(), i == selected)) activate = i;
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
        const Platform::FlatMenuItem &fi = items[ranked[activate]];
        open = false;
        if(fi.item->enabled) return fi.item;
    } else if(close) {
        open = false;
    }
    return nullptr;
}

}
}
