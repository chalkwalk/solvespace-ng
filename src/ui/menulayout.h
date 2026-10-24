//-----------------------------------------------------------------------------
// NG's menu bar: SolveSpace's commands rearranged, renamed and joined by the ui.* actions
// (owner review 2026-10-17, docs/review/2026-10-17/answers.md). The items the core built are
// moved, never recreated, so its pointers to them (check marks, Undo) stay valid; renamed items
// keep their old label for the palette's search; anything the layout doesn't place stays in
// its menu.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_UI_MENULAYOUT_H
#define SOLVESPACE_UI_MENULAYOUT_H

#include <functional>
#include <string>
#include <vector>
#include "keymap.h"
#include "platform/sdl/menumodel.h"

namespace SolveSpace {
namespace Ui {

// `actionChecked` says whether a toggle action (ui.toggle-outliner...) is on, for its check
// mark; it may be empty.
void ApplyMenuLayout(Platform::MenuBarSdl &bar,
                     std::function<bool(const std::string &action)> actionChecked);

// Commands NG drops from its menus (and so from the palette): the classic property browser,
// whose screens all have panels (M1c task 10, docs/m1-parity.md).
bool RemovedFromMenus(Command c);

// What NG calls a command (its menu label: "Line", "Link Part..."), for the shelf, pies and
// panels; the core's label for a command the layout doesn't rename.
std::string CommandLabel(Command c);

// The key shown beside a menu item in the current keymap: its action's or command's own,
// else its alias's (Distance / Diameter shows D, the smart dimension, in Modern).
std::string ShortcutText(const Platform::MenuItemSdl &item, const keymap::Keymap &keymap);

// How many of the menu bar's titles (their widths, in order) fit in `available`; when not all
// do, the rest go in a More menu `moreWidth` wide (a phone's narrow window).
size_t MenusThatFit(const std::vector<float> &widths, float available, float moreWidth);

// The canvas's context menu as the core built it, in NG's words ("Group Properties"), with
// NG's actions for a selection first: Grab, Dimension, Constrain..., Hide Group.
void ApplyContextMenuLayout(Platform::MenuSdl &menu, bool itemsSelected);

}
}

#endif
