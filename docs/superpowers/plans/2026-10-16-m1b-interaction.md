# M1b — Interaction Model Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Complete the Blender-style "Modern" interaction model in `solvespace-ng`: pie menus,
`Tab` sketch focus, `G` grab with axis locks and typed distances, hide/isolate, numpad views and
frame-selected, smart dimension, a status/HUD bar, and typed values that become real constraints
while drawing lines, rectangles and circles.

**Architecture:** Behaviour lives in core `GraphicsWindow` methods (new files `src/viewpresets.cpp`,
`src/visibility.cpp`, `src/sketchfocus.cpp`, `src/grab.cpp`, `src/typedinput.cpp`,
`src/constraintprobe.cpp`) so the headless test suite can drive it. A registry of `ui.*` actions
(`src/ui/uiactions.*`, in `solvespace-ng-support`) gives every new action a stable name, a palette
label and a key binding. Modal tools (pie, grab, typed drawing) get first refusal on key chords
through a new `KeyRouter::modal` hook. ImGui drawing (pies, status bar) stays in the
`solvespace-ng` executable and is smoke-tested with the debug script driver.

**Tech Stack:** C++20, SDL3, Dear ImGui 1.92 (docking), the M1a `keymap`/`palette` libs, a new
MIT `libs/pie` (pie geometry and keys), SolveSpace core.

