//-----------------------------------------------------------------------------
// An in-memory menu tree for the SDL backend.
//-----------------------------------------------------------------------------
#include "platform/sdl/menumodel.h"

namespace SolveSpace {
namespace Platform {

std::string StripMnemonic(const std::string &label) {
    std::string r;
    for(size_t i = 0; i < label.size(); i++) {
        if(label[i] == '&') {
            if(i + 1 < label.size() && label[i + 1] == '&') {
                r += '&';
                i++;
            }
            continue;
        }
        r += label[i];
    }
    return r;
}

void MenuItemSdl::SetAccelerator(KeyboardEvent a) { accel = a; hasAccel = true; }
void MenuItemSdl::SetIndicator(Indicator t)       { indicator = t; }
void MenuItemSdl::SetEnabled(bool e)              { enabled = e; }
void MenuItemSdl::SetActive(bool a)               { active = a; }

std::shared_ptr<MenuItem> MenuSdl::AddItem(const std::string &label,
                                           std::function<void()> onTrigger, bool mnemonics) {
    auto item       = std::make_shared<MenuItemSdl>();
    item->label     = mnemonics ? StripMnemonic(label) : label;
    item->onTrigger = onTrigger;
    entries.push_back({ Entry::Kind::ITEM, item, nullptr, item->label });
    return item;
}

std::shared_ptr<Menu> MenuSdl::AddSubMenu(const std::string &label) {
    auto sub = std::make_shared<MenuSdl>();
    entries.push_back({ Entry::Kind::SUBMENU, nullptr, sub, StripMnemonic(label) });
    return sub;
}

void MenuSdl::AddSeparator() {
    entries.push_back({ Entry::Kind::SEPARATOR, nullptr, nullptr, "" });
}

void MenuSdl::PopUp() {
    popupRequested = true;
    if(onPopUp) onPopUp(this);
}

void MenuSdl::Clear() {
    entries.clear();
}

std::shared_ptr<Menu> MenuBarSdl::AddSubMenu(const std::string &label) {
    auto sub = std::make_shared<MenuSdl>();
    menus.push_back({ StripMnemonic(label), sub });
    return sub;
}

void MenuBarSdl::Clear() {
    menus.clear();
}

static void Flatten(const MenuSdl &menu, const std::string &prefix,
                    std::vector<FlatMenuItem> *out) {
    for(const MenuSdl::Entry &e : menu.entries) {
        if(e.kind == MenuSdl::Entry::Kind::ITEM) {
            out->push_back({ prefix + " › " + e.label, e.item });
        } else if(e.kind == MenuSdl::Entry::Kind::SUBMENU) {
            Flatten(*e.submenu, prefix + " › " + e.label, out);
        }
    }
}

std::vector<FlatMenuItem> FlattenMenuBar(const MenuBarSdl &bar) {
    std::vector<FlatMenuItem> out;
    for(const auto &m : bar.menus) Flatten(*m.second, m.first, &out);
    return out;
}

}
}
