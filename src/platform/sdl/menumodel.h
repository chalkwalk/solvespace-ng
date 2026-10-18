//-----------------------------------------------------------------------------
// An in-memory menu tree for the SDL backend, drawn with Dear ImGui and flattened for the
// command palette.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_SDL_MENUMODEL_H
#define SOLVESPACE_SDL_MENUMODEL_H

#include "solvespace.h"

namespace SolveSpace {
namespace Platform {

std::string StripMnemonic(const std::string &label); // "&File" -> "File", "A&&B" -> "A&B"

class MenuItemSdl final : public MenuItem {
public:
    std::string   label;           // mnemonic-stripped
    KeyboardEvent accel     = {};
    bool          hasAccel  = false;
    Indicator     indicator = Indicator::NONE;
    bool          enabled   = true;
    bool          active    = false;
    // NG's menu layout (ui/menulayout.h): a ui.* action this item runs instead of a command,
    // another keymap name whose keys do the same job (shown when the item has none of its
    // own), a former label the palette still finds it by, and a live check mark.
    std::string           action, keyAlias, searchAlias;
    std::function<bool()> isActive;

    void SetAccelerator(KeyboardEvent a) override;
    void SetIndicator(Indicator t) override;
    void SetEnabled(bool e) override;
    void SetActive(bool a) override;
};

class MenuSdl final : public Menu {
public:
    struct Entry {
        enum class Kind { ITEM, SUBMENU, SEPARATOR };
        Kind                         kind;
        std::shared_ptr<MenuItemSdl> item;
        std::shared_ptr<MenuSdl>     submenu;
        std::string                  label;
    };

    std::vector<Entry>             entries;
    bool                           popupRequested = false;
    // Installed by the backend; runs the nested loop that shows this menu as a popup.
    std::function<void(MenuSdl *)> onPopUp;

    std::shared_ptr<MenuItem> AddItem(const std::string &label,
                                      std::function<void()> onTrigger = std::function<void()>(),
                                      bool mnemonics = true) override;
    std::shared_ptr<Menu> AddSubMenu(const std::string &label) override;
    void AddSeparator() override;
    void PopUp() override;
    void Clear() override;
};

class MenuBarSdl final : public MenuBar {
public:
    std::vector<std::pair<std::string, std::shared_ptr<MenuSdl>>> menus;

    std::shared_ptr<Menu> AddSubMenu(const std::string &label) override;
    void Clear() override;
};

struct FlatMenuItem {
    std::string                  path;   // e.g. "Sketch › Circle"
    std::shared_ptr<MenuItemSdl> item;
};

std::vector<FlatMenuItem> FlattenMenuBar(const MenuBarSdl &bar);

}
}

#endif
