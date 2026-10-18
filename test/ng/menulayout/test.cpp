#include "solvespace.h"
#include "platform/sdl/menumodel.h"
#include "ui/keypresets.h"
#include "ui/menulayout.h"
#include "harness.h"

using namespace SolveSpace::Platform;

static std::shared_ptr<MenuItemSdl> Add(std::shared_ptr<Menu> m, const char *label, Command c) {
    auto item = std::static_pointer_cast<MenuItemSdl>(m->AddItem(label));
    item->commandId = (uint32_t)c;
    return item;
}

// A small menu bar as the core builds it.
static MenuBarSdl StockBar(std::shared_ptr<MenuItemSdl> *exportImage,
                           std::shared_ptr<MenuItemSdl> *exitItem,
                           std::shared_ptr<MenuItemSdl> *unknown) {
    MenuBarSdl bar;
    auto file    = bar.AddSubMenu("&File");
    Add(file, "&New", Command::NEW);
    *exportImage = Add(file, "Export &Image...", Command::EXPORT_IMAGE);
    file->AddSeparator();
    *exitItem = Add(file, "E&xit", Command::EXIT);
    auto view = bar.AddSubMenu("&View");
    Add(view, "Zoom &In", Command::ZOOM_IN);
    auto units = view->AddSubMenu("Dimension &Units");
    Add(units, "Dimensions in &Millimeters", Command::UNITS_MM);
    auto help = bar.AddSubMenu("&Help");
    help->AddSubMenu("&Language");
    *unknown = Add(help, "Something New", Command::NONE);
    return bar;
}

static const MenuSdl::Entry *Find(const MenuSdl &m, const std::string &label) {
    for(const MenuSdl::Entry &e : m.entries) {
        if(e.label == label) return &e;
    }
    return nullptr;
}

static std::shared_ptr<MenuSdl> Top(const MenuBarSdl &bar, const std::string &title) {
    for(auto &m : bar.menus) {
        if(m.first == title) return m.second;
    }
    return nullptr;
}

TEST_CASE(layout_moves_and_renames_the_same_items) {
    std::shared_ptr<MenuItemSdl> exportImage, exitItem, unknown;
    MenuBarSdl bar = StockBar(&exportImage, &exitItem, &unknown);
    SolveSpace::Ui::ApplyMenuLayout(bar, {});
    auto file = Top(bar, "File");
    CHECK_TRUE(file != nullptr);
    if(!file) return;
    // Export items live in an Export submenu, as the same item objects, renamed.
    const MenuSdl::Entry *exp = Find(*file, "Export");
    CHECK_TRUE(exp && exp->kind == MenuSdl::Entry::Kind::SUBMENU);
    if(exp) {
        const MenuSdl::Entry *img = Find(*exp->submenu, "Image...");
        CHECK_TRUE(img && img->item == exportImage);
    }
    // Exit is Quit, and the old name stays searchable.
    const MenuSdl::Entry *quit = Find(*file, "Quit");
    CHECK_TRUE(quit && quit->item == exitItem);
    CHECK_EQ_STR(exitItem->searchAlias, "Exit");
}

TEST_CASE(layout_keeps_what_it_does_not_place) {
    std::shared_ptr<MenuItemSdl> exportImage, exitItem, unknown;
    MenuBarSdl bar = StockBar(&exportImage, &exitItem, &unknown);
    SolveSpace::Ui::ApplyMenuLayout(bar, {});
    auto help = Top(bar, "Help");
    CHECK_TRUE(help != nullptr);
    if(!help) return;
    bool found = false, language = false;
    for(const MenuSdl::Entry &e : help->entries) {
        found |= e.item == unknown;
        language |= e.kind == MenuSdl::Entry::Kind::SUBMENU && e.label == "Language";
    }
    CHECK_TRUE(found && language);
    // The units submenu moved with its items.
    auto view = Top(bar, "View");
    CHECK_TRUE(view && Find(*view, "Units") != nullptr);
}