**Spec:** `AGENTS.md` §4 (rules 1–2), §5 (UI/UX, the Modern keymap table, "typed values become
constraints"), §8 (M1 scope and testing). Roadmap: the "M1b" section at the end of
`docs/superpowers/plans/2026-10-15-m1a-sdl-imgui-shell.md`.

**Branch:** `ng/m1b-interaction`, cut from `ng/m1a-shell` (M1a is not merged into `master` yet;
when it is, M1b merges after it).

## Global Constraints

- All fork code is GPLv3 with the usual header; `libs/pie/` is MIT, has its own `LICENSE`, and
  includes no SolveSpace headers (`AGENTS.md` §2).
- No new dependencies (`AGENTS.md` §2). No `.slvs` format change: hiding uses the existing
  `Group.visible`; typed values create ordinary existing constraint types.
- Every user action is a registered, named command runnable headlessly (`AGENTS.md` §4 rule 2):
  new actions are `ui.*` names in the registry; their behaviour is a core method a test calls.
- The existing `test/` suite stays green on every commit; build both `build/` (stock options) and
  `build-ng/` (`-DENABLE_NG_GUI=ON`) before each commit.
- Old frontends must keep building: core changes may not depend on SDL or ImGui.
- Code style: `.clang-format` (4 spaces, 100 columns, attached braces); match surrounding code.
- "With nothing selected, commands act on the item under the cursor" (`AGENTS.md` §5).
- Commits end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Standing duty (`AGENTS.md` §9): if a change fixes stock SolveSpace behaviour, also cut a
  `pr/<topic>` branch from `github-master`, verify it there, push it and tell the owner.

## Decisions this plan makes (owner may veto at review)

1. **Angle typed while drawing a line.** SolveSpace has no "line to workplane axis" angle
   constraint. Multiples of 90° become `HORIZONTAL`/`VERTICAL`. Any other angle adds a short
   construction reference line from the start point (coincident + horizontal) and an `ANGLE`
   constraint between it and the new line. This honours "a typed value creates a real
   constraint", at the cost of one extra construction line per angled segment.
2. **`H` / `Alt+H` / `Shift+H` work on groups** (`Group.visible`, saved in the file, not undoable,
   like the property browser's checkboxes). SolveSpace has no per-entity hide. Isolate keeps the
   `#references` group visible.
3. **`Tab` = sketch focus.** `Tab` activates the group that owns the selected/hovered item and
   looks straight onto its workplane; `Tab` again returns to the previous group and view. With
   nothing selected or hovered, `Tab` looks onto the active group's workplane (and back).
4. **`D` becomes smart dimension**; `constrain-distance` keeps its menu item and palette entry but
   loses `D` in the Modern preset. `Num.` moves from `center-view` to frame-selected.
5. **Typed input accepts digits, `.`, `-` and the numpad**, parsed as a SolveSpace expression in
   the current display units. Full expressions in the HUD wait for M1c's properties fields.
6. **Grab axes**: `X`/`Y`/`Z` are the active workplane's u/v/normal when one is active, else world
   X/Y/Z; pressing the same axis again unlocks it. While grabbing, `X` never deletes.

## Review Focus

1. `X` (bound to `delete` in Modern) pressed during a grab must lock the axis, never delete. Test:
   Task 11 `grab_x_locks_axis_and_never_deletes`.
2. Cancelling a grab after the solver has moved other points restores the sketch exactly and
   leaves no redo entry. Test: Task 10 `grab_cancel_restores_without_redo`.
3. `Tab` out after the remembered group was deleted must not crash; it falls back to the last
   group. Test: Task 6 `tab_back_after_group_deleted_falls_back`.
4. Typed values respect display units (`1` with inches = 25.4 mm). Test: Task 2
   `typed_fields_use_display_units`.
5. A focused text field (palette, dimension editor) beats every modal tool: chords go to the field,
   not to the pie/grab. Test: Task 2 `text_input_beats_modal_tools`.

## File map

| File | Responsibility | Task |
|---|---|---|
| `src/ui/uiactions.{h,cpp}` (ng-support) | `ui.*` action list, bind/run registry, palette entries, `RunNamedCommand` | 1 |
| `src/ui/palettewidget.{h,cpp}`, `src/ui/ngui.{h,cpp}`, `src/platform/guisdl.cpp` | palette over menu items + ui actions; deferred run; modal hook | 1, 2, 3 |
| `src/ui/keyrouter.{h,cpp}` | `modal` hook, `RouteResult::MODAL` | 2 |
| `src/ui/typedfields.{h,cpp}` (ng-support) | HUD number fields | 2 |
| `libs/pie/` (MIT) | sector geometry, pie keys | 3 |
| `src/ui/piemenu.{h,cpp}` (exe) | ImGui pie widget | 3 |
| `src/ui/coreactions.{h,cpp}` (ng-support) | binds core-backed `ui.*` actions | 4–11 |
| `src/viewpresets.cpp` (core) | numpad views, hover fallback, frame selected | 4 |
| `src/visibility.cpp` (core) | hide / unhide all / isolate | 5 |
| `src/sketchfocus.cpp` (core) | `Tab` sketch focus | 6 |
| `src/ui/statusmodel.{h,cpp}` (ng-support) | status bar content | 7 |
| `src/ui/ngui.cpp` | status bar drawing | 7 |
| `src/constraintprobe.cpp` (core) | smart dimension, constraint applicability | 8, 9 |
| `src/grab.cpp`, `src/mouse.cpp`, `src/graphicswin.cpp` (core) | grab | 10 |
| `src/ui/grabtool.{h,cpp}` (ng-support) | grab keys/HUD | 11 |
| `src/typedinput.cpp`, `src/mouse.cpp` (core) | typed line/rectangle/circle commits | 12 |
| `src/ui/drawtool.{h,cpp}` (ng-support) | typed drawing keys/HUD | 13 |
| `res/keymaps/modern.keymap` | bindings, added by the task that adds each action | 3–13 |
| `src/ui.h` | `GraphicsWindow` declarations for all core methods | 4–12 |
| `test/ng/*/test.cpp`, `test/libs/pie/test.cpp`, `test/CMakeLists.txt` | tests | all |

New core `.cpp` files go into the same source list as `mouse.cpp` in `src/CMakeLists.txt`; new
ng-support files go into `solvespace-ng-support`; exe-only files into `solvespace-ng`; new test
files into the `if(ENABLE_NG_GUI)` block of `test/CMakeLists.txt` (core-only tests that need no NG
code go in the main list instead).

Build and test commands used throughout:

```sh
cmake --build build -j$(nproc) && build/bin/solvespace-testsuite            # stock options
cmake --build build-ng -j$(nproc) && build-ng/bin/solvespace-testsuite      # NG on
build-ng/bin/solvespace-testsuite <regex>                                   # one area
```

Smoke run of the app (headful, real SDL events; must exit by itself):

```sh
timeout -s KILL 30 env SOLVESPACE_NG_SCRIPT="<steps>; quit" build-ng/bin/solvespace-ng test/core/commands/line.slvs
```

---

### Task 1: UI action registry; palette lists `ui.*` actions

**Files:**
- Create: `src/ui/uiactions.h`, `src/ui/uiactions.cpp`, `test/ng/uiactions/test.cpp`
- Modify: `src/ui/keyrouter.cpp` (`IsKnownUiAction`), `src/ui/palettewidget.{h,cpp}`,
  `src/ui/ngui.{h,cpp}`, `src/platform/guisdl.cpp`, `src/CMakeLists.txt`, `test/CMakeLists.txt`

**Interfaces:**
- Consumes: `Platform::FlattenMenuBar`, `CommandName`, `CommandFromName`, `keymap::Keymap`.
- Produces:
  ```cpp
  namespace SolveSpace::Ui {
  struct UiActionInfo { const char *name; const char *label; };
  const std::vector<UiActionInfo> &UiActionList();
  class UiActions {
  public:
      void Bind(const std::string &name, std::function<void()> run,
                std::function<bool()> enabled = nullptr);
      bool Run(const std::string &name);              // false: unknown, unbound or disabled
      bool IsEnabled(const std::string &name) const;  // bound and (no predicate or predicate true)
      std::vector<std::string> Unbound() const;       // listed names with no run function
  };
  UiActions &Actions();
  bool RunNamedCommand(const std::string &name);      // "ui.*" -> Actions().Run, else core Command
  struct PaletteEntry { std::string label, commandName; bool enabled; std::function<void()> run; };
  std::vector<PaletteEntry> BuildPaletteEntries(const std::vector<Platform::FlatMenuItem> &menu);
  }
  ```
  `DrawNgUi` now returns `std::function<void()>` (empty if nothing chosen). `IsKnownUiAction` is
  true exactly for names in `UiActionList()`.

- [ ] **Step 1: Create the branch**

```sh
git switch ng/m1a-shell && git switch -c ng/m1b-interaction
```

- [ ] **Step 2: Write the failing test** — `test/ng/uiactions/test.cpp`

```cpp
#include <set>
#include "solvespace.h"
#include "ui/uiactions.h"
#include "ui/keyrouter.h"

#include "harness.h"

using namespace SolveSpace::Ui;

TEST_CASE(action_list_names_are_unique_ui_names) {
    std::set<std::string> seen;
    for(const UiActionInfo &a : UiActionList()) {
        CHECK_TRUE(std::string(a.name).rfind("ui.", 0) == 0);
        CHECK_TRUE(seen.insert(a.name).second);
        CHECK_TRUE(IsKnownUiAction(a.name));
        CHECK_TRUE(std::string(a.label).find(" \xE2\x80\xBA ") != std::string::npos); // "A › B"
    }
    CHECK_FALSE(IsKnownUiAction("ui.no-such-action"));
}

TEST_CASE(bind_and_run_actions) {
    UiActions acts;
    CHECK_FALSE(acts.Run("ui.toggle-theme")); // unbound
    int runs = 0;
    bool enabled = false;
    acts.Bind("ui.toggle-theme", [&] { runs++; }, [&] { return enabled; });
    CHECK_FALSE(acts.Run("ui.toggle-theme")); // disabled
    enabled = true;
    CHECK_TRUE(acts.Run("ui.toggle-theme"));
    CHECK_TRUE(runs == 1);
    CHECK_FALSE(acts.Run("ui.no-such-action"));
    auto unbound = acts.Unbound();
    CHECK_TRUE(std::find(unbound.begin(), unbound.end(), "ui.toggle-theme") == unbound.end());
    CHECK_TRUE(std::find(unbound.begin(), unbound.end(), "ui.command-palette") != unbound.end());
}

TEST_CASE(run_named_command_dispatches_core_commands) {
    CHECK_LOAD("../../core/commands/line.slvs");
    SS.GW.ClearSelection();
    CHECK_TRUE(RunNamedCommand("select-all"));
    CHECK_TRUE(SS.GW.selection.n > 0);
    CHECK_FALSE(RunNamedCommand("not-a-command"));
}

TEST_CASE(palette_entries_include_ui_actions) {
    Actions().Bind("ui.toggle-theme", [] {});
    auto entries = BuildPaletteEntries({});
    bool found = false;
    for(const PaletteEntry &e : entries) {
        if(e.commandName == "ui.toggle-theme") {
            found = true;
            CHECK_TRUE(e.enabled);
            CHECK_TRUE(bool(e.run));
        }
    }
    CHECK_TRUE(found);
}
```

Add `ng/uiactions/test.cpp` to the NG test list in `test/CMakeLists.txt`.

- [ ] **Step 3: Run it to verify it fails**

Run: `cmake --build build-ng -j$(nproc)`
Expected: compile error, `ui/uiactions.h` not found.

- [ ] **Step 4: Implement** — `src/ui/uiactions.h`

```cpp
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
    bool                  enabled = true;
    std::function<void()> run;
};

std::vector<PaletteEntry> BuildPaletteEntries(const std::vector<Platform::FlatMenuItem> &menu);

}
}

#endif
```

`src/ui/uiactions.cpp`:

```cpp
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
    entries[name].run();
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
    for(const Platform::FlatMenuItem &fi : menu) {
        PaletteEntry e;
        e.label = fi.path;
        if(fi.item->commandId) {
            if(const char *n = CommandName((Command)fi.item->commandId)) e.commandName = n;
        }
        e.enabled = fi.item->enabled;
        auto item = fi.item;
        e.run     = [item] { if(item->onTrigger) item->onTrigger(); };
        out.push_back(std::move(e));
    }
    for(const UiActionInfo &a : UiActionList()) {
        PaletteEntry e;
        e.label       = a.label;
        e.commandName = a.name;
        e.enabled     = Actions().IsEnabled(a.name);
        std::string name = a.name;
        e.run         = [name] { Actions().Run(name); };
        out.push_back(std::move(e));
    }
    return out;
}

}
}
```

In `src/ui/keyrouter.cpp`, replace the body of `IsKnownUiAction`:

```cpp
bool IsKnownUiAction(const std::string &name) {
    for(const UiActionInfo &a : UiActionList()) {
        if(name == a.name) return true;
    }
    return false;
}
```
(add `#include "ui/uiactions.h"`). Add `ui/uiactions.cpp` to `solvespace-ng-support`.

Palette: change `CommandPalette::Draw` to take `const std::vector<PaletteEntry> &entries` and
return `std::function<void()>` (the chosen enabled entry's `run`, or empty). Labels come from
`entries[i].label`; the shortcut column uses `keymap.ChordsFor(entries[i].commandName)`; the
disabled state uses `entries[i].enabled`. Delete `ShortcutFor` (no longer used).

`ngui.h/.cpp`: `std::function<void()> DrawNgUi(Platform::MenuBarSdl *menuBar, const keymap::Keymap &keymap)`;
it passes `BuildPaletteEntries(Platform::FlattenMenuBar(*menuBar))` to the palette.
`InitNgUi()` binds its own actions:

```cpp
void InitNgUi() {
    ApplyTheme(LoadThemeChoice());
    Actions().Bind("ui.command-palette", [] { gPalette.Open(); });
    Actions().Bind("ui.toggle-theme", [] { ToggleTheme(); });
}

void HandleUiAction(const std::string &action) {
    if(!Actions().Run(action)) dbp("UI action '%s' is unknown, unbound or disabled", action.c_str());
}
```

`guisdl.cpp`:
- In `InitSdlWindowAndImGui`, before `LoadKeymap()`:
  ```cpp
  Ui::Actions().Bind("ui.toggle-menu-bar", [] {
      gShowMenuBar = !gShowMenuBar;
      GetSettings()->FreezeBool("NG_ShowMenuBar", gShowMenuBar);
  });
  ```
  and after all binding, `ssassert(Ui::Actions().Unbound().empty(), "every ui.* action must be bound");`
- `LoadKeymap()`'s `onUiAction` becomes `[](const std::string &a) { Ui::HandleUiAction(a); };`
  (delete the `gUiActionHandler` indirection if nothing else uses it).
- `RenderFrame`: `if(auto run = Ui::DrawNgUi(bar.get(), gKeyRouter.Keymap())) gDeferred.Post(run);`

- [ ] **Step 5: Run the tests**

Run: `cmake --build build-ng -j$(nproc) && build-ng/bin/solvespace-testsuite`
Expected: all pass, including the 4 new cases. `cmake --build build -j$(nproc) && build/bin/solvespace-testsuite` also passes.

- [ ] **Step 6: Smoke run**

```sh
timeout -s KILL 30 env SOLVESPACE_NG_SCRIPT="action ui.command-palette; keys theme; wait 3; shot /tmp/claude-1002/m1b-t1.png; quit" build-ng/bin/solvespace-ng test/core/commands/line.slvs
```
Expected: exits 0; the screenshot shows the palette listing "View › Toggle Theme" with its
`Ctrl+Alt+T` shortcut.

- [ ] **Step 7: Commit**

```bash
git add src/ui/uiactions.* src/ui/keyrouter.cpp src/ui/palettewidget.* src/ui/ngui.* \
        src/platform/guisdl.cpp src/CMakeLists.txt test/CMakeLists.txt test/ng/uiactions
git commit -m "NG: ui.* action registry; the palette lists UI actions too.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: Modal key layer and typed HUD fields

**Files:**
- Create: `src/ui/typedfields.{h,cpp}`, `test/ng/typedfields/test.cpp`
- Modify: `src/ui/keyrouter.{h,cpp}`, `test/ng/keyrouter/test.cpp`, `src/ui/ngui.{h,cpp}`,
  `src/platform/guisdl.cpp`, CMake lists

**Interfaces:**
- Produces:
  ```cpp
  enum class RouteResult { UNBOUND, UI_ACTION, COMMAND, SUPPRESSED, MODAL };
  // KeyRouter: consulted after the text-input check and before the keymap.
  std::function<bool(const keymap::Chord &)> modal;
  class TypedFields {
  public:
      explicit TypedFields(std::vector<std::string> labels);
      bool HandleChord(const keymap::Chord &c);
      size_t Count() const; int Active() const;
      const std::string &Label(int i) const; const std::string &Text(int i) const;
      bool Any() const; void Clear();
      std::optional<double> ValueMm(int i) const;   // display units -> mm
      std::optional<double> ValueRaw(int i) const;  // no unit conversion (angles)
  };
  bool Ui::ModalKey(const keymap::Chord &c);       // ngui: composes the active modal tools
  ```

- [ ] **Step 1: Write the failing tests**

Append to `test/ng/keyrouter/test.cpp`:

```cpp
TEST_CASE(modal_tools_get_chords_first) {
    std::string err;
    KeyRouter r;
    r.SetKeymap(*keymap::Keymap::Parse("delete: X\nline-segment: L\n", &err));
    int fired = 0;
    r.onCommand = [&](Command) { fired++; };
    r.modal = [](const keymap::Chord &c) { return c.key == "x"; };
    CHECK_TRUE(r.Route(C("X"), false) == RouteResult::MODAL);
    CHECK_TRUE(fired == 0);
    CHECK_TRUE(r.Route(C("L"), false) == RouteResult::COMMAND); // the tool declined it
}

TEST_CASE(text_input_beats_modal_tools) {
    std::string err;
    KeyRouter r;
    r.SetKeymap(*keymap::Keymap::Parse("delete: X\n", &err));
    int modalCalls = 0;
    r.modal = [&](const keymap::Chord &) { modalCalls++; return true; };
    CHECK_TRUE(r.Route(C("X"), true) == RouteResult::SUPPRESSED);
    CHECK_TRUE(modalCalls == 0);
}
```

`test/ng/typedfields/test.cpp`:

```cpp
#include "solvespace.h"
#include "ui/typedfields.h"

#include "harness.h"

using namespace SolveSpace::Ui;

static keymap::Chord C(const char *s) { return *keymap::ParseChord(s); }

TEST_CASE(typed_fields_collect_digits_and_cycle) {
    TypedFields f({"length", "angle"});
    CHECK_FALSE(f.Any());
    for(const char *k : {"2", "5", "Tab", "3", "Num0"}) CHECK_TRUE(f.HandleChord(C(k)));
    CHECK_EQ_STR(f.Text(0), "25");
    CHECK_EQ_STR(f.Text(1), "30");
    CHECK_TRUE(f.Active() == 1);
    CHECK_TRUE(f.HandleChord(C("Backspace")));
    CHECK_EQ_STR(f.Text(1), "3");
    CHECK_TRUE(f.HandleChord(C("Tab")));     // wraps
    CHECK_TRUE(f.Active() == 0);
    CHECK_TRUE(f.HandleChord(C("Shift+Tab"))); // back
    CHECK_TRUE(f.Active() == 1);
    CHECK_FALSE(f.HandleChord(C("L")));      // not ours
    CHECK_FALSE(f.HandleChord(C("Ctrl+2")));
    f.Clear();
    CHECK_FALSE(f.Any());
    CHECK_TRUE(f.Active() == 0);
}

TEST_CASE(typed_fields_parse_values) {
    TypedFields f({"d"});
    for(const char *k : {"-", "1", ".", "5"}) f.HandleChord(C(k));
    CHECK_TRUE(f.ValueRaw(0).has_value());
    CHECK_EQ_EPS(*f.ValueRaw(0), -1.5);
    f.Clear();
    for(const char *k : {"1", ".", ".", "2"}) f.HandleChord(C(k));
    CHECK_FALSE(f.ValueRaw(0).has_value()); // "1..2" is not a number
    f.Clear();
    CHECK_FALSE(f.ValueMm(0).has_value());  // empty
}

TEST_CASE(typed_fields_use_display_units) {
    Unit saved = SS.viewUnits;
    TypedFields f({"d"});
    f.HandleChord(C("1"));
    SS.viewUnits = Unit::MM;
    CHECK_EQ_EPS(*f.ValueMm(0), 1.0);
    SS.viewUnits = Unit::INCHES;
    CHECK_EQ_EPS(*f.ValueMm(0), 25.4);
    SS.viewUnits = saved;
}
```

- [ ] **Step 2: Run them to verify they fail**

Run: `cmake --build build-ng -j$(nproc)`
Expected: compile errors: no member `modal`, no `RouteResult::MODAL`, no `ui/typedfields.h`.

- [ ] **Step 3: Implement**

`keyrouter.h`: add `MODAL` to `RouteResult`; add to `KeyRouter`:
```cpp
    // The active modal tool (pie, grab, typed drawing) sees chords before the keymap; return
    // true to consume one. Never called while a text field has the keyboard.
    std::function<bool(const keymap::Chord &)> modal;
```
`keyrouter.cpp`, in `Route` after the text-input line:
```cpp
    if(modal && modal(chord)) return RouteResult::MODAL;
```

`src/ui/typedfields.h`:
```cpp
//-----------------------------------------------------------------------------
// Number fields typed into the HUD while a modal tool runs (Blender-style "type a value").
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_UI_TYPEDFIELDS_H
#define SOLVESPACE_UI_TYPEDFIELDS_H

#include <optional>
#include <string>
#include <vector>
#include "keymap.h"

namespace SolveSpace {
namespace Ui {

class TypedFields {
public:
    explicit TypedFields(std::vector<std::string> labels);
    // Digits, '.', '-', the numpad, Backspace, Tab / Shift+Tab. True if consumed.
    bool HandleChord(const keymap::Chord &c);
    size_t Count() const { return labels.size(); }
    int Active() const { return active; }
    const std::string &Label(int i) const { return labels[i]; }
    const std::string &Text(int i) const { return texts[i]; }
    bool Any() const;
    void Clear();
    std::optional<double> ValueMm(int i) const;
    std::optional<double> ValueRaw(int i) const;

private:
    std::vector<std::string> labels, texts;
    int active = 0;
};

}
}

#endif
```

`src/ui/typedfields.cpp`:
```cpp
//-----------------------------------------------------------------------------
// Number fields typed into the HUD.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "ui/typedfields.h"

namespace SolveSpace {
namespace Ui {

TypedFields::TypedFields(std::vector<std::string> l) : labels(std::move(l)), texts(labels.size()) {}

static char TypedChar(const keymap::Chord &c) {
    if(c.ctrl || c.alt) return 0;
    const std::string &k = c.key;
    if(k.size() == 1 && !c.shift && ((k[0] >= '0' && k[0] <= '9') || k[0] == '.' || k[0] == '-'))
        return k[0];
    if(k.size() == 4 && k.rfind("Num", 0) == 0 && k[3] >= '0' && k[3] <= '9') return k[3];
    if(k == "Num.") return '.';
    if(k == "Num-") return '-';
    return 0;
}

bool TypedFields::HandleChord(const keymap::Chord &c) {
    if(labels.empty() || c.ctrl || c.alt) return false;
    if(char ch = TypedChar(c)) {
        texts[active] += ch;
        return true;
    }
    if(c.key == "Backspace") {
        if(!texts[active].empty()) texts[active].pop_back();
        return true;
    }
    if(c.key == "Tab") {
        int n  = (int)labels.size();
        active = c.shift ? (active + n - 1) % n : (active + 1) % n;
        return true;
    }
    return false;
}

bool TypedFields::Any() const {
    for(const std::string &t : texts) if(!t.empty()) return true;
    return false;
}

void TypedFields::Clear() {
    for(std::string &t : texts) t.clear();
    active = 0;
}

std::optional<double> TypedFields::ValueRaw(int i) const {
    if(texts[i].empty()) return std::nullopt;
    Expr *e = Expr::From(texts[i], /*popUpError=*/false);
    if(!e) return std::nullopt;
    return e->Eval();
}

std::optional<double> TypedFields::ValueMm(int i) const {
    if(texts[i].empty()) return std::nullopt;
    Expr *e = Expr::From(texts[i], /*popUpError=*/false);
    if(!e) return std::nullopt;
    return SS.ExprToMm(e);
}

}
}
```
(If `Expr::From("1..2")` parses as something, make `ValueRaw` also reject texts with more than one
`.` and record a ruling.)

`ngui.h/.cpp`: add `bool ModalKey(const keymap::Chord &c);` returning `false` for now (Tasks 3, 11,
13 add tools). `guisdl.cpp` `LoadKeymap()`: `gKeyRouter.modal = Ui::ModalKey;`.

- [ ] **Step 4: Run the tests**

Run: `cmake --build build-ng -j$(nproc) && build-ng/bin/solvespace-testsuite`
Expected: all pass. Stock `build/` passes.

- [ ] **Step 5: Commit**

```bash
git add src/ui/keyrouter.* src/ui/typedfields.* src/ui/ngui.* src/platform/guisdl.cpp \
        src/CMakeLists.txt test/CMakeLists.txt test/ng/keyrouter test/ng/typedfields
git commit -m "NG: modal key layer ahead of the keymap; typed HUD number fields.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: Pie menu (MIT geometry lib + ImGui widget)

**Files:**
- Create: `libs/pie/{CMakeLists.txt,LICENSE,pie.h,pie.cpp}`, `test/libs/pie/test.cpp`,
  `src/ui/piemenu.{h,cpp}`
- Modify: `libs/CMakeLists.txt`, `test/CMakeLists.txt` (main list + link `pie`),
  `src/CMakeLists.txt` (`solvespace-ng` gets `ui/piemenu.cpp`, links `pie`), `src/ui/ngui.cpp`,
  `THIRD_PARTIES.txt` is unchanged (own code)

**Interfaces:**
- Produces:
  ```cpp
  namespace pie {
  int  SectorAt(float dx, float dy, int count, float deadZone); // -1 in dead zone / no items
  void ItemDirection(int i, int count, float *x, float *y);      // unit vector, screen coords
  enum class KeyKind { NONE, PICK, CONFIRM, CANCEL };
  struct KeyResult { KeyKind kind; int index; };
  KeyResult KeyAction(std::string_view key, bool modifiers, int count);
  }
  namespace SolveSpace::Ui {
  struct PieItem { std::string label, commandName; bool enabled = true; };
  class PieMenu {
  public:
      void Open(std::string title, std::vector<PieItem> items); // centred at the mouse
      bool IsOpen() const;
      bool HandleChord(const keymap::Chord &c);                 // consumes every chord while open
      std::string Draw();  // chosen item's commandName ("" if none); closes on pick or cancel
  };
  PieMenu &Pie();         // the single pie
  }
  ```
  Item 0 is at 12 o'clock, then clockwise.

- [ ] **Step 1: Write the failing test** — `test/libs/pie/test.cpp`

```cpp
#include "pie.h"
#include "harness.h"

TEST_CASE(sector_at_compass_points) {
    CHECK_TRUE(pie::SectorAt(0, -10, 8, 5) == 0);  // up
    CHECK_TRUE(pie::SectorAt(10, 0, 8, 5) == 2);   // right
    CHECK_TRUE(pie::SectorAt(0, 10, 8, 5) == 4);   // down
    CHECK_TRUE(pie::SectorAt(-10, 0, 8, 5) == 6);  // left
    CHECK_TRUE(pie::SectorAt(1, 1, 8, 5) == -1);   // dead zone
    CHECK_TRUE(pie::SectorAt(10, 0, 0, 5) == -1);  // no items
    CHECK_TRUE(pie::SectorAt(-3, 9, 1, 5) == 0);   // one item owns the circle
}

TEST_CASE(sector_boundaries) {
    // 8 items: item 0 spans -22.5..22.5 degrees from up, clockwise.
    auto dir = [](double deg, float *x, float *y) {
        *x = (float)(100 * sin(deg * M_PI / 180));
        *y = (float)(-100 * cos(deg * M_PI / 180));
    };
    float x, y;
    dir(20, &x, &y);  CHECK_TRUE(pie::SectorAt(x, y, 8, 5) == 0);
    dir(25, &x, &y);  CHECK_TRUE(pie::SectorAt(x, y, 8, 5) == 1);
    dir(-20, &x, &y); CHECK_TRUE(pie::SectorAt(x, y, 8, 5) == 0);
    dir(-25, &x, &y); CHECK_TRUE(pie::SectorAt(x, y, 8, 5) == 7);
}

TEST_CASE(item_direction_matches_sector) {
    for(int n = 1; n <= 9; n++) {
        for(int i = 0; i < n; i++) {
            float x, y;
            pie::ItemDirection(i, n, &x, &y);
            CHECK_EQ_EPS(x * x + y * y, 1.0);
            CHECK_TRUE(pie::SectorAt(x * 50, y * 50, n, 5) == i);
        }
    }
}

TEST_CASE(pie_keys) {
    CHECK_TRUE(pie::KeyAction("3", false, 8).kind == pie::KeyKind::PICK);
    CHECK_TRUE(pie::KeyAction("3", false, 8).index == 2);
    CHECK_TRUE(pie::KeyAction("Num1", false, 8).index == 0);
    CHECK_TRUE(pie::KeyAction("9", false, 8).kind == pie::KeyKind::NONE); // no 9th item
    CHECK_TRUE(pie::KeyAction("Enter", false, 8).kind == pie::KeyKind::CONFIRM);
    CHECK_TRUE(pie::KeyAction("Esc", false, 8).kind == pie::KeyKind::CANCEL);
    CHECK_TRUE(pie::KeyAction("3", true, 8).kind == pie::KeyKind::NONE);  // with modifiers
}
```

Add `libs/pie/test.cpp` next to `libs/keymap/test.cpp` and `pie` to
`target_link_libraries(solvespace-testsuite keymap palette pie)`.

- [ ] **Step 2: Run it to verify it fails**

Run: `cmake --build build -j$(nproc)`
Expected: `pie.h` not found.

- [ ] **Step 3: Implement the library**

`libs/pie/LICENSE`: copy `libs/palette/LICENSE` (MIT, same holder line).
`libs/pie/CMakeLists.txt`:
```cmake
add_library(pie STATIC pie.cpp)
target_include_directories(pie PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(pie PUBLIC cxx_std_20)
```
`libs/CMakeLists.txt`: `add_subdirectory(pie)`.

`libs/pie/pie.h`:
```cpp
// SPDX-License-Identifier: MIT
// Pie menu geometry and keys. Independent of SolveSpace and of any UI toolkit.
#ifndef PIE_PIE_H
#define PIE_PIE_H

#include <string_view>

namespace pie {

// Item 0 is centred straight up (12 o'clock) and items continue clockwise, each owning an equal
// sector. Screen coordinates: y grows downwards. Returns -1 inside the dead zone or if count < 1.
int SectorAt(float dx, float dy, int count, float deadZone);
void ItemDirection(int i, int count, float *x, float *y);

enum class KeyKind { NONE, PICK, CONFIRM, CANCEL };
struct KeyResult {
    KeyKind kind  = KeyKind::NONE;
    int     index = -1;
};
// "1".."9" / "Num1".."Num9" pick an item; Enter/NumEnter/Space confirm the highlighted one;
// Esc cancels. Keys with modifiers do nothing.
KeyResult KeyAction(std::string_view key, bool modifiers, int count);

}

#endif
```

`libs/pie/pie.cpp`:
```cpp
// SPDX-License-Identifier: MIT
#include "pie.h"

#include <cmath>

namespace pie {

static const double Tau = 6.283185307179586;

int SectorAt(float dx, float dy, int count, float deadZone) {
    if(count < 1 || std::hypot(dx, dy) < deadZone) return -1;
    double a = std::atan2((double)dx, (double)-dy); // 0 = up, clockwise positive
    if(a < 0) a += Tau;
    double sector = Tau / count;
    return (int)std::floor((a + sector / 2) / sector) % count;
}

void ItemDirection(int i, int count, float *x, float *y) {
    double a = Tau * i / count;
    *x = (float)std::sin(a);
    *y = (float)-std::cos(a);
}

KeyResult KeyAction(std::string_view key, bool modifiers, int count) {
    if(modifiers) return {};
    std::string_view digit = key;
    if(digit.size() == 4 && digit.substr(0, 3) == "Num") digit.remove_prefix(3);
    if(digit.size() == 1 && digit[0] >= '1' && digit[0] <= '9') {
        int i = digit[0] - '1';
        if(i < count) return { KeyKind::PICK, i };
        return {};
    }
    if(key == "Enter" || key == "NumEnter" || key == "Space") return { KeyKind::CONFIRM, -1 };
    if(key == "Esc") return { KeyKind::CANCEL, -1 };
    return {};
}

}
```

- [ ] **Step 4: Run the tests**

Run: `cmake --build build -j$(nproc) && build/bin/solvespace-testsuite pie`
Expected: 4 cases pass.

- [ ] **Step 5: Implement the ImGui widget** — `src/ui/piemenu.h`

```cpp
//-----------------------------------------------------------------------------
// A radial (pie) menu drawn over the viewport, Blender-style.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_UI_PIEMENU_H
#define SOLVESPACE_UI_PIEMENU_H

#include <string>
#include <vector>
#include "keymap.h"

namespace SolveSpace {
namespace Ui {

struct PieItem {
    std::string label;
    std::string commandName; // core command or ui.* action
    bool        enabled = true;
};

class PieMenu {
public:
    void Open(std::string title, std::vector<PieItem> items); // centred at the mouse
    bool IsOpen() const { return open; }
    bool HandleChord(const keymap::Chord &c);                 // consumes every chord while open
    std::string Draw();

private:
    bool                 open = false;
    std::string          title;
    std::vector<PieItem> items;
    float                cx = 0, cy = 0;
    int                  highlighted = -1;
    int                  picked      = -1;
    bool                 cancelled   = false;
};

PieMenu &Pie();

}
}

#endif
```

`src/ui/piemenu.cpp`:
```cpp
//-----------------------------------------------------------------------------
// A radial (pie) menu drawn over the viewport.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "imgui.h"
#include "pie.h"
#include "ui/piemenu.h"

namespace SolveSpace {
namespace Ui {

PieMenu &Pie() {
    static PieMenu pie;
    return pie;
}

void PieMenu::Open(std::string t, std::vector<PieItem> it) {
    title       = std::move(t);
    items       = std::move(it);
    ImVec2 m    = ImGui::GetIO().MousePos;
    cx          = m.x;
    cy          = m.y;
    highlighted = -1;
    picked      = -1;
    cancelled   = false;
    open        = true;
}

bool PieMenu::HandleChord(const keymap::Chord &c) {
    if(!open) return false;
    pie::KeyResult r = pie::KeyAction(c.key, c.ctrl || c.alt || c.shift, (int)items.size());
    switch(r.kind) {
        case pie::KeyKind::PICK:    picked = r.index; break;
        case pie::KeyKind::CONFIRM: picked = highlighted; break;
        case pie::KeyKind::CANCEL:  cancelled = true; break;
        case pie::KeyKind::NONE:    break;
    }
    return true;
}

std::string PieMenu::Draw() {
    if(!open) return "";
    ImGuiIO &io = ImGui::GetIO();
    // A transparent full-viewport window: it owns the mouse while the pie is open, so clicks
    // don't reach the canvas (the backend checks WantCaptureMouse).
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("##pie", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove);
    ImGui::InvisibleButton("##pie-capture", io.DisplaySize,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

    const float radius = 110.0f, dead = 24.0f;
    highlighted = pie::SectorAt(io.MousePos.x - cx, io.MousePos.y - cy, (int)items.size(), dead);
    ImDrawList *dl  = ImGui::GetWindowDrawList();
    ImGuiStyle &st  = ImGui::GetStyle();
    dl->AddCircleFilled(ImVec2(cx, cy), dead, ImGui::GetColorU32(ImGuiCol_PopupBg));
    dl->AddCircle(ImVec2(cx, cy), dead, ImGui::GetColorU32(ImGuiCol_Border), 0, 2.0f);
    ImVec2 ts = ImGui::CalcTextSize(title.c_str());
    dl->AddText(ImVec2(cx - ts.x / 2, cy - dead - ts.y - 6), ImGui::GetColorU32(ImGuiCol_Text),
                title.c_str());
    if(items.empty()) {
        const char *none = "Nothing applies to the selection";
        ImVec2 ns = ImGui::CalcTextSize(none);
        dl->AddText(ImVec2(cx - ns.x / 2, cy + dead + 6), ImGui::GetColorU32(ImGuiCol_TextDisabled),
                    none);
    }
    for(int i = 0; i < (int)items.size(); i++) {
        float dx, dy;
        pie::ItemDirection(i, (int)items.size(), &dx, &dy);
        std::string text = std::to_string(i + 1) + "  " + items[i].label;
        ImVec2 sz = ImGui::CalcTextSize(text.c_str());
        ImVec2 pad(st.FramePadding.x * 2, st.FramePadding.y * 2);
        ImVec2 c(cx + dx * radius, cy + dy * radius);
        // Push labels outward so they never overlap the centre on the left/right.
        c.x += dx * sz.x / 2;
        ImVec2 a(c.x - sz.x / 2 - pad.x, c.y - sz.y / 2 - pad.y);
        ImVec2 b(c.x + sz.x / 2 + pad.x, c.y + sz.y / 2 + pad.y);
        ImGuiCol bg = i == highlighted ? ImGuiCol_ButtonHovered : ImGuiCol_PopupBg;
        dl->AddRectFilled(a, b, ImGui::GetColorU32(bg), st.FrameRounding);
        dl->AddRect(a, b, ImGui::GetColorU32(ImGuiCol_Border), st.FrameRounding);
        dl->AddText(ImVec2(a.x + pad.x, a.y + pad.y),
                    ImGui::GetColorU32(items[i].enabled ? ImGuiCol_Text : ImGuiCol_TextDisabled),
                    text.c_str());
    }
    if(ImGui::IsMouseClicked(ImGuiMouseButton_Left)) picked = highlighted;
    if(ImGui::IsMouseClicked(ImGuiMouseButton_Right)) cancelled = true;
    // An empty pie only explains itself; any click or confirm closes it.
    if(items.empty() && (picked >= 0 || ImGui::IsMouseClicked(ImGuiMouseButton_Left))) {
        cancelled = true;
    }
    ImGui::End();

    std::string result;
    if(picked >= 0 && picked < (int)items.size() && items[picked].enabled) {
        result = items[picked].commandName;
        open   = false;
    } else if(cancelled) {
        open = false;
    }
    picked = -1;
    return result;
}

}
}
```

`ngui.cpp`:
- `DrawNgUi`: before the palette, `if(Pie().IsOpen()) { std::string name = Pie().Draw(); if(!name.empty()) return [name] { RunNamedCommand(name); }; }`
- `ModalKey`: `if(Pie().IsOpen()) return Pie().HandleChord(c); return false;`
- `CapturesText()` stays palette-only (the pie is modal, not a text field).

- [ ] **Step 6: Run the tests and a smoke build**

Run: `cmake --build build-ng -j$(nproc) && build-ng/bin/solvespace-testsuite && cmake --build build -j$(nproc) && build/bin/solvespace-testsuite`
Expected: all pass (the widget has no caller yet; Task 4 opens it).

- [ ] **Step 7: Commit**

```bash
git add libs/pie libs/CMakeLists.txt test/libs/pie test/CMakeLists.txt src/ui/piemenu.* \
        src/ui/ngui.cpp src/CMakeLists.txt
git commit -m "NG: pie menu widget; MIT pie geometry/keys library.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: Numpad views, frame selected, add pie and view pie

**Files:**
- Create: `src/viewpresets.cpp`, `src/ui/coreactions.{h,cpp}`, `test/core/viewpresets/test.cpp`,
  `test/ng/coreactions/test.cpp`
- Modify: `src/ui.h`, `src/ui/uiactions.cpp` (list), `src/ui/ngui.cpp`, `src/platform/guisdl.cpp`,
  `res/keymaps/modern.keymap`, CMake lists

**Interfaces:**
- Produces (core, `src/ui.h`):
  ```cpp
  enum class ViewPreset { FRONT, BACK, RIGHT, LEFT, TOP, BOTTOM };  // namespace SolveSpace
  void ViewPresetBasis(ViewPreset p, Vector *right, Vector *up);     // Z up, Blender convention
  // GraphicsWindow:
  void SnapToView(ViewPreset p, bool animate);
  bool SelectHoveredIfNothingSelected();   // true if it selected the hovered item
  void FrameSelected();                    // hover fallback, then zoom to fit the selection
  ```
- Produces (ng-support): `void Ui::RegisterCoreUiActions(bool animate);` binds every core-backed
  `ui.*` action (this task: the six views and frame-selected; later tasks add theirs here).
- Actions added to `UiActionList()`: `ui.view-front`, `ui.view-back`, `ui.view-right`,
  `ui.view-left`, `ui.view-top`, `ui.view-bottom`, `ui.view-frame-selected`, `ui.pie-add`,
  `ui.pie-view`.

- [ ] **Step 1: Write the failing tests**

`test/core/viewpresets/test.cpp` (main test list; copy `test/core/commands/line.slvs` here):

```cpp
#include "solvespace.h"

#include "harness.h"

static bool Near(Vector a, Vector b) { return a.Minus(b).Magnitude() < 1e-9; }

TEST_CASE(view_preset_bases_are_z_up) {
    Vector r, u;
    ViewPresetBasis(ViewPreset::FRONT, &r, &u);
    CHECK_TRUE(Near(r, Vector::From(1, 0, 0)) && Near(u, Vector::From(0, 0, 1)));
    ViewPresetBasis(ViewPreset::BACK, &r, &u);
    CHECK_TRUE(Near(r, Vector::From(-1, 0, 0)) && Near(u, Vector::From(0, 0, 1)));
    ViewPresetBasis(ViewPreset::RIGHT, &r, &u);
    CHECK_TRUE(Near(r, Vector::From(0, 1, 0)) && Near(u, Vector::From(0, 0, 1)));
    ViewPresetBasis(ViewPreset::LEFT, &r, &u);
    CHECK_TRUE(Near(r, Vector::From(0, -1, 0)) && Near(u, Vector::From(0, 0, 1)));
    ViewPresetBasis(ViewPreset::TOP, &r, &u);
    CHECK_TRUE(Near(r, Vector::From(1, 0, 0)) && Near(u, Vector::From(0, 1, 0)));
    ViewPresetBasis(ViewPreset::BOTTOM, &r, &u);
    CHECK_TRUE(Near(r, Vector::From(1, 0, 0)) && Near(u, Vector::From(0, -1, 0)));
}

TEST_CASE(snap_to_view_without_animation) {
    CHECK_LOAD("line.slvs");
    SS.GW.SnapToView(ViewPreset::FRONT, /*animate=*/false);
    CHECK_TRUE(Near(SS.GW.projRight, Vector::From(1, 0, 0)));
    CHECK_TRUE(Near(SS.GW.projUp, Vector::From(0, 0, 1)));
}

TEST_CASE(hover_fallback_selects_only_when_empty) {
    CHECK_LOAD("line.slvs");
    hEntity line = {};
    for(Entity &e : SK.entity) {
        if(e.type == Entity::Type::LINE_SEGMENT) line = e.h;
    }
    CHECK_TRUE(line.v != 0);
    SS.GW.ClearSelection();
    SS.GW.hover.Clear();
    CHECK_FALSE(SS.GW.SelectHoveredIfNothingSelected()); // nothing hovered
    SS.GW.hover.entity = line;
    CHECK_TRUE(SS.GW.SelectHoveredIfNothingSelected());
    CHECK_TRUE(SS.GW.IsSelected(line));
    CHECK_FALSE(SS.GW.SelectHoveredIfNothingSelected()); // already has a selection
}
```

`test/ng/coreactions/test.cpp` (copy `line.slvs` here too):

```cpp
#include "solvespace.h"
#include "ui/coreactions.h"
#include "ui/uiactions.h"

#include "harness.h"

using namespace SolveSpace::Ui;

TEST_CASE(view_actions_are_bound_and_run) {
    CHECK_LOAD("line.slvs");
    RegisterCoreUiActions(/*animate=*/false);
    CHECK_TRUE(Actions().Run("ui.view-right"));
    CHECK_TRUE(SS.GW.projRight.Minus(Vector::From(0, 1, 0)).Magnitude() < 1e-9);
    CHECK_TRUE(Actions().Run("ui.view-frame-selected"));
}
```

- [ ] **Step 2: Run them to verify they fail**

Run: `cmake --build build-ng -j$(nproc)`
Expected: compile errors (`ViewPresetBasis`, `SnapToView`, `ui/coreactions.h` missing).

- [ ] **Step 3: Implement the core**

`src/ui.h`: before `class GraphicsWindow`, add
```cpp
enum class ViewPreset { FRONT, BACK, RIGHT, LEFT, TOP, BOTTOM };
// Screen right/up for a preset, Z up (Blender's numpad convention).
void ViewPresetBasis(ViewPreset p, Vector *right, Vector *up);
```
and in `GraphicsWindow` (public, near `AnimateOnto`), under a `// SolveSpace NG interaction` comment:
```cpp
    void SnapToView(ViewPreset p, bool animate);
    bool SelectHoveredIfNothingSelected();
    void FrameSelected();
```

`src/viewpresets.cpp`:
```cpp
//-----------------------------------------------------------------------------
// Preset views (numpad 1/3/7 and their opposites), the hover fallback for commands, and
// frame-selected.
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {

void ViewPresetBasis(ViewPreset p, Vector *right, Vector *up) {
    switch(p) {
        case ViewPreset::FRONT:  *right = Vector::From( 1, 0, 0); *up = Vector::From(0, 0, 1); return;
        case ViewPreset::BACK:   *right = Vector::From(-1, 0, 0); *up = Vector::From(0, 0, 1); return;
        case ViewPreset::RIGHT:  *right = Vector::From(0,  1, 0); *up = Vector::From(0, 0, 1); return;
        case ViewPreset::LEFT:   *right = Vector::From(0, -1, 0); *up = Vector::From(0, 0, 1); return;
        case ViewPreset::TOP:    *right = Vector::From( 1, 0, 0); *up = Vector::From(0,  1, 0); return;
        case ViewPreset::BOTTOM: *right = Vector::From( 1, 0, 0); *up = Vector::From(0, -1, 0); return;
    }
    ssassert(false, "Unexpected view preset");
}

void GraphicsWindow::SnapToView(ViewPreset p, bool animate) {
    Vector right, up;
    ViewPresetBasis(p, &right, &up);
    if(animate && window) {
        AnimateOnto(Quaternion::From(right, up), offset);
    } else {
        projRight = right;
        projUp    = up;
        Invalidate();
    }
    SS.ScheduleShowTW();
}

bool GraphicsWindow::SelectHoveredIfNothingSelected() {
    if(selection.n > 0 || hover.IsEmpty()) return false;
    MakeSelected(&hover);
    return true;
}

void GraphicsWindow::FrameSelected() {
    SelectHoveredIfNothingSelected();
    ZoomToFit(/*includingInvisibles=*/false, /*useSelection=*/true);
    Invalidate();
}

} // namespace SolveSpace
```
(If `Invalidate()` dereferences a null `window`, guard it with `if(window)` inside
`GraphicsWindow::Invalidate` and record a ruling.)

`src/ui/coreactions.h`:
```cpp
//-----------------------------------------------------------------------------
// Binds the ui.* actions whose behaviour lives in the core (headless-testable).
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_UI_COREACTIONS_H
#define SOLVESPACE_UI_COREACTIONS_H

namespace SolveSpace {
namespace Ui {

void RegisterCoreUiActions(bool animate);

}
}

#endif
```

`src/ui/coreactions.cpp`:
```cpp
//-----------------------------------------------------------------------------
// Core-backed ui.* actions.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "ui/coreactions.h"
#include "ui/uiactions.h"

namespace SolveSpace {
namespace Ui {

void RegisterCoreUiActions(bool animate) {
    UiActions &a = Actions();
    auto view = [&a, animate](const char *name, ViewPreset p) {
        a.Bind(name, [p, animate] { SS.GW.SnapToView(p, animate); });
    };
    view("ui.view-front",  ViewPreset::FRONT);
    view("ui.view-back",   ViewPreset::BACK);
    view("ui.view-right",  ViewPreset::RIGHT);
    view("ui.view-left",   ViewPreset::LEFT);
    view("ui.view-top",    ViewPreset::TOP);
    view("ui.view-bottom", ViewPreset::BOTTOM);
    a.Bind("ui.view-frame-selected", [] { SS.GW.FrameSelected(); });
}

}
}
```

Add to `UiActionList()` (keep palette order):
```cpp
        { "ui.view-front",          "View \xE2\x80\xBA Front" },
        { "ui.view-back",           "View \xE2\x80\xBA Back" },
        { "ui.view-right",          "View \xE2\x80\xBA Right" },
        { "ui.view-left",           "View \xE2\x80\xBA Left" },
        { "ui.view-top",            "View \xE2\x80\xBA Top" },
        { "ui.view-bottom",         "View \xE2\x80\xBA Bottom" },
        { "ui.view-frame-selected", "View \xE2\x80\xBA Frame Selected" },
        { "ui.pie-add",             "Sketch \xE2\x80\xBA Add\xE2\x80\xA6 (pie)" },
        { "ui.pie-view",            "View \xE2\x80\xBA Views\xE2\x80\xA6 (pie)" },
```

`ngui.cpp` `InitNgUi()` binds the pies:
```cpp
    Actions().Bind("ui.pie-add", [] {
        Pie().Open("Add", {
            { "Line", "line-segment" }, { "Rectangle", "rectangle" }, { "Circle", "circle" },
            { "Arc", "arc" }, { "Spline", "cubic-spline" }, { "Point", "datum-point" },
            { "Text", "ttf-text" }, { "Workplane", "workplane" } });
    });
    Actions().Bind("ui.pie-view", [] {
        Pie().Open("View", {
            { "Top", "ui.view-top" }, { "Right", "ui.view-right" },
            { "Isometric", "nearest-iso" }, { "Front", "ui.view-front" },
            { "Bottom", "ui.view-bottom" }, { "Left", "ui.view-left" },
            { "Frame Selected", "ui.view-frame-selected" }, { "Back", "ui.view-back" } });
    });
```
Check every core name used here exists in `src/commandnames.cpp` (`nearest-iso` included); a
missing one is a bug in this step. `guisdl.cpp`: call `Ui::RegisterCoreUiActions(/*animate=*/true)`
right after `Ui::InitNgUi()`.

`res/keymaps/modern.keymap`: replace the "M1b adds" comment line's covered items and add:
```
ui.pie-add:             Shift+A
ui.pie-view:            `
ui.view-front:          Num1
ui.view-back:           Ctrl+Num1
ui.view-right:          Num3
ui.view-left:           Ctrl+Num3
ui.view-top:            Num7
ui.view-bottom:         Ctrl+Num7
ui.view-frame-selected: ., Num.
```
and change `center-view:` to no binding (delete its line; it stays in the menu and palette).

- [ ] **Step 4: Run the tests**

Run: `cmake --build build-ng -j$(nproc) && build-ng/bin/solvespace-testsuite && cmake --build build -j$(nproc) && build/bin/solvespace-testsuite`
Expected: all pass (the stock build runs `core/viewpresets`).

- [ ] **Step 5: Smoke run**

```sh
timeout -s KILL 30 env SOLVESPACE_NG_SCRIPT="key Num1; wait 30; shot /tmp/claude-1002/m1b-front.png; key Shift+A; wait 3; shot /tmp/claude-1002/m1b-addpie.png; key Esc; key \`; wait 3; shot /tmp/claude-1002/m1b-viewpie.png; key 1; wait 30; quit" build-ng/bin/solvespace-ng test/core/commands/line.slvs
```
Expected: exits 0; screenshots show the front view, the Add pie with 8 numbered items and the
View pie. Look at them.

- [ ] **Step 6: Commit**

```bash
git add src/ui.h src/viewpresets.cpp src/ui/coreactions.* src/ui/uiactions.cpp src/ui/ngui.cpp \
        src/platform/guisdl.cpp res/keymaps/modern.keymap src/CMakeLists.txt test/CMakeLists.txt \
        test/core/viewpresets test/ng/coreactions
git commit -m "NG: numpad views, frame selected, Add and View pies.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: Hide, unhide all, isolate (`H`, `Alt+H`, `Shift+H`)

**Files:**
- Create: `src/visibility.cpp`, `test/core/visibility/test.cpp` (copy
  `test/group/translate_nd/normal.slvs` in as `groups.slvs`)
- Modify: `src/ui.h`, `src/ui/uiactions.cpp`, `src/ui/coreactions.cpp`,
  `test/ng/coreactions/test.cpp`, `res/keymaps/modern.keymap`, CMake lists

**Interfaces:**
- Produces (`GraphicsWindow`):
  ```cpp
  std::vector<hGroup> GroupsOfSelection(); // hover fallback; groups of selected entities/constraints
  int  HideSelectedGroups();               // returns how many groups it hid
  void ShowAllGroups();
  int  IsolateSelectedGroups();            // returns how many groups stay visible (0 = no-op)
  ```
- Actions: `ui.hide`, `ui.unhide-all`, `ui.isolate`.

- [ ] **Step 1: Write the failing test** — `test/core/visibility/test.cpp`

```cpp
#include "solvespace.h"

#include "harness.h"

// The first line segment in the given group.
static hEntity LineIn(hGroup hg) {
    for(Entity &e : SK.entity) {
        if(e.group == hg && e.type == Entity::Type::LINE_SEGMENT) return e.h;
    }
    return Entity::NO_ENTITY;
}

// The first group after #references that owns a line segment.
static hGroup SketchGroup() {
    for(hGroup hg : SK.groupOrder) {
        if(hg != Group::HGROUP_REFERENCES && LineIn(hg).v) return hg;
    }
    return {};
}

TEST_CASE(hide_selected_then_show_all) {
    CHECK_LOAD("groups.slvs");
    hGroup sketch = SketchGroup();
    CHECK_TRUE(sketch.v != 0);
    SS.GW.ClearSelection();
    SS.GW.MakeSelected(LineIn(sketch));
    CHECK_TRUE(SS.GW.HideSelectedGroups() == 1);
    CHECK_FALSE(SK.GetGroup(sketch)->visible);
    CHECK_TRUE(SS.GW.selection.n == 0);
    SS.GW.ShowAllGroups();
    for(hGroup hg : SK.groupOrder) CHECK_TRUE(SK.GetGroup(hg)->visible);
}

TEST_CASE(isolate_keeps_selection_groups_and_references) {
    CHECK_LOAD("groups.slvs");
    hGroup sketch = SketchGroup();
    SS.GW.ClearSelection();
    SS.GW.MakeSelected(LineIn(sketch));
    CHECK_TRUE(SS.GW.IsolateSelectedGroups() == 1);
    for(hGroup hg : SK.groupOrder) {
        bool keep = hg == sketch || hg == Group::HGROUP_REFERENCES;
        CHECK_TRUE(SK.GetGroup(hg)->visible == keep);
    }
}

TEST_CASE(hide_and_isolate_without_target_do_nothing) {
    CHECK_LOAD("groups.slvs");
    SS.GW.ClearSelection();
    SS.GW.hover.Clear();
    CHECK_TRUE(SS.GW.HideSelectedGroups() == 0);
    CHECK_TRUE(SS.GW.IsolateSelectedGroups() == 0);
    for(hGroup hg : SK.groupOrder) CHECK_TRUE(SK.GetGroup(hg)->visible);
}

TEST_CASE(hide_uses_hover_when_nothing_selected) {
    CHECK_LOAD("groups.slvs");
    hGroup sketch = SketchGroup();
    SS.GW.ClearSelection();
    SS.GW.hover.Clear();
    SS.GW.hover.entity = LineIn(sketch);
    CHECK_TRUE(SS.GW.HideSelectedGroups() == 1);
    CHECK_FALSE(SK.GetGroup(sketch)->visible);
}
```
(The fixture must have every group visible when saved; if not, the test's first check fails
for the wrong reason — open it and confirm `Group.visible=1` throughout.)

Add to `test/ng/coreactions/test.cpp`:
```cpp
TEST_CASE(hide_actions_are_bound) {
    RegisterCoreUiActions(false);
    CHECK_TRUE(Actions().IsEnabled("ui.hide"));
    CHECK_TRUE(Actions().IsEnabled("ui.unhide-all"));
    CHECK_TRUE(Actions().IsEnabled("ui.isolate"));
}
```

- [ ] **Step 2: Run to verify failure**

Run: `cmake --build build -j$(nproc)`
Expected: compile error, `HideSelectedGroups` is not a member.

- [ ] **Step 3: Implement** — declarations in `src/ui.h` (NG interaction block); `src/visibility.cpp`:

```cpp
//-----------------------------------------------------------------------------
// Hide / unhide all / isolate, at group level (SolveSpace has no per-entity hide).
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {

std::vector<hGroup> GraphicsWindow::GroupsOfSelection() {
    SelectHoveredIfNothingSelected();
    std::vector<hGroup> out;
    auto add = [&](hGroup hg) {
        if(std::find(out.begin(), out.end(), hg) == out.end()) out.push_back(hg);
    };
    for(const Selection &s : selection) {
        if(s.entity.v) {
            if(Entity *e = SK.entity.FindByIdNoOops(s.entity)) add(e->group);
        }
        if(s.constraint.v) {
            if(Constraint *c = SK.constraint.FindByIdNoOops(s.constraint)) add(c->group);
        }
    }
    return out;
}

int GraphicsWindow::HideSelectedGroups() {
    std::vector<hGroup> groups = GroupsOfSelection();
    for(hGroup hg : groups) SK.GetGroup(hg)->visible = false;
    ClearSelection();
    if(!groups.empty()) {
        persistentDirty = true;
        Invalidate();
        SS.ScheduleShowTW();
    }
    return (int)groups.size();
}

void GraphicsWindow::ShowAllGroups() {
    for(hGroup hg : SK.groupOrder) SK.GetGroup(hg)->visible = true;
    persistentDirty = true;
    // Shown groups may never have been generated.
    SS.GenerateAll();
    Invalidate();
    SS.ScheduleShowTW();
}

int GraphicsWindow::IsolateSelectedGroups() {
    std::vector<hGroup> keep = GroupsOfSelection();
    if(keep.empty()) return 0;
    for(hGroup hg : SK.groupOrder) {
        bool k = hg == Group::HGROUP_REFERENCES ||
                 std::find(keep.begin(), keep.end(), hg) != keep.end();
        SK.GetGroup(hg)->visible = k;
    }
    ClearSelection();
    persistentDirty = true;
    SS.GenerateAll();
    Invalidate();
    SS.ScheduleShowTW();
    return (int)keep.size();
}

} // namespace SolveSpace
```

`coreactions.cpp`:
```cpp
    a.Bind("ui.hide",       [] { SS.GW.HideSelectedGroups(); });
    a.Bind("ui.unhide-all", [] { SS.GW.ShowAllGroups(); });
    a.Bind("ui.isolate",    [] { SS.GW.IsolateSelectedGroups(); });
```
`UiActionList()`: `{ "ui.hide", "View › Hide Group" }`, `{ "ui.unhide-all", "View › Show All Groups" }`,
`{ "ui.isolate", "View › Isolate Group" }` (with `\xE2\x80\xBA`).
Keymap: `ui.hide: H`, `ui.unhide-all: Alt+H`, `ui.isolate: Shift+H`.

- [ ] **Step 4: Run the tests** — both builds, full suites. Expected: all pass.

- [ ] **Step 5: Commit**

```bash
git add src/ui.h src/visibility.cpp src/ui/uiactions.cpp src/ui/coreactions.cpp \
        res/keymaps/modern.keymap src/CMakeLists.txt test/CMakeLists.txt test/core/visibility \
        test/ng/coreactions
git commit -m "NG: H / Alt+H / Shift+H hide, show all and isolate groups.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 6: `Tab` sketch focus

**Files:**
- Create: `src/sketchfocus.cpp`, `test/core/sketchfocus/test.cpp` (copy `groups.slvs` from Task 5)
- Modify: `src/ui.h`, `src/ui/uiactions.cpp`, `src/ui/coreactions.cpp`, `res/keymaps/modern.keymap`,
  CMake lists

**Interfaces:**
- Produces (`GraphicsWindow`):
  ```cpp
  struct {
      bool       active;
      hGroup     group;
      Quaternion view;
      Vector     offset;
  } sketchFocusReturn;
  bool ToggleSketchFocus(bool animate); // false if there was nothing to do
  void ActivateGroup(hGroup hg);       // what the property browser's "active" radio does
  void LookOntoActiveWorkplane(bool animate);
  ```
- Action: `ui.toggle-sketch`.

- [ ] **Step 1: Write the failing test**

```cpp
#include "solvespace.h"

#include "harness.h"

static hEntity LineIn(hGroup hg) {
    for(Entity &e : SK.entity) {
        if(e.group == hg && e.type == Entity::Type::LINE_SEGMENT) return e.h;
    }
    return Entity::NO_ENTITY;
}

static hGroup FirstSketch() {
    for(hGroup hg : SK.groupOrder) {
        if(hg != Group::HGROUP_REFERENCES && LineIn(hg).v) return hg;
    }
    return {};
}

TEST_CASE(tab_into_group_of_selection_and_back) {
    CHECK_LOAD("groups.slvs");
    hGroup last = SK.groupOrder[SK.groupOrder.n - 1];
    SS.GW.ActivateGroup(last);
    Vector right0 = SS.GW.projRight;
    hGroup sketch = FirstSketch();
    CHECK_TRUE(sketch != last);
    SS.GW.ClearSelection();
    SS.GW.MakeSelected(LineIn(sketch));
    CHECK_TRUE(SS.GW.ToggleSketchFocus(/*animate=*/false));
    CHECK_TRUE(SS.GW.activeGroup == sketch);
    CHECK_TRUE(SS.GW.sketchFocusReturn.active);
    if(SS.GW.LockedInWorkplane()) {
        Entity *wp = SK.GetEntity(SS.GW.ActiveWorkplane());
        Vector u = wp->Normal()->NormalU();
        CHECK_TRUE(SS.GW.projRight.Minus(u).Magnitude() < 1e-6);
    }
    CHECK_TRUE(SS.GW.ToggleSketchFocus(false));
    CHECK_TRUE(SS.GW.activeGroup == last);
    CHECK_TRUE(SS.GW.projRight.Minus(right0).Magnitude() < 1e-6);
    CHECK_FALSE(SS.GW.sketchFocusReturn.active);
}

TEST_CASE(tab_with_nothing_targets_active_workplane) {
    CHECK_LOAD("groups.slvs");
    hGroup sketch = FirstSketch();
    SS.GW.ActivateGroup(sketch);
    SS.GW.ClearSelection();
    SS.GW.hover.Clear();
    SS.GW.SnapToView(ViewPreset::FRONT, false);
    CHECK_TRUE(SS.GW.ToggleSketchFocus(false));
    CHECK_TRUE(SS.GW.activeGroup == sketch);
    CHECK_TRUE(SS.GW.sketchFocusReturn.active);
    CHECK_TRUE(SS.GW.ToggleSketchFocus(false)); // back to the front view
    CHECK_TRUE(SS.GW.projUp.Minus(Vector::From(0, 0, 1)).Magnitude() < 1e-6);
}

TEST_CASE(tab_back_after_group_deleted_falls_back) {
    CHECK_LOAD("groups.slvs");
    hGroup last = SK.groupOrder[SK.groupOrder.n - 1];
    SS.GW.ActivateGroup(last);
    hGroup sketch = FirstSketch();
    SS.GW.ClearSelection();
    SS.GW.MakeSelected(LineIn(sketch));
    CHECK_TRUE(SS.GW.ToggleSketchFocus(false));
    // The remembered group disappears (as after an undo or a delete).
    SS.GW.sketchFocusReturn.group = { 0x7fffffff };
    CHECK_TRUE(SS.GW.ToggleSketchFocus(false));
    CHECK_TRUE(SS.GW.activeGroup == SK.groupOrder[SK.groupOrder.n - 1]);
}
```

- [ ] **Step 2: Run to verify failure** — `cmake --build build -j$(nproc)`; Expected: no member
  `ToggleSketchFocus`.

- [ ] **Step 3: Implement** — `src/sketchfocus.cpp`:

```cpp
//-----------------------------------------------------------------------------
// Tab: focus the sketch that owns the selection (activate its group, look onto its workplane),
// and Tab again to go back. Blender's Edit Mode, SolveSpace-style.
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {

void GraphicsWindow::ActivateGroup(hGroup hg) {
    activeGroup = hg;
    SK.GetGroup(hg)->Activate();
    ClearSuper();
    SS.GenerateAll();
}

void GraphicsWindow::LookOntoActiveWorkplane(bool animate) {
    if(!LockedInWorkplane()) return;
    if(animate && window) {
        AnimateOntoWorkplane();
        return;
    }
    Entity *w  = SK.GetEntity(ActiveWorkplane());
    Quaternion q = w->Normal()->NormalGetNum();
    projRight  = q.RotationU();
    projUp     = q.RotationV();
    offset     = SK.GetEntity(w->point[0])->PointGetNum().ScaledBy(-1);
    Invalidate();
}

bool GraphicsWindow::ToggleSketchFocus(bool animate) {
    if(sketchFocusReturn.active) {
        sketchFocusReturn.active = false;
        hGroup back = sketchFocusReturn.group;
        if(!SK.group.FindByIdNoOops(back)) back = SK.groupOrder[SK.groupOrder.n - 1];
        if(back != activeGroup) ActivateGroup(back);
        if(animate && window) {
            AnimateOnto(sketchFocusReturn.view, sketchFocusReturn.offset);
        } else {
            projRight = sketchFocusReturn.view.RotationU();
            projUp    = sketchFocusReturn.view.RotationV();
            offset    = sketchFocusReturn.offset;
            Invalidate();
        }
        return true;
    }

    hGroup target = activeGroup;
    std::vector<hGroup> groups = GroupsOfSelection();
    if(!groups.empty()) target = groups[0];
    if(target == activeGroup && !LockedInWorkplane()) return false;

    sketchFocusReturn.active = true;
    sketchFocusReturn.group  = activeGroup;
    sketchFocusReturn.view   = Quaternion::From(projRight, projUp);
    sketchFocusReturn.offset = offset;
    if(target != activeGroup) ActivateGroup(target);
    LookOntoActiveWorkplane(animate);
    return true;
}

} // namespace SolveSpace
```
Initialise `sketchFocusReturn = {}` wherever `GraphicsWindow::Init()` resets view state, and clear
`sketchFocusReturn.active` in `SolveSpaceUI::Load`/new-file paths (search for where
`activeGroup` is reset on load and add it beside that).

`coreactions.cpp`: `a.Bind("ui.toggle-sketch", [animate] { SS.GW.ToggleSketchFocus(animate); });`
List: `{ "ui.toggle-sketch", "Sketch › Enter/Exit Sketch" }`. Keymap: `ui.toggle-sketch: Tab`.

- [ ] **Step 4: Run the tests** — both builds; Expected: all pass.

- [ ] **Step 5: Commit**

```bash
git add src/ui.h src/sketchfocus.cpp src/ui/uiactions.cpp src/ui/coreactions.cpp \
        res/keymaps/modern.keymap src/*.cpp src/CMakeLists.txt test/CMakeLists.txt test/core/sketchfocus
git commit -m "NG: Tab focuses the sketch that owns the selection, Tab again returns.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 7: Status/HUD bar

**Files:**
- Create: `src/ui/statusmodel.{h,cpp}`, `test/ng/statusmodel/test.cpp` (copy `line.slvs`)
- Modify: `src/ui/ngui.cpp`, `src/platform/guisdl.cpp` (canvas height excludes the bar),
  CMake lists

**Interfaces:**
- Produces:
  ```cpp
  struct KeyHint { std::string keys, label; };
  struct StatusModel {
      std::string tool;                 // pending description or the modal tool's text
      std::vector<KeyHint> hints;
      std::string group;                // active group's name
      int dof = -1;
      std::string solve;                // "Solved", "Redundant", "Didn't converge", "Too many unknowns"
      bool solveProblem = false;
  };
  StatusModel BuildStatus(const keymap::Keymap &km, const std::string &modalTool,
                          const std::vector<KeyHint> &modalHints);
  ```

- [ ] **Step 1: Write the failing test**

```cpp
#include "solvespace.h"
#include "ui/keypresets.h"
#include "ui/statusmodel.h"

#include "harness.h"

using namespace SolveSpace::Ui;

static bool HasHint(const StatusModel &m, const char *keys, const char *label) {
    for(const KeyHint &h : m.hints) if(h.keys == keys && h.label == label) return true;
    return false;
}

TEST_CASE(idle_status_shows_bound_keys_and_solve_state) {
    CHECK_LOAD("line.slvs");
    std::string err;
    keymap::Keymap km = *ModernPreset(&err);
    StatusModel m = BuildStatus(km, "", {});
    CHECK_TRUE(m.tool.empty());
    CHECK_TRUE(HasHint(m, "Space", "Commands"));
    CHECK_TRUE(HasHint(m, "Shift+A", "Add"));
    Group *g = SK.GetGroup(SS.GW.activeGroup);
    CHECK_EQ_STR(m.group, g->name);
    CHECK_TRUE(m.dof == g->solved.dof);
    CHECK_EQ_STR(m.solve, "Solved");
    CHECK_FALSE(m.solveProblem);
}

TEST_CASE(pending_and_modal_text_win) {
    CHECK_LOAD("line.slvs");
    keymap::Keymap km;
    SS.GW.pending.operation   = GraphicsWindow::Pending::COMMAND;
    SS.GW.pending.description = "click first point of line";
    StatusModel m = BuildStatus(km, "", {});
    CHECK_EQ_STR(m.tool, "click first point of line");
    m = BuildStatus(km, "Grab · X", { { "Esc", "Cancel" } });
    CHECK_EQ_STR(m.tool, "Grab · X");
    CHECK_TRUE(HasHint(m, "Esc", "Cancel"));
    SS.GW.ClearPending();
}

TEST_CASE(unbound_hints_are_left_out) {
    CHECK_LOAD("line.slvs");
    keymap::Keymap km;
    StatusModel m = BuildStatus(km, "", {});
    CHECK_TRUE(m.hints.empty());
}
```

- [ ] **Step 2: Run to verify failure** — Expected: `ui/statusmodel.h` not found.

- [ ] **Step 3: Implement** — `src/ui/statusmodel.h` with the interface above (header comment:
"What the status/HUD bar shows: the active tool, the keys that work now, DOF and solver state.").
`src/ui/statusmodel.cpp`:

```cpp
#include "solvespace.h"
#include "ui/statusmodel.h"

namespace SolveSpace {
namespace Ui {

static void AddHint(StatusModel *m, const keymap::Keymap &km, const char *command,
                    const char *label) {
    auto chords = km.ChordsFor(command);
    if(!chords.empty()) m->hints.push_back({ keymap::FormatChord(chords[0]), label });
}

StatusModel BuildStatus(const keymap::Keymap &km, const std::string &modalTool,
                        const std::vector<KeyHint> &modalHints) {
    StatusModel m;
    if(!modalTool.empty()) {
        m.tool  = modalTool;
        m.hints = modalHints;
    } else if(SS.GW.pending.operation != GraphicsWindow::Pending::NONE) {
        if(SS.GW.pending.description) m.tool = SS.GW.pending.description;
        m.hints = modalHints;
        AddHint(&m, km, "unselect-all", "Cancel");
    } else {
        AddHint(&m, km, "ui.command-palette", "Commands");
        AddHint(&m, km, "ui.pie-add", "Add");
        AddHint(&m, km, "ui.pie-constrain", "Constrain");
        AddHint(&m, km, "ui.smart-dimension", "Dimension");
        AddHint(&m, km, "ui.grab", "Grab");
        AddHint(&m, km, "ui.toggle-sketch", "Sketch");
        AddHint(&m, km, "ui.pie-view", "Views");
    }
    if(Group *g = SK.group.FindByIdNoOops(SS.GW.activeGroup)) {
        m.group = g->name;
        m.dof   = g->solved.dof;
        switch(g->solved.how) {
            case SolveResult::OKAY:                     m.solve = "Solved"; break;
            case SolveResult::REDUNDANT_OKAY:           m.solve = "Redundant"; break;
            case SolveResult::DIDNT_CONVERGE:
            case SolveResult::REDUNDANT_DIDNT_CONVERGE: m.solve = "Didn't converge"; break;
            case SolveResult::TOO_MANY_UNKNOWNS:        m.solve = "Too many unknowns"; break;
        }
        m.solveProblem = g->solved.how != SolveResult::OKAY;
    }
    return m;
}

}
}
```
(`ui.pie-constrain`, `ui.smart-dimension` and `ui.grab` arrive in Tasks 8–11; unbound names are
simply skipped, which `unbound_hints_are_left_out` pins.)

Drawing (`ngui.cpp`): a `DrawStatusBar(const keymap::Keymap &km)` called every frame from
`DrawNgUi`: a borderless ImGui window anchored to the bottom edge, full width, height
`ImGui::GetFrameHeight() + 2 * style.WindowPadding.y`, flags
`NoDecoration | NoSavedSettings | NoMove | NoFocusOnAppearing | NoBringToFrontOnFocus | NoNav`.
Layout, left to right: tool text (bold colour `ImGuiCol_Text`); hints as `keys` in the mono font
(`MonoFont()`) inside a rounded `ImGuiCol_FrameBg` box followed by `label` in `TextDisabled`;
right-aligned: `group`, `DOF n` (mono), and `solve` in `ImGuiCol_PlotHistogram` when fine or
`ImGuiCol_PlotLinesHovered` when `solveProblem`. `ngui` keeps the modal tool's text and hints in a
`static std::string gModalTool; static std::vector<KeyHint> gModalHints;` set by Tasks 11 and 13;
until then both are empty. `ngui.h` exposes `float StatusBarHeight();` and `guisdl.cpp` reports the
main window's content height minus it, so the canvas never draws under the bar (find where the main
window's `GetContentSize` is answered and subtract it there).

- [ ] **Step 4: Run tests and smoke**

Run both builds and suites (all pass), then:
```sh
timeout -s KILL 30 env SOLVESPACE_NG_SCRIPT="wait 3; shot /tmp/claude-1002/m1b-status.png; key L; wait 3; shot /tmp/claude-1002/m1b-status-line.png; quit" build-ng/bin/solvespace-ng test/core/commands/line.slvs
```
Expected: the bar shows hints when idle and "click first point of line" after `L`. Look at both.

- [ ] **Step 5: Commit**

```bash
git add src/ui/statusmodel.* src/ui/ngui.* src/platform/guisdl.cpp src/CMakeLists.txt \
        test/CMakeLists.txt test/ng/statusmodel
git commit -m "NG: status/HUD bar with the active tool, live key hints, DOF and solver state.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

- [ ] **Step 6: Owner checkpoint (does not stop execution)**

Push the branch (`git push -u origin ng/m1b-interaction`) and tell the owner, in one short
message, what is testable now: `Shift+A` and `` ` `` pies, numpad 1/3/7 (+`Ctrl`), `.`, `H`,
`Alt+H`, `Shift+H`, `Tab`, and the status bar. Then continue with Task 8.

---

### Task 8: Smart dimension (`D`)

**Files:**
- Create: `src/constraintprobe.cpp`, `test/core/smartdimension/test.cpp` (copy
  `test/core/commands/line.slvs`, `test/constraint/angle/normal.slvs` as `angle.slvs`, and
  `test/constraint/diameter/normal.slvs` as `diameter.slvs`)
- Modify: `src/ui.h`, `src/ui/uiactions.cpp`, `src/ui/coreactions.cpp`, `res/keymaps/modern.keymap`,
  CMake lists

**Interfaces:**
- Produces (`GraphicsWindow`):
  ```cpp
  Command SmartDimensionCommand(); // reads gs after GroupSelection(): ANGLE or DISTANCE_DIA
  void    SmartDimension();        // hover fallback, then Constraint::MenuConstrain(that command)
  ```
- Action: `ui.smart-dimension`.

- [ ] **Step 1: Write the failing test**

```cpp
#include "solvespace.h"

#include "harness.h"

static int Count(Constraint::Type t) {
    int n = 0;
    for(Constraint &c : SK.constraint) if(c.type == t) n++;
    return n;
}

static std::vector<hEntity> EntitiesOfType(Entity::Type t) {
    std::vector<hEntity> out;
    for(Entity &e : SK.entity) if(e.type == t && e.group == SS.GW.activeGroup) out.push_back(e.h);
    return out;
}

TEST_CASE(two_lines_get_an_angle) {
    CHECK_LOAD("angle.slvs");
    SS.immediatelyEditDimension = false;
    auto lines = EntitiesOfType(Entity::Type::LINE_SEGMENT);
    CHECK_TRUE(lines.size() >= 2);
    int before = Count(Constraint::Type::ANGLE);
    SS.GW.ClearSelection();
    SS.GW.MakeSelected(lines[0]);
    SS.GW.MakeSelected(lines[1]);
    SS.GW.SmartDimension();
    CHECK_TRUE(Count(Constraint::Type::ANGLE) == before + 1);
}

TEST_CASE(one_line_gets_a_length) {
    CHECK_LOAD("line.slvs");
    SS.immediatelyEditDimension = false;
    auto lines = EntitiesOfType(Entity::Type::LINE_SEGMENT);
    int before = Count(Constraint::Type::PT_PT_DISTANCE);
    SS.GW.ClearSelection();
    SS.GW.MakeSelected(lines[0]);
    SS.GW.SmartDimension();
    CHECK_TRUE(Count(Constraint::Type::PT_PT_DISTANCE) == before + 1);
}

TEST_CASE(circle_gets_a_diameter) {
    CHECK_LOAD("diameter.slvs");
    SS.immediatelyEditDimension = false;
    std::vector<hEntity> circles = EntitiesOfType(Entity::Type::CIRCLE);
    auto arcs = EntitiesOfType(Entity::Type::ARC_OF_CIRCLE);
    circles.insert(circles.end(), arcs.begin(), arcs.end());
    CHECK_TRUE(!circles.empty());
    int before = Count(Constraint::Type::DIAMETER);
    SS.GW.ClearSelection();
    SS.GW.MakeSelected(circles[0]);
    SS.GW.SmartDimension();
    CHECK_TRUE(Count(Constraint::Type::DIAMETER) == before + 1);
}

TEST_CASE(smart_dimension_uses_hover) {
    CHECK_LOAD("line.slvs");
    SS.immediatelyEditDimension = false;
    auto lines = EntitiesOfType(Entity::Type::LINE_SEGMENT);
    int before = SK.constraint.n;
    SS.GW.ClearSelection();
    SS.GW.hover.Clear();
    SS.GW.hover.entity = lines[0];
    SS.GW.SmartDimension();
    CHECK_TRUE(SK.constraint.n == before + 1);
}
```
(If a fixture's active group is not the one holding the geometry, select the entities' group with
`SS.GW.ActivateGroup` from Task 6 first and record a ruling. If `line.slvs` already has a length on
its line, MenuConstrain's duplicate check removes the new one: pick a fixture without it and record
which.)

- [ ] **Step 2: Run to verify failure** — Expected: no member `SmartDimension`.

- [ ] **Step 3: Implement** — `src/constraintprobe.cpp`:

```cpp
//-----------------------------------------------------------------------------
// Which constraints fit the current selection: smart dimension and the constrain pie.
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {

Command GraphicsWindow::SmartDimensionCommand() {
    if((gs.vectors == 2 || gs.vectors == 3 || gs.vectors == 4) && gs.n == gs.vectors) {
        return Command::ANGLE;
    }
    return Command::DISTANCE_DIA;
}

void GraphicsWindow::SmartDimension() {
    SelectHoveredIfNothingSelected();
    GroupSelection();
    Constraint::MenuConstrain(SmartDimensionCommand());
}

} // namespace SolveSpace
```
`coreactions.cpp`: `a.Bind("ui.smart-dimension", [] { SS.GW.SmartDimension(); });`
List: `{ "ui.smart-dimension", "Constrain › Smart Dimension" }`.
Keymap: `ui.smart-dimension: D`; delete the `constrain-distance: D` line.

- [ ] **Step 4: Run tests** — both builds; all pass.

- [ ] **Step 5: Commit**

```bash
git add src/ui.h src/constraintprobe.cpp src/ui/uiactions.cpp src/ui/coreactions.cpp \
        res/keymaps/modern.keymap src/CMakeLists.txt test/CMakeLists.txt test/core/smartdimension
git commit -m "NG: D is smart dimension (angle for two lines, else distance/diameter).

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 9: Constraint applicability and the constrain pie (`F`)

**Files:**
- Modify: `src/constraintprobe.cpp`, `src/sketch.h`, `src/ui/uiactions.cpp`, `src/ui/ngui.cpp`,
  `res/keymaps/modern.keymap`
- Create: `test/core/constraintprobe/test.cpp` (copy `line.slvs`)

**Interfaces:**
- Produces:
  ```cpp
  // Constraint (static): true if MenuConstrain(id) would act on the current SS.GW.gs.
  // Call SS.GW.GroupSelection() first. Geometry checks inside the tangent helpers are not
  // mirrored: a tangent may still report a bad selection when chosen.
  static bool AppliesToSelection(Command id);
  static const std::vector<Command> &PieCommands(); // F-pie candidates, in priority order
  ```
- Action: `ui.pie-constrain`.

- [ ] **Step 1: Write the failing test** — a cross-check of the predicate against
  `MenuConstrain` itself over many selections:

```cpp
#include "solvespace.h"

#include "harness.h"

// Adds unconstrained geometry to the active sketch: two lines, a circle, an arc, a point.
static std::vector<hEntity> AddGeometry() {
    std::vector<hEntity> pool;
    auto at = [](double x, double y) {
        Entity *wp = SK.GetEntity(SS.GW.ActiveWorkplane());
        Vector o = SK.GetEntity(wp->point[0])->PointGetNum();
        return o.Plus(wp->Normal()->NormalU().ScaledBy(x)).Plus(wp->Normal()->NormalV().ScaledBy(y));
    };
    hRequest l1 = SS.GW.AddRequest(Request::Type::LINE_SEGMENT, false);
    SK.GetEntity(l1.entity(1))->PointForceTo(at(100, 100));
    SK.GetEntity(l1.entity(2))->PointForceTo(at(160, 120));
    hRequest l2 = SS.GW.AddRequest(Request::Type::LINE_SEGMENT, false);
    SK.GetEntity(l2.entity(1))->PointForceTo(at(100, 200));
    SK.GetEntity(l2.entity(2))->PointForceTo(at(130, 260));
    hRequest c = SS.GW.AddRequest(Request::Type::CIRCLE, false);
    SK.GetEntity(c.entity(1))->PointForceTo(at(300, 100));
    SK.GetEntity(c.entity(64))->DistanceForceTo(20);
    hRequest a = SS.GW.AddRequest(Request::Type::ARC_OF_CIRCLE, false);
    SK.GetEntity(a.entity(1))->PointForceTo(at(300, 300));
    SK.GetEntity(a.entity(2))->PointForceTo(at(330, 300));
    SK.GetEntity(a.entity(3))->PointForceTo(at(300, 330));
    hRequest p = SS.GW.AddRequest(Request::Type::DATUM_POINT, false);
    SK.GetEntity(p.entity(0))->PointForceTo(at(50, 50));
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    pool = { l1.entity(0), l1.entity(1), l2.entity(0), l2.entity(2), c.entity(0), a.entity(0),
             p.entity(0) };
    return pool;
}

// (file header) #include "commandnames.h" after "solvespace.h", for CommandName().

TEST_CASE(applicability_matches_menu_constrain) {
    CHECK_LOAD("line.slvs");
    SS.immediatelyEditDimension = false;
    std::vector<hEntity> pool = AddGeometry();
    SS.UndoRemember(); // a base state every probe returns to
    std::vector<std::vector<hEntity>> selections;
    for(size_t i = 0; i < pool.size(); i++) {
        selections.push_back({ pool[i] });
        for(size_t j = i + 1; j < pool.size(); j++) {
            selections.push_back({ pool[i], pool[j] });
            for(size_t k = j + 1; k < pool.size(); k++) {
                selections.push_back({ pool[i], pool[j], pool[k] });
            }
        }
    }
    const Command checked[] = {
        Command::DISTANCE_DIA, Command::ON_ENTITY, Command::EQUAL, Command::RATIO,
        Command::DIFFERENCE, Command::AT_MIDPOINT, Command::SYMMETRIC, Command::HORIZONTAL,
        Command::VERTICAL, Command::ANGLE, Command::PARALLEL, Command::PERPENDICULAR,
        Command::WHERE_DRAGGED, Command::ORIENTED_SAME };
    int mismatches = 0;
    for(const auto &sel : selections) {
        for(Command cmd : checked) {
            SS.GW.ClearSelection();
            for(hEntity he : sel) SS.GW.MakeSelected(he);
            SS.GW.GroupSelection();
            bool predicted = Constraint::AppliesToSelection(cmd);
            int before = SK.constraint.n;
            int undoBefore = SS.undo.cnt;
            Constraint::MenuConstrain(cmd);
            bool acted = SK.constraint.n != before;
            while(SS.undo.cnt > undoBefore) SS.UndoUndo();
            SS.UndoClearStack(&SS.redo);
            if(predicted != acted) {
                mismatches++;
                fprintf(stderr, "mismatch: command %s, %d items, predicted %d, acted %d\n",
                        CommandName(cmd), (int)sel.size(), predicted, acted);
            }
        }
    }
    CHECK_TRUE(mismatches == 0);
}

TEST_CASE(pie_commands_are_constraint_commands) {
    for(Command c : Constraint::PieCommands()) CHECK_TRUE(CommandName(c) != nullptr);
    CHECK_TRUE(Constraint::PieCommands().size() >= 8);
}
```
(Tangent cases need shared endpoints, which this pool lacks, so curve-tangent PARALLEL selections
are predicted false by count where MenuConstrain also fails: if the cross-check shows a mismatch in
a tangent case, the fix is to make the predicate return false for that count combination *only*
if MenuConstrain fails for every geometry; otherwise exclude that combination from the test and
record a ruling. Never change MenuConstrain.)

- [ ] **Step 2: Run to verify failure** — Expected: no member `AppliesToSelection`.

- [ ] **Step 3: Implement** — declare in `class Constraint` (`src/sketch.h`, next to
`MenuConstrain`), define in `src/constraintprobe.cpp`. Mirror each `case` of `MenuConstrain`
(`src/constraint.cpp:256` onward) exactly:

```cpp
bool Constraint::AppliesToSelection(Command id) {
    auto const &gs = SS.GW.gs;
    bool inWorkplane = SS.GW.LockedInWorkplane();
    switch(id) {
        case Command::DISTANCE_DIA:
        case Command::REF_DISTANCE:
            return (gs.points == 2 && gs.n == 2) ||
                   (gs.lineSegments == 1 && gs.n == 1) ||
                   (gs.vectors == 1 && gs.points == 2 && gs.n == 3) ||
                   (gs.workplanes == 1 && gs.points == 1 && gs.n == 2) ||
                   (gs.lineSegments == 1 && gs.points == 1 && gs.n == 2) ||
                   (gs.faces == 1 && gs.points == 1 && gs.n == 2) ||
                   (gs.circlesOrArcs == 1 && gs.n == 1);
        case Command::ON_ENTITY:
            return (gs.points >= 2 && gs.points == gs.n) ||
                   (gs.points == 1 && gs.workplanes == 1 && gs.n == 2) ||
                   (gs.points == 1 && gs.lineSegments == 1 && gs.n == 2) ||
                   (gs.points == 1 && gs.circlesOrArcs == 1 && gs.n == 2) ||
                   (gs.points == 1 && gs.faces >= 1 && gs.n == gs.points + gs.faces);
        case Command::EQUAL:
            return (gs.lineSegments >= 2 && gs.lineSegments == gs.n) ||
                   (gs.lineSegments == 2 && gs.points == 2 && gs.n == 4) ||
                   (gs.lineSegments == 1 && gs.points == 2 && gs.n == 3) ||
                   (gs.lineSegments == 2 && gs.points == 1 && gs.n == 3) ||
                   (gs.circlesOrArcs >= 2 && gs.circlesOrArcs == gs.n) ||
                   (gs.arcs == 1 && gs.lineSegments == 1 && gs.n == 2);
        case Command::RATIO:
        case Command::DIFFERENCE:
            return (gs.lineSegments == 2 && gs.n == 2) || (gs.arcs == 2 && gs.n == 2) ||
                   (gs.lineSegments == 1 && gs.arcs == 1 && gs.n == 2);
        case Command::AT_MIDPOINT:
            return (gs.lineSegments == 1 && gs.points == 1 && gs.n == 2) ||
                   (gs.lineSegments == 1 && gs.workplanes == 1 && gs.n == 2);
        case Command::SYMMETRIC: {
            bool ok = (gs.points == 2 && ((gs.workplanes == 1 && gs.n == 3) || gs.n == 2)) ||
                      (gs.lineSegments == 1 && ((gs.workplanes == 1 && gs.n == 2) || gs.n == 1)) ||
                      (inWorkplane && gs.lineSegments == 2 && gs.n == 2) ||
                      (inWorkplane && gs.lineSegments == 1 && gs.points == 2 && gs.n == 3);
            // Two points or one line with no symmetry entity become horizontal/vertical symmetry,
            // which needs a workplane.
            bool needsWorkplane = (gs.points == 2 && gs.n == 2) ||
                                  (gs.lineSegments == 1 && gs.n == 1);
            return ok && (!needsWorkplane || inWorkplane);
        }
        case Command::VERTICAL:
        case Command::HORIZONTAL:
            if(SS.GW.ActiveWorkplane() == Entity::FREE_IN_3D) return false;
            return (gs.lineSegments > 0 && gs.lineSegments == gs.n) ||
                   (gs.points >= 2 && gs.n == gs.points);
        case Command::ORIENTED_SAME:
            return gs.anyNormals == 2 && gs.n == 2;
        case Command::OTHER_ANGLE:
        case Command::REFERENCE:
            return gs.constraints == 1 && gs.n == 0;
        case Command::ANGLE:
        case Command::REF_ANGLE:
            return (gs.vectors == 3 && gs.n == 3) || (gs.vectors == 4 && gs.n == 4) ||
                   (gs.vectors == 2 && gs.n == 2);
        case Command::PARALLEL:
            return (gs.faces == 2 && gs.n == 2) || (gs.vectors > 1 && gs.vectors == gs.n) ||
                   (gs.lineSegments == 1 && gs.arcs == 1 &&
                    (gs.n == 2 || (gs.points == 1 && gs.n == 3))) ||
                   (gs.lineSegments == 1 && gs.cubics == 1 &&
                    (gs.n == 2 || (gs.points == 1 && gs.n == 3))) ||
                   (inWorkplane && gs.cubics + gs.arcs == 2 &&
                    (gs.n == 2 || (gs.points == 2 && gs.n == 4)));
        case Command::PERPENDICULAR:
            return (gs.faces == 2 && gs.n == 2) || (gs.vectors == 2 && gs.n == 2);
        case Command::WHERE_DRAGGED:
            return gs.points == 1 && gs.n == 1;
        case Command::COMMENT:
            return true;
        default:
            return false;
    }
}

const std::vector<Command> &Constraint::PieCommands() {
    static const std::vector<Command> list = {
        Command::ON_ENTITY, Command::HORIZONTAL, Command::VERTICAL, Command::PARALLEL,
        Command::PERPENDICULAR, Command::EQUAL, Command::AT_MIDPOINT, Command::SYMMETRIC,
        Command::DISTANCE_DIA, Command::ANGLE, Command::RATIO, Command::DIFFERENCE,
        Command::ORIENTED_SAME, Command::WHERE_DRAGGED, Command::REFERENCE, Command::OTHER_ANGLE };
    return list;
}
```
Before writing the SYMMETRIC/HORIZONTAL branches, read the exact source at
`src/constraint.cpp` (the `SYMMETRIC` case and the `c.entityA == Entity::NO_ENTITY` block after it)
to confirm the workplane requirement; the cross-check test is the judge.

`ngui.cpp`, bind the pie (labels come from the core menu text so they match the menus and are
translated):
```cpp
    Actions().Bind("ui.pie-constrain", [] {
        SS.GW.SelectHoveredIfNothingSelected();
        SS.GW.GroupSelection();
        std::vector<PieItem> items;
        for(Command c : Constraint::PieCommands()) {
            if(items.size() == 8) break;
            if(!Constraint::AppliesToSelection(c)) continue;
            items.push_back({ MenuLabelFor(c), CommandName(c) });
        }
        Pie().Open("Constrain", std::move(items));
    });
```
with `static std::string MenuLabelFor(Command c)` in `ngui.cpp`:
`std::string l = GraphicsWindow::MenuLabel(c); return l.empty() ? CommandName(c) : Platform::StripMnemonic(l);`.
`Menu[]` is file-local to `src/graphicswin.cpp` (line 41), so add this there, declared `static`
in `GraphicsWindow` (`Translate` is what `PopulateMainMenu` uses, and returns `std::string`):
```cpp
// The translated menu text of a command ("&Horizontal"), or "" if it has no menu item.
std::string GraphicsWindow::MenuLabel(Command id) {
    for(int i = 0; Menu[i].level >= 0; i++) {
        if(Menu[i].cmd == id && Menu[i].label) return Translate(Menu[i].label);
    }
    return "";
}
```
List: `{ "ui.pie-constrain", "Constrain › Constrain… (pie)" }`. Keymap: `ui.pie-constrain: F`.

- [ ] **Step 4: Run tests** — both builds; all pass, `mismatches == 0`.

- [ ] **Step 5: Smoke run**

```sh
timeout -s KILL 30 env SOLVESPACE_NG_SCRIPT="key A; key F; wait 3; shot /tmp/claude-1002/m1b-fpie.png; key Esc; quit" build-ng/bin/solvespace-ng test/core/commands/line.slvs
```
Expected: the pie shows only constraints valid for "everything selected". Look at it.

- [ ] **Step 6: Commit**

```bash
git add src/sketch.h src/constraintprobe.cpp src/graphicswin.cpp src/ui.h src/ui/uiactions.cpp \
        src/ui/ngui.cpp res/keymaps/modern.keymap test/CMakeLists.txt test/core/constraintprobe
git commit -m "NG: F opens a constrain pie with only the constraints the selection allows.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 10: Grab in the core (`Pending::GRABBING`)

**Files:**
- Create: `src/grab.cpp`, `test/core/grab/test.cpp` (copy `line.slvs`)
- Modify: `src/ui.h` (enum value, state, methods), `src/mouse.cpp`, `src/graphicswin.cpp`, CMake

**Interfaces:**
- Produces (`GraphicsWindow`):
  ```cpp
  // Pending::GRABBING = 12
  struct {
      Point2d                                 startMouse, lastMouse;
      std::vector<std::pair<hEntity, Vector>> start;  // dragged points, start positions
      int                                     axis;   // -1 free, 0/1/2 = X/Y/Z (or u/v/n)
      std::optional<double>                   typedMm;
  } grab;
  bool   StartGrab(Point2d mouse);   // selection, else hover; false (no undo entry) if nothing
  void   GrabMouseMoved(Point2d mouse);
  void   GrabSetAxis(int axis);      // same axis again unlocks
  void   GrabSetTyped(std::optional<double> mm);
  void   GrabConfirm();
  void   GrabCancel();               // exact restore, no redo entry
  Vector GrabAxisVector(int axis);   // workplane u/v/n when locked in one, else world X/Y/Z
  ```

- [ ] **Step 1: Write the failing test**

```cpp
#include "solvespace.h"

#include "harness.h"

// A fresh, unconstrained line in the active sketch; returns its request.
static hRequest FreeLine() {
    Entity *wp = SK.GetEntity(SS.GW.ActiveWorkplane());
    Vector o = SK.GetEntity(wp->point[0])->PointGetNum();
    hRequest hr = SS.GW.AddRequest(Request::Type::LINE_SEGMENT, false);
    SK.GetEntity(hr.entity(1))->PointForceTo(o.Plus(Vector::From(200, 200, 0)));
    SK.GetEntity(hr.entity(2))->PointForceTo(o.Plus(Vector::From(260, 230, 0)));
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    return hr;
}

static Vector P(hEntity h) { return SK.GetEntity(h)->PointGetNum(); }

TEST_CASE(grab_moves_selection_with_the_mouse) {
    CHECK_LOAD("line.slvs");
    hRequest hr = FreeLine();
    Vector a0 = P(hr.entity(1)), b0 = P(hr.entity(2));
    SS.GW.ClearSelection();
    SS.GW.MakeSelected(hr.entity(0));
    CHECK_TRUE(SS.GW.StartGrab(Point2d::From(0, 0)));
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::GRABBING);
    SS.GW.GrabMouseMoved(Point2d::From(40, 0));
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Vector d = SS.GW.projRight.ScaledBy(40 / SS.GW.scale);
    CHECK_TRUE(P(hr.entity(1)).Minus(a0.Plus(d)).Magnitude() < 1e-6);
    CHECK_TRUE(P(hr.entity(2)).Minus(b0.Plus(d)).Magnitude() < 1e-6);
}

TEST_CASE(grab_axis_lock_and_typed_distance) {
    CHECK_LOAD("line.slvs");
    hRequest hr = FreeLine();
    Vector a0 = P(hr.entity(1));
    SS.GW.ClearSelection();
    SS.GW.MakeSelected(hr.entity(0));
    CHECK_TRUE(SS.GW.StartGrab(Point2d::From(0, 0)));
    SS.GW.GrabSetAxis(0);
    Vector ax = SS.GW.GrabAxisVector(0);
    SS.GW.GrabMouseMoved(Point2d::From(30, 50));
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Vector moved = P(hr.entity(1)).Minus(a0);
    CHECK_TRUE(moved.Minus(ax.ScaledBy(moved.Dot(ax))).Magnitude() < 1e-6); // only along X
    SS.GW.GrabSetTyped(12.0);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_TRUE(P(hr.entity(1)).Minus(a0.Plus(ax.ScaledBy(12))).Magnitude() < 1e-6);
    SS.GW.GrabSetAxis(0); // same axis again: unlocked
    CHECK_TRUE(SS.GW.grab.axis == -1);
}

TEST_CASE(grab_cancel_restores_without_redo) {
    CHECK_LOAD("line.slvs");
    hRequest hr = FreeLine();
    // Tie another point to the dragged one so the solver moves it too.
    hRequest other = FreeLine();
    Constraint::ConstrainCoincident(hr.entity(2), other.entity(1));
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Vector a0 = P(hr.entity(1)), o0 = P(other.entity(2)), c0 = P(other.entity(1));
    int undo0 = SS.undo.cnt;
    SS.GW.ClearSelection();
    SS.GW.MakeSelected(hr.entity(0));
    CHECK_TRUE(SS.GW.StartGrab(Point2d::From(0, 0)));
    SS.GW.GrabMouseMoved(Point2d::From(80, 40));
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_TRUE(P(other.entity(1)).Minus(c0).Magnitude() > 1e-3); // the solver moved it
    SS.GW.GrabCancel();
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::NONE);
    CHECK_TRUE(P(hr.entity(1)).Minus(a0).Magnitude() < 1e-9);
    CHECK_TRUE(P(other.entity(1)).Minus(c0).Magnitude() < 1e-9);
    CHECK_TRUE(P(other.entity(2)).Minus(o0).Magnitude() < 1e-9);
    CHECK_TRUE(SS.undo.cnt == undo0);
    CHECK_TRUE(SS.redo.cnt == 0);
}

TEST_CASE(grab_confirm_is_one_undo_step) {
    CHECK_LOAD("line.slvs");
    hRequest hr = FreeLine();
    Vector a0 = P(hr.entity(1));
    int undo0 = SS.undo.cnt;
    SS.GW.ClearSelection();
    SS.GW.MakeSelected(hr.entity(0));
    CHECK_TRUE(SS.GW.StartGrab(Point2d::From(0, 0)));
    SS.GW.GrabMouseMoved(Point2d::From(50, 0));
    SS.GW.GrabConfirm();
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::NONE);
    CHECK_TRUE(SS.undo.cnt == undo0 + 1);
    RUN_COMMAND("undo");
    CHECK_TRUE(P(hr.entity(1)).Minus(a0).Magnitude() < 1e-9);
}

TEST_CASE(grab_with_nothing_does_nothing) {
    CHECK_LOAD("line.slvs");
    int undo0 = SS.undo.cnt;
    SS.GW.ClearSelection();
    SS.GW.hover.Clear();
    CHECK_FALSE(SS.GW.StartGrab(Point2d::From(0, 0)));
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::NONE);
    CHECK_TRUE(SS.undo.cnt == undo0);
}

TEST_CASE(escape_cancels_a_grab) {
    CHECK_LOAD("line.slvs");
    hRequest hr = FreeLine();
    Vector a0 = P(hr.entity(1));
    SS.GW.ClearSelection();
    SS.GW.MakeSelected(hr.entity(0));
    CHECK_TRUE(SS.GW.StartGrab(Point2d::From(0, 0)));
    SS.GW.GrabMouseMoved(Point2d::From(50, 0));
    RUN_COMMAND("unselect-all");
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::NONE);
    CHECK_TRUE(P(hr.entity(1)).Minus(a0).Magnitude() < 1e-9);
}
```
(`FreeLine` places points with world offsets in the fixture's XY workplane; if the fixture's
workplane is not XY, use the workplane u/v as in Task 9's `AddGeometry` and record it.)

- [ ] **Step 2: Run to verify failure** — Expected: no `Pending::GRABBING`.

- [ ] **Step 3: Implement**

`src/ui.h`: add `GRABBING = 12,` to `enum class Pending`; add the `grab` struct and methods to
`GraphicsWindow` (needs `<optional>`, already available through `solvespace.h` if not add it).

`src/grab.cpp`:
```cpp
//-----------------------------------------------------------------------------
// G: grab the selection and move it under the solver without holding a mouse button, with
// X/Y/Z axis locks and a typed distance. Confirm with a click or Enter; Esc or right-click
// cancels and restores the sketch exactly.
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {

Vector GraphicsWindow::GrabAxisVector(int axis) {
    if(LockedInWorkplane()) {
        EntityBase *n = SK.GetEntity(ActiveWorkplane())->Normal();
        switch(axis) {
            case 0: return n->NormalU();
            case 1: return n->NormalV();
            case 2: return n->NormalN();
        }
    }
    switch(axis) {
        case 0: return Vector::From(1, 0, 0);
        case 1: return Vector::From(0, 1, 0);
        case 2: return Vector::From(0, 0, 1);
    }
    ssassert(false, "Unexpected grab axis");
}

bool GraphicsWindow::StartGrab(Point2d mouse) {
    if(pending.operation != Pending::NONE) return false;
    SelectHoveredIfNothingSelected();
    pending.points.Clear();
    for(const Selection &s : selection) {
        if(s.entity.v) StartDraggingByEntity(s.entity);
    }
    if(pending.points.n == 0) return false;

    SS.UndoRemember();
    grab.startMouse = mouse;
    grab.lastMouse  = mouse;
    grab.axis       = -1;
    grab.typedMm.reset();
    grab.start.clear();
    for(hEntity h : pending.points) grab.start.push_back({ h, SK.GetEntity(h)->PointGetNum() });
    pending.operation   = Pending::GRABBING;
    pending.description = _("grab: move the mouse; X/Y/Z lock an axis; type a distance");
    return true;
}

void GraphicsWindow::GrabMouseMoved(Point2d mouse) {
    if(pending.operation != Pending::GRABBING) return;
    Vector delta;
    if(grab.axis >= 0 && grab.typedMm) {
        delta = GrabAxisVector(grab.axis).ScaledBy(*grab.typedMm);
    } else {
        delta = projRight.ScaledBy((mouse.x - grab.startMouse.x) / scale)
                    .Plus(projUp.ScaledBy((mouse.y - grab.startMouse.y) / scale));
        if(grab.axis >= 0) {
            Vector ax = GrabAxisVector(grab.axis);
            delta     = ax.ScaledBy(delta.Dot(ax));
        }
    }
    for(auto &[h, p0] : grab.start) {
        SK.GetEntity(h)->PointForceTo(p0.Plus(delta));
        SS.MarkGroupDirtyByEntity(h);
    }
    grab.lastMouse = mouse;
    Invalidate();
}

void GraphicsWindow::GrabSetAxis(int axis) {
    grab.axis = grab.axis == axis ? -1 : axis;
    GrabMouseMoved(grab.lastMouse);
}

void GraphicsWindow::GrabSetTyped(std::optional<double> mm) {
    grab.typedMm = mm;
    GrabMouseMoved(grab.lastMouse);
}

void GraphicsWindow::GrabConfirm() {
    if(pending.operation != Pending::GRABBING) return;
    ClearPending();
    grab.start.clear();
    Invalidate();
}

void GraphicsWindow::GrabCancel() {
    if(pending.operation != Pending::GRABBING) return;
    ClearPending();
    grab.start.clear();
    // StartGrab remembered the state just before the grab; return to it and forget the redo
    // entry the undo would leave.
    SS.UndoUndo();
    SS.UndoClearStack(&SS.redo);
    SS.UndoEnableMenus();
}

} // namespace SolveSpace
```
`StartGrab` also sets `grab.lastMouse = mouse;`. The `typedMm`
without an axis is ignored (the HUD says "lock an axis to type a distance").

`src/mouse.cpp`:
- In `MouseMoved`, in the dragging `switch(pending.operation)`, add
  `case Pending::GRABBING: GrabMouseMoved(mp); break;`. GRABBING must not be cleared by the
  `!leftDown` check near the top (it only lists DRAGGING_POINTS/MARQUEE — leave it so).
- In `MouseLeftDown`, right after `SS.TW.HideEditControl();`:
  `if(pending.operation == Pending::GRABBING) { GrabConfirm(); return; }`
- At the top of `MouseRightUp` (after `Invalidate();`):
  `if(pending.operation == Pending::GRABBING) { GrabCancel(); return; }`

`src/graphicswin.cpp`, `MenuEdit` `case Command::UNSELECT_ALL:` first lines:
```cpp
            if(SS.GW.pending.operation == Pending::GRABBING) {
                SS.GW.GrabCancel();
                break;
            }
```

- [ ] **Step 4: Run tests** — both builds; all pass (including every existing drag/undo test).

- [ ] **Step 5: Commit**

```bash
git add src/ui.h src/grab.cpp src/mouse.cpp src/graphicswin.cpp src/CMakeLists.txt \
        test/CMakeLists.txt test/core/grab
git commit -m "Core: grab mode (move selection under the solver, axis locks, typed distance).

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 11: Grab tool keys and HUD (`G`)

**Files:**
- Create: `src/ui/grabtool.{h,cpp}`, `test/ng/grabtool/test.cpp` (copy `line.slvs`)
- Modify: `src/ui/uiactions.cpp`, `src/ui/coreactions.cpp`, `src/ui/ngui.cpp`,
  `res/keymaps/modern.keymap`, CMake lists

**Interfaces:**
- Consumes: Task 10's grab API; `TypedFields`; `KeyRouter::modal`; `KeyHint`.
- Produces:
  ```cpp
  class GrabTool {
  public:
      bool Start();                                  // StartGrab at the current mouse position
      bool Active() const;                           // pending.operation == GRABBING
      bool HandleChord(const keymap::Chord &c);      // see below
      std::string Description() const;              // "Grab · X · 12" etc.
      std::vector<KeyHint> Hints() const;
  };
  GrabTool &Grab();
  ```
  Keys while active: `X`/`Y`/`Z` axis; typed-field keys update the distance; `Enter`/`NumEnter`
  confirm; `Esc` cancels; any other chord without `Ctrl` is swallowed; `Ctrl` chords pass through.
- Action: `ui.grab`.

- [ ] **Step 1: Write the failing test**

```cpp
#include "solvespace.h"
#include "ui/grabtool.h"
#include "ui/keyrouter.h"

#include "harness.h"

using namespace SolveSpace::Ui;

static keymap::Chord C(const char *s) { return *keymap::ParseChord(s); }

static hRequest FreeLine() {
    Entity *wp = SK.GetEntity(SS.GW.ActiveWorkplane());
    Vector o = SK.GetEntity(wp->point[0])->PointGetNum();
    hRequest hr = SS.GW.AddRequest(Request::Type::LINE_SEGMENT, false);
    SK.GetEntity(hr.entity(1))->PointForceTo(o.Plus(Vector::From(200, 200, 0)));
    SK.GetEntity(hr.entity(2))->PointForceTo(o.Plus(Vector::From(260, 230, 0)));
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    return hr;
}

TEST_CASE(grab_tool_axis_and_typed_distance) {
    CHECK_LOAD("line.slvs");
    hRequest hr = FreeLine();
    Vector a0 = SK.GetEntity(hr.entity(1))->PointGetNum();
    SS.GW.ClearSelection();
    SS.GW.MakeSelected(hr.entity(0));
    CHECK_TRUE(Grab().Start());
    CHECK_TRUE(Grab().HandleChord(C("Y")));
    for(const char *k : {"1", "5"}) CHECK_TRUE(Grab().HandleChord(C(k)));
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    Vector ay = SS.GW.GrabAxisVector(1);
    CHECK_TRUE(SK.GetEntity(hr.entity(1))->PointGetNum().Minus(a0.Plus(ay.ScaledBy(15)))
                   .Magnitude() < 1e-6);
    CHECK_TRUE(Grab().Description().find("15") != std::string::npos);
    CHECK_TRUE(Grab().HandleChord(C("Enter")));
    CHECK_FALSE(Grab().Active());
}

TEST_CASE(grab_x_locks_axis_and_never_deletes) {
    CHECK_LOAD("line.slvs");
    hRequest hr = FreeLine();
    int requests = SK.request.n;
    SS.GW.ClearSelection();
    SS.GW.MakeSelected(hr.entity(0));
    std::string err;
    KeyRouter r;
    r.SetKeymap(*keymap::Keymap::Parse("delete: X\n", &err));
    r.modal = [](const keymap::Chord &c) { return Grab().Active() && Grab().HandleChord(c); };
    CHECK_TRUE(Grab().Start());
    CHECK_TRUE(r.Route(C("X"), false) == RouteResult::MODAL);
    CHECK_TRUE(SS.GW.grab.axis == 0);
    CHECK_TRUE(SK.request.n == requests);
    CHECK_TRUE(Grab().HandleChord(C("Esc")));
    CHECK_FALSE(Grab().Active());
    CHECK_TRUE(r.Route(C("X"), false) == RouteResult::COMMAND); // after the grab, X deletes again
}
```

- [ ] **Step 2: Run to verify failure** — Expected: `ui/grabtool.h` not found.

- [ ] **Step 3: Implement** — `src/ui/grabtool.h` declares the interface above (header comment
"The G grab tool: keys and HUD text over GraphicsWindow's grab mode.") plus private
`TypedFields field{{"distance"}};`. `src/ui/grabtool.cpp`:

```cpp
#include "solvespace.h"
#include "ui/grabtool.h"

namespace SolveSpace {
namespace Ui {

GrabTool &Grab() {
    static GrabTool tool;
    return tool;
}

bool GrabTool::Start() {
    field.Clear();
    return SS.GW.StartGrab(SS.GW.currentMousePosition);
}

bool GrabTool::Active() const {
    return SS.GW.pending.operation == GraphicsWindow::Pending::GRABBING;
}

bool GrabTool::HandleChord(const keymap::Chord &c) {
    if(!Active() || c.ctrl) return false;
    if(!c.alt && !c.shift && (c.key == "x" || c.key == "y" || c.key == "z")) {
        SS.GW.GrabSetAxis(c.key[0] - 'x');
        return true;
    }
    if(field.HandleChord(c)) {
        SS.GW.GrabSetTyped(field.ValueMm(0));
        return true;
    }
    if(c.key == "Enter" || c.key == "NumEnter") {
        SS.GW.GrabConfirm();
        return true;
    }
    if(c.key == "Esc") {
        SS.GW.GrabCancel();
        return true;
    }
    return true; // swallow everything else while grabbing
}

std::string GrabTool::Description() const {
    std::string d = "Grab";
    int axis = SS.GW.grab.axis;
    if(axis >= 0) d += std::string(" \xC2\xB7 ") + "XYZ"[axis];
    if(!field.Text(0).empty()) {
        d += " \xC2\xB7 " + field.Text(0);
        if(axis < 0) d += " (lock an axis to use a typed distance)";
    }
    return d;
}

std::vector<KeyHint> GrabTool::Hints() const {
    return { { "X Y Z", "Axis" }, { "0-9", "Distance" }, { "Enter", "Confirm" },
             { "Esc", "Cancel" } };
}

}
}
```
`coreactions.cpp`: `a.Bind("ui.grab", [] { Grab().Start(); });`
List: `{ "ui.grab", "Edit › Grab" }`. Keymap: `ui.grab: G`.
`ngui.cpp` `ModalKey`: after the pie, `if(Grab().Active()) return Grab().HandleChord(c);`; and
each frame set `gModalTool`/`gModalHints` from `Grab()` when active.

- [ ] **Step 4: Run tests** — both builds; all pass.

- [ ] **Step 5: Smoke run**

```sh
timeout -s KILL 30 env SOLVESPACE_NG_SCRIPT="key A; key G; key X; keys 10; wait 3; shot /tmp/claude-1002/m1b-grab.png; key Esc; wait 3; quit" build-ng/bin/solvespace-ng test/core/commands/line.slvs
```
Expected: the screenshot shows the sketch moved 10 along X and the status bar "Grab · X · 10";
after Esc the file is unchanged (no unsaved-changes prompt blocks the quit — if it does, the
cancel left an undo entry: a bug).

- [ ] **Step 6: Commit**

```bash
git add src/ui/grabtool.* src/ui/uiactions.cpp src/ui/coreactions.cpp src/ui/ngui.cpp \
        res/keymaps/modern.keymap src/CMakeLists.txt test/CMakeLists.txt test/ng/grabtool
git commit -m "NG: G grabs the selection; X/Y/Z lock, typed distance, Enter/Esc.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 12: Typed values become constraints (core)

**Files:**
- Create: `src/typedinput.cpp`, `test/core/typedinput/test.cpp` (copy `line.slvs`)
- Modify: `src/mouse.cpp` (extract `ContinueLineChain`; null-window guards), `src/ui.h`, CMake

**Interfaces:**
- Produces (`GraphicsWindow`):
  ```cpp
  void ContinueLineChain(Vector v);  // the "start the next segment" half of a line click
  bool CommitTypedLine(std::optional<double> lengthMm, std::optional<double> angleDeg);
  bool CommitTypedCircle(double diameterMm);
  bool CommitTypedRectangle(std::optional<double> widthMm, std::optional<double> heightMm);
  enum class TypedKind { NONE, LINE, CIRCLE, RECTANGLE };
  TypedKind TypedInputKind();        // what the current pending operation accepts
  ```

- [ ] **Step 1: Write the failing test**

```cpp
#include "solvespace.h"

#include "harness.h"

// Workplane coordinates -> screen coordinates for MouseLeftDown.
static Point2d Screen(double u, double v) {
    Entity *wp = SK.GetEntity(SS.GW.ActiveWorkplane());
    Vector o = SK.GetEntity(wp->point[0])->PointGetNum();
    Vector p = o.Plus(wp->Normal()->NormalU().ScaledBy(u)).Plus(wp->Normal()->NormalV().ScaledBy(v));
    return SS.GW.ProjectPoint(p);
}

static void Click(double u, double v) {
    Point2d s = Screen(u, v);
    SS.GW.MouseLeftDown(s.x, s.y, false, false);
    SS.GW.MouseLeftUp(s.x, s.y, false, false);
}

static void BeginTool(Command c) {
    SS.showToolbar = false;
    SS.GW.LookOntoActiveWorkplane(false);
    SS.GW.ActivateCommand(c);
}

static int Count(Constraint::Type t) {
    int n = 0;
    for(Constraint &c : SK.constraint) if(c.type == t) n++;
    return n;
}

static double AngleDeg(hRequest hr) {
    Entity *wp = SK.GetEntity(SS.GW.ActiveWorkplane());
    Vector d = SK.GetEntity(hr.entity(2))->PointGetNum().Minus(
               SK.GetEntity(hr.entity(1))->PointGetNum());
    double a = atan2(d.Dot(wp->Normal()->NormalV()), d.Dot(wp->Normal()->NormalU())) * 180 / PI;
    return a < 0 ? a + 360 : a;
}

static double Length(hRequest hr) {
    return SK.GetEntity(hr.entity(2))->PointGetNum().Minus(
           SK.GetEntity(hr.entity(1))->PointGetNum()).Magnitude();
}

TEST_CASE(typed_line_length_and_angle) {
    CHECK_LOAD("line.slvs");
    BeginTool(Command::LINE_SEGMENT);
    Click(300, 300);
    CHECK_TRUE(SS.GW.TypedInputKind() == GraphicsWindow::TypedKind::LINE);
    hRequest first = SS.GW.pending.request;
    int dist = Count(Constraint::Type::PT_PT_DISTANCE), ang = Count(Constraint::Type::ANGLE);
    CHECK_TRUE(SS.GW.CommitTypedLine(25.0, 30.0));
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_TRUE(Count(Constraint::Type::PT_PT_DISTANCE) == dist + 1);
    CHECK_TRUE(Count(Constraint::Type::ANGLE) == ang + 1);
    CHECK_EQ_EPS(Length(first), 25.0);
    CHECK_TRUE(fabs(AngleDeg(first) - 30.0) < 1e-6);
    // The chain continues from the new end point, as after a click.
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::DRAGGING_NEW_LINE_POINT);
    CHECK_TRUE(SS.GW.pending.request != first);
}

TEST_CASE(typed_right_angles_become_horizontal_vertical) {
    CHECK_LOAD("line.slvs");
    BeginTool(Command::LINE_SEGMENT);
    Click(300, 300);
    int h = Count(Constraint::Type::HORIZONTAL), v = Count(Constraint::Type::VERTICAL);
    int ang = Count(Constraint::Type::ANGLE);
    CHECK_TRUE(SS.GW.CommitTypedLine(10.0, 180.0));
    CHECK_TRUE(SS.GW.CommitTypedLine(10.0, 270.0));
    CHECK_TRUE(Count(Constraint::Type::HORIZONTAL) == h + 1);
    CHECK_TRUE(Count(Constraint::Type::VERTICAL) == v + 1);
    CHECK_TRUE(Count(Constraint::Type::ANGLE) == ang);
}

TEST_CASE(typed_obtuse_and_negative_angles) {
    CHECK_LOAD("line.slvs");
    BeginTool(Command::LINE_SEGMENT);
    Click(300, 300);
    hRequest a = SS.GW.pending.request;
    CHECK_TRUE(SS.GW.CommitTypedLine(20.0, 150.0));
    hRequest b = SS.GW.pending.request;
    CHECK_TRUE(SS.GW.CommitTypedLine(20.0, -45.0));
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_TRUE(fabs(AngleDeg(a) - 150.0) < 1e-6);
    CHECK_TRUE(fabs(AngleDeg(b) - 315.0) < 1e-6);
}

TEST_CASE(typed_length_only_keeps_direction) {
    CHECK_LOAD("line.slvs");
    BeginTool(Command::LINE_SEGMENT);
    Click(300, 300);
    hRequest first = SS.GW.pending.request;
    Point2d s = Screen(340, 300); // drag towards +u
    SS.GW.MouseMoved(s.x, s.y, false, false, false, false, false);
    SK.GetEntity(first.entity(2))->PointForceTo(SS.GW.UnProjectPoint(s)); // headless: no paint
    CHECK_TRUE(SS.GW.CommitTypedLine(12.0, std::nullopt));
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_EQ_EPS(Length(first), 12.0);
    CHECK_TRUE(fabs(AngleDeg(first)) < 1e-6 || fabs(AngleDeg(first) - 360) < 1e-6);
}

TEST_CASE(typed_circle_diameter) {
    CHECK_LOAD("line.slvs");
    BeginTool(Command::CIRCLE);
    Click(300, 300);
    CHECK_TRUE(SS.GW.TypedInputKind() == GraphicsWindow::TypedKind::CIRCLE);
    hEntity circle = SS.GW.pending.circle;
    int d = Count(Constraint::Type::DIAMETER);
    CHECK_TRUE(SS.GW.CommitTypedCircle(40.0));
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_TRUE(Count(Constraint::Type::DIAMETER) == d + 1);
    CHECK_EQ_EPS(SK.GetEntity(circle)->CircleGetRadiusNum(), 20.0);
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::NONE);
}

TEST_CASE(typed_rectangle_size) {
    CHECK_LOAD("line.slvs");
    BeginTool(Command::RECTANGLE);
    Click(300, 300);
    CHECK_TRUE(SS.GW.TypedInputKind() == GraphicsWindow::TypedKind::RECTANGLE);
    hEntity corner = SS.GW.pending.point;
    int dist = Count(Constraint::Type::PT_PT_DISTANCE);
    CHECK_TRUE(SS.GW.CommitTypedRectangle(30.0, 20.0));
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    CHECK_TRUE(Count(Constraint::Type::PT_PT_DISTANCE) == dist + 2);
    Entity *wp = SK.GetEntity(SS.GW.ActiveWorkplane());
    Vector o = SK.GetEntity(wp->point[0])->PointGetNum();
    Vector p = SK.GetEntity(corner)->PointGetNum().Minus(o);
    CHECK_EQ_EPS(fabs(p.Dot(wp->Normal()->NormalU()) - 300), 30.0);
    CHECK_EQ_EPS(fabs(p.Dot(wp->Normal()->NormalV()) - 300), 20.0);
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::NONE);
}

TEST_CASE(typed_input_outside_a_tool_is_refused) {
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(SS.GW.TypedInputKind() == GraphicsWindow::TypedKind::NONE);
    CHECK_FALSE(SS.GW.CommitTypedLine(10.0, 0.0));
    CHECK_FALSE(SS.GW.CommitTypedCircle(10.0));
}
```

- [ ] **Step 2: Run to verify failure** — Expected: no member `CommitTypedLine`.

- [ ] **Step 3: Make clicks work headless (guard + refactor, behaviour unchanged)**

`src/mouse.cpp`:
- `MouseMoved`: first line becomes `if(window && window->IsEditorVisible()) return;`
- `MouseLeftDown`: `if(window && window->IsEditorVisible()) {`
- Extract from `MouseLeftDown`'s `case Pending::DRAGGING_NEW_LINE_POINT:` everything from
  `// Create a new line segment, so that we continue drawing.` through
  `pending.description = _("click next point of line, or press Esc");` into
  `void GraphicsWindow::ContinueLineChain(Vector v)` (same code, `v` as the parameter), and call
  it there: `ContinueLineChain(v);`.

Run the full suite now: Expected: everything still passes (the new test still fails to compile).

- [ ] **Step 4: Implement** — `src/typedinput.cpp`:

```cpp
//-----------------------------------------------------------------------------
// Typed values while drawing become real constraints: "25 Tab 30 Enter" on a line gives a
// length and an angle constraint, then drawing continues as after a click.
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {

static hConstraint AddValued(Constraint::Type type, hEntity ptA, hEntity ptB, hEntity entityA,
                             hEntity entityB, double val) {
    Constraint c = {};
    c.group     = SS.GW.activeGroup;
    c.workplane = SS.GW.ActiveWorkplane();
    c.type      = type;
    c.ptA       = ptA;
    c.ptB       = ptB;
    c.entityA   = entityA;
    c.entityB   = entityB;
    c.valA      = val;
    return Constraint::AddConstraint(&c, /*rememberForUndo=*/false);
}

GraphicsWindow::TypedKind GraphicsWindow::TypedInputKind() {
    switch(pending.operation) {
        case Pending::DRAGGING_NEW_LINE_POINT: return TypedKind::LINE;
        case Pending::DRAGGING_NEW_RADIUS:     return TypedKind::CIRCLE;
        case Pending::DRAGGING_NEW_POINT:
            if(pending.command == Command::RECTANGLE && pending.requests.n == 4) {
                return TypedKind::RECTANGLE;
            }
            return TypedKind::NONE;
        default: return TypedKind::NONE;
    }
}

bool GraphicsWindow::CommitTypedLine(std::optional<double> lengthMm,
                                     std::optional<double> angleDeg) {
    if(TypedInputKind() != TypedKind::LINE || (!lengthMm && !angleDeg)) return false;
    hRequest hr     = pending.request;
    hEntity  startP = hr.entity(1), endP = hr.entity(2);
    Vector   start  = SK.GetEntity(startP)->PointGetNum();
    Vector   cur    = SK.GetEntity(endP)->PointGetNum().Minus(start);
    bool     inWp   = LockedInWorkplane();
    Vector   u = projRight, v = projUp;
    if(inWp) {
        EntityBase *n = SK.GetEntity(ActiveWorkplane())->Normal();
        u = n->NormalU();
        v = n->NormalV();
    }
    double len = lengthMm ? *lengthMm : cur.Magnitude();
    if(len < LENGTH_EPS) return false;
    Vector dir = cur.Magnitude() > LENGTH_EPS ? cur.WithMagnitude(1) : u;
    double a = 0;
    if(angleDeg && inWp) {
        a   = fmod(fmod(*angleDeg, 360) + 360, 360);
        dir = u.ScaledBy(cos(a * PI / 180)).Plus(v.ScaledBy(sin(a * PI / 180)));
    }
    Vector end = start.Plus(dir.ScaledBy(len));
    SK.GetEntity(endP)->PointForceTo(end);

    if(lengthMm) AddValued(Constraint::Type::PT_PT_DISTANCE, startP, endP,
                           Entity::NO_ENTITY, Entity::NO_ENTITY, len);
    if(angleDeg && inWp) {
        if(fabs(remainder(a, 180)) < 1e-9) {
            Constraint::Constrain(Constraint::Type::HORIZONTAL, Entity::NO_ENTITY,
                                  Entity::NO_ENTITY, hr.entity(0));
        } else if(fabs(remainder(a - 90, 180)) < 1e-9) {
            Constraint::Constrain(Constraint::Type::VERTICAL, Entity::NO_ENTITY,
                                  Entity::NO_ENTITY, hr.entity(0));
        } else {
            // No line-to-axis angle constraint exists: measure against a horizontal
            // construction reference line from the start point.
            hRequest ref = AddRequest(Request::Type::LINE_SEGMENT, /*rememberForUndo=*/false);
            SK.GetRequest(ref)->construction = true;
            SK.GetEntity(ref.entity(1))->PointForceTo(start);
            SK.GetEntity(ref.entity(2))->PointForceTo(start.Plus(u.ScaledBy(len)));
            Constraint::ConstrainCoincident(ref.entity(1), startP);
            Constraint::Constrain(Constraint::Type::HORIZONTAL, Entity::NO_ENTITY,
                                  Entity::NO_ENTITY, ref.entity(0));
            AddValued(Constraint::Type::ANGLE, Entity::NO_ENTITY, Entity::NO_ENTITY,
                      ref.entity(0), hr.entity(0), a <= 180 ? a : 360 - a);
        }
    }
    SS.MarkGroupDirty(activeGroup);
    ContinueLineChain(end);
    Invalidate();
    return true;
}

bool GraphicsWindow::CommitTypedCircle(double diameterMm) {
    if(TypedInputKind() != TypedKind::CIRCLE || diameterMm <= LENGTH_EPS) return false;
    Entity *circle = SK.GetEntity(pending.circle);
    SK.GetEntity(circle->distance)->DistanceForceTo(diameterMm / 2);
    AddValued(Constraint::Type::DIAMETER, Entity::NO_ENTITY, Entity::NO_ENTITY, pending.circle,
              Entity::NO_ENTITY, diameterMm);
    SS.MarkGroupDirty(activeGroup);
    ClearPending();
    Invalidate();
    return true;
}

bool GraphicsWindow::CommitTypedRectangle(std::optional<double> widthMm,
                                          std::optional<double> heightMm) {
    if(TypedInputKind() != TypedKind::RECTANGLE || (!widthMm && !heightMm)) return false;
    hRequest lns[4];
    for(int i = 0; i < 4; i++) lns[i] = pending.requests[i];
    EntityBase *n  = SK.GetEntity(ActiveWorkplane())->Normal();
    Vector u = n->NormalU(), v = n->NormalV();
    // MouseLeftDown's rectangle case: lns[2].entity(1) is the fixed corner, pending.point
    // (lns[1].entity(2)) the dragged one; odd lines are horizontal, even ones vertical.
    Vector fixed = SK.GetEntity(lns[2].entity(1))->PointGetNum();
    Vector cur   = SK.GetEntity(pending.point)->PointGetNum().Minus(fixed);
    double du = cur.Dot(u), dv = cur.Dot(v);
    if(widthMm)  du = (du < 0 ? -1 : 1) * *widthMm;
    if(heightMm) dv = (dv < 0 ? -1 : 1) * *heightMm;
    Vector corner = fixed.Plus(u.ScaledBy(du)).Plus(v.ScaledBy(dv));
    SK.GetEntity(pending.point)->PointForceTo(corner);
    // Move the two side corners too, so the solver starts from the final shape.
    for(int i = 0; i < 4; i++) {
        for(int j = 1; j <= 2; j++) {
            Entity *p = SK.GetEntity(lns[i].entity(j));
            Vector  d = p->PointGetNum().Minus(fixed);
            bool    farU = fabs(d.Dot(u)) > LENGTH_EPS || p->h == pending.point;
            bool    farV = fabs(d.Dot(v)) > LENGTH_EPS || p->h == pending.point;
            p->PointForceTo(fixed.Plus(u.ScaledBy(farU ? du : 0)).Plus(v.ScaledBy(farV ? dv : 0)));
        }
    }
    if(widthMm) AddValued(Constraint::Type::PT_PT_DISTANCE, lns[1].entity(1), lns[1].entity(2),
                          Entity::NO_ENTITY, Entity::NO_ENTITY, *widthMm);
    if(heightMm) AddValued(Constraint::Type::PT_PT_DISTANCE, lns[0].entity(1), lns[0].entity(2),
                           Entity::NO_ENTITY, Entity::NO_ENTITY, *heightMm);
    SS.MarkGroupDirty(activeGroup);
    ClearPending();
    Invalidate();
    return true;
}

} // namespace SolveSpace
```
Check against the rectangle code (`src/mouse.cpp`, `case Command::RECTANGLE`) which of `lns[0]`/
`lns[1]` is horizontal (`i % 2` → HORIZONTAL) before trusting the width/height line choice; the
`typed_rectangle_size` test is the judge. If moving the side corners by sign tests proves fragile
(all four corners start at the same point, so `farU/farV` are false for all but the dragged
point), replace the loop with: set the dragged corner, then `SS.GenerateAll()` and let the
existing horizontal/vertical constraints place the others — and record the ruling.

- [ ] **Step 5: Run tests** — both builds; all pass.

- [ ] **Step 6: Commit**

```bash
git add src/ui.h src/typedinput.cpp src/mouse.cpp src/CMakeLists.txt test/CMakeLists.txt \
        test/core/typedinput
