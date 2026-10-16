#include "solvespace.h"
#include "platform/sdl/menumodel.h"

#include "harness.h"

using namespace SolveSpace::Platform;

TEST_CASE(strip_mnemonics) {
    CHECK_EQ_STR(StripMnemonic("&File"), "File");
    CHECK_EQ_STR(StripMnemonic("Save &As..."), "Save As...");
    CHECK_EQ_STR(StripMnemonic("A&&B"), "A&B");
}

TEST_CASE(flatten_builds_paths_and_skips_separators) {
    MenuBarSdl bar;
    auto file = bar.AddSubMenu("&File");
    int fired = 0;
    file->AddItem("&New", [&] { fired++; });
    file->AddSeparator();
    auto recent = file->AddSubMenu("Open &Recent");
    recent->AddItem("/tmp/a.slvs", [] {}, /*mnemonics=*/false);
    auto flat = FlattenMenuBar(bar);
    CHECK_TRUE(flat.size() == 2);
    CHECK_EQ_STR(flat[0].path, "File › New");
    CHECK_EQ_STR(flat[1].path, "File › Open Recent › /tmp/a.slvs");
    flat[0].item->onTrigger();
    CHECK_TRUE(fired == 1);
}

TEST_CASE(item_state_is_recorded) {
    MenuBarSdl bar;
    auto view = bar.AddSubMenu("&View");
    auto item = std::static_pointer_cast<MenuItemSdl>(view->AddItem("Show &Grid"));
    item->SetIndicator(MenuItem::Indicator::CHECK_MARK);
    item->SetActive(true);
    item->SetEnabled(false);
    CHECK_TRUE(item->active && !item->enabled);
    CHECK_TRUE(item->indicator == MenuItem::Indicator::CHECK_MARK);
}

TEST_CASE(menu_items_default_to_no_command) {
    MenuItemSdl item;
    CHECK_TRUE(item.commandId == 0);
}
