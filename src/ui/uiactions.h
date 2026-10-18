//-----------------------------------------------------------------------------
// The "ui.*" actions: stable names and palette labels, bound to behaviour at startup.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_UI_UIACTIONS_H
#define SOLVESPACE_UI_UIACTIONS_H

#include <functional>
#include <map>
#include <string>
#include <vector>
#include "platform/sdl/menumodel.h"

namespace SolveSpace {
namespace Ui {

struct UiActionInfo {
    const char *name;  // "ui.toggle-theme"
    const char *label; // "View › Toggle Theme" (shown in the palette)
};

// Every ui.* action, in palette order. Keymaps may only bind names listed here.
const std::vector<UiActionInfo> &UiActionList();

class UiActions {
public:
    void Bind(const std::string &name, std::function<void()> run,
              std::function<bool()> enabled = nullptr);
    bool Run(const std::string &name);
    bool IsEnabled(const std::string &name) const;
    std::vector<std::string> Unbound() const;

private:
    struct Entry {
        std::function<void()> run;
        std::function<bool()> enabled;
    };
    std::map<std::string, Entry> entries;
};

UiActions &Actions();

// Runs a ui.* action or a core command by its stable name. False if unknown or not run.
bool RunNamedCommand(const std::string &name);

struct PaletteEntry {
    std::string           label;       // "Sketch › Circle"
    std::string           commandName; // for the shortcut column; may be empty
    std::string           keyAlias;    // shortcut column fallback (menulayout.h)
    std::string           keywords;    // searched too: a menu item's former name
    bool                  enabled = true;
    std::function<void()> run;
};

std::vector<PaletteEntry> BuildPaletteEntries(const std::vector<Platform::FlatMenuItem> &menu);

}
}

#endif
