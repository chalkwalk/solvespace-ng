//-----------------------------------------------------------------------------
// The command palette: fuzzy search over every menu item.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "commandnames.h"
#include "fuzzy.h"
#include "imgui.h"
#include "ui/palettewidget.h"

namespace SolveSpace {
namespace Ui {

void CommandPalette::Open() {
    open       = true;
    framesOpen = 0;
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

void CommandPalette::Draw(const std::vector<Platform::FlatMenuItem> &items,
                          const keymap::Keymap &keymap) {
    if(!open) return;
    ImGuiIO &io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.2f),
                            ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(std::min(640.0f, io.DisplaySize.x - 32), 0));
    ImGui::Begin("##palette", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_AlwaysAutoResize);
    if(framesOpen == 0) ImGui::SetKeyboardFocusHere();
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##query", "Type a command...", query, sizeof(query));

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
        if(fi.item->enabled && fi.item->onTrigger) fi.item->onTrigger();
    } else if(close) {
        open = false;
    }
}

}
}
