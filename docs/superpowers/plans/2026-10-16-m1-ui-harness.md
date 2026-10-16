# UI Interaction Harness Implementation Plan

> Written and executed in one session while the owner was away ("the more complete a test
> harness you can make, in terms of simulating the user interactions for tests, the better").
> Executed natively (superpowers:executing-plans); rulings go in the final report.

**Goal:** End-to-end tests that run the real `solvespace-ng` app in-process and drive it the way
a person does: key chords, typed text, mouse moves, clicks, drags, wheel, menu and panel clicks,
dialogs. Assertions read the model (`SK`, `SS`) and UI state.

**Architecture:** A new test executable, `solvespace-ng-uitest`, links the real SDL backend
(`guisdl.cpp`), the GL renderer and the NG UI instead of `guinone.cpp`. It runs on SDL's
`offscreen` video driver (EGL, no display needed; verified 2026-10-16 with Mesa). A *driver*
API in the backend pushes real `SDL_Event`s and renders frames synchronously, so events take the
same path as in the app: ImGui → capture rules → `KeyRouter` (modal, text capture, keymap) →
UI actions / core commands → `GraphicsWindow` mouse handlers → deferred queue → frame.
ImGui's test-engine hooks (MIT, in ImGui core; the Test Engine itself is not used) record item
rectangles by label so tests can click menu items and buttons by name.

**Tech stack:** C++20, SDL3 (offscreen driver), Dear ImGui 1.92.9b, existing `test/harness.cpp`.

**Spec:** `AGENTS.md` §8 "Testing requirements" (headless command tests; UI behaviour tested by
driving commands from scripts) and §9.

## Global Constraints

- `make test_solvespace` stays green and keeps meaning "the whole gate": it also runs the UI
  tests when `ENABLE_NG_GUI=ON`.
- No new dependencies (§2). Do not use the Dear ImGui Test Engine (non-OSI licence).
- Tests must never touch the user's real settings or keymap (`SDL_GetPrefPath`).
- Tests never block: every dialog and nested loop is answered by the driver or fails the case.
- C++20, `.clang-format` style, GPLv3 headers for SolveSpace-derived files.

## Review Focus

1. A test that opens a modal (context menu, file dialog, message box) with no scripted answer
   must fail fast, not hang the suite.
2. State leaking between shuffled cases (settings, keymap, ImGui window state, pie/grab/draw
   singletons) must not make results order-dependent.
3. Mouse coordinates: model → window conversion must match `GraphicsWindow::MouseEvent`
   (origin at the centre, y up) and the menu bar / toolbar must still receive their clicks.
4. Modifier keys held during clicks (Shift/Ctrl-click selection) must reach the core.
5. Animated view changes and timers must settle deterministically before assertions.

---

### Task 1: Driver mode in the SDL backend

**Files:** `src/platform/sdl/uidriver.h` (new), `src/platform/guisdl.cpp`,
`src/platform/sdl/timerregistry.{h,cpp}`, `test/ng/backend/test.cpp`.

- `SOLVESPACE_NG_CONFIG_DIR` overrides the preference directory.
- A second TOPLEVEL window (the harness calls `SS.Init()` per case) reuses the SDL window and GL
  context but recreates the ImGui context and the NG UI state.
- `TimerRegistry` tracks deadlines (`EarliestDue()`), unit-tested.
- Driver API (`Platform::Driver`): `Enable`, `Frame`, `Settle`, `Key`, `KeyDown`, `KeyUp`,
  `Type`, `Move`, `Press`, `Release`, `Click`, `DoubleClick`, `Drag`, `Wheel`, `WindowSize`,
  `ToWindow(Vector)`, `Screenshot`, `ResetSettings`, `ReloadKeymap`, `Dialogs()`,
  `OnMessage(policy)`, `QueueFile(Path)`, `DuringModal(fn)`.
- Held modifiers reach `MakeMouseEvent`; the wheel uses the event's own mouse position.
- Driver mode: message boxes are recorded and answered by policy; file dialogs take queued
  paths or cancel; nested loops run queued `DuringModal` steps and otherwise fail and dismiss
  after a bounded number of idle frames.
- The debug script reuses the driver's event helpers.

### Task 2: `solvespace-ng-uitest` executable

**Files:** `test/CMakeLists.txt`, `test/harness.cpp` (an `NG_UITEST` mode in `main`),
`test/ngui/smoke/test.cpp`.

- `main` in `NG_UITEST` mode: offscreen driver by default, a fresh temporary config directory,
  `Driver::Enable()`, settings reset before each `SS.Init()`, toolbar left as in the app, a
  screenshot `<build>/test/ngui-failures/<case>.png` for each failing case.
- `test_solvespace` also runs `solvespace-ng-uitest` when `ENABLE_NG_GUI=ON`.
- Smoke test: open fixture, `Space`, type `line segment`, `Return` → line tool pending.

### Task 3: Item lookup through ImGui hooks

**Files:** `src/CMakeLists.txt` (`IMGUI_ENABLE_TEST_ENGINE` on `imgui-sdl3`),
`src/platform/sdl/itemregistry.{h,cpp}` (new, hook implementations, active only in driver mode),
`test/ngui/menus/test.cpp`.

- `Driver::FindItem(label)` / `ClickItem(label)` / `ClickMenu({"Sketch", "Line Segment"})`.
- Tests: menu bar opens the line tool; a checked View menu item toggles; palette mouse click.

### Task 4: Canvas and property-browser helpers

**Files:** `guisdl.cpp`, `src/platform/sdl/uidriver.h`, `test/ngui/textwindow/test.cpp`.

- `Driver::ToolWindowRect()`, `Driver::ClickText(str)`: finds `str` in `SS.TW`'s text grid and
  clicks it inside the property-browser panel.
- `Driver::EditorVisible()` / typing into the dimension editor.

### Task 5: Interaction suites

**Files:** `test/ngui/<area>/test.cpp` for drawing, typed values, pies, grab, constrain and
dimension, sketch focus, visibility, views, undo, selection, editor, context menu, keymap,
theme, dialogs.

### Task 6: Docs

AGENTS.md §8/§9: how to write and run UI tests; the debug-script note points to the harness.
