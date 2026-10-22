//-----------------------------------------------------------------------------
// The "ui.*" action registry.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "commandnames.h"
#include "ui/uiactions.h"

namespace SolveSpace {
namespace Ui {

const std::vector<UiActionInfo> &UiActionList() {
    static const std::vector<UiActionInfo> list = {
        { "ui.command-palette", "View \xE2\x80\xBA Command Palette" },
        { "ui.toggle-menu-bar", "View \xE2\x80\xBA Toggle Menu Bar" },
        { "ui.toggle-theme",    "View \xE2\x80\xBA Toggle Theme" },
        { "ui.toggle-outliner", "View \xE2\x80\xBA Outliner" },
        { "ui.toggle-properties", "View \xE2\x80\xBA Properties" },
        { "ui.toggle-problems", "View \xE2\x80\xBA Problems" },
        { "ui.view-front",          "View \xE2\x80\xBA Front" },
        { "ui.view-back",           "View \xE2\x80\xBA Back" },
        { "ui.view-right",          "View \xE2\x80\xBA Right" },
        { "ui.view-left",           "View \xE2\x80\xBA Left" },
        { "ui.view-top",            "View \xE2\x80\xBA Top" },
        { "ui.view-bottom",         "View \xE2\x80\xBA Bottom" },
        { "ui.view-frame-selected", "View \xE2\x80\xBA Frame Selected" },
        { "ui.hide",                "View \xE2\x80\xBA Hide Group" },
        { "ui.unhide-all",          "View \xE2\x80\xBA Show All Groups" },
        { "ui.isolate",             "View \xE2\x80\xBA Isolate Group" },
        { "ui.toggle-sketch",       "Sketch \xE2\x80\xBA Enter/Exit Sketch" },
        { "ui.grab",                "Edit \xE2\x80\xBA Grab" },
        { "ui.smart-dimension",     "Constrain \xE2\x80\xBA Smart Dimension" },
        { "ui.pie-add",             "Sketch \xE2\x80\xBA Add\xE2\x80\xA6 (pie)" },
        { "ui.pie-constrain",       "Constrain \xE2\x80\xBA Constrain\xE2\x80\xA6 (pie)" },
        { "ui.pie-view",            "View \xE2\x80\xBA Views\xE2\x80\xA6 (pie)" },
        { "ui.show-shortcuts",      "Help \xE2\x80\xBA Keyboard Shortcuts" },
        { "ui.fit-table",           "Inspect \xE2\x80\xBA Fit Table" },
        { "ui.pie-joint",           "New Group \xE2\x80\xBA Joints\xE2\x80\xA6 (pie)" },
    };
    return list;
}

void UiActions::Bind(const std::string &name, std::function<void()> run,
                     std::function<bool()> enabled) {
    entries[name] = { std::move(run), std::move(enabled) };
}

bool UiActions::IsEnabled(const std::string &name) const {
    auto it = entries.find(name);
    if(it == entries.end() || !it->second.run) return false;
    return !it->second.enabled || it->second.enabled();
}

bool UiActions::Run(const std::string &name) {
    if(!IsEnabled(name)) return false;
    // Copy: the action may rebind itself.
    std::function<void()> run = entries[name].run;
    run();
    return true;
}

std::vector<std::string> UiActions::Unbound() const {
    std::vector<std::string> out;
    for(const UiActionInfo &a : UiActionList()) {
        auto it = entries.find(a.name);
        if(it == entries.end() || !it->second.run) out.push_back(a.name);
    }
    return out;
}

UiActions &Actions() {
    static UiActions actions;
    return actions;
}

bool RunNamedCommand(const std::string &name) {
    if(name.rfind("ui.", 0) == 0) return Actions().Run(name);
    Command cmd = CommandFromName(name);
    if(cmd == Command::NONE) return false;
    SS.GW.ActivateCommand(cmd);
    return true;
}

std::vector<PaletteEntry> BuildPaletteEntries(const std::vector<Platform::FlatMenuItem> &menu) {
    std::vector<PaletteEntry> out;
    std::set<std::string>     inMenus; // actions the menus already offer
    for(const Platform::FlatMenuItem &fi : menu) {
        PaletteEntry e;
        e.label = fi.path;
        if(!fi.item->action.empty()) {
            e.commandName = fi.item->action;
            inMenus.insert(fi.item->action);
        } else if(fi.item->commandId) {
            if(const char *n = CommandName((Command)fi.item->commandId)) e.commandName = n;
        }
        e.keyAlias = fi.item->keyAlias;
        e.keywords = fi.item->searchAlias;
        e.enabled = fi.item->enabled;
        auto item = fi.item;
        e.run     = [item] { if(item->onTrigger) item->onTrigger(); };
        out.push_back(std::move(e));
    }
    for(const UiActionInfo &a : UiActionList()) {
        if(inMenus.count(a.name)) continue;
        PaletteEntry e;
        e.label          = a.label;
        e.commandName    = a.name;
        e.enabled        = Actions().IsEnabled(a.name);
        std::string name = a.name;
        e.run            = [name] { Actions().Run(name); };
        out.push_back(std::move(e));
    }
    return out;
}

}
}