git commit -m "Core: typed length/angle/diameter/size while drawing become constraints.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 13: Typed drawing keys and HUD

**Files:**
- Create: `src/ui/drawtool.{h,cpp}`, `test/ng/drawtool/test.cpp` (copy `line.slvs`)
- Modify: `src/ui/ngui.cpp` (modal + HUD), CMake lists

**Interfaces:**
- Consumes: Task 12 (`TypedInputKind`, `CommitTyped*`), `TypedFields`.
- Produces:
  ```cpp
  class DrawTool {
  public:
      bool Active();                               // TypedInputKind() != NONE
      bool HandleChord(const keymap::Chord &c);
      std::string Description();                   // "Line · length 25 · angle 30_"
      std::vector<KeyHint> Hints();
  };
  DrawTool &Draw();
  ```
  Keys while active: typed-field keys and `Tab`/`Shift+Tab` (fields reset whenever the tool kind
  or `pending.request`/`pending.circle` changes); `Enter`/`NumEnter` commit if any field is
  filled (else not consumed); `Esc` clears filled fields (consumed) or, if none, is not consumed
  (so the keymap's `Esc` cancels drawing as before). Everything else is not consumed.

- [ ] **Step 1: Write the failing test**

```cpp
#include "solvespace.h"
#include "ui/drawtool.h"

#include "harness.h"

using namespace SolveSpace::Ui;

static keymap::Chord C(const char *s) { return *keymap::ParseChord(s); }

static void ClickAt(double u, double v) {
    Entity *wp = SK.GetEntity(SS.GW.ActiveWorkplane());
    Vector o = SK.GetEntity(wp->point[0])->PointGetNum();
    Point2d s = SS.GW.ProjectPoint(
        o.Plus(wp->Normal()->NormalU().ScaledBy(u)).Plus(wp->Normal()->NormalV().ScaledBy(v)));
    SS.GW.MouseLeftDown(s.x, s.y, false, false);
    SS.GW.MouseLeftUp(s.x, s.y, false, false);
}

TEST_CASE(typed_keys_draw_a_constrained_line) {
    CHECK_LOAD("line.slvs");
    SS.showToolbar = false;
    SS.GW.LookOntoActiveWorkplane(false);
    SS.GW.ActivateCommand(Command::LINE_SEGMENT);
    CHECK_FALSE(Draw().Active());       // before the first click there is nothing to type
    ClickAt(300, 300);
    CHECK_TRUE(Draw().Active());
    int before = SK.constraint.n;
    for(const char *k : {"2", "5", "Tab", "3", "0"}) CHECK_TRUE(Draw().HandleChord(C(k)));
    CHECK_TRUE(Draw().Description().find("25") != std::string::npos);
    CHECK_TRUE(Draw().HandleChord(C("Enter")));
    CHECK_TRUE(SK.constraint.n >= before + 2);  // length + angle (+ reference line constraints)
    CHECK_TRUE(Draw().Active());                // the chain continues
    CHECK_TRUE(Draw().Description().find("25") == std::string::npos); // fields reset
}

TEST_CASE(esc_clears_fields_first_then_falls_through) {
    CHECK_LOAD("line.slvs");
    SS.showToolbar = false;
    SS.GW.LookOntoActiveWorkplane(false);
    SS.GW.ActivateCommand(Command::CIRCLE);
    ClickAt(300, 300);
    CHECK_TRUE(Draw().HandleChord(C("4")));
    CHECK_TRUE(Draw().HandleChord(C("Esc")));   // clears the field
    CHECK_FALSE(Draw().HandleChord(C("Esc")));  // now the keymap's Esc cancels the circle
    CHECK_FALSE(Draw().HandleChord(C("Enter"))); // nothing typed: not consumed
    CHECK_FALSE(Draw().HandleChord(C("L")));
}
```

- [ ] **Step 2: Run to verify failure** — Expected: `ui/drawtool.h` not found.

- [ ] **Step 3: Implement** — `src/ui/drawtool.h` (header comment "Typed values while drawing
lines, rectangles and circles: HUD fields that become constraints on Enter.") and
`src/ui/drawtool.cpp`:

```cpp
#include "solvespace.h"
#include "ui/drawtool.h"

namespace SolveSpace {
namespace Ui {

DrawTool &Draw() {
    static DrawTool tool;
    return tool;
}

void DrawTool::Sync() {
    GraphicsWindow::TypedKind kind = SS.GW.TypedInputKind();
    uint32_t target = kind == GraphicsWindow::TypedKind::CIRCLE ? SS.GW.pending.circle.v
                                                                : SS.GW.pending.request.v;
    if(kind == lastKind && target == lastTarget) return;
    lastKind   = kind;
    lastTarget = target;
    switch(kind) {
        case GraphicsWindow::TypedKind::LINE:      fields = TypedFields({"length", "angle"}); break;
        case GraphicsWindow::TypedKind::CIRCLE:    fields = TypedFields({"diameter"}); break;
        case GraphicsWindow::TypedKind::RECTANGLE: fields = TypedFields({"width", "height"}); break;
        case GraphicsWindow::TypedKind::NONE:      fields = TypedFields({}); break;
    }
}

bool DrawTool::Active() {
    Sync();
    return lastKind != GraphicsWindow::TypedKind::NONE;
}

bool DrawTool::HandleChord(const keymap::Chord &c) {
    if(!Active() || c.ctrl || c.alt) return false;
    if(fields.HandleChord(c)) return true;
    if(c.key == "Esc") {
        if(!fields.Any()) return false;
        fields.Clear();
        return true;
    }
    if(c.key != "Enter" && c.key != "NumEnter") return false;
    if(!fields.Any()) return false;
    bool ok = false;
    switch(lastKind) {
        case GraphicsWindow::TypedKind::LINE:
            ok = SS.GW.CommitTypedLine(fields.ValueMm(0), fields.ValueRaw(1));
            break;
        case GraphicsWindow::TypedKind::CIRCLE:
            if(auto d = fields.ValueMm(0)) ok = SS.GW.CommitTypedCircle(*d);
            break;
        case GraphicsWindow::TypedKind::RECTANGLE:
            ok = SS.GW.CommitTypedRectangle(fields.ValueMm(0), fields.ValueMm(1));
            break;
        case GraphicsWindow::TypedKind::NONE: break;
    }
    if(ok) {
        lastKind = GraphicsWindow::TypedKind::NONE; // force a reset on the next Sync()
        Sync();
    }
    return true; // a bad value keeps the fields for correction
}

std::string DrawTool::Description() {
    if(!Active()) return "";
    static const char *names[] = { "", "Line", "Circle", "Rectangle" };
    std::string d = names[(int)lastKind];
    for(int i = 0; i < (int)fields.Count(); i++) {
        d += " \xC2\xB7 " + fields.Label(i) + " " + fields.Text(i);
        if(i == fields.Active()) d += "_";
    }
    return d;
}

std::vector<KeyHint> DrawTool::Hints() {
    return { { "0-9", "Value" }, { "Tab", "Next field" }, { "Enter", "Apply" },
             { "Esc", "Clear / cancel" } };
}

}
}
```
with private members `void Sync(); TypedFields fields{{}}; GraphicsWindow::TypedKind lastKind =
GraphicsWindow::TypedKind::NONE; uint32_t lastTarget = 0;` in the header. Check that
`enum class TypedKind { NONE, LINE, CIRCLE, RECTANGLE }` order matches `names[]`.

`ngui.cpp` `ModalKey`: after the grab, `if(Draw().Active()) return Draw().HandleChord(c);`; each
frame, when `Draw().Active()`, set `gModalTool = Draw().Description()` and
`gModalHints = Draw().Hints()`.

- [ ] **Step 4: Run tests** — both builds; all pass.

- [ ] **Step 5: Smoke run**

```sh
timeout -s KILL 30 env SOLVESPACE_NG_SCRIPT="key L; wait 2; key Tab; wait 2; quit" build-ng/bin/solvespace-ng test/core/commands/line.slvs
```
then a scripted line (the debug script cannot click; if it has no click step, add
`click X Y` to `src/platform/sdl/debugscript.cpp` with a test in `test/ng/debugscript/test.cpp`
first, then):
```sh
timeout -s KILL 30 env SOLVESPACE_NG_SCRIPT="key L; click 400 300; keys 25; key Tab; keys 30; wait 2; shot /tmp/claude-1002/m1b-typed-hud.png; key Enter; wait 5; key Esc; key Esc; wait 3; shot /tmp/claude-1002/m1b-typed-line.png; quit" build-ng/bin/solvespace-ng test/core/commands/line.slvs
```
Expected: the HUD reads "Line · length 25 · angle 30_"; the second screenshot shows a 25 mm line at
30° with its two constraints and the dashed reference line. Look at both.

- [ ] **Step 6: Commit**

```bash
git add src/ui/drawtool.* src/ui/ngui.cpp src/platform/sdl/debugscript.* src/CMakeLists.txt \
        test/CMakeLists.txt test/ng/drawtool test/ng/debugscript
git commit -m "NG: type values while drawing (Tab cycles, Enter applies) to add constraints.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 14: Keymap review, docs, full verification

**Files:**
- Modify: `res/keymaps/modern.keymap` (header comment), `AGENTS.md` (§5 keymap table notes and
  §9 test notes), `docs/superpowers/plans/2026-10-15-m1a-sdl-imgui-shell.md` (mark M1b's roadmap
  entry "planned in 2026-10-16-m1b-interaction.md")

- [ ] **Step 1: Keymap header and conflicts**

Replace the "M1b adds:" comment in `modern.keymap` with a list of the `ui.*` actions now bound.
Then check for clashes the parser would reject and for keys the modal tools shadow:

Run: `build-ng/bin/solvespace-testsuite keyrouter` (the preset must parse) and read the keymap:
digits and `Num0`–`Num9` are typed values while drawing or grabbing, so numpad views do not fire
then (intended, Decision 5). `Tab` is sketch focus except while drawing (cycles fields).

- [ ] **Step 2: Update `AGENTS.md`**

In §5's keymap table: `Tab` row → "Sketch focus: activate the group of the selection and look onto
its workplane; Tab again returns (cycles HUD fields while drawing)"; `H` row → "Hide / show all /
isolate **groups**"; `D` row unchanged in meaning; add a line under the table:
"Typed angles that are not multiples of 90° add a construction reference line plus an angle
constraint (decided 2026-10-16: SolveSpace has no line-to-axis angle constraint)." In §9, add:
"Modal tools (pie, grab, typed drawing) see chords before the keymap via `KeyRouter::modal`;
test them headlessly through `Ui::Grab()`, `Ui::Draw()` and the core `Grab*`/`CommitTyped*`."

- [ ] **Step 3: Full verification**

```sh
cmake --build build -j$(nproc) && build/bin/solvespace-testsuite
cmake --build build-ng -j$(nproc) && build-ng/bin/solvespace-testsuite
cmake --build build-ci -j$(nproc) && build-ci/bin/solvespace-testsuite   # Clang ASan/UBSan
```
Expected: all three green; the sanitizer build reports no leaks or UB in the new tests.
Then run every smoke command from Tasks 4, 7, 9, 11 and 13 once more and look at each screenshot.

- [ ] **Step 4: Upstream check**

Run `git diff ng/m1a-shell --stat -- src/` and decide whether any change fixes stock behaviour
(the null-window guards in `MouseMoved`/`MouseLeftDown` only matter headlessly: not a stock bug,
no `pr/` branch). Record the decision in the final report.

- [ ] **Step 5: Commit and push**

```bash
git add res/keymaps/modern.keymap AGENTS.md docs/superpowers/plans/2026-10-15-m1a-sdl-imgui-shell.md
git commit -m "AGENTS.md, keymap: document the M1b interaction model.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
git push origin ng/m1b-interaction
```

M1b is done when Tasks 1–14 are complete, the final whole-branch review's Critical/Important
findings are fixed, and the owner has tried the pies, `Tab`, `G` and typed lines in the app.
