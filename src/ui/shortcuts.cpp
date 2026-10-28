//-----------------------------------------------------------------------------
// The Keyboard Shortcuts panel, which also edits the keymap (#17).
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "imgui.h"
#include "ui/fields.h"
#include "ui/keyrouter.h"
#include "ui/panels.h"
#include "ui/shortcuts.h"
#include "ui/uiactions.h"

namespace SolveSpace {
namespace Ui {

// A key being changed: a click on a key waits for the next chord, which replaces it ("+" adds
// one); Backspace or Delete removes it and Esc gives up.
static struct {
    bool                         waiting = false;
    std::string                  command;
    std::optional<keymap::Chord> was, pressed;
    bool                         finished = false;
} gEdit;
static std::string                              gNote; // what the last change took away
static std::function<void(const std::string &)> gWriter;

void SetKeymapWriter(std::function<void(const std::string &)> writer) {
    gWriter = std::move(writer);
}

const std::string &ShortcutsNote() {
    return gNote;
}

bool ShortcutsTakeKey(const keymap::Chord &c) {
    if(!gEdit.waiting || gEdit.finished) return gEdit.waiting;
    bool bare = !c.ctrl && !c.shift && !c.alt;
    if(bare && c.key == "Esc") {
        gEdit = {};
    } else {
        if(!(bare && (c.key == "Backspace" || c.key == "Delete"))) gEdit.pressed = c;
        gEdit.finished = true;
    }
    SS.GW.Invalidate();
    return true;
}

std::function<void()> DrawShortcuts(const Platform::MenuBarSdl &menuBar,
                                    const keymap::Keymap &keymap) {
    Panel &panel = ShortcutsPanel();
    if(!panel.Shown()) {
        gEdit = {};
        return nullptr;
    }
    std::function<void()> run;
    // Whatever the palette offers (menus and actions), with the keys it has now.
    std::vector<PaletteEntry> entries = BuildPaletteEntries(Platform::FlattenMenuBar(menuBar));
    // The name a row's keys are bound to: the one that has keys, else the command's.
    auto bindingName = [&](const PaletteEntry &e) {
        for(const std::string &name : { e.commandName, e.keyAlias }) {
            if(!name.empty() && !keymap.ChordsFor(name).empty()) return name;
        }
        return e.commandName.empty() ? e.keyAlias : e.commandName;
    };
    if(gEdit.finished) {
        if(gEdit.was || gEdit.pressed) {
            std::string    displaced;
            keymap::Keymap edited =
                Rebind(keymap, gEdit.command, gEdit.was, gEdit.pressed, &displaced);
            gNote.clear();
            if(!displaced.empty()) {
                std::string label = displaced;
                for(const PaletteEntry &e : entries) {
                    if(bindingName(e) == displaced) label = e.label;
                }
                gNote = ssprintf(_("%s no longer runs %s."),
                                 keymap::FormatChord(*gEdit.pressed).c_str(), label.c_str());
            }
            std::string text = edited.Serialize();
            run = [text] {
                if(gWriter) gWriter(text);
            };
        }
        gEdit = {};
    }
    if(panel.Begin()) {
        static char filter[64] = "";
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##shortcuts-filter", _("filter"), filter, sizeof(filter));
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped("%s", _("Click a key to change it: press the new one, Delete to "
                                   "remove it or Esc to keep it."));
        ImGui::PopStyleColor();
        if(!gNote.empty()) ImGui::TextWrapped("%s", gNote.c_str());
        if(ImGui::SmallButton(LabelId(_("use the preset's keys"), "##keys-preset").c_str())) {
            gNote.clear();
            run = [] {
                if(gWriter) gWriter("");
            };
        }
        std::string f = filter;
        for(char &c : f) c = (char)tolower((unsigned char)c);
        if(ImGui::BeginTable("##shortcuts", 2, ImGuiTableFlags_RowBg |
                                                   ImGuiTableFlags_SizingStretchProp)) {
            for(const PaletteEntry &e : entries) {
                std::string name = bindingName(e);
                if(name.empty()) continue;
                std::vector<keymap::Chord> chords = keymap.ChordsFor(name);
                std::string text = e.label;
                for(const keymap::Chord &c : chords) text += " " + keymap::FormatChord(c);
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
                const char *waiting = _("press a key");
                for(size_t i = 0; i <= chords.size(); i++) {
                    std::optional<keymap::Chord> was;
                    if(i < chords.size()) was = chords[i];
                    bool here = gEdit.waiting && gEdit.command == name && gEdit.was == was;
                    std::string label = here ? waiting
                                             : (was ? keymap::FormatChord(*was) : std::string("+"));
                    std::string id = was ? "##key-" + name + "-" + std::to_string(i)
                                         : "##key-add-" + name;
                    if(i > 0) ImGui::SameLine();
                    if(ImGui::SmallButton((label + id).c_str())) {
                        gEdit         = {};
                        gEdit.waiting = true;
                        gEdit.command = name;
                        gEdit.was     = was;
                    }
                }
            }
            ImGui::EndTable();
        }
        panel.End();
    }
    return run;
}

}
}