TEST_CASE(layout_adds_ui_actions_with_their_keys_and_check_marks) {
    std::shared_ptr<MenuItemSdl> exportImage, exitItem, unknown;
    MenuBarSdl bar = StockBar(&exportImage, &exitItem, &unknown);
    bool outlinerShown = true;
    SolveSpace::Ui::ApplyMenuLayout(bar, [&](const std::string &action) {
        return action == "ui.toggle-outliner" && outlinerShown;
    });
    auto view = Top(bar, "View");
    const MenuSdl::Entry *outliner = view ? Find(*view, "Outliner") : nullptr;
    CHECK_TRUE(outliner && outliner->item->action == "ui.toggle-outliner");
    if(outliner) {
        CHECK_TRUE(outliner->item->isActive && outliner->item->isActive());
        outlinerShown = false;
        CHECK_FALSE(outliner->item->isActive());
    }
    auto views = view ? Find(*view, "Standard Views") : nullptr;
    CHECK_TRUE(views && Find(*views->submenu, "Front") != nullptr);
}

TEST_CASE(menus_show_the_keys_of_the_current_preset) {
    std::string err;
    keymap::Keymap modern = *SolveSpace::Ui::ModernPreset(&err);
    keymap::Keymap classic = SolveSpace::Ui::ClassicPreset();
    MenuItemSdl distance;
    distance.commandId = (uint32_t)Command::DISTANCE_DIA;
    distance.keyAlias  = "ui.smart-dimension";
    // Modern reaches it through D (smart dimension); Classic binds it directly.
    CHECK_EQ_STR(SolveSpace::Ui::ShortcutText(distance, modern), "D");
    CHECK_FALSE(SolveSpace::Ui::ShortcutText(distance, classic).empty());
    MenuItemSdl outliner;
    outliner.action = "ui.toggle-outliner";
    CHECK_EQ_STR(SolveSpace::Ui::ShortcutText(outliner, modern), "O");
    CHECK_TRUE(SolveSpace::Ui::ShortcutText(outliner, classic).empty()); // no key in Classic
}

// (P13) The canvas's context menu: NG's names, and NG's actions for what is selected first.
static MenuSdl StockContextMenu(std::shared_ptr<MenuItemSdl> *groupInfo) {
    MenuSdl menu;
    *groupInfo = std::static_pointer_cast<MenuItemSdl>(menu.AddItem("Group Info"));
    menu.AddItem("Style Info");
    menu.AddSeparator();
    menu.AddItem("Delete");
    return menu;
}

TEST_CASE(context_menu_uses_ng_names_and_actions) {
    std::shared_ptr<MenuItemSdl> groupInfo;
    MenuSdl menu = StockContextMenu(&groupInfo);
    SolveSpace::Ui::ApplyContextMenuLayout(menu, /*itemsSelected=*/true);
    const char *actions[][2] = { { "Grab", "ui.grab" },
                                 { "Dimension", "ui.smart-dimension" },
                                 { "Constrain...", "ui.pie-constrain" },
                                 { "Hide Group", "ui.hide" } };
    CHECK_TRUE(menu.entries.size() == 4 + 1 + 4);
    if(menu.entries.size() != 9) return;
    for(int i = 0; i < 4; i++) {
        CHECK_TRUE(menu.entries[i].label == actions[i][0]);
        CHECK_TRUE(menu.entries[i].item->label == actions[i][0]);
        CHECK_TRUE(menu.entries[i].item->action == actions[i][1]);
    }
    CHECK_TRUE(menu.entries[4].kind == MenuSdl::Entry::Kind::SEPARATOR);
    const MenuSdl::Entry *props = Find(menu, "Group Properties");
    CHECK_TRUE(props && props->item == groupInfo && groupInfo->label == "Group Properties");
    CHECK_TRUE(Find(menu, "Style Properties") != nullptr);
    CHECK_TRUE(Find(menu, "Group Info") == nullptr);
}

TEST_CASE(context_menu_without_a_selection_gets_no_actions) {
    std::shared_ptr<MenuItemSdl> groupInfo;
    MenuSdl menu;
    menu.AddItem("Select All");
    SolveSpace::Ui::ApplyContextMenuLayout(menu, /*itemsSelected=*/false);
    CHECK_TRUE(menu.entries.size() == 1);
}
