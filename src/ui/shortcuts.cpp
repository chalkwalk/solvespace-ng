//-----------------------------------------------------------------------------
// The Keyboard Shortcuts panel.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "imgui.h"
#include "ui/panels.h"
#include "ui/shortcuts.h"
#include "ui/uiactions.h"

namespace SolveSpace {
namespace Ui {

std::function<void()> DrawShortcuts(const Platform::MenuBarSdl &menuBar,
                                    const keymap::Keymap &keymap) {
    Panel &panel = ShortcutsPanel();
    if(!panel.Shown()) return nullptr;
    std::function<void()> run;
    if(panel.Begin()) {
        static char filter[64] = "";
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##shortcuts-filter", "filter", filter, sizeof(filter));
        std::string f = filter;
        for(char &c : f) c = (char)tolower((unsigned char)c);
        // Whatever the palette offers (menus and actions), with the key it has now.
        std::vector<PaletteEntry> entries = BuildPaletteEntries(Platform::FlattenMenuBar(menuBar));
        if(ImGui::BeginTable("##shortcuts", 2, ImGuiTableFlags_RowBg |
                                                   ImGuiTableFlags_SizingStretchProp)) {
            for(const PaletteEntry &e : entries) {
                std::string key;
                for(const std::string &name : { e.commandName, e.keyAlias }) {
                    if(name.empty() || !key.empty()) continue;
                    std::vector<keymap::Chord> chords = keymap.ChordsFor(name);
                    for(size_t i = 0; i < chords.size(); i++) {
                        key += (i ? ", " : "") + keymap::FormatChord(chords[i]);
                    }
                }
                if(key.empty()) continue;
                std::string text = e.label + " " + key;
                for(char &c : text) c = (char)tolower((unsigned char)c);
                // Every word of the filter, anywhere ("sketch line" finds Sketch > Line).
                bool match = true;
                for(size_t at = 0; at < f.size() && match;) {
                    size_t end = f.find(' ', at);
                    if(end == std::string::npos) end = f.size();
                    if(end > at) match = text.find(f.substr(at, end - at)) != std::string::npos;
                    at = end + 1;
                }
                if(!match) continue;
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                // A click runs it, as the palette would.
                if(ImGui::Selectable(e.label.c_str()) && e.enabled) run = e.run;
                ImGui::TableNextColumn();
                ImGui::TextDisabled("%s", key.c_str());
            }
            ImGui::EndTable();
        }
        panel.End();
    }
    return run;
}

}
}
