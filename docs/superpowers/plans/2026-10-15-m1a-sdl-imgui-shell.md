# M1a — SDL3 + Dear ImGui Shell Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship `solvespace-ng`: today's SolveSpace running inside a single SDL3 window with a Dear ImGui overlay. It has a floating property-browser panel, an ImGui menu bar, a command palette, a remappable keymap engine with **Modern** (Blender-style) and **SolveSpace Classic** presets, and a dark/light theme. The old GTK/Qt/Win32/Cocoa frontends keep building untouched.

**Architecture:** The new frontend is a new **`Platform::` backend** (`src/platform/guisdl.cpp`). It implements the same interface (`src/platform/gui.h`) as `guigtk.cpp` and `guiqt.cpp`, so `solvespace-core` runs **unchanged** from day one. The rendering pipeline is:

1. The graphics window (GW) draws straight into the default framebuffer.
2. The text window (TW, the property browser) draws into an offscreen GL framebuffer whose texture is shown inside a floating ImGui window.
3. ImGui draws on top of everything.

All keyboard input goes through a keymap engine to stable command names (from M0's `commandnames`). Core keyboard accelerators are bypassed. Blocking platform calls (`Menu::PopUp`, file dialogs) run nested event loops, as the GTK backend does. Pure logic lives in small, separately tested modules:
- `libs/keymap` and `libs/palette`: MIT, no SolveSpace code;
- `src/platform/sdl/*`: settings file, menu model, key translation;
- `src/ui/*`: presets, key routing, palette widget, theme.

**Tech Stack:** C++20; SDL **3.4.16** (`release-3.4.16`; system SDL3 on Linux when available); Dear ImGui **v1.92.9b-docking**; OpenGL 2.1 compatibility profile (Linux/macOS) and GLES 3 via ANGLE (Windows); Inter + JetBrains Mono fonts (OFL).

**Spec:** `AGENTS.md` (§2 Licensing, §4 Architecture, §5 UI/UX, §8 M1, §9). **Prerequisite:** M0 plan `docs/superpowers/plans/2026-10-15-m0-foundations.md` completed: C++20, `CommandName`/`CommandFromName`, `GraphicsWindow::MenuCommands()`, `RUN_COMMAND`.

## Global Constraints

- All fork/app code GPLv3. `libs/keymap/` and `libs/palette/` are **MIT**, each with its own `LICENSE`, and **must not include any SolveSpace header** (`AGENTS.md` §2).
- Allowed new dependencies in this plan: SDL3 (zlib), Dear ImGui (MIT), Inter and JetBrains Mono (OFL). Nothing else without the owner's approval.
- Dependency direction: `solvespace-core` must not include anything from `src/platform/sdl/`, `src/ui/`, ImGui or SDL. The only core edits allowed in this plan are: Task 5 (a data field on `Platform::MenuItem` and setting it in `PopulateMainMenu`) and Task 13 (a `SOLVESPACE_ANGLE_SHARED` switch in `src/render/gl3shader.h` for Windows).
- Graphics: OpenGL limited to what `src/render/rendergl3.cpp` already uses (GLSL 1.20 desktop / ESSL 1.00), so the context must be a **compatibility** profile on desktop GL. The renderer uses no VAOs, which a core profile requires.
- The old frontends (GTK, Qt, Win32, Cocoa, HTML) must keep building. `ENABLE_NG_GUI` defaults to `OFF` until Task 13 turns it on in CI.
- The existing `test/` suite stays green on every commit. New logic gets tests first.
- Keymap command names are the M0 stable ids. UI-level actions use the `ui.` prefix (e.g. `ui.command-palette`).
- Code style: `.clang-format` (4 spaces, 100 cols, attached braces, no space before parens).

## Review Focus

1. **The keymap file a user edits by hand is broken**: unknown command, malformed chord, duplicate chord, empty file, Windows line endings. Expected: the app starts on the preset, the error is reported with a line number, and nothing crashes. Tests are in Task 2 (`parse_errors_report_line`, `crlf_and_comments_ok`) and Task 10 (`bad_user_keymap_falls_back_to_preset`).
2. **Typing into an ImGui text field** (palette, dimension editor) must never trigger single-key commands. With the palette open, pressing `L` types "l" and doesn't start a line. Task 10 adds `RouteKey` with an explicit `textInputActive` flag and tests it (`text_input_swallows_chords`).
3. **Timers firing after their owner is destroyed**, or `RunAfter` re-armed before the old one fires, must never call a dangling `onTimeout` or fire twice. Task 7 adds a serial-number scheme and a headless test of the bookkeeping (`timer_registry_rearm_and_cancel`).
4. **Save dialog with no extension typed** must append the selected filter's extension (the Qt backend needed the same fix in commit 2efb82c3). Task 7 tests the pure helper `EnsureExtension`.
5. **Non-US keyboard layouts and shifted symbols**: `Shift+3`, `>` (Shift+.), `+` (Shift+= or numpad), AltGr characters. Classic bindings like `'>'` show-grid and `S|'3'` group-sketch-3d must still trigger, and AltGr-produced characters must not be read as Ctrl+Alt chords. Task 6 tests `ChordFromKey` for these.

---

## File Structure

| Path | Responsibility |
|---|---|
| `extlib/SDL` (submodule, `release-3.4.16`) | Vendored SDL3 for Windows/macOS, and Linux without system SDL3 |
| `extlib/imgui` (submodule, `v1.92.9b-docking`) | Dear ImGui core + SDL3/OpenGL3 backends |
| `libs/CMakeLists.txt` | Adds `keymap` and `palette` |
| `libs/keymap/{LICENSE,keymap.h,keymap.cpp,CMakeLists.txt}` | MIT. `Chord` parse/format, `Keymap` bind/lookup/parse/serialise |
| `libs/palette/{LICENSE,fuzzy.h,fuzzy.cpp,CMakeLists.txt}` | MIT. Fuzzy subsequence scoring and ranking |
| `src/platform/sdl/settingsfile.{h,cpp}` | `Platform::Settings` backed by a `key=value` text file |
| `src/platform/sdl/menumodel.{h,cpp}` | In-memory `Menu`/`MenuBar`/`MenuItem` tree + flattening for the palette |
| `src/platform/sdl/keychord.{h,cpp}` | SDL key → `keymap::Chord`; `Platform::KeyboardEvent` accelerator → `Chord` |
| `src/platform/sdl/dialoghelpers.{h,cpp}` | Pure helpers: `EnsureExtension`, SDL filter-pattern building |
| `src/platform/sdl/timerregistry.{h,cpp}` | Main-thread timer serial bookkeeping (testable without SDL) |
| `src/platform/guisdl.cpp` | The SDL3/ImGui `Platform::` backend: window, GL, events, timers, dialogs, TW panel, menus, nested loops |
| `src/ui/keypresets.{h,cpp}` | Classic preset (generated from the core menu table) and Modern preset (resource) |
| `src/ui/keyrouter.{h,cpp}` | Chord → `ui.*` action or core `Command`; text-input suppression |
| `src/ui/palettewidget.{h,cpp}` | ImGui command palette over the menu model |
| `src/ui/theme.{h,cpp}` | Fonts and dark/light ImGui style from tokens |
| `src/ui/ngui.{h,cpp}` | The per-frame UI entry points the backend calls |
| `res/keymaps/modern.keymap` | Modern preset (draft) |
| `res/fonts/ng/{Inter-Regular.ttf,JetBrainsMono-Regular.ttf,OFL-Inter.txt,OFL-JetBrainsMono.txt}` | UI fonts + licences |
| `test/ng/*/test.cpp`, `test/libs/*/test.cpp` | Tests for all of the above |

Build targets: `keymap`, `palette` (always built). With `-DENABLE_NG_GUI=ON`: `imgui-sdl3`, `solvespace-ng-support` (settingsfile, menumodel, keychord, dialoghelpers, timerregistry, keypresets, keyrouter), `solvespace-ng` (executable).

---

### Task 1: Vendor SDL3 and Dear ImGui; build switch

**Files:**
- Modify: `.gitmodules` (via `git submodule add`)
- Modify: `CMakeLists.txt` (option + dependency block)
- Modify: `src/CMakeLists.txt` (`imgui-sdl3` library)

**Interfaces:**
- Produces: CMake option `ENABLE_NG_GUI`; variable `NG_SDL_TARGET` (an SDL3 link target); static library target `imgui-sdl3` with public include dirs `extlib/imgui` and `extlib/imgui/backends`.

- [ ] **Step 1: Add submodules at pinned tags**

```bash
cd solvespace
git submodule add https://github.com/libsdl-org/SDL.git extlib/SDL
git -C extlib/SDL checkout release-3.4.16
git submodule add https://github.com/ocornut/imgui.git extlib/imgui
git -C extlib/imgui checkout v1.92.9b-docking
git -C extlib/SDL describe --tags && git -C extlib/imgui describe --tags
```
Expected output: `release-3.4.16` and `v1.92.9b-docking`.

- [ ] **Step 2: Add the option**

In `CMakeLists.txt`, after the `USE_QT_GUI` option:
```cmake
set(ENABLE_NG_GUI     OFF CACHE BOOL
    "Whether to build the SDL3 + Dear ImGui interface (solvespace-ng)")
```

- [ ] **Step 3: Add the dependency block**

In `CMakeLists.txt`, directly after the `# GUI dependencies` `if(ENABLE_GUI) ... endif()` block:
```cmake
# SolveSpace NG (SDL3 + Dear ImGui) dependencies

if(ENABLE_NG_GUI)
    if(NOT ENABLE_GUI)
        message(FATAL_ERROR "ENABLE_NG_GUI requires ENABLE_GUI (for OpenGL and the renderer)")
    endif()

    if(NOT (WIN32 OR APPLE))
        find_package(SDL3 3.2 CONFIG QUIET)
    endif()
    if(SDL3_FOUND)
        message(STATUS "Using system SDL3 ${SDL3_VERSION}")
        set(NG_SDL_TARGET SDL3::SDL3)
    else()
        message(STATUS "Using in-tree SDL3")
        include(AddVendoredSubdirectory)
        set(SDL_SHARED       OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC       ON  CACHE BOOL "" FORCE)
        set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
        set(SDL_EXAMPLES     OFF CACHE BOOL "" FORCE)
        add_vendored_subdirectory(extlib/SDL)
        set(NG_SDL_TARGET SDL3::SDL3-static)
    endif()
endif()
```

- [ ] **Step 4: Add the ImGui library**

In `src/CMakeLists.txt`, before `# solvespace graphical executable`:
```cmake
# Dear ImGui with SDL3 + OpenGL3 backends, for solvespace-ng

if(ENABLE_GUI AND ENABLE_NG_GUI)
    set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/extlib/imgui)
    add_library(imgui-sdl3 STATIC
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp)
    target_include_directories(imgui-sdl3 PUBLIC
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends)
    target_compile_definitions(imgui-sdl3 PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS)
    target_link_libraries(imgui-sdl3 PUBLIC ${NG_SDL_TARGET})
endif()
```

- [ ] **Step 5: Configure and build the library**

Owner installs SDL3 dev headers first (Linux): `! sudo apt install libsdl3-dev`.
```bash
cd build && cmake .. -DENABLE_NG_GUI=ON && make -j$(nproc) imgui-sdl3
```
Expected: `Using system SDL3 3.4.x` (or `Using in-tree SDL3`), then `libimgui-sdl3.a` built with no errors.

- [ ] **Step 6: Default build unaffected; commit**

```bash
cmake .. -DENABLE_NG_GUI=OFF && make -j$(nproc) && make test_solvespace
git add .gitmodules extlib/SDL extlib/imgui CMakeLists.txt src/CMakeLists.txt
git commit -m "NG: vendor SDL3 3.4.16 and Dear ImGui 1.92.9b-docking behind ENABLE_NG_GUI."
```
Expected: suite `0 failed`.

---

### Task 2: `libs/keymap` (MIT): chords and keymaps

**Files:**
- Create: `libs/CMakeLists.txt`, `libs/keymap/CMakeLists.txt`, `libs/keymap/LICENSE`, `libs/keymap/keymap.h`, `libs/keymap/keymap.cpp`
- Modify: `CMakeLists.txt` (add `add_subdirectory(libs)` immediately before `add_subdirectory(src)`. Find the line with `grep -n "add_subdirectory(src)" CMakeLists.txt`)
- Create: `test/libs/keymap/test.cpp`; Modify: `test/CMakeLists.txt`

**Interfaces:**
- Produces (namespace `keymap`):
  ```cpp
  struct Chord { std::string key; bool ctrl = false; bool shift = false; bool alt = false;
                 bool operator==(const Chord &) const = default; };
  std::optional<Chord> ParseChord(std::string_view text);   // "Ctrl+Shift+S", "Plus", ">", "F3", "Num5"
  std::string FormatChord(const Chord &chord);               // canonical: Ctrl+Alt+Shift+Key
  class Keymap {
  public:
      bool Bind(const std::string &command, const Chord &chord, std::string *error);
      const std::string *Lookup(const Chord &chord) const;    // nullptr if unbound
      std::vector<Chord> ChordsFor(const std::string &command) const;
      std::vector<std::string> Commands() const;              // sorted, unique
      static std::optional<Keymap> Parse(std::string_view text, std::string *error);
      std::string Serialize() const;                          // round-trips through Parse
  };
  ```
- **Key naming rules:**
  - Letters are stored **lowercase** (`"s"`) and formatted uppercase (`"S"`).
  - Named keys: `Space Tab Esc Enter Backspace Delete Insert Home End PageUp PageDown Up Down Left Right F1…F24 Num0…Num9 Num. Num/ Num* Num- NumEnter Plus`.
  - `+` is written `Plus`. Any other single printable character is the key itself (`>`, `-`, `=`, `,`, `` ` ``).
  - Modifier order on output is `Ctrl+Alt+Shift+`.
- **File format:** one binding line per command, `<command>: <chord>[, <chord>…]`. `#` starts a comment, blank lines are ignored, CR is stripped.

- [ ] **Step 1: Write the failing tests**

`test/libs/keymap/test.cpp`:
```cpp
#include "harness.h"
#include "keymap.h"

using keymap::Chord;
using keymap::Keymap;

TEST_CASE(parse_and_format_chords) {
    auto c = keymap::ParseChord("Ctrl+Shift+S");
    CHECK_TRUE(c.has_value());
    CHECK_EQ_STR(c->key, "s");
    CHECK_TRUE(c->ctrl && c->shift && !c->alt);
    CHECK_EQ_STR(keymap::FormatChord(*c), "Ctrl+Shift+S");
    CHECK_EQ_STR(keymap::FormatChord(*keymap::ParseChord("shift+ctrl+s")), "Ctrl+Shift+S");
    CHECK_EQ_STR(keymap::FormatChord(*keymap::ParseChord("Plus")), "Plus");
    CHECK_EQ_STR(keymap::FormatChord(*keymap::ParseChord(">")), ">");
    CHECK_EQ_STR(keymap::FormatChord(*keymap::ParseChord("Ctrl+,")), "Ctrl+,");
    CHECK_EQ_STR(keymap::FormatChord(*keymap::ParseChord("Num5")), "Num5");
    CHECK_EQ_STR(keymap::FormatChord(*keymap::ParseChord("alt+a")), "Alt+A");
}

TEST_CASE(reject_bad_chords) {
    CHECK_FALSE(keymap::ParseChord("").has_value());
    CHECK_FALSE(keymap::ParseChord("Ctrl+").has_value());
    CHECK_FALSE(keymap::ParseChord("Hyper+A").has_value());
    CHECK_FALSE(keymap::ParseChord("Ctrl+Ctrl+A").has_value());
    CHECK_FALSE(keymap::ParseChord("NotAKey").has_value());
}

TEST_CASE(bind_lookup_and_conflicts) {
    Keymap km;
    std::string err;
    CHECK_TRUE(km.Bind("save", *keymap::ParseChord("Ctrl+S"), &err));
    CHECK_FALSE(km.Bind("save-as", *keymap::ParseChord("Ctrl+S"), &err));
    CHECK_TRUE(err.find("Ctrl+S") != std::string::npos);
    CHECK_TRUE(err.find("save") != std::string::npos);
    const std::string *cmd = km.Lookup(*keymap::ParseChord("Ctrl+S"));
    CHECK_TRUE(cmd != nullptr && *cmd == "save");
    CHECK_TRUE(km.Lookup(*keymap::ParseChord("Ctrl+Q")) == nullptr);
    CHECK_TRUE(km.Bind("save", *keymap::ParseChord("F2"), &err));
    CHECK_TRUE(km.ChordsFor("save").size() == 2);
}

TEST_CASE(parse_file_round_trip) {
    std::string err;
    auto km = Keymap::Parse("# comment\n\nsave: Ctrl+S, F2\nundo: Ctrl+Z\n", &err);
    CHECK_TRUE(km.has_value());
    auto again = Keymap::Parse(km->Serialize(), &err);
    CHECK_TRUE(again.has_value());
    CHECK_EQ_STR(again->Serialize(), km->Serialize());
    CHECK_TRUE(again->ChordsFor("save").size() == 2);
}

TEST_CASE(crlf_and_comments_ok) {
    std::string err;
    auto km = Keymap::Parse("undo: Ctrl+Z   # trailing\r\nredo: Ctrl+Shift+Z\r\n", &err);
    CHECK_TRUE(km.has_value());
    CHECK_TRUE(*km->Lookup(*keymap::ParseChord("Ctrl+Shift+Z")) == "redo");
}

TEST_CASE(parse_errors_report_line) {
    std::string err;
    CHECK_FALSE(Keymap::Parse("undo: Ctrl+Z\nbroken line\n", &err).has_value());
    CHECK_TRUE(err.find("line 2") != std::string::npos);
    CHECK_FALSE(Keymap::Parse("a: Ctrl+Z\nb: Ctrl+Z\n", &err).has_value());
    CHECK_TRUE(err.find("line 2") != std::string::npos);
    CHECK_FALSE(Keymap::Parse("a: Bogus+Q\n", &err).has_value());
    CHECK_TRUE(err.find("line 1") != std::string::npos);
    auto empty = Keymap::Parse("", &err);
    CHECK_TRUE(empty.has_value() && empty->Commands().empty());
}
```

`test/CMakeLists.txt`: add `libs/keymap/test.cpp` to `testsuite_SOURCES`, and after `target_link_libraries(solvespace-testsuite ...)` add:
```cmake
target_link_libraries(solvespace-testsuite keymap)
```
(Task 3 changes this line to `keymap palette`.)

- [ ] **Step 2: Run to verify failure**

`cd build && cmake .. && make solvespace-testsuite 2>&1 | grep -m2 error`. Expected: `keymap.h: No such file`.

- [ ] **Step 3: Create the library skeleton**

`libs/CMakeLists.txt`:
```cmake
# Independent MIT-licensed helper libraries. Nothing here may include SolveSpace headers.
add_subdirectory(keymap)
```
`libs/keymap/CMakeLists.txt`:
```cmake
add_library(keymap STATIC keymap.cpp)
target_include_directories(keymap PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(keymap PUBLIC cxx_std_20)
```
`libs/keymap/LICENSE`: the standard MIT licence text, `Copyright (c) 2026 ChalkWalk`.

Write `libs/CMakeLists.txt` with only `add_subdirectory(keymap)` for now; Task 3 adds the `add_subdirectory(palette)` line.

- [ ] **Step 4: Write `keymap.h`**

```cpp
// SPDX-License-Identifier: MIT
// Keyboard chords and command keymaps. Independent of SolveSpace.
#ifndef KEYMAP_KEYMAP_H
#define KEYMAP_KEYMAP_H

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace keymap {

struct Chord {
    std::string key;    // lowercase letter, single printable char, or a named key
    bool        ctrl  = false;
    bool        shift = false;
    bool        alt   = false;

    bool operator==(const Chord &) const = default;
};

std::optional<Chord> ParseChord(std::string_view text);
std::string FormatChord(const Chord &chord);

class Keymap {
public:
    // Adds a binding. Fails (returning false and describing the clash in *error) if the
    // chord is already bound to any command.
    bool Bind(const std::string &command, const Chord &chord, std::string *error);
    const std::string *Lookup(const Chord &chord) const;
    std::vector<Chord> ChordsFor(const std::string &command) const;
    std::vector<std::string> Commands() const;

    static std::optional<Keymap> Parse(std::string_view text, std::string *error);
    std::string Serialize() const;

private:
    std::map<std::string, std::string>              commandByChord; // FormatChord -> command
    std::map<std::string, std::vector<Chord>>       chordsByCommand;
};

}

#endif
```

- [ ] **Step 5: Write `keymap.cpp`**

```cpp
// SPDX-License-Identifier: MIT
#include "keymap.h"

#include <algorithm>
#include <cctype>

namespace keymap {

static const char *const NamedKeys[] = {
    "Space", "Tab", "Esc", "Enter", "Backspace", "Delete", "Insert", "Home", "End",
    "PageUp", "PageDown", "Up", "Down", "Left", "Right", "Plus",
    "Num0", "Num1", "Num2", "Num3", "Num4", "Num5", "Num6", "Num7", "Num8", "Num9",
    "Num.", "Num/", "Num*", "Num-", "NumEnter",
};

static std::string Lower(std::string_view s) {
    std::string r(s);
    for(char &c : r) c = (char)std::tolower((unsigned char)c);
    return r;
}

static std::optional<std::string> NormalizeKey(std::string_view k) {
    if(k.empty()) return std::nullopt;
    if(k.size() == 1) {
        unsigned char c = (unsigned char)k[0];
        if(std::isalpha(c)) return std::string(1, (char)std::tolower(c));
        if(std::isgraph(c) && c != '+') return std::string(k);
        return std::nullopt;
    }
    std::string lk = Lower(k);
    for(const char *name : NamedKeys) {
        if(Lower(name) == lk) return std::string(name);
    }
    if(lk[0] == 'f' && lk.size() <= 3 &&
       std::all_of(lk.begin() + 1, lk.end(), [](char c) { return std::isdigit((unsigned char)c); })) {
        int n = std::stoi(lk.substr(1));
        if(n >= 1 && n <= 24) return "F" + std::to_string(n);
    }
    return std::nullopt;
}

std::optional<Chord> ParseChord(std::string_view text) {
    Chord chord;
    std::vector<std::string_view> parts;
    size_t start = 0;
    // Split on '+', but a trailing "+" component can't occur since '+' is spelled "Plus".
    while(true) {
        size_t plus = text.find('+', start);
        parts.push_back(text.substr(start, plus == std::string_view::npos ? plus : plus - start));
        if(plus == std::string_view::npos) break;
        start = plus + 1;
    }
    for(size_t i = 0; i + 1 < parts.size(); i++) {
        std::string m = Lower(parts[i]);
        bool *flag = m == "ctrl" ? &chord.ctrl : m == "shift" ? &chord.shift
                   : m == "alt"  ? &chord.alt  : nullptr;
        if(flag == nullptr || *flag) return std::nullopt;
        *flag = true;
    }
    std::optional<std::string> key = NormalizeKey(parts.back());
    if(!key) return std::nullopt;
    chord.key = *key;
    return chord;
}

std::string FormatChord(const Chord &chord) {
    std::string out;
    if(chord.ctrl)  out += "Ctrl+";
    if(chord.alt)   out += "Alt+";
    if(chord.shift) out += "Shift+";
    if(chord.key.size() == 1 && std::isalpha((unsigned char)chord.key[0])) {
        out += (char)std::toupper((unsigned char)chord.key[0]);
    } else {
        out += chord.key;
    }
    return out;
}

bool Keymap::Bind(const std::string &command, const Chord &chord, std::string *error) {
    std::string name = FormatChord(chord);
    auto it = commandByChord.find(name);
    if(it != commandByChord.end()) {
        if(error) *error = name + " is already bound to " + it->second;
        return false;
    }
    commandByChord[name] = command;
    chordsByCommand[command].push_back(chord);
    return true;
}

const std::string *Keymap::Lookup(const Chord &chord) const {
    auto it = commandByChord.find(FormatChord(chord));
    return it == commandByChord.end() ? nullptr : &it->second;
}

std::vector<Chord> Keymap::ChordsFor(const std::string &command) const {
    auto it = chordsByCommand.find(command);
    return it == chordsByCommand.end() ? std::vector<Chord>{} : it->second;
}

std::vector<std::string> Keymap::Commands() const {
    std::vector<std::string> out;
    for(const auto &kv : chordsByCommand) out.push_back(kv.first);
    return out;
}

static std::string_view Trim(std::string_view s) {
    while(!s.empty() && std::isspace((unsigned char)s.front())) s.remove_prefix(1);
    while(!s.empty() && std::isspace((unsigned char)s.back()))  s.remove_suffix(1);
    return s;
}

std::optional<Keymap> Keymap::Parse(std::string_view text, std::string *error) {
    Keymap km;
    int lineNo = 0;
    size_t pos = 0;
    while(pos <= text.size()) {
        size_t nl = text.find('\n', pos);
        std::string_view line = text.substr(pos, nl == std::string_view::npos ? nl : nl - pos);
        pos = (nl == std::string_view::npos) ? text.size() + 1 : nl + 1;
        lineNo++;
        size_t hash = line.find('#');
        if(hash != std::string_view::npos) line = line.substr(0, hash);
        line = Trim(line);
        if(line.empty()) continue;
        auto fail = [&](const std::string &why) -> std::optional<Keymap> {
            if(error) *error = "line " + std::to_string(lineNo) + ": " + why;
            return std::nullopt;
        };
        size_t colon = line.find(':');
        if(colon == std::string_view::npos) return fail("expected '<command>: <chord>[, ...]'");
        std::string command(Trim(line.substr(0, colon)));
        if(command.empty()) return fail("missing command name");
        std::string_view rest = line.substr(colon + 1);
        while(true) {
            size_t comma = rest.find(',');
            // A lone "," key is written "Ctrl+," etc.; only split on commas followed by space
            // or end, so the chord "Ctrl+," survives.
            while(comma != std::string_view::npos && comma + 1 < rest.size() &&
                  !std::isspace((unsigned char)rest[comma + 1])) {
                comma = rest.find(',', comma + 1);
            }
            std::string_view item = Trim(rest.substr(0, comma));
            auto chord = ParseChord(item);
            if(!chord) return fail("bad chord '" + std::string(item) + "'");
            std::string bindError;
            if(!km.Bind(command, *chord, &bindError)) return fail(bindError);
            if(comma == std::string_view::npos) break;
            rest = rest.substr(comma + 1);
        }
    }
    return km;
}

std::string Keymap::Serialize() const {
    std::string out;
    for(const auto &kv : chordsByCommand) {
        out += kv.first + ":";
        for(size_t i = 0; i < kv.second.size(); i++) {
            out += (i == 0 ? " " : ", ") + FormatChord(kv.second[i]);
        }
        out += "\n";
    }
    return out;
}

}
```

- [ ] **Step 6: Run the tests**

`make -j$(nproc) solvespace-testsuite && ./bin/solvespace-testsuite libs/keymap`. Expected: 6 cases, `0 failed`.

- [ ] **Step 7: Full suite and commit**

```bash
make test_solvespace
git add libs CMakeLists.txt test/libs/keymap test/CMakeLists.txt
git commit -m "Add MIT keymap library: chords, keymap parse/serialize, conflict detection."
```

---

### Task 3: `libs/palette` (MIT): fuzzy matching

**Files:**
- Create: `libs/palette/{CMakeLists.txt,LICENSE,fuzzy.h,fuzzy.cpp}`; Modify: `libs/CMakeLists.txt` (add `add_subdirectory(palette)`)
- Create: `test/libs/palette/test.cpp`; Modify: `test/CMakeLists.txt`

**Interfaces:**
- Produces (namespace `palette`):
  ```cpp
  int FuzzyScore(std::string_view query, std::string_view text); // -1 = no match; higher = better
  std::vector<size_t> Rank(std::string_view query, const std::vector<std::string> &items,
                           size_t limit);                         // indices, best first, stable
  ```
- Scoring rules:
  - Case-insensitive subsequence match.
  - +10 per matched character.
  - +15 if it starts a word (start of string, or preceded by space, `-`, `_`, `/`, `›`, `(`).
  - +8 if it immediately follows the previous match.
  - −1 per skipped character between matches.
  - An empty query scores 0 for every item, so the original order is kept.

- [ ] **Step 1: Failing tests**

`test/libs/palette/test.cpp`:
```cpp
#include "harness.h"
#include "fuzzy.h"

TEST_CASE(subsequence_matching) {
    CHECK_TRUE(palette::FuzzyScore("ls", "Line Segment") > 0);
    CHECK_TRUE(palette::FuzzyScore("xyz", "Line Segment") == -1);
    CHECK_TRUE(palette::FuzzyScore("LINE", "line segment") > 0);
    CHECK_TRUE(palette::FuzzyScore("", "anything") == 0);
}

TEST_CASE(word_starts_and_runs_rank_higher) {
    std::vector<std::string> items = { "Export Image...", "Extrude" };
    auto r = palette::Rank("ext", items, 10);
    CHECK_TRUE(r.size() == 2 && r[0] == 1);
    items = { "Sketch › Circle", "Constrain › Equal Length / Radius", "Arc of a Circle" };
    r = palette::Rank("circ", items, 10);
    CHECK_TRUE(r.size() == 2);
    CHECK_TRUE(r[0] == 0);
}

TEST_CASE(rank_limit_and_stability) {
    std::vector<std::string> items = { "aa", "ab", "ac", "zz" };
    auto r = palette::Rank("a", items, 2);
    CHECK_TRUE(r.size() == 2 && r[0] == 0 && r[1] == 1);
    r = palette::Rank("", items, 10);
    CHECK_TRUE(r.size() == 4 && r[3] == 3);
}
```
Add `libs/palette/test.cpp` to `testsuite_SOURCES`, and change the Task 2 line to `target_link_libraries(solvespace-testsuite keymap palette)`.

- [ ] **Step 2: Verify failure** (`fuzzy.h: No such file`).

- [ ] **Step 3: Implement**

`libs/palette/CMakeLists.txt`:
```cmake
add_library(palette STATIC fuzzy.cpp)
target_include_directories(palette PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(palette PUBLIC cxx_std_20)
```
`libs/palette/LICENSE`: MIT, `Copyright (c) 2026 ChalkWalk`.

`libs/palette/fuzzy.h`:
```cpp
// SPDX-License-Identifier: MIT
#ifndef PALETTE_FUZZY_H
#define PALETTE_FUZZY_H
#include <string>
#include <string_view>
#include <vector>
namespace palette {
int FuzzyScore(std::string_view query, std::string_view text);
std::vector<size_t> Rank(std::string_view query, const std::vector<std::string> &items,
                         size_t limit);
}
#endif
```
`libs/palette/fuzzy.cpp`:
```cpp
// SPDX-License-Identifier: MIT
#include "fuzzy.h"
#include <algorithm>
#include <cctype>

namespace palette {

static bool IsWordStart(std::string_view text, size_t i) {
    if(i == 0) return true;
    unsigned char p = (unsigned char)text[i - 1];
    // "›" is UTF-8 E2 80 BA; its last byte precedes the next word's space anyway, so
    // checking the space covers it. Bytes >= 0x80 also count as separators.
    return p == ' ' || p == '-' || p == '_' || p == '/' || p == '(' || p >= 0x80;
}

int FuzzyScore(std::string_view query, std::string_view text) {
    if(query.empty()) return 0;
    int score = 0;
    size_t qi = 0;
    size_t lastMatch = std::string_view::npos;
    for(size_t ti = 0; ti < text.size() && qi < query.size(); ti++) {
        if(std::tolower((unsigned char)text[ti]) != std::tolower((unsigned char)query[qi])) {
            continue;
        }
        score += 10;
        if(IsWordStart(text, ti)) score += 15;
        if(lastMatch != std::string_view::npos) {
            if(ti == lastMatch + 1) score += 8;
            else score -= (int)(ti - lastMatch - 1);
        }
        lastMatch = ti;
        qi++;
    }
    return qi == query.size() ? std::max(score, 1) : -1;
}

std::vector<size_t> Rank(std::string_view query, const std::vector<std::string> &items,
                         size_t limit) {
    std::vector<std::pair<int, size_t>> scored;
    for(size_t i = 0; i < items.size(); i++) {
        int s = FuzzyScore(query, items[i]);
        if(s >= 0) scored.push_back({ s, i });
    }
    std::stable_sort(scored.begin(), scored.end(),
                     [](const auto &a, const auto &b) { return a.first > b.first; });
    std::vector<size_t> out;
    for(size_t i = 0; i < scored.size() && i < limit; i++) out.push_back(scored[i].second);
    return out;
}

}
```

- [ ] **Step 4: Run** `./bin/solvespace-testsuite libs/palette`. Expected `0 failed`.

If `word_starts_and_runs_rank_higher` fails on the circle case, print both scores. "Sketch › Circle" must beat "Arc of a Circle" because of its shorter gap before the match. If not, raise the word-start bonus to 20 rather than changing the test's intent.

- [ ] **Step 5: Commit**

```bash
make test_solvespace
git add libs test/libs/palette test/CMakeLists.txt
git commit -m "Add MIT palette library: fuzzy subsequence scoring and ranking."
```

---

### Task 4: Settings file

**Files:**
- Create: `src/platform/sdl/settingsfile.h`, `src/platform/sdl/settingsfile.cpp`
- Modify: `src/CMakeLists.txt` (create the `solvespace-ng-support` library)
- Create: `test/ng/settingsfile/test.cpp`; Modify: `test/CMakeLists.txt`

**Interfaces:**
- Produces:
  ```cpp
  namespace SolveSpace::Platform {
  class SettingsFile final : public Settings {
  public:
      explicit SettingsFile(Path path);   // loads if the file exists; missing file = empty
      ~SettingsFile() override;            // calls Save()
      bool Save();                         // atomic-ish: write to path + ".tmp", then rename
      // Settings overrides: FreezeInt/ThawInt/FreezeFloat/ThawFloat/FreezeString/ThawString
  };
  }
  ```
- File format: UTF-8 lines `key=value`. Strings escape `\` → `\\` and newline → `\n`. Floats use `%.17g` (C locale is already forced by SolveSpace). Unknown or garbled lines are ignored.

- [ ] **Step 1: Failing test**

`test/ng/settingsfile/test.cpp`:
```cpp
#include "harness.h"
#include "platform/sdl/settingsfile.h"

using namespace SolveSpace::Platform;

static Path TempSettingsPath(const char *name) {
    return Path::From(std::string(getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp"))
               .Join(std::string("ssng-test-") + name + ".txt");
}

TEST_CASE(round_trip_all_types) {
    Path path = TempSettingsPath("roundtrip");
    remove(path.raw.c_str());
    {
        SettingsFile s(path);
        s.FreezeInt("Int", 42);
        s.FreezeFloat("Float", 0.1);
        s.FreezeString("Str", "a=b\nc\\d");
        s.FreezeBool("Bool", true);
    }
    SettingsFile s(path);
    CHECK_TRUE(s.ThawInt("Int") == 42);
    CHECK_EQ_EPS(s.ThawFloat("Float"), 0.1);
    CHECK_EQ_STR(s.ThawString("Str"), "a=b\nc\\d");
    CHECK_TRUE(s.ThawBool("Bool"));
    CHECK_TRUE(s.ThawInt("Missing", 7) == 7);
    remove(path.raw.c_str());
}

TEST_CASE(garbage_lines_are_ignored) {
    Path path = TempSettingsPath("garbage");
    CHECK_TRUE(WriteFile(path, "no equals sign\nInt=notanumber\nGood=5\n"));
    SettingsFile s(path);
    CHECK_TRUE(s.ThawInt("Int", 3) == 3);
    CHECK_TRUE(s.ThawInt("Good") == 5);
    remove(path.raw.c_str());
}
```
Tests run on Windows CI too, so `TMPDIR` falls back to `/tmp` here. In Task 13, switch to `std::filesystem::temp_directory_path()` if Windows fails.

`test/CMakeLists.txt` (after the `target_link_libraries` lines):
```cmake
if(ENABLE_NG_GUI)
    target_sources(solvespace-testsuite PRIVATE
        ng/settingsfile/test.cpp)
    target_link_libraries(solvespace-testsuite solvespace-ng-support)
endif()
```

- [ ] **Step 2: Verify failure** (missing header).

- [ ] **Step 3: Create the support library**

In `src/CMakeLists.txt`, inside the `if(ENABLE_GUI AND ENABLE_NG_GUI)` block from Task 1, after `imgui-sdl3`:
```cmake
    add_library(solvespace-ng-support STATIC
        platform/sdl/settingsfile.cpp)
    target_include_directories(solvespace-ng-support PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
    target_link_libraries(solvespace-ng-support PUBLIC solvespace-core keymap palette)
```

- [ ] **Step 4: Implement**

`src/platform/sdl/settingsfile.h`:
```cpp
//-----------------------------------------------------------------------------
// Platform::Settings stored as a key=value text file, for the SDL backend.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_SDL_SETTINGSFILE_H
#define SOLVESPACE_SDL_SETTINGSFILE_H

#include <map>
#include "solvespace.h"

namespace SolveSpace {
namespace Platform {

class SettingsFile final : public Settings {
public:
    explicit SettingsFile(Path path);
    ~SettingsFile() override;
    bool Save();

    void FreezeInt(const std::string &key, uint32_t value) override;
    uint32_t ThawInt(const std::string &key, uint32_t defaultValue = 0) override;
    void FreezeFloat(const std::string &key, double value) override;
    double ThawFloat(const std::string &key, double defaultValue = 0.0) override;
    void FreezeString(const std::string &key, const std::string &value) override;
    std::string ThawString(const std::string &key,
                           const std::string &defaultValue = "") override;

private:
    Path                               path;
    std::map<std::string, std::string> values;
    bool                               dirty = false;
};

}
}

#endif
```
`src/platform/sdl/settingsfile.cpp`:
```cpp
#include "platform/sdl/settingsfile.h"

namespace SolveSpace {
namespace Platform {

static std::string Escape(const std::string &s) {
    std::string r;
    for(char c : s) {
        if(c == '\\') r += "\\\\";
        else if(c == '\n') r += "\\n";
        else r += c;
    }
    return r;
}

static std::string Unescape(const std::string &s) {
    std::string r;
    for(size_t i = 0; i < s.size(); i++) {
        if(s[i] == '\\' && i + 1 < s.size()) {
            r += (s[i + 1] == 'n') ? '\n' : s[i + 1];
            i++;
        } else {
            r += s[i];
        }
    }
    return r;
}

SettingsFile::SettingsFile(Path path) : path(path) {
    std::string data;
    if(!ReadFile(path, &data)) return;
    size_t pos = 0;
    while(pos < data.size()) {
        size_t nl = data.find('\n', pos);
        std::string line = data.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        pos = (nl == std::string::npos) ? data.size() : nl + 1;
        if(!line.empty() && line.back() == '\r') line.pop_back();
        size_t eq = line.find('=');
        if(eq == std::string::npos || eq == 0) continue;
        values[line.substr(0, eq)] = Unescape(line.substr(eq + 1));
    }
}

SettingsFile::~SettingsFile() {
    Save();
}

bool SettingsFile::Save() {
    if(!dirty) return true;
    std::string data;
    for(const auto &kv : values) data += kv.first + "=" + Escape(kv.second) + "\n";
    Path tmp = Path::From(path.raw + ".tmp");
    if(!WriteFile(tmp, data)) return false;
    remove(path.raw.c_str()); // rename() can't overwrite on Windows
    if(rename(tmp.raw.c_str(), path.raw.c_str()) != 0) return false;
    dirty = false;
    return true;
}

void SettingsFile::FreezeInt(const std::string &key, uint32_t value) {
    FreezeString(key, std::to_string(value));
}

uint32_t SettingsFile::ThawInt(const std::string &key, uint32_t defaultValue) {
    auto it = values.find(key);
    if(it == values.end()) return defaultValue;
    char *end;
    unsigned long v = strtoul(it->second.c_str(), &end, 10);
    return (*end == '\0' && !it->second.empty()) ? (uint32_t)v : defaultValue;
}

void SettingsFile::FreezeFloat(const std::string &key, double value) {
    FreezeString(key, ssprintf("%.17g", value));
}

double SettingsFile::ThawFloat(const std::string &key, double defaultValue) {
    auto it = values.find(key);
    if(it == values.end()) return defaultValue;
    char *end;
    double v = strtod(it->second.c_str(), &end);
    return (*end == '\0' && !it->second.empty()) ? v : defaultValue;
}

void SettingsFile::FreezeString(const std::string &key, const std::string &value) {
    values[key] = value;
    dirty = true;
}

std::string SettingsFile::ThawString(const std::string &key, const std::string &defaultValue) {
    auto it = values.find(key);
    return it == values.end() ? defaultValue : it->second;
}

}
}
```
(`Settings::FreezeBool`/`ThawBool` have base-class implementations in `platform/gui.cpp` built on Int, so the test's bool round-trip goes through `FreezeInt`.)

- [ ] **Step 5: Run** `cmake .. -DENABLE_NG_GUI=ON && make -j$(nproc) solvespace-testsuite && ./bin/solvespace-testsuite ng/settingsfile`. Expected `0 failed`.

- [ ] **Step 6: Commit**

```bash
make test_solvespace
git add src/platform/sdl src/CMakeLists.txt test/ng/settingsfile test/CMakeLists.txt
git commit -m "NG: key=value settings file backend."
```

---

### Task 5: Menu model (+ command ids on menu items)

**Files:**
- Modify: `src/platform/gui.h` (add data member `uint32_t commandId = 0;` to `class MenuItem`, after `onTrigger`)
- Modify: `src/graphicswin.cpp` (`PopulateMainMenu`: set `commandId` on the items it creates for menu commands)
- Create: `src/platform/sdl/menumodel.{h,cpp}`; Modify: `src/CMakeLists.txt` (add `platform/sdl/menumodel.cpp` to `solvespace-ng-support`)
- Create: `test/ng/menumodel/test.cpp`; Modify: `test/CMakeLists.txt` (add `ng/menumodel/test.cpp` to the NG `target_sources`)

**Interfaces:**
- Consumes: `Platform::Menu`, `MenuBar`, `MenuItem` (`src/platform/gui.h:158-205`).
- Produces:
  ```cpp
  namespace SolveSpace::Platform {
  std::string StripMnemonic(const std::string &label);   // "&File" -> "File", "A&&B" -> "A&B"
  class MenuItemSdl final : public MenuItem {
  public:
      std::string    label;            // mnemonic-stripped
      KeyboardEvent  accel = {};  bool hasAccel = false;
      Indicator      indicator = Indicator::NONE;
      bool           enabled = true, active = false;
      void SetAccelerator(KeyboardEvent a) override; void SetIndicator(Indicator t) override;
      void SetEnabled(bool e) override;              void SetActive(bool a) override;
  };
  class MenuSdl final : public Menu {
  public:
      struct Entry { enum class Kind { ITEM, SUBMENU, SEPARATOR } kind;
                     std::shared_ptr<MenuItemSdl> item; std::shared_ptr<MenuSdl> submenu;
                     std::string label; };
      std::vector<Entry> entries;
      bool popupRequested = false;     // set by PopUp(); the backend drives the nested loop
      std::function<void(MenuSdl *)> onPopUp; // installed by the backend; runs the nested loop
      std::shared_ptr<MenuItem> AddItem(const std::string &label, std::function<void()> onTrigger,
                                        bool mnemonics) override;
      std::shared_ptr<Menu> AddSubMenu(const std::string &label) override;
      void AddSeparator() override;  void PopUp() override;  void Clear() override;
  };
  class MenuBarSdl final : public MenuBar {
  public:
      std::vector<std::pair<std::string, std::shared_ptr<MenuSdl>>> menus;
      std::shared_ptr<Menu> AddSubMenu(const std::string &label) override;
      void Clear() override;
  };
  struct FlatMenuItem { std::string path; std::shared_ptr<MenuItemSdl> item; }; // "Sketch › Circle"
  std::vector<FlatMenuItem> FlattenMenuBar(const MenuBarSdl &bar);
  }
  ```

- [ ] **Step 1: Failing test**

`test/ng/menumodel/test.cpp`:
```cpp
#include "harness.h"
#include "platform/sdl/menumodel.h"

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

TEST_CASE(main_menu_items_carry_command_ids) {
    // GraphicsWindow::PopulateMainMenu is exercised in the app; here we check the contract
    // on the base class default.
    MenuItemSdl item;
    CHECK_TRUE(item.commandId == 0);
}
```

- [ ] **Step 2: Verify failure.**

- [ ] **Step 3: Add `commandId` to the core interface**

In `src/platform/gui.h`, `class MenuItem`, after `std::function<void()> onTrigger;`:
```cpp
    // The Command this item activates, or 0. Lets non-native frontends (palette, keymap
    // display) relate menu items to commands without parsing labels.
    uint32_t                commandId = 0;
```
In `src/graphicswin.cpp`, `PopulateMainMenu()`, find where ordinary items are created. Read the function: it is the `else` branch after the `LOCALE` handling, calling `currentSubMenu->AddItem(label, ...)` and storing the result in `menuItems`. Immediately after that `AddItem` call, add:
```cpp
            menuItem->commandId = (uint32_t)Menu[i].cmd;
```
(Use whatever local name the function gives the returned `MenuItemRef`.)

- [ ] **Step 4: Implement `menumodel.h/.cpp`**

Header as in the Interfaces block, wrapped in include guards with `#include "solvespace.h"`.

`src/platform/sdl/menumodel.cpp`:
```cpp
#include "platform/sdl/menumodel.h"

namespace SolveSpace {
namespace Platform {

std::string StripMnemonic(const std::string &label) {
    std::string r;
    for(size_t i = 0; i < label.size(); i++) {
        if(label[i] == '&') {
            if(i + 1 < label.size() && label[i + 1] == '&') { r += '&'; i++; }
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
    auto item = std::make_shared<MenuItemSdl>();
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
```

- [ ] **Step 5: Run** `./bin/solvespace-testsuite ng/menumodel` → `0 failed`; then `make -j$(nproc) && make test_solvespace` → all old frontends still build and the suite passes.

- [ ] **Step 6: Commit**

```bash
git add src/platform/gui.h src/graphicswin.cpp src/platform/sdl/menumodel.* src/CMakeLists.txt \
        test/ng/menumodel test/CMakeLists.txt
git commit -m "NG: in-memory menu model; tag main-menu items with their Command."
```

---

### Task 6: Key translation and the two presets

**Files:**
- Create: `src/platform/sdl/keychord.{h,cpp}`
- Create: `src/ui/keypresets.{h,cpp}`
- Create: `res/keymaps/modern.keymap`; Modify: `res/CMakeLists.txt` (add `keymaps/modern.keymap` to the `add_resources(` list, next to `fonts/unicode.lff.gz`)
- Modify: `src/CMakeLists.txt` (add both `.cpp` to `solvespace-ng-support`; link `${NG_SDL_TARGET}` headers: `target_link_libraries(solvespace-ng-support PUBLIC SDL3::Headers)`)
- Modify: `AGENTS.md` §5 keymap table: arc becomes `Shift+C` (fixes the `A` clash with select-all)
- Create: `test/ng/keys/test.cpp`; Modify: `test/CMakeLists.txt`

**Interfaces:**
- Consumes: `keymap::Chord`, `keymap::Keymap` (Task 2); `CommandName` and `GraphicsWindow::MenuCommands()` (M0); `GraphicsWindow::AcceleratorForCommand(Command)` (`src/ui.h:558`, called as `SS.GW.AcceleratorForCommand`); `Platform::LoadResource` (`src/platform/platform.h:75`).
- Produces:
  ```cpp
  // src/platform/sdl/keychord.h
  namespace SolveSpace::Platform {
  // base     = keycode with no modifiers (SDL_GetKeyFromScancode(sc, SDL_KMOD_NONE, false))
  // produced = keycode with the event's modifiers (SDL_GetKeyFromScancode(sc, mod, false))
  std::optional<keymap::Chord> ChordFromKey(SDL_Keycode base, SDL_Keycode produced, SDL_Keymod mod);
  std::optional<keymap::Chord> ChordFromAccelerator(const KeyboardEvent &accel);
  }
  // src/ui/keypresets.h
  namespace SolveSpace::Ui {
  keymap::Keymap ClassicPreset();                          // from the core menu table
  std::optional<keymap::Keymap> ModernPreset(std::string *error); // from res/keymaps/modern.keymap
  }
  ```
- **Chord normalisation rules** (shared by both functions, so they always agree):
  - Letters and digits use the **unshifted** key and keep `shift`.
  - Other printable characters use the **produced** character with `shift = false`, e.g. Shift+. → `>` and Shift+= → `Plus`.
  - Named keys keep `shift`.
  - `ctrl` = Ctrl, or Cmd on macOS. `alt` = Alt.
  - On Windows, **AltGr** reports as Ctrl+Alt. When both are held and the produced key is printable and differs from the base key, treat it as a plain character with no modifiers.

- [ ] **Step 1: Failing tests**

`test/ng/keys/test.cpp`:
```cpp
#include "harness.h"
#include <SDL3/SDL_keycode.h>
#include "platform/sdl/keychord.h"
#include "ui/keypresets.h"
#include "commandnames.h"

using namespace SolveSpace::Platform;

static std::string F(std::optional<keymap::Chord> c) {
    return c ? keymap::FormatChord(*c) : std::string("<none>");
}

TEST_CASE(letters_digits_and_shift) {
    CHECK_EQ_STR(F(ChordFromKey(SDLK_S, SDLK_S, SDL_KMOD_LCTRL)), "Ctrl+S");
    CHECK_EQ_STR(F(ChordFromKey(SDLK_S, SDLK_S, SDL_KMOD_LSHIFT)), "Shift+S");
    CHECK_EQ_STR(F(ChordFromKey(SDLK_3, SDLK_HASH, SDL_KMOD_LSHIFT)), "Shift+3");
}

TEST_CASE(shifted_symbols_use_produced_char) {
    CHECK_EQ_STR(F(ChordFromKey(SDLK_PERIOD, SDLK_GREATER, SDL_KMOD_LSHIFT)), ">");
    CHECK_EQ_STR(F(ChordFromKey(SDLK_EQUALS, SDLK_PLUS, SDL_KMOD_LSHIFT)), "Plus");
    CHECK_EQ_STR(F(ChordFromKey(SDLK_KP_PLUS, SDLK_KP_PLUS, SDL_KMOD_NONE)), "Plus");
    CHECK_EQ_STR(F(ChordFromKey(SDLK_EQUALS, SDLK_EQUALS, SDL_KMOD_NONE)), "=");
}

TEST_CASE(named_keys) {
    CHECK_EQ_STR(F(ChordFromKey(SDLK_TAB, SDLK_TAB, SDL_KMOD_NONE)), "Tab");
    CHECK_EQ_STR(F(ChordFromKey(SDLK_ESCAPE, SDLK_ESCAPE, SDL_KMOD_NONE)), "Esc");
    CHECK_EQ_STR(F(ChordFromKey(SDLK_F11, SDLK_F11, SDL_KMOD_LCTRL)), "Ctrl+F11");
    CHECK_EQ_STR(F(ChordFromKey(SDLK_KP_5, SDLK_KP_5, SDL_KMOD_NONE)), "Num5");
    CHECK_EQ_STR(F(ChordFromKey(SDLK_SPACE, SDLK_SPACE, SDL_KMOD_NONE)), "Space");
}

TEST_CASE(modifier_only_presses_are_ignored) {
    CHECK_EQ_STR(F(ChordFromKey(SDLK_LSHIFT, SDLK_LSHIFT, SDL_KMOD_LSHIFT)), "<none>");
}

TEST_CASE(altgr_character_is_plain) {
    // German layout: AltGr+Q produces '@'; SDL reports Ctrl+Alt on Windows.
    CHECK_EQ_STR(F(ChordFromKey(SDLK_Q, SDLK_AT, (SDL_Keymod)(SDL_KMOD_LCTRL | SDL_KMOD_RALT))),
                 "@");
}

TEST_CASE(classic_preset_covers_every_accelerator) {
    keymap::Keymap classic = SolveSpace::Ui::ClassicPreset();
    for(Command cmd : GraphicsWindow::MenuCommands()) {
        Platform::KeyboardEvent accel = SS.GW.AcceleratorForCommand(cmd);
        bool hasAccel = accel.key == Platform::KeyboardEvent::Key::FUNCTION || accel.chr != 0;
        if(!hasAccel) continue;
        CHECK_TRUE(!classic.ChordsFor(CommandName(cmd)).empty());
    }
    CHECK_TRUE(*classic.Lookup(*keymap::ParseChord(">")) == "show-grid");
    CHECK_TRUE(*classic.Lookup(*keymap::ParseChord("Shift+3")) == "group-sketch-3d");
    CHECK_TRUE(*classic.Lookup(*keymap::ParseChord("=")) == "zoom-in");
    CHECK_TRUE(*classic.Lookup(*keymap::ParseChord("Backspace")) == "unselect-all");
    CHECK_TRUE(*classic.Lookup(*keymap::ParseChord("F3")) == "nearest-iso");
    CHECK_TRUE(*classic.Lookup(*keymap::ParseChord("Ctrl+K")) == "ui.command-palette");
}

TEST_CASE(modern_preset_is_valid) {
    std::string err;
    auto modern = SolveSpace::Ui::ModernPreset(&err);
    CHECK_TRUE(modern.has_value());
    for(const std::string &name : modern->Commands()) {
        bool known = name.rfind("ui.", 0) == 0 || CommandFromName(name) != Command::NONE;
        CHECK_TRUE(known);
    }
    CHECK_TRUE(*modern->Lookup(*keymap::ParseChord("A")) == "select-all");
    CHECK_TRUE(*modern->Lookup(*keymap::ParseChord("Shift+C")) == "arc");
    CHECK_TRUE(*modern->Lookup(*keymap::ParseChord("Space")) == "ui.command-palette");
}
```
Add `ng/keys/test.cpp` to the NG `target_sources` in `test/CMakeLists.txt`.

- [ ] **Step 2: Verify failure.**

- [ ] **Step 3: Implement `keychord.h/.cpp`**

`src/platform/sdl/keychord.h`:
```cpp
#ifndef SOLVESPACE_SDL_KEYCHORD_H
#define SOLVESPACE_SDL_KEYCHORD_H
#include <optional>
#include <SDL3/SDL_keycode.h>
#include "keymap.h"
#include "solvespace.h"
namespace SolveSpace {
namespace Platform {
std::optional<keymap::Chord> ChordFromKey(SDL_Keycode base, SDL_Keycode produced, SDL_Keymod mod);
std::optional<keymap::Chord> ChordFromAccelerator(const KeyboardEvent &accel);
}
}
#endif
```
`src/platform/sdl/keychord.cpp`:
```cpp
#include "platform/sdl/keychord.h"

namespace SolveSpace {
namespace Platform {

static std::optional<std::string> NamedKey(SDL_Keycode k) {
    switch(k) {
        case SDLK_SPACE:     return "Space";
        case SDLK_TAB:       return "Tab";
        case SDLK_ESCAPE:    return "Esc";
        case SDLK_RETURN:    return "Enter";
        case SDLK_BACKSPACE: return "Backspace";
        case SDLK_DELETE:    return "Delete";
        case SDLK_INSERT:    return "Insert";
        case SDLK_HOME:      return "Home";
        case SDLK_END:       return "End";
        case SDLK_PAGEUP:    return "PageUp";
        case SDLK_PAGEDOWN:  return "PageDown";
        case SDLK_UP:        return "Up";
        case SDLK_DOWN:      return "Down";
        case SDLK_LEFT:      return "Left";
        case SDLK_RIGHT:     return "Right";
        case SDLK_KP_PLUS:   return "Plus";
        case SDLK_KP_MINUS:  return "Num-";
        case SDLK_KP_MULTIPLY: return "Num*";
        case SDLK_KP_DIVIDE: return "Num/";
        case SDLK_KP_PERIOD: return "Num.";
        case SDLK_KP_ENTER:  return "NumEnter";
        default: break;
    }
    if(k >= SDLK_KP_1 && k <= SDLK_KP_9) return "Num" + std::to_string(1 + (k - SDLK_KP_1));
    if(k == SDLK_KP_0) return "Num0";
    if(k >= SDLK_F1 && k <= SDLK_F12) return "F" + std::to_string(1 + (k - SDLK_F1));
    if(k >= SDLK_F13 && k <= SDLK_F24) return "F" + std::to_string(13 + (k - SDLK_F13));
    return std::nullopt;
}

static bool IsModifierKey(SDL_Keycode k) {
    return k == SDLK_LSHIFT || k == SDLK_RSHIFT || k == SDLK_LCTRL || k == SDLK_RCTRL ||
           k == SDLK_LALT || k == SDLK_RALT || k == SDLK_LGUI || k == SDLK_RGUI ||
           k == SDLK_MODE || k == SDLK_CAPSLOCK;
}

std::optional<keymap::Chord> ChordFromKey(SDL_Keycode base, SDL_Keycode produced,
                                          SDL_Keymod mod) {
    if(IsModifierKey(base)) return std::nullopt;
    keymap::Chord c;
#if defined(__APPLE__)
    c.ctrl = (mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0;
#else
    c.ctrl = (mod & SDL_KMOD_CTRL) != 0;
#endif
    c.alt   = (mod & SDL_KMOD_ALT) != 0;
    c.shift = (mod & SDL_KMOD_SHIFT) != 0;

    if(auto named = NamedKey(base)) {
        c.key = *named;
        if(c.key == "Plus") c.shift = false;
        return c;
    }
    bool baseAlnum = base < 0x80 && std::isalnum((int)base);
    bool producedPrintable = produced >= 0x21 && produced < 0x7f;
    // AltGr (reported as Ctrl+Alt) producing a different printable character: plain char.
    if(c.ctrl && c.alt && producedPrintable && produced != base) {
        return keymap::Chord{ std::string(1, (char)produced) };
    }
    if(baseAlnum) {
        c.key = std::string(1, (char)std::tolower((int)base));
        return c;
    }
    if(producedPrintable) {
        c.shift = false;
        c.key = (produced == '+') ? "Plus" : std::string(1, (char)produced);
        return c;
    }
    return std::nullopt;
}

std::optional<keymap::Chord> ChordFromAccelerator(const KeyboardEvent &accel) {
    keymap::Chord c;
    c.ctrl  = accel.controlDown;
    c.shift = accel.shiftDown;
    if(accel.key == KeyboardEvent::Key::FUNCTION) {
        if(accel.num < 1 || accel.num > 24) return std::nullopt;
        c.key = "F" + std::to_string(accel.num);
        return c;
    }
    char32_t ch = accel.chr;
    switch(ch) {
        case 0:      return std::nullopt;
        case '\t':   c.key = "Tab";       return c;
        case '\x1b': c.key = "Esc";       return c;
        case '\x7f': c.key = "Delete";    return c;
        case '\b':   c.key = "Backspace"; return c;
        case ' ':    c.key = "Space";     return c;
        case '+':    c.key = "Plus"; c.shift = false; return c;
    }
    if(ch < 0x80 && std::isalnum((int)ch)) {
        c.key = std::string(1, (char)std::tolower((int)ch));
        return c;
    }
    if(ch >= 0x21 && ch < 0x7f) {
        c.key = std::string(1, (char)ch);
        c.shift = false;
        return c;
    }
    return std::nullopt;
}

}
}
```

- [ ] **Step 4: Write the Modern keymap**

`res/keymaps/modern.keymap`:
```
# SolveSpace NG "Modern" keymap (Blender-style). DRAFT: tuned during M1.
# Format: <command>: <chord>[, <chord>...]
# Command names are the stable ids from src/commandnames.cpp; "ui." names are UI actions.
# M1b adds: Tab (sketch mode), Shift+A (add pie), F (constrain pie), G (grab), H/Alt+H/Shift+H,
# ` (view pie), numpad views, "." frame selected.

ui.command-palette:     Space, F3
ui.toggle-menu-bar:     F10
ui.toggle-theme:        Ctrl+Alt+T

new:                    Ctrl+N
open:                   Ctrl+O
save:                   Ctrl+S
save-as:                Ctrl+Shift+S
exit:                   Ctrl+Q
undo:                   Ctrl+Z
redo:                   Ctrl+Shift+Z, Ctrl+Y
cut:                    Ctrl+X
copy:                   Ctrl+C
paste:                  Ctrl+V
paste-transformed:      Ctrl+Shift+V
delete:                 Delete, X
select-all:             A
unselect-all:           Alt+A, Esc
select-edge-chain:      Ctrl+L
configuration:          Ctrl+,
regenerate-all:         F5

datum-point:            P
line-segment:           L
construction-line-segment: Shift+L
rectangle:              B
circle:                 C
arc:                    Shift+C
tangent-arc:            Alt+C
cubic-spline:           Shift+B
ttf-text:               Shift+T
toggle-construction:    T
split-curves:           I

constrain-distance:     D
constrain-reference-distance: Shift+D
constrain-angle:        Alt+D

group-extrude:          E
group-revolve:          Shift+R
group-sketch-workplane: Shift+W
group-link:             Ctrl+I

zoom-in:                Plus, =
zoom-out:               -
zoom-to-fit:            Home
center-view:            Num.
perspective:            Num5
show-grid:              Shift+G
show-property-browser:  N
show-toolbar:           Ctrl+Shift+T
full-screen:            F11
```

Add to `res/CMakeLists.txt` in the `add_resources(` list (after `fonts/unicode.lff.gz`):
```cmake
    keymaps/modern.keymap
```

- [ ] **Step 5: Implement `keypresets.h/.cpp`**

`src/ui/keypresets.h`:
```cpp
#ifndef SOLVESPACE_UI_KEYPRESETS_H
#define SOLVESPACE_UI_KEYPRESETS_H
#include <optional>
#include <string>
#include "keymap.h"
namespace SolveSpace {
namespace Ui {
keymap::Keymap ClassicPreset();
std::optional<keymap::Keymap> ModernPreset(std::string *error);
}
}
#endif
```
`src/ui/keypresets.cpp`:
```cpp
#include "solvespace.h"
#include "commandnames.h"
#include "platform/sdl/keychord.h"
#include "ui/keypresets.h"

namespace SolveSpace {
namespace Ui {

keymap::Keymap ClassicPreset() {
    keymap::Keymap km;
    std::string err;
    for(Command cmd : GraphicsWindow::MenuCommands()) {
        auto chord = Platform::ChordFromAccelerator(SS.GW.AcceleratorForCommand(cmd));
        if(!chord) continue;
        if(!km.Bind(CommandName(cmd), *chord, &err)) dbp("classic keymap: %s", err.c_str());
    }
    // GraphicsWindow::KeyboardEvent's hard-coded extras.
    km.Bind("unselect-all", *keymap::ParseChord("Backspace"), &err);
    km.Bind("zoom-in", *keymap::ParseChord("="), &err);
    // F3 is Nearest Isometric View in Classic, so the palette gets a free chord instead.
    km.Bind("ui.command-palette", *keymap::ParseChord("Ctrl+K"), &err);
    return km;
}

std::optional<keymap::Keymap> ModernPreset(std::string *error) {
    size_t size;
    const char *data = (const char *)Platform::LoadResource("keymaps/modern.keymap", &size);
    return keymap::Keymap::Parse(std::string_view(data, size), error);
}

}
}
```
Add `platform/sdl/keychord.cpp` and `ui/keypresets.cpp` to `solvespace-ng-support`, plus:
```cmake
    target_link_libraries(solvespace-ng-support PUBLIC SDL3::Headers)
```
(If the vendored SDL doesn't export `SDL3::Headers`, use `target_include_directories(... ${CMAKE_SOURCE_DIR}/extlib/SDL/include)`.)

- [ ] **Step 6: Fix the AGENTS.md keymap clash**

In `AGENTS.md` §5 keymap table, replace the row `` | `L` `B` `C` `A` | Direct: line, box/rectangle, circle, arc | `` with:
```markdown
| `L` `B` `C` `Shift+C` | Direct: line, box/rectangle, circle, arc (`A` is select-all) |
```

- [ ] **Step 7: Run** `./bin/solvespace-testsuite ng/keys` → `0 failed`.

If `classic_preset_covers_every_accelerator` reports a bind conflict in `dbp` output, two menu entries share an accelerator. Report it to the owner; don't silently drop one.

- [ ] **Step 8: Commit**

```bash
make test_solvespace
git add src/platform/sdl/keychord.* src/ui res/keymaps res/CMakeLists.txt src/CMakeLists.txt \
        test/ng/keys test/CMakeLists.txt AGENTS.md
git commit -m "NG: SDL key -> chord translation; Classic (generated) and Modern keymap presets."
```

---

### Task 7: The SDL backend: window, GL, events, timers, dialogs

This is the largest task. It produces a runnable `solvespace-ng` showing the model, the old toolbar and working mouse interaction. Keys still go through the core accelerators until Task 10. The property browser is created but not drawn until Task 8, and menus aren't drawn until Task 9.

**Files:**
- Create: `src/platform/sdl/timerregistry.{h,cpp}`, `src/platform/sdl/dialoghelpers.{h,cpp}`
- Create: `src/platform/guisdl.cpp`
- Modify: `src/CMakeLists.txt` (support-lib sources + `solvespace-ng` executable)
- Create: `test/ng/backend/test.cpp`; Modify: `test/CMakeLists.txt`

**Interfaces:**
- Consumes: `SettingsFile` (Task 4), `MenuSdl`/`MenuBarSdl` (Task 5), all of `src/platform/gui.h`.
- Produces:
  ```cpp
  // timerregistry.h — main-thread bookkeeping, no SDL dependency
  namespace SolveSpace::Platform {
  class TimerRegistry {
  public:
      uint64_t Arm(std::weak_ptr<Timer> owner);   // returns a new serial
      void     Cancel(uint64_t serial);
      std::shared_ptr<Timer> Take(uint64_t serial); // nullptr if cancelled/stale/dead
  };
  }
  // dialoghelpers.h
  namespace SolveSpace::Platform {
  // Appends "." + first extension of filters[filterIndex] (or filters[0] if index < 0)
  // when `path` has no extension. Returns `path` unchanged otherwise or if no filters.
  std::string EnsureExtension(const std::string &path, const std::vector<FileFilter> &filters,
                              int filterIndex);
  // "slvs;stl" style pattern for SDL_DialogFileFilter.
  std::string SdlFilterPattern(const FileFilter &filter);
  }
  // guisdl.cpp — internal hooks used by later tasks (declared in the .cpp, file-static):
  //   static void DrawToolWindows();          // Task 8 fills in
  //   static void DrawMainMenuBar();          // Task 9 fills in
  //   static bool RouteKeyDown(const SDL_Event &e); // Task 10 fills in; returns consumed
  ```

- [ ] **Step 1: Failing tests for the pure helpers**

`test/ng/backend/test.cpp`:
```cpp
#include "harness.h"
#include "platform/sdl/timerregistry.h"
#include "platform/sdl/dialoghelpers.h"

using namespace SolveSpace::Platform;

class DummyTimer final : public Timer {
public:
    void RunAfter(unsigned) override {}
};

TEST_CASE(timer_registry_rearm_and_cancel) {
    TimerRegistry reg;
    auto t = std::make_shared<DummyTimer>();
    uint64_t first = reg.Arm(t);
    uint64_t second = reg.Arm(t);         // re-armed before first fired
    CHECK_TRUE(first != second);
    reg.Cancel(first);
    CHECK_TRUE(reg.Take(first) == nullptr);
    CHECK_TRUE(reg.Take(second) == t);
    CHECK_TRUE(reg.Take(second) == nullptr); // fires once
    uint64_t third = reg.Arm(t);
    t.reset();                               // owner destroyed before firing
    CHECK_TRUE(reg.Take(third) == nullptr);
}

TEST_CASE(ensure_extension) {
    std::vector<FileFilter> f = { { "SolveSpace models", { "slvs" } },
                                  { "Meshes", { "stl", "obj" } } };
    CHECK_EQ_STR(EnsureExtension("/a/b", f, 1), "/a/b.stl");
    CHECK_EQ_STR(EnsureExtension("/a/b", f, -1), "/a/b.slvs");
    CHECK_EQ_STR(EnsureExtension("/a/b.slvs", f, 1), "/a/b.slvs");
    CHECK_EQ_STR(EnsureExtension("/a.dir/b", f, 0), "/a.dir/b.slvs");
    CHECK_EQ_STR(EnsureExtension("/a/b", {}, 0), "/a/b");
    CHECK_EQ_STR(SdlFilterPattern(f[1]), "stl;obj");
}
```
Add `ng/backend/test.cpp` to the NG test sources.

- [ ] **Step 2: Verify failure.**

- [ ] **Step 3: Implement `TimerRegistry`**

`src/platform/sdl/timerregistry.h`:
```cpp
#ifndef SOLVESPACE_SDL_TIMERREGISTRY_H
#define SOLVESPACE_SDL_TIMERREGISTRY_H
#include <memory>
#include <unordered_map>
#include "solvespace.h"
namespace SolveSpace {
namespace Platform {
// Maps timer-event serials to their owners. Only touched on the main thread; the SDL timer
// thread only ever pushes an event carrying the serial.
class TimerRegistry {
public:
    uint64_t Arm(std::weak_ptr<Timer> owner);
    void     Cancel(uint64_t serial);
    std::shared_ptr<Timer> Take(uint64_t serial);
private:
    uint64_t nextSerial = 1;
    std::unordered_map<uint64_t, std::weak_ptr<Timer>> pending;
};
}
}
#endif
```
`src/platform/sdl/timerregistry.cpp`:
```cpp
#include "platform/sdl/timerregistry.h"
namespace SolveSpace {
namespace Platform {
uint64_t TimerRegistry::Arm(std::weak_ptr<Timer> owner) {
    uint64_t serial = nextSerial++;
    pending[serial] = owner;
    return serial;
}
void TimerRegistry::Cancel(uint64_t serial) {
    pending.erase(serial);
}
std::shared_ptr<Timer> TimerRegistry::Take(uint64_t serial) {
    auto it = pending.find(serial);
    if(it == pending.end()) return nullptr;
    std::shared_ptr<Timer> owner = it->second.lock();
    pending.erase(it);
    return owner;
}
}
}
```
The re-arm case works because `TimerImplSdl::RunAfter` (Step 5) calls `Cancel(oldSerial)` before `Arm`. The test calls `Cancel(first)` explicitly to mirror that.

- [ ] **Step 4: Implement dialog helpers**

`src/platform/sdl/dialoghelpers.h`: declarations as in Interfaces, with `#include "solvespace.h"`.

`src/platform/sdl/dialoghelpers.cpp`:
```cpp
#include "platform/sdl/dialoghelpers.h"
namespace SolveSpace {
namespace Platform {
std::string EnsureExtension(const std::string &path, const std::vector<FileFilter> &filters,
                            int filterIndex) {
    if(filters.empty()) return path;
    size_t slash = path.find_last_of("/\\");
    size_t dot   = path.find_last_of('.');
    if(dot != std::string::npos && (slash == std::string::npos || dot > slash)) return path;
    const FileFilter &f =
        filters[(filterIndex >= 0 && filterIndex < (int)filters.size()) ? filterIndex : 0];
    if(f.extensions.empty()) return path;
    return path + "." + f.extensions[0];
}
std::string SdlFilterPattern(const FileFilter &filter) {
    std::string out;
    for(size_t i = 0; i < filter.extensions.size(); i++) {
        if(i) out += ";";
        out += filter.extensions[i];
    }
    return out;
}
}
}
```
Add `platform/sdl/timerregistry.cpp` and `platform/sdl/dialoghelpers.cpp` to `solvespace-ng-support`. Run `./bin/solvespace-testsuite ng/backend` → `0 failed`. Commit:
```bash
git add src/platform/sdl/timerregistry.* src/platform/sdl/dialoghelpers.* src/CMakeLists.txt \
        test/ng/backend test/CMakeLists.txt
git commit -m "NG: timer registry and file-dialog helpers."
```

- [ ] **Step 5: Write `src/platform/guisdl.cpp`**

```cpp
//-----------------------------------------------------------------------------
// The SDL3 + Dear ImGui platform backend (solvespace-ng).
//
// One SDL window, one GL context. The graphics window draws into the default framebuffer;
// TOOL windows (the property browser) draw into offscreen framebuffers shown as ImGui panels.
//-----------------------------------------------------------------------------
#include <filesystem>
#include <SDL3/SDL.h>
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"
#include "solvespace.h"
#include "platform/sdl/settingsfile.h"
#include "platform/sdl/menumodel.h"
#include "platform/sdl/timerregistry.h"
#include "platform/sdl/dialoghelpers.h"

namespace SolveSpace {
namespace Platform {

class WindowImplSdl;

static SDL_Window    *gSdlWindow   = nullptr;
static SDL_GLContext  gGlContext   = nullptr;
static Uint32         gTimerEvent  = 0;
static bool           gQuit        = false;
static bool           gNeedsFrame  = true;
static TimerRegistry  gTimers;
static std::weak_ptr<WindowImplSdl>              gMainWindow;
static std::vector<std::weak_ptr<WindowImplSdl>> gToolWindows;

static void DrawToolWindows();               // Task 8
static void DrawMainMenuBar();               // Task 9
static bool RouteKeyDown(const SDL_Event &e); // Task 10
static void RenderFrame();

//-----------------------------------------------------------------------------
// Fatal errors
//-----------------------------------------------------------------------------

void FatalError(const std::string &message) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SolveSpace: fatal error",
                             message.c_str(), gSdlWindow);
    abort();
}

//-----------------------------------------------------------------------------
// Settings
//-----------------------------------------------------------------------------

SettingsRef GetSettings() {
    static std::shared_ptr<SettingsFile> settings;
    if(!settings) {
        char *pref = SDL_GetPrefPath("solvespace", "solvespace-ng");
        Path dir = Path::From(pref ? pref : ".");
        SDL_free(pref);
        settings = std::make_shared<SettingsFile>(dir.Join("settings.txt"));
    }
    return settings;
}

//-----------------------------------------------------------------------------
// Timers
//-----------------------------------------------------------------------------

static Uint32 SDLCALL TimerCallback(void *userdata, SDL_TimerID, Uint32) {
    SDL_Event ev = {};
    ev.type       = gTimerEvent;
    ev.user.data1 = userdata;   // the serial, smuggled as a pointer-sized integer
    SDL_PushEvent(&ev);
    return 0;
}

class TimerImplSdl final : public Timer, public std::enable_shared_from_this<TimerImplSdl> {
public:
    SDL_TimerID id     = 0;
    uint64_t    serial = 0;

    ~TimerImplSdl() override { Cancel(); }

    void Cancel() {
        if(id) { SDL_RemoveTimer(id); id = 0; }
        if(serial) { gTimers.Cancel(serial); serial = 0; }
    }

    void RunAfter(unsigned milliseconds) override {
        Cancel();
        serial = gTimers.Arm(weak_from_this());
        void *token = (void *)(uintptr_t)serial;
        if(milliseconds == 0) {
            TimerCallback(token, 0, 0);
        } else {
            id = SDL_AddTimer(milliseconds, TimerCallback, token);
        }
    }
};

TimerRef CreateTimer() {
    return std::make_shared<TimerImplSdl>();
}

static void HandleTimerEvent(const SDL_Event &ev) {
    uint64_t serial = (uint64_t)(uintptr_t)ev.user.data1;
    std::shared_ptr<Timer> timer = gTimers.Take(serial);
    if(!timer) return;
    auto sdlTimer = std::static_pointer_cast<TimerImplSdl>(timer);
    if(sdlTimer->serial != serial) return;
    sdlTimer->id = 0;
    sdlTimer->serial = 0;
    if(sdlTimer->onTimeout) sdlTimer->onTimeout();
}

//-----------------------------------------------------------------------------
// Menus
//-----------------------------------------------------------------------------

static std::shared_ptr<MenuBarSdl> gMainMenu;

MenuRef CreateMenu() {
    return std::make_shared<MenuSdl>();   // Task 9 installs onPopUp
}

MenuBarRef GetOrCreateMainMenu(bool *unique) {
    *unique = false;
    if(!gMainMenu) gMainMenu = std::make_shared<MenuBarSdl>();
    return gMainMenu;
}

//-----------------------------------------------------------------------------
// Windows
//-----------------------------------------------------------------------------

class WindowImplSdl final : public Window {
public:
    Kind        kind;
    std::string title;
    bool        visible      = true;
    double      width        = 800, height = 600;   // logical pixels
    Cursor      cursor       = Cursor::POINTER;
    MenuBarRef  menuBar;
    // Editor overlay (dimension/value entry), positioned in this window's coordinates.
    bool        editorVisible = false, editorFocusPending = false;
    double      editorX = 0, editorY = 0, editorFontHeight = 0, editorMinWidth = 0;
    bool        editorMonospace = false;
    std::string editorText;
    // Tooltip
    std::string tooltipText;
    double      tooltipX = 0, tooltipY = 0, tooltipW = 0, tooltipH = 0;
    // Scrollbar (TOOL windows)
    bool        scrollbarVisible = false;
    double      scrollMin = 0, scrollMax = 0, scrollPage = 0, scrollPos = 0;
    // Offscreen target (TOOL windows); filled in by Task 8.
    unsigned    fbo = 0, fboColor = 0, fboDepth = 0;
    int         fboWidth = 0, fboHeight = 0;

    explicit WindowImplSdl(Kind kind) : kind(kind) {}

    double GetPixelDensity() override {
        float scale = SDL_GetDisplayContentScale(SDL_GetDisplayForWindow(gSdlWindow));
        return 96.0 * (scale > 0 ? scale : 1.0f);
    }
    double GetDevicePixelRatio() override {
        float d = SDL_GetWindowPixelDensity(gSdlWindow);
        return d > 0 ? d : 1.0;
    }

    bool IsVisible() override { return visible; }
    void SetVisible(bool v) override {
        visible = v;
        if(kind == Kind::TOPLEVEL) { v ? SDL_ShowWindow(gSdlWindow) : SDL_HideWindow(gSdlWindow); }
        Invalidate();
    }
    void Focus() override {
        if(kind == Kind::TOPLEVEL) SDL_RaiseWindow(gSdlWindow);
    }

    bool IsFullScreen() override {
        return (SDL_GetWindowFlags(gSdlWindow) & SDL_WINDOW_FULLSCREEN) != 0;
    }
    void SetFullScreen(bool fs) override {
        SDL_SetWindowFullscreen(gSdlWindow, fs);
        if(onFullScreen) onFullScreen(fs);
    }

    void SetTitle(const std::string &t) override {
        title = t;
        if(kind == Kind::TOPLEVEL) SDL_SetWindowTitle(gSdlWindow, t.c_str());
    }
    bool SetTitleForFilename(const Path &filename) override {
        SetTitle(filename.IsEmpty() ? "(new sketch)" : filename.FileName());
        return true;
    }

    void SetMenuBar(MenuBarRef mb) override { menuBar = mb; }

    void GetContentSize(double *w, double *h) override {
        if(kind == Kind::TOPLEVEL) {
            int iw, ih;
            SDL_GetWindowSize(gSdlWindow, &iw, &ih);
            width = iw; height = ih;
        }
        *w = width; *h = height;
    }
    void SetMinContentSize(double w, double h) override {
        if(kind == Kind::TOPLEVEL) SDL_SetWindowMinimumSize(gSdlWindow, (int)w, (int)h);
    }

    void FreezePosition(SettingsRef settings, const std::string &key) override {
        if(kind != Kind::TOPLEVEL) {
            settings->FreezeBool(key + "_Visible", visible);
            return;
        }
        int x, y, w, h;
        SDL_GetWindowPosition(gSdlWindow, &x, &y);
        SDL_GetWindowSize(gSdlWindow, &w, &h);
        settings->FreezeInt(key + "_Left", (uint32_t)x);
        settings->FreezeInt(key + "_Top", (uint32_t)y);
        settings->FreezeInt(key + "_Width", (uint32_t)w);
        settings->FreezeInt(key + "_Height", (uint32_t)h);
        settings->FreezeBool(key + "_Maximized",
                             (SDL_GetWindowFlags(gSdlWindow) & SDL_WINDOW_MAXIMIZED) != 0);
    }
    void ThawPosition(SettingsRef settings, const std::string &key) override {
        if(kind != Kind::TOPLEVEL) {
            visible = settings->ThawBool(key + "_Visible", true);
            return;
        }
        int w = (int)settings->ThawInt(key + "_Width", 1280);
        int h = (int)settings->ThawInt(key + "_Height", 800);
        SDL_SetWindowSize(gSdlWindow, w, h);
        int x = (int)settings->ThawInt(key + "_Left", (uint32_t)SDL_WINDOWPOS_CENTERED);
        int y = (int)settings->ThawInt(key + "_Top", (uint32_t)SDL_WINDOWPOS_CENTERED);
        SDL_SetWindowPosition(gSdlWindow, x, y);
        if(settings->ThawBool(key + "_Maximized", false)) SDL_MaximizeWindow(gSdlWindow);
    }

    void SetCursor(Cursor c) override { cursor = c; }
    void SetTooltip(const std::string &text, double x, double y, double w, double h) override {
        tooltipText = text; tooltipX = x; tooltipY = y; tooltipW = w; tooltipH = h;
    }

    bool IsEditorVisible() override { return editorVisible; }
    void ShowEditor(double x, double y, double fontHeight, double minWidth, bool isMonospace,
                    const std::string &text) override {
        editorVisible = true; editorFocusPending = true;
        editorX = x; editorY = y; editorFontHeight = fontHeight; editorMinWidth = minWidth;
        editorMonospace = isMonospace; editorText = text;
        Invalidate();
    }
    void HideEditor() override { editorVisible = false; Invalidate(); }

    void SetScrollbarVisible(bool v) override { scrollbarVisible = v; }
    void ConfigureScrollbar(double min, double max, double page) override {
        scrollMin = min; scrollMax = max; scrollPage = page;
    }
    double GetScrollbarPosition() override { return scrollPos; }
    void SetScrollbarPosition(double pos) override {
        scrollPos = std::max(scrollMin, std::min(pos, scrollMax - scrollPage));
    }

    void Invalidate() override { gNeedsFrame = true; }
};

static void InitSdlWindowAndImGui();

WindowRef CreateWindow(Window::Kind kind, WindowRef parentWindow) {
    auto window = std::make_shared<WindowImplSdl>(kind);
    if(kind == Window::Kind::TOPLEVEL) {
        ssassert(gMainWindow.expired(), "only one TOPLEVEL window is supported");
        InitSdlWindowAndImGui();
        gMainWindow = window;
    } else {
        window->width = 420; window->height = 640;
        gToolWindows.push_back(window);
    }
    return window;
}

void Open3DConnexion() {}
void Close3DConnexion() {}
void Request3DConnexionEventsForWindow(WindowRef window) {}

//-----------------------------------------------------------------------------
// Message dialogs
//-----------------------------------------------------------------------------

class MessageDialogImplSdl final : public MessageDialog {
public:
    Type                                type = Type::INFORMATION;
    std::string                         title, message, description;
    std::vector<std::pair<std::string, Response>> buttons;
    int                                 defaultIndex = -1;

    void SetType(Type t) override { type = t; }
    void SetTitle(std::string t) override { title = t; }
    void SetMessage(std::string m) override { message = m; }
    void SetDescription(std::string d) override { description = d; }
    void AddButton(std::string label, Response response, bool isDefault) override {
        if(isDefault) defaultIndex = (int)buttons.size();
        buttons.push_back({ StripMnemonic(label), response });
    }

    Response RunModal() override {
        std::vector<SDL_MessageBoxButtonData> sdlButtons;
        for(size_t i = 0; i < buttons.size(); i++) {
            SDL_MessageBoxButtonData b = {};
            b.buttonID = (int)i;
            b.text     = buttons[i].first.c_str();
            if((int)i == defaultIndex) b.flags |= SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
            if(buttons[i].second == Response::CANCEL) b.flags |= SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
            sdlButtons.push_back(b);
        }
        std::string text = description.empty() ? message : message + "\n\n" + description;
        SDL_MessageBoxData data = {};
        data.flags = type == Type::ERROR   ? SDL_MESSAGEBOX_ERROR
                   : type == Type::WARNING ? SDL_MESSAGEBOX_WARNING
                                           : SDL_MESSAGEBOX_INFORMATION;
        data.window     = gSdlWindow;
        data.title      = title.c_str();
        data.message    = text.c_str();
        data.numbuttons = (int)sdlButtons.size();
        data.buttons    = sdlButtons.data();
        int pressed = -1;
        if(!SDL_ShowMessageBox(&data, &pressed) || pressed < 0) return Response::NONE;
        return buttons[pressed].second;
    }
};

MessageDialogRef CreateMessageDialog(WindowRef parentWindow) {
    return std::make_shared<MessageDialogImplSdl>();
}

//-----------------------------------------------------------------------------
// Nested event loop (used by blocking file dialogs and context menus)
//-----------------------------------------------------------------------------

static void PumpEventsOnce(bool wait);

static void RunNestedLoop(const std::function<bool()> &done) {
    while(!done() && !gQuit) {
        PumpEventsOnce(/*wait=*/true);
        RenderFrame();
    }
}

//-----------------------------------------------------------------------------
// File dialogs
//-----------------------------------------------------------------------------

class FileDialogImplSdl final : public FileDialog {
public:
    bool                    isSave;
    std::string             title;
    Path                    filename;
    std::vector<FileFilter> filters;

    explicit FileDialogImplSdl(bool isSave) : isSave(isSave) {}

    void SetTitle(std::string t) override { title = t; }
    void SetCurrentName(std::string name) override { filename = filename.Parent().Join(name); }
    Path GetFilename() override { return filename; }
    void SetFilename(Path path) override { filename = path; }
    void SuggestFilename(Path path) override { filename = path; }
    void AddFilter(std::string name, std::vector<std::string> extensions) override {
        filters.push_back({ name, extensions });
    }
    void FreezeChoices(SettingsRef settings, const std::string &key) override {
        settings->FreezeString("Dialog_" + key + "_Folder", filename.Parent().raw);
    }
    void ThawChoices(SettingsRef settings, const std::string &key) override {
        std::string folder = settings->ThawString("Dialog_" + key + "_Folder", "");
        if(!folder.empty() && filename.IsEmpty()) filename = Path::From(folder).Join("");
    }

    bool RunModal() override {
        struct Result {
            std::atomic<bool> done{ false };
            std::mutex        mutex;
            std::string       path;
            int               filter = -1;
        } result;

        std::vector<std::string> patterns;
        for(const FileFilter &f : filters) patterns.push_back(SdlFilterPattern(f));
        std::vector<SDL_DialogFileFilter> sdlFilters;
        for(size_t i = 0; i < filters.size(); i++) {
            sdlFilters.push_back({ filters[i].name.c_str(), patterns[i].c_str() });
        }

        // SDL may invoke this on another thread.
        auto callback = [](void *userdata, const char *const *filelist, int filter) {
            Result *r = (Result *)userdata;
            {
                std::lock_guard<std::mutex> lock(r->mutex);
                if(filelist && filelist[0]) r->path = filelist[0];
                r->filter = filter;
            }
            r->done = true;
        };
        const char *location = filename.IsEmpty() ? nullptr : filename.raw.c_str();
        if(isSave) {
            SDL_ShowSaveFileDialog(callback, &result, gSdlWindow, sdlFilters.data(),
                                   (int)sdlFilters.size(), location);
        } else {
            SDL_ShowOpenFileDialog(callback, &result, gSdlWindow, sdlFilters.data(),
                                   (int)sdlFilters.size(), location, /*allow_many=*/false);
        }
        RunNestedLoop([&] { return result.done.load(); });

        std::lock_guard<std::mutex> lock(result.mutex);
        if(result.path.empty()) return false;
        std::string chosen = result.path;
        if(isSave) chosen = EnsureExtension(chosen, filters, result.filter);
        filename = Path::From(chosen);
        return true;
    }
};

FileDialogRef CreateOpenFileDialog(WindowRef parentWindow) {
    return std::make_shared<FileDialogImplSdl>(/*isSave=*/false);
}

FileDialogRef CreateSaveFileDialog(WindowRef parentWindow) {
    return std::make_shared<FileDialogImplSdl>(/*isSave=*/true);
}

//-----------------------------------------------------------------------------
// Application-wide APIs
//-----------------------------------------------------------------------------

std::vector<Path> GetFontFiles() {
    std::vector<std::string> roots;
#if defined(_WIN32)
    if(const char *windir = getenv("WINDIR")) roots.push_back(std::string(windir) + "\\Fonts");
    if(const char *local = getenv("LOCALAPPDATA"))
        roots.push_back(std::string(local) + "\\Microsoft\\Windows\\Fonts");
#elif defined(__APPLE__)
    roots = { "/System/Library/Fonts", "/Library/Fonts" };
    if(const char *home = getenv("HOME")) roots.push_back(std::string(home) + "/Library/Fonts");
#else
    roots = { "/usr/share/fonts", "/usr/local/share/fonts" };
    if(const char *home = getenv("HOME")) {
        roots.push_back(std::string(home) + "/.local/share/fonts");
        roots.push_back(std::string(home) + "/.fonts");
    }
#endif
    std::vector<Path> fonts;
    std::error_code ec;
    for(const std::string &root : roots) {
        for(auto it = std::filesystem::recursive_directory_iterator(
                root, std::filesystem::directory_options::skip_permission_denied, ec);
            it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
            if(ec) break;
            std::string ext = it->path().extension().string();
            for(char &c : ext) c = (char)tolower((unsigned char)c);
            if(ext == ".ttf" || ext == ".otf") fonts.push_back(Path::From(it->path().string()));
        }
    }
    return fonts;
}

void OpenInBrowser(const std::string &url) {
    SDL_OpenURL(url.c_str());
}

std::vector<std::string> InitGui(int argc, char **argv) {
    std::vector<std::string> args = InitCli(argc, argv);
    if(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        FatalError(std::string("SDL_Init failed: ") + SDL_GetError());
    }
    gTimerEvent = SDL_RegisterEvents(1);

    int count = 0;
    SDL_Locale **locales = SDL_GetPreferredLocales(&count);
    bool localeSet = false;
    for(int i = 0; locales && i < count && !localeSet; i++) {
        std::string name = locales[i]->language;
        if(locales[i]->country) name += std::string("_") + locales[i]->country;
        localeSet = SetLocale(name);
    }
    SDL_free(locales);
    if(!localeSet) SetLocale("en_US");
    return args;
}

static void InitSdlWindowAndImGui() {
#if defined(_WIN32)
    // ANGLE: GLES 3.0 context (see Task 13).
    SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER, "1");
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    const char *glslVersion = "#version 300 es";
#else
    // Compatibility context: the renderer uses GLSL 1.20 and no VAOs.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    const char *glslVersion = "#version 120";
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

    gSdlWindow = SDL_CreateWindow("SolveSpace", 1280, 800,
                                  SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                                  SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN);
    if(!gSdlWindow) {
        // Retry without MSAA; some drivers reject it.
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
        gSdlWindow = SDL_CreateWindow("SolveSpace", 1280, 800,
                                      SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                                      SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN);
    }
    if(!gSdlWindow) FatalError(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
    gGlContext = SDL_GL_CreateContext(gSdlWindow);
    if(!gGlContext) FatalError(std::string("SDL_GL_CreateContext failed: ") + SDL_GetError());
    SDL_GL_MakeCurrent(gSdlWindow, gGlContext);
    SDL_GL_SetSwapInterval(1);
    SDL_StartTextInput(gSdlWindow);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;   // layout persists through Platform::Settings instead
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui_ImplSDL3_InitForOpenGL(gSdlWindow, gGlContext);
    ImGui_ImplOpenGL3_Init(glslVersion);
    SDL_ShowWindow(gSdlWindow);
}

static WindowImplSdl *MainWindow() {
    auto w = gMainWindow.lock();
    return w.get();
}

static MouseEvent MakeMouseEvent(MouseEvent::Type type, double x, double y) {
    MouseEvent me = {};
    me.type = type;
    me.x = x;
    me.y = y;
    SDL_Keymod mod = SDL_GetModState();
    me.shiftDown   = (mod & SDL_KMOD_SHIFT) != 0;
    me.controlDown = (mod & SDL_KMOD_CTRL) != 0;
    return me;
}

static MouseEvent::Button ButtonFromSdl(Uint8 b) {
    switch(b) {
        case SDL_BUTTON_LEFT:   return MouseEvent::Button::LEFT;
        case SDL_BUTTON_MIDDLE: return MouseEvent::Button::MIDDLE;
        case SDL_BUTTON_RIGHT:  return MouseEvent::Button::RIGHT;
        default:                return MouseEvent::Button::NONE;
    }
}

static MouseEvent::Button ButtonFromState(SDL_MouseButtonFlags state) {
    if(state & SDL_BUTTON_LMASK) return MouseEvent::Button::LEFT;
    if(state & SDL_BUTTON_MMASK) return MouseEvent::Button::MIDDLE;
    if(state & SDL_BUTTON_RMASK) return MouseEvent::Button::RIGHT;
    return MouseEvent::Button::NONE;
}

// Forwards an SDL event to the graphics window unless ImGui owns the mouse/keyboard.
static void ForwardToMainWindow(const SDL_Event &ev) {
    WindowImplSdl *w = MainWindow();
    if(!w) return;
    ImGuiIO &io = ImGui::GetIO();
    switch(ev.type) {
        case SDL_EVENT_MOUSE_MOTION: {
            if(io.WantCaptureMouse) return;
            MouseEvent me = MakeMouseEvent(MouseEvent::Type::MOTION, ev.motion.x, ev.motion.y);
            me.button = ButtonFromState(ev.motion.state);
            if(w->onMouseEvent) w->onMouseEvent(me);
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            if(io.WantCaptureMouse) return;
            MouseEvent::Type t = ev.type == SDL_EVENT_MOUSE_BUTTON_UP ? MouseEvent::Type::RELEASE
                               : ev.button.clicks >= 2 ? MouseEvent::Type::DBL_PRESS
                                                       : MouseEvent::Type::PRESS;
            MouseEvent me = MakeMouseEvent(t, ev.button.x, ev.button.y);
            me.button = ButtonFromSdl(ev.button.button);
            if(w->onMouseEvent) w->onMouseEvent(me);
            break;
        }
        case SDL_EVENT_MOUSE_WHEEL: {
            if(io.WantCaptureMouse) return;
            float mx, my;
            SDL_GetMouseState(&mx, &my);
            MouseEvent me = MakeMouseEvent(MouseEvent::Type::SCROLL_VERT, mx, my);
            me.scrollDelta = ev.wheel.y;
            if(w->onMouseEvent) w->onMouseEvent(me);
            break;
        }
        case SDL_EVENT_WINDOW_MOUSE_LEAVE: {
            MouseEvent me = MakeMouseEvent(MouseEvent::Type::LEAVE, 0, 0);
            if(w->onMouseEvent) w->onMouseEvent(me);
            break;
        }
        case SDL_EVENT_KEY_DOWN: {
            if(io.WantCaptureKeyboard) return;
            RouteKeyDown(ev);
            break;
        }
        default: break;
    }
}

static void HandleEvent(const SDL_Event &ev) {
    gNeedsFrame = true;
    if(ev.type == gTimerEvent) {
        HandleTimerEvent(ev);
        return;
    }
    ImGui_ImplSDL3_ProcessEvent(&ev);
    switch(ev.type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
            WindowImplSdl *w = MainWindow();
            if(w && w->onClose) w->onClose(); else gQuit = true;
            return;
        }
        default:
            ForwardToMainWindow(ev);
    }
}

static void PumpEventsOnce(bool wait) {
    SDL_Event ev;
    bool got = wait && !gNeedsFrame ? SDL_WaitEventTimeout(&ev, 500) : SDL_PollEvent(&ev);
    while(got) {
        HandleEvent(ev);
        got = SDL_PollEvent(&ev);
    }
}

static void DrawEditorOverlay(WindowImplSdl *w, ImVec2 origin);   // below
static void DrawTooltip(WindowImplSdl *w, ImVec2 origin);

static void RenderFrame() {
    if(!gSdlWindow) return;
    gNeedsFrame = false;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    WindowImplSdl *main = MainWindow();
    DrawMainMenuBar();
    DrawToolWindows();
    if(main) {
        DrawEditorOverlay(main, ImVec2(0, 0));
        DrawTooltip(main, ImVec2(0, 0));
        if(!ImGui::GetIO().WantCaptureMouse) {
            ImGui::SetMouseCursor(main->cursor == Window::Cursor::HAND ? ImGuiMouseCursor_Hand
                                                                       : ImGuiMouseCursor_Arrow);
        }
    }
    ImGui::Render();

    int pw, ph;
    SDL_GetWindowSizeInPixels(gSdlWindow, &pw, &ph);
    if(main && main->visible && main->onRender) {
        main->onRender();          // renderer sets its own viewport
    } else {
        glViewport(0, 0, pw, ph);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    glViewport(0, 0, pw, ph);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(gSdlWindow);
}

static void DrawEditorOverlay(WindowImplSdl *w, ImVec2 origin) {
    if(!w->editorVisible) return;
    // Mirror guigtk.cpp WindowImplGtk::ShowEditor placement: the core passes the text
    // baseline-left; the entry's top is y - fontHeight. Verify against GTK and adjust.
    ImGui::SetNextWindowPos(ImVec2(origin.x + (float)w->editorX,
                                   origin.y + (float)(w->editorY - w->editorFontHeight)));
    ImGui::SetNextWindowSize(ImVec2((float)std::max(w->editorMinWidth, 120.0), 0));
    ImGui::Begin("##editor", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_AlwaysAutoResize);
    if(w->editorFocusPending) { ImGui::SetKeyboardFocusHere(); w->editorFocusPending = false; }
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s", w->editorText.c_str());
    ImGui::SetNextItemWidth(-1);
    if(ImGui::InputText("##text", buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
        w->editorVisible = false;
        if(w->onEditingDone) w->onEditingDone(buf);
    } else {
        w->editorText = buf;
        if(ImGui::IsKeyPressed(ImGuiKey_Escape)) w->editorVisible = false;
    }
    ImGui::End();
}

static void DrawTooltip(WindowImplSdl *w, ImVec2 origin) {
    if(w->tooltipText.empty()) return;
    ImVec2 m = ImGui::GetIO().MousePos;
    float x = m.x - origin.x, y = m.y - origin.y;
    if(x >= w->tooltipX && x <= w->tooltipX + w->tooltipW &&
       y >= w->tooltipY && y <= w->tooltipY + w->tooltipH) {
        ImGui::SetTooltip("%s", w->tooltipText.c_str());
    }
}

void RunGui() {
    while(!gQuit) {
        PumpEventsOnce(/*wait=*/true);
        if(gNeedsFrame) RenderFrame();
    }
}

void ExitGui() {
    gQuit = true;
}

void ClearGui() {
    if(gSdlWindow) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        SDL_GL_DestroyContext(gGlContext);
        SDL_DestroyWindow(gSdlWindow);
        gSdlWindow = nullptr;
    }
    std::static_pointer_cast<SettingsFile>(GetSettings())->Save();
    SDL_Quit();
}

// Filled in by later tasks.
static void DrawToolWindows() {}
static void DrawMainMenuBar() {}
static bool RouteKeyDown(const SDL_Event &ev) {
    // Until Task 10: translate to the legacy KeyboardEvent so core accelerators work.
    WindowImplSdl *w = MainWindow();
    if(!w || !w->onKeyboardEvent) return false;
    KeyboardEvent ke = {};
    ke.type        = KeyboardEvent::Type::PRESS;
    ke.shiftDown   = (ev.key.mod & SDL_KMOD_SHIFT) != 0;
    ke.controlDown = (ev.key.mod & SDL_KMOD_CTRL) != 0;
    if(ev.key.key >= SDLK_F1 && ev.key.key <= SDLK_F12) {
        ke.key = KeyboardEvent::Key::FUNCTION;
        ke.num = 1 + (int)(ev.key.key - SDLK_F1);
    } else if(ev.key.key < 0x80) {
        ke.key = KeyboardEvent::Key::CHARACTER;
        ke.chr = (char32_t)ev.key.key;
    } else {
        return false;
    }
    return w->onKeyboardEvent(ke);
}

}
}
```
The GL calls (`glViewport`, `glClear`, `glClearColor`) need the same headers as the renderer: add `#include "render/gl3shader.h"` after `solvespace.h`, which pulls in the right GL/GLES headers per platform. Also add `#include <atomic>` and `#include <mutex>`.

- [ ] **Step 6: Add the executable target**

In `src/CMakeLists.txt`, inside the `if(ENABLE_GUI AND ENABLE_NG_GUI)` block, after `solvespace-ng-support`:
```cmake
    add_executable(solvespace-ng WIN32 MACOSX_BUNDLE
        ${solvespace_core_gl_SOURCES}
        platform/entrygui.cpp
        platform/guisdl.cpp
        render/gl3shader.cpp
        render/rendergl3.cpp
        $<TARGET_PROPERTY:resources,EXTRA_SOURCES>)
    add_dependencies(solvespace-ng resources)
    target_link_libraries(solvespace-ng PRIVATE
        solvespace-core solvespace-ng-support imgui-sdl3 ${NG_SDL_TARGET} ${OPENGL_LIBRARIES})
```
`entrygui.cpp` provides `main()` and calls `InitGui`/`RunGui`/`ClearGui`, so no new entry point is needed. On Windows it defines `WinMain` calling `main(0, NULL)`. SDL3 doesn't hijack `main` unless `SDL_main.h` is included, and we don't include it.

- [ ] **Step 7: Build and smoke-test by hand**

```bash
cd build && cmake .. -DENABLE_NG_GUI=ON && make -j$(nproc) solvespace-ng && ./bin/solvespace-ng ../test/request/line_segment/normal.slvs
```
Expected (verify each, and note results in the commit message):
1. A window opens showing the line from the fixture on SolveSpace's usual background, with the old toolbar on the left.
2. Middle-drag orbits, wheel zooms, and left-drag on a point moves it.
3. Classic keys work through the temporary `RouteKeyDown`: `s` starts a line segment, `Esc` cancels, `Ctrl+Z` undoes.
4. Double-clicking a dimension opens the ImGui editor at roughly the right place. Enter applies, Esc cancels.
5. Closing the window prompts "save changes?" through an SDL message box when the model is modified.
6. Ctrl+S on a new sketch opens the native save dialog. Typing a name without an extension saves `<name>.slvs`.

If the editor overlay sits a line too high or low, open `src/platform/guigtk.cpp`, find `ShowEditor`, copy its exact placement arithmetic into `DrawEditorOverlay`, and replace the comment with a reference to it.

If the window is black on Wayland, run with `SDL_VIDEO_DRIVER=x11` to isolate it. Report to the owner whichever driver works. A Wayland fix may need `SDL_HINT_VIDEO_WAYLAND_PREFER_LIBDECOR` or an EGL compatibility-profile tweak.

- [ ] **Step 8: Run tests and commit**

```bash
make test_solvespace
git add src/platform/guisdl.cpp src/CMakeLists.txt
git commit -m "NG: SDL3/ImGui platform backend; solvespace-ng runs the existing app."
```

---

### Task 8: Property browser as a floating panel

**Files:**
- Modify: `src/platform/guisdl.cpp` (replace the empty `DrawToolWindows`; add FBO helpers)

**Interfaces:**
- Consumes: `WindowImplSdl` fields `fbo`, `fboColor`, `fboDepth`, `fboWidth`, `fboHeight`, `scroll*`, `editor*` (Task 7).
- Produces: the TOOL window renders into an offscreen framebuffer, shown in the ImGui window titled with the window's `title` (default "Property Browser"). Mouse and scroll events in the image go to the TW's `onMouseEvent` / `onScrollbarAdjusted`.

- [ ] **Step 1: Add the framebuffer helper (in `guisdl.cpp`, above `RenderFrame`)**

```cpp
#if defined(__APPLE__)
#   define glGenFramebuffers         glGenFramebuffersEXT
#   define glBindFramebuffer         glBindFramebufferEXT
#   define glFramebufferTexture2D    glFramebufferTexture2DEXT
#   define glGenRenderbuffers        glGenRenderbuffersEXT
#   define glBindRenderbuffer        glBindRenderbufferEXT
#   define glRenderbufferStorage     glRenderbufferStorageEXT
#   define glFramebufferRenderbuffer glFramebufferRenderbufferEXT
#   define glDeleteFramebuffers      glDeleteFramebuffersEXT
#   define glDeleteRenderbuffers     glDeleteRenderbuffersEXT
#   define GL_FRAMEBUFFER            GL_FRAMEBUFFER_EXT
#   define GL_RENDERBUFFER           GL_RENDERBUFFER_EXT
#   define GL_COLOR_ATTACHMENT0      GL_COLOR_ATTACHMENT0_EXT
#   define GL_DEPTH_ATTACHMENT       GL_DEPTH_ATTACHMENT_EXT
#endif

static void EnsureFramebuffer(WindowImplSdl *w, int pw, int ph) {
    if(w->fbo && w->fboWidth == pw && w->fboHeight == ph) return;
    if(w->fbo) {
        glDeleteFramebuffers(1, &w->fbo);
        glDeleteTextures(1, &w->fboColor);
        glDeleteRenderbuffers(1, &w->fboDepth);
    }
    glGenTextures(1, &w->fboColor);
    glBindTexture(GL_TEXTURE_2D, w->fboColor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pw, ph, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenRenderbuffers(1, &w->fboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, w->fboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, pw, ph);
    glGenFramebuffers(1, &w->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, w->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, w->fboColor, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, w->fboDepth);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    w->fboWidth = pw;
    w->fboHeight = ph;
}

static void RenderToolWindowsOffscreen() {
    for(auto &weak : gToolWindows) {
        auto w = weak.lock();
        if(!w || !w->visible || !w->onRender) continue;
        double ratio = w->GetDevicePixelRatio();
        int pw = std::max(1, (int)(w->width * ratio)), ph = std::max(1, (int)(w->height * ratio));
        EnsureFramebuffer(w.get(), pw, ph);
        glBindFramebuffer(GL_FRAMEBUFFER, w->fbo);
        w->onRender();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}
```
In `RenderFrame`, call `RenderToolWindowsOffscreen();` right after `ImGui::Render();` and before the main window's `onRender`. The ImGui draw lists only sample the texture when `ImGui_ImplOpenGL3_RenderDrawData` runs, so this ordering shows the current frame's content.

- [ ] **Step 2: Replace `DrawToolWindows`**

```cpp
static void DrawToolWindows() {
    for(auto it = gToolWindows.begin(); it != gToolWindows.end();) {
        auto w = it->lock();
        if(!w) { it = gToolWindows.erase(it); continue; }
        ++it;
        if(!w->visible) continue;

        ImGui::SetNextWindowSize(ImVec2(440, 680), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 460, 40),
                                ImGuiCond_FirstUseEver);
        bool open = true;
        std::string name = (w->title.empty() ? std::string("Property Browser") : w->title) +
                           "###toolwindow";
        ImGui::Begin(name.c_str(), &open, ImGuiWindowFlags_NoScrollbar |
                                          ImGuiWindowFlags_NoScrollWithMouse);
        ImVec2 avail  = ImGui::GetContentRegionAvail();
        float  sbW    = w->scrollbarVisible ? 14.0f : 0.0f;
        double newW = std::max(1.0f, avail.x - sbW), newH = std::max(1.0f, avail.y);
        if(newW != w->width || newH != w->height) { w->width = newW; w->height = newH; }

        ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::Image((ImTextureID)(intptr_t)w->fboColor, ImVec2((float)w->width, (float)w->height),
                     ImVec2(0, 1), ImVec2(1, 0));   // flip: GL origin is bottom-left
        bool hovered = ImGui::IsItemHovered();

        // Synthesize Platform mouse events for the image area.
        ImGuiIO &io = ImGui::GetIO();
        double lx = io.MousePos.x - origin.x, ly = io.MousePos.y - origin.y;
        auto send = [&](MouseEvent::Type type, MouseEvent::Button button) {
            MouseEvent me = MakeMouseEvent(type, lx, ly);
            me.button = button;
            if(w->onMouseEvent) w->onMouseEvent(me);
        };
        if(hovered) {
            if(io.MouseDelta.x != 0 || io.MouseDelta.y != 0) {
                send(MouseEvent::Type::MOTION, io.MouseDown[0] ? MouseEvent::Button::LEFT
                                                               : MouseEvent::Button::NONE);
            }
            for(int b = 0; b < 3; b++) {
                MouseEvent::Button btn = b == 0 ? MouseEvent::Button::LEFT
                                       : b == 1 ? MouseEvent::Button::RIGHT
                                                : MouseEvent::Button::MIDDLE;
                if(ImGui::IsMouseDoubleClicked(b)) send(MouseEvent::Type::DBL_PRESS, btn);
                else if(ImGui::IsMouseClicked(b)) send(MouseEvent::Type::PRESS, btn);
                if(ImGui::IsMouseReleased(b)) send(MouseEvent::Type::RELEASE, btn);
            }
            if(io.MouseWheel != 0 && w->scrollbarVisible) {
                w->SetScrollbarPosition(w->scrollPos - io.MouseWheel * 3);
                if(w->onScrollbarAdjusted) w->onScrollbarAdjusted(w->scrollPos);
            }
            ImGui::SetMouseCursor(w->cursor == Window::Cursor::HAND ? ImGuiMouseCursor_Hand
                                                                    : ImGuiMouseCursor_Arrow);
            w->wasHovered = true;
        } else if(w->wasHovered) {
            // Mouse left the image: let the core clear its hover highlight once.
            send(MouseEvent::Type::LEAVE, MouseEvent::Button::NONE);
            w->wasHovered = false;
        }

        if(w->scrollbarVisible) {
            ImGui::SameLine(0, 0);
            float maxStart = (float)std::max(w->scrollMin, w->scrollMax - w->scrollPage);
            float inverted = maxStart - (float)w->scrollPos;   // slider bottom = scrolled down
            ImGui::PushID("scroll");
            if(ImGui::VSliderFloat("##sb", ImVec2(sbW, (float)w->height), &inverted,
                                   (float)w->scrollMin, maxStart, "")) {
                w->SetScrollbarPosition(maxStart - inverted);
                if(w->onScrollbarAdjusted) w->onScrollbarAdjusted(w->scrollPos);
            }
            ImGui::PopID();
        }

        DrawEditorOverlay(w.get(), origin);
        DrawTooltip(w.get(), origin);
        ImGui::End();

        if(!open) {
            if(w->onClose) w->onClose(); else w->visible = false;
        }
    }
}
```
Add the field `bool wasHovered = false;` to `WindowImplSdl` (next to the scrollbar fields) for the leave detection above.

- [ ] **Step 3: Manual verification**

`./bin/solvespace-ng ../test/request/line_segment/normal.slvs`:
1. A "Property Browser" panel floats at the top right showing the group list exactly as the old text window does.
2. Clicking a group's hyperlink (e.g. "show"/"hide" or a group name) behaves as in the GTK build.
3. Wheel scrolling works when the list is long. Use a bigger model, e.g. `test/group/boolean_tangent_fillet/*.slvs`.
4. Double-clicking an editable value (e.g. extrude depth on a group screen) opens the editor *inside the panel*, at the value.
5. Closing the panel with its × hides it. "Show Property Browser" (Tab in Classic) brings it back.
6. Resizing the panel reflows the text, and on HiDPI the text is crisp, not blurry.

- [ ] **Step 4: Commit**

```bash
make test_solvespace
git add src/platform/guisdl.cpp
git commit -m "NG: property browser rendered offscreen into a floating ImGui panel."
```

---

### Task 9: ImGui menu bar and context menus

**Files:**
- Modify: `src/platform/guisdl.cpp` (replace the empty `DrawMainMenuBar`; implement `MenuSdl::onPopUp` with a nested loop; set it in `CreateMenu`)

**Interfaces:**
- Consumes: `MenuBarSdl`, `MenuSdl`, `MenuItemSdl` (Task 5); `RunNestedLoop` (Task 7).
- Produces: `static bool gShowMenuBar` (default `true`; persisted as `NG_ShowMenuBar`; toggled by `ui.toggle-menu-bar` in Task 10); `static std::string ShortcutTextFor(const MenuItemSdl &)`. Until Task 10 this shows the legacy accelerator; Task 10 switches it to the active keymap.

- [ ] **Step 1: Shared menu-entry renderer + menu bar**

Add above `DrawToolWindows`:
```cpp
static bool gShowMenuBar = true;

static std::string ShortcutTextFor(const MenuItemSdl &item) {
    return item.hasAccel ? AcceleratorDescription(item.accel) : std::string();
}

// Returns true if an item was activated (so popups can close).
static bool DrawMenuEntries(const MenuSdl &menu) {
    bool activated = false;
    for(const MenuSdl::Entry &e : menu.entries) {
        switch(e.kind) {
            case MenuSdl::Entry::Kind::SEPARATOR:
                ImGui::Separator();
                break;
            case MenuSdl::Entry::Kind::SUBMENU:
                if(ImGui::BeginMenu(e.label.c_str())) {
                    activated |= DrawMenuEntries(*e.submenu);
                    ImGui::EndMenu();
                }
                break;
            case MenuSdl::Entry::Kind::ITEM: {
                const MenuItemSdl &item = *e.item;
                std::string shortcut = ShortcutTextFor(item);
                bool checked = item.indicator != MenuItem::Indicator::NONE && item.active;
                if(ImGui::MenuItem(item.label.c_str(), shortcut.empty() ? nullptr : shortcut.c_str(),
                                   checked, item.enabled)) {
                    if(item.onTrigger) item.onTrigger();
                    activated = true;
                }
                break;
            }
        }
    }
    return activated;
}

static void DrawMainMenuBar() {
    WindowImplSdl *main = MainWindow();
    if(!gShowMenuBar || !main || !main->menuBar) return;
    auto bar = std::static_pointer_cast<MenuBarSdl>(main->menuBar);
    if(ImGui::BeginMainMenuBar()) {
        for(auto &m : bar->menus) {
            if(ImGui::BeginMenu(m.first.c_str())) {
                DrawMenuEntries(*m.second);
                ImGui::EndMenu();
            }
        }
        ImGui::EndMainMenuBar();
    }
}
```
Delete the placeholder `static void DrawMainMenuBar() {}` at the bottom of the file.

- [ ] **Step 2: Context menus via a nested loop**

```cpp
static MenuSdl *gActivePopup = nullptr;
static bool     gPopupOpened = false;

static void DrawActivePopup() {
    if(!gActivePopup) return;
    if(!gPopupOpened) { ImGui::OpenPopup("##context"); gPopupOpened = true; }
    if(ImGui::BeginPopup("##context")) {
        if(DrawMenuEntries(*gActivePopup)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    } else {
        gActivePopup = nullptr;   // dismissed (click outside / Esc) or item chosen
    }
}

MenuRef CreateMenu() {
    auto menu = std::make_shared<MenuSdl>();
    menu->onPopUp = [](MenuSdl *m) {
        gActivePopup = m;
        gPopupOpened = false;
        gNeedsFrame = true;
        RunNestedLoop([] { return gActivePopup == nullptr; });
        m->popupRequested = false;
    };
    return menu;
}
```
Replace the Task 7 `CreateMenu` with this one. Call `DrawActivePopup();` in `RenderFrame` right after `DrawToolWindows();`.

While a popup is active, the graphics window must not receive mouse events. ImGui sets `WantCaptureMouse` while a popup is open, which `ForwardToMainWindow` already respects.

- [ ] **Step 3: Manual verification**

1. The menu bar shows File/Edit/View/New Group/Sketch/Constrain/Analyze/Help with checkmarks (View › Show Snap Grid) and greyed items (e.g. Undo with nothing to undo).
2. File › Open Recent lists recent files.
3. Right-click on a line → the context menu appears at the cursor. Choosing "Delete" deletes it. Esc or a click elsewhere dismisses it without action.
4. Help › Language submenu switches the UI language live.

- [ ] **Step 4: Commit**

```bash
make test_solvespace
git add src/platform/guisdl.cpp
git commit -m "NG: ImGui main menu bar and context menus."
```

---

### Task 10: Key routing through the keymap

**Files:**
- Create: `src/ui/keyrouter.{h,cpp}`; Modify: `src/CMakeLists.txt` (add `ui/keyrouter.cpp` to `solvespace-ng-support`)
- Modify: `src/platform/guisdl.cpp` (replace `RouteKeyDown`; shortcut text from the keymap)
- Create: `test/ng/keyrouter/test.cpp`; Modify: `test/CMakeLists.txt`

**Interfaces:**
- Consumes: `ClassicPreset`, `ModernPreset` (Task 6); `ChordFromKey` (Task 6); `CommandFromName`, `CommandName` (M0); `SettingsFile` (Task 4).
- Produces:
  ```cpp
  namespace SolveSpace::Ui {
  enum class RouteResult { UNBOUND, UI_ACTION, COMMAND, SUPPRESSED };
  class KeyRouter {
  public:
      std::function<void(const std::string &uiAction)> onUiAction; // "ui.command-palette" ...
      std::function<void(Command)>                     onCommand;  // defaults to SS.GW.ActivateCommand
      void SetKeymap(keymap::Keymap km);
      const keymap::Keymap &Keymap() const;
      RouteResult Route(const keymap::Chord &chord, bool textInputActive);
  };
  // Loads the user's keymap: preset from setting "NG_KeymapPreset" ("modern"|"classic", default
  // "modern"), then, if <prefdir>/keymap.txt exists and parses, that file replaces the preset.
  // On a parse error, logs it (dbp) and returns the preset; *error receives the message.
  keymap::Keymap LoadUserKeymap(const std::string &preset, const std::string &userFileText,
                                std::string *error);
  }
  ```
- Rules:
  - When `textInputActive`, only chords with Ctrl or Alt, or the keys `Esc`, `F1`–`F24`, are routed. Everything else returns `SUPPRESSED`, so typing never triggers commands.
  - Chords that map to unknown command names return `UNBOUND` (and are logged once).

- [ ] **Step 1: Failing tests**

`test/ng/keyrouter/test.cpp`:
```cpp
#include "harness.h"
#include "ui/keyrouter.h"

using namespace SolveSpace;
using namespace SolveSpace::Ui;

static keymap::Chord C(const char *s) { return *keymap::ParseChord(s); }

TEST_CASE(routes_commands_and_ui_actions) {
    std::string err;
    KeyRouter r;
    r.SetKeymap(*keymap::Keymap::Parse("line-segment: L\nui.command-palette: Space\n", &err));
    Command got = Command::NONE;
    std::string action;
    r.onCommand  = [&](Command c) { got = c; };
    r.onUiAction = [&](const std::string &a) { action = a; };
    CHECK_TRUE(r.Route(C("L"), false) == RouteResult::COMMAND);
    CHECK_TRUE(got == Command::LINE_SEGMENT);
    CHECK_TRUE(r.Route(C("Space"), false) == RouteResult::UI_ACTION);
    CHECK_EQ_STR(action, "ui.command-palette");
    CHECK_TRUE(r.Route(C("Q"), false) == RouteResult::UNBOUND);
}

TEST_CASE(text_input_swallows_chords) {
    std::string err;
    KeyRouter r;
    r.SetKeymap(*keymap::Keymap::Parse("line-segment: L\nsave: Ctrl+S\nunselect-all: Esc\n", &err));
    int fired = 0;
    r.onCommand = [&](Command) { fired++; };
    CHECK_TRUE(r.Route(C("L"), true) == RouteResult::SUPPRESSED);
    CHECK_TRUE(fired == 0);
    CHECK_TRUE(r.Route(C("Ctrl+S"), true) == RouteResult::COMMAND);
    CHECK_TRUE(r.Route(C("Esc"), true) == RouteResult::COMMAND);
    CHECK_TRUE(fired == 2);
}

TEST_CASE(unknown_command_names_are_unbound) {
    std::string err;
    KeyRouter r;
    r.SetKeymap(*keymap::Keymap::Parse("not-a-command: K\n", &err));
    CHECK_TRUE(r.Route(C("K"), false) == RouteResult::UNBOUND);
}

TEST_CASE(bad_user_keymap_falls_back_to_preset) {
    std::string err;
    keymap::Keymap km = LoadUserKeymap("modern", "save: Ctrl+S\nbroken\n", &err);
    CHECK_TRUE(err.find("line 2") != std::string::npos);
    CHECK_TRUE(*km.Lookup(C("Shift+C")) == "arc");   // the modern preset, not the user file
    err.clear();
    km = LoadUserKeymap("classic", "", &err);
    CHECK_TRUE(err.empty());
    CHECK_TRUE(*km.Lookup(C("S")) == "line-segment");
    km = LoadUserKeymap("modern", "undo: F9\n", &err);
    CHECK_TRUE(*km.Lookup(C("F9")) == "undo");
    CHECK_TRUE(km.Lookup(C("Shift+C")) == nullptr);  // a valid user file replaces the preset
}
```
Add to the NG test sources.

- [ ] **Step 2: Verify failure.**

- [ ] **Step 3: Implement**

`src/ui/keyrouter.h`: as in Interfaces, with includes `<functional>`, `"keymap.h"`, `"solvespace.h"`.

`src/ui/keyrouter.cpp`:
```cpp
#include "ui/keyrouter.h"
#include "ui/keypresets.h"
#include "commandnames.h"

namespace SolveSpace {
namespace Ui {

void KeyRouter::SetKeymap(keymap::Keymap km) { map = std::move(km); }
const keymap::Keymap &KeyRouter::Keymap() const { return map; }

static bool AllowedDuringTextInput(const keymap::Chord &c) {
    if(c.ctrl || c.alt) return true;
    if(c.key == "Esc") return true;
    return c.key.size() >= 2 && c.key[0] == 'F' && isdigit((unsigned char)c.key[1]);
}

RouteResult KeyRouter::Route(const keymap::Chord &chord, bool textInputActive) {
    if(textInputActive && !AllowedDuringTextInput(chord)) return RouteResult::SUPPRESSED;
    const std::string *name = map.Lookup(chord);
    if(!name) return RouteResult::UNBOUND;
    if(name->rfind("ui.", 0) == 0) {
        if(onUiAction) onUiAction(*name);
        return RouteResult::UI_ACTION;
    }
    Command cmd = CommandFromName(*name);
    if(cmd == Command::NONE) {
        dbp("keymap: unknown command '%s'", name->c_str());
        return RouteResult::UNBOUND;
    }
    if(onCommand) onCommand(cmd); else SS.GW.ActivateCommand(cmd);
    return RouteResult::COMMAND;
}

keymap::Keymap LoadUserKeymap(const std::string &preset, const std::string &userFileText,
                              std::string *error) {
    keymap::Keymap base;
    if(preset == "classic") {
        base = ClassicPreset();
    } else {
        std::string presetError;
        auto modern = ModernPreset(&presetError);
        ssassert(modern.has_value(), "built-in modern keymap must parse");
        base = *modern;
    }
    if(userFileText.empty()) return base;
    std::string parseError;
    auto user = keymap::Keymap::Parse(userFileText, &parseError);
    if(!user) {
        std::string message = "keymap.txt " + parseError;
        dbp("%s; using the %s preset", message.c_str(), preset.c_str());
        if(error) *error = message;
        return base;
    }
    return *user;
}

}
}
```
Add a private member `keymap::Keymap map;` to `KeyRouter` in the header.

- [ ] **Step 4: Wire it into the backend**

In `guisdl.cpp` add includes `"platform/sdl/keychord.h"`, `"ui/keyrouter.h"`, and:
```cpp
static Ui::KeyRouter gKeyRouter;
static std::function<void(const std::string &)> gUiActionHandler; // set by ngui (Task 11)

static void LoadKeymap() {
    SettingsRef settings = GetSettings();
    std::string preset = settings->ThawString("NG_KeymapPreset", "modern");
    char *pref = SDL_GetPrefPath("solvespace", "solvespace-ng");
    std::string text;
    ReadFile(Path::From(pref ? pref : ".").Join("keymap.txt"), &text);
    SDL_free(pref);
    std::string error;
    gKeyRouter.SetKeymap(Ui::LoadUserKeymap(preset, text, &error));
    if(!error.empty()) Error("%s", error.c_str());   // SolveSpace's user-facing error box
    gKeyRouter.onUiAction = [](const std::string &a) {
        if(a == "ui.toggle-menu-bar") {
            gShowMenuBar = !gShowMenuBar;
            GetSettings()->FreezeBool("NG_ShowMenuBar", gShowMenuBar);
        } else if(gUiActionHandler) {
            gUiActionHandler(a);
        }
    };
}
```
Call `LoadKeymap();` at the end of `InitSdlWindowAndImGui()`, and set `gShowMenuBar = GetSettings()->ThawBool("NG_ShowMenuBar", true);` there too.

The core `Error()` is in `src/util.cpp` (`void Error(const char *fmt, ...)`). Grep to confirm the exact signature. If showing a dialog during startup misbehaves, defer it with a one-shot `CreateTimer()->RunAfter(0)`.

Replace `RouteKeyDown`:
```cpp
static bool RouteKeyDown(const SDL_Event &ev) {
    SDL_Keycode base     = SDL_GetKeyFromScancode(ev.key.scancode, SDL_KMOD_NONE, false);
    SDL_Keycode produced = SDL_GetKeyFromScancode(ev.key.scancode, ev.key.mod, false);
    auto chord = ChordFromKey(base, produced, ev.key.mod);
    if(!chord) return false;
    bool textInput = ImGui::GetIO().WantTextInput;
    return gKeyRouter.Route(*chord, textInput) != Ui::RouteResult::UNBOUND;
}
```
In `ForwardToMainWindow`'s `SDL_EVENT_KEY_DOWN` case, remove the `io.WantCaptureKeyboard` early return so `Route` can apply its own text-input rule. Keep returning early when `ev.key.repeat` is set and the chord has no modifiers, so auto-repeat doesn't start ten line tools.

Replace `ShortcutTextFor`:
```cpp
static std::string ShortcutTextFor(const MenuItemSdl &item) {
    if(item.commandId == 0) return "";
    const char *name = CommandName((Command)item.commandId);
    if(!name) return "";
    auto chords = gKeyRouter.Keymap().ChordsFor(name);
    return chords.empty() ? "" : keymap::FormatChord(chords[0]);
}
```

- [ ] **Step 5: Run tests; manual check**

`./bin/solvespace-testsuite ng/keyrouter` → `0 failed`. Then launch `solvespace-ng`:
1. With the Modern preset (default): `L` starts a line, `C` a circle, `Shift+C` an arc, `A` selects all, `Esc` deselects, `N` toggles the property browser, `F10` hides and shows the menu bar.
2. Menu items show the Modern shortcuts (Sketch › Line Segment shows `L`).
3. While editing a dimension value, typing `L` types "l" and does not start a line.
4. Set Classic: write `NG_KeymapPreset=classic` into `<prefdir>/settings.txt` (on Linux `~/.local/share/solvespace/solvespace-ng/settings.txt`), relaunch, and confirm `s` starts a line segment.
5. Put a deliberately broken `keymap.txt` in the pref dir and relaunch. An error box names the line, and the app runs on the preset.

- [ ] **Step 6: Commit**

```bash
make test_solvespace
git add src/ui/keyrouter.* src/platform/guisdl.cpp src/CMakeLists.txt test/ng/keyrouter test/CMakeLists.txt
git commit -m "NG: route all keys through the keymap (Modern default, Classic preset, user keymap.txt)."
```

---

### Task 11: Command palette

**Files:**
- Create: `src/ui/palettewidget.{h,cpp}`, `src/ui/ngui.{h,cpp}`; Modify: `src/CMakeLists.txt` (add both to the `solvespace-ng` executable sources, since they use ImGui. Don't add them to the support lib)
- Modify: `src/platform/guisdl.cpp` (call `Ui::DrawNgUi`, set `gUiActionHandler`)

**Interfaces:**
- Consumes: `FlattenMenuBar` (Task 5); `palette::Rank` (Task 3); `KeyRouter::Keymap()` (Task 10).
- Produces:
  ```cpp
  namespace SolveSpace::Ui {
  // ngui.h
  void InitNgUi();                                   // after ImGui context creation
  void DrawNgUi(Platform::MenuBarSdl *menuBar, const keymap::Keymap &keymap);
  void HandleUiAction(const std::string &action);    // "ui.command-palette", "ui.toggle-theme"
  // palettewidget.h
  class CommandPalette {
  public:
      void Open();
      bool IsOpen() const;
      // Draws if open. Activates the chosen item's onTrigger (only if enabled) and closes.
      void Draw(const std::vector<Platform::FlatMenuItem> &items, const keymap::Keymap &keymap);
  };
  }
  ```

The palette's ranking logic is already covered by Task 3's tests. The widget is drawn UI and is verified by the manual checklist. Keep the widget thin and put any new logic in a testable function.

- [ ] **Step 1: Implement `palettewidget.cpp`**

```cpp
#include "solvespace.h"
#include "commandnames.h"
#include "fuzzy.h"
#include "imgui.h"
#include "ui/palettewidget.h"

namespace SolveSpace {
namespace Ui {

void CommandPalette::Open() { open = true; justOpened = true; query[0] = '\0'; selected = 0; }
bool CommandPalette::IsOpen() const { return open; }

void CommandPalette::Draw(const std::vector<Platform::FlatMenuItem> &items,
                          const keymap::Keymap &keymap) {
    if(!open) return;
    ImGuiIO &io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.2f),
                            ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(std::min(640.0f, io.DisplaySize.x - 32), 0));
    ImGui::Begin("##palette", nullptr, ImGuiWindowFlags_NoDecoration |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
    if(justOpened) { ImGui::SetKeyboardFocusHere(); justOpened = false; }
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##q", "Type a command…", query, sizeof(query));

    std::vector<std::string> labels;
    for(const auto &it : items) labels.push_back(it.path);
    std::vector<size_t> ranked = palette::Rank(query, labels, 12);
    if(selected >= (int)ranked.size()) selected = std::max(0, (int)ranked.size() - 1);
    if(ImGui::IsKeyPressed(ImGuiKey_DownArrow)) selected = std::min(selected + 1, (int)ranked.size() - 1);
    if(ImGui::IsKeyPressed(ImGuiKey_UpArrow))   selected = std::max(selected - 1, 0);

    int activate = -1;
    for(int i = 0; i < (int)ranked.size(); i++) {
        const Platform::FlatMenuItem &fi = items[ranked[i]];
        std::string shortcut;
        if(fi.item->commandId) {
            if(const char *name = CommandName((Command)fi.item->commandId)) {
                auto chords = keymap.ChordsFor(name);
                if(!chords.empty()) shortcut = keymap::FormatChord(chords[0]);
            }
        }
        ImGui::BeginDisabled(!fi.item->enabled);
        if(ImGui::Selectable(fi.path.c_str(), i == selected)) activate = i;
        if(!shortcut.empty()) {
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(shortcut.c_str()).x);
            ImGui::TextDisabled("%s", shortcut.c_str());
        }
        ImGui::EndDisabled();
    }
    if(ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
        activate = selected;
    }
    bool close = ImGui::IsKeyPressed(ImGuiKey_Escape) ||
                 (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !justOpened);
    ImGui::End();

    if(activate >= 0 && activate < (int)ranked.size()) {
        const Platform::FlatMenuItem &fi = items[ranked[activate]];
        open = false;
        if(fi.item->enabled && fi.item->onTrigger) fi.item->onTrigger();
    } else if(close) {
        open = false;
    }
}

}
}
```
`palettewidget.h` declares the class with private members `bool open = false, justOpened = false; char query[256] = {}; int selected = 0;`.

- [ ] **Step 2: Implement `ngui.cpp`**

```cpp
#include "solvespace.h"
#include "imgui.h"
#include "ui/ngui.h"
#include "ui/palettewidget.h"

namespace SolveSpace {
namespace Ui {

static CommandPalette gPalette;

void InitNgUi() {
}

void HandleUiAction(const std::string &action) {
    if(action == "ui.command-palette") gPalette.Open();
    else dbp("unknown UI action '%s'", action.c_str());
}

void DrawNgUi(Platform::MenuBarSdl *menuBar, const keymap::Keymap &keymap) {
    if(menuBar && gPalette.IsOpen()) {
        gPalette.Draw(Platform::FlattenMenuBar(*menuBar), keymap);
    }
}

}
}
```
In `guisdl.cpp`: include `"ui/ngui.h"`. At the end of `InitSdlWindowAndImGui`, call `Ui::InitNgUi(); gUiActionHandler = Ui::HandleUiAction;`. In `RenderFrame`, after `DrawActivePopup();`, call:
```cpp
    if(main && main->menuBar) {
        Ui::DrawNgUi(static_cast<MenuBarSdl *>(main->menuBar.get()), gKeyRouter.Keymap());
    }
```

- [ ] **Step 3: Manual verification**

1. `Space` opens a centred palette. Typing `ext` puts "New Group › Extrude" first. `Enter` runs it; it's greyed and inert if no sketch is active.
2. Arrow keys move the selection, and `Esc` or a click outside closes the palette.
3. With the palette open, typing letters never triggers tools (Review Focus 2).
4. Recent files appear as "File › Open Recent › …".

- [ ] **Step 4: Commit**

```bash
make test_solvespace
git add src/ui/palettewidget.* src/ui/ngui.* src/platform/guisdl.cpp src/CMakeLists.txt
git commit -m "NG: command palette over the menu model."
```

---

### Task 12: Theme and fonts

**Files:**
- Create: `res/fonts/ng/Inter-Regular.ttf`, `res/fonts/ng/JetBrainsMono-Regular.ttf`, `res/fonts/ng/OFL-Inter.txt`, `res/fonts/ng/OFL-JetBrainsMono.txt`; Modify: `res/CMakeLists.txt`
- Create: `src/ui/theme.{h,cpp}`; Modify: `src/CMakeLists.txt` (add to the `solvespace-ng` sources)
- Modify: `THIRD_PARTIES.txt` (add Inter, JetBrains Mono, SDL3, Dear ImGui entries)

**Interfaces:**
- Produces:
  ```cpp
  namespace SolveSpace::Ui {
  enum class ThemeChoice { DARK, LIGHT };
  ThemeChoice LoadThemeChoice();          // setting "NG_Theme", default DARK
  void ApplyTheme(ThemeChoice choice);    // loads fonts once, sets ImGuiStyle colours/metrics
  void ToggleTheme();                     // flips, applies, persists
  ImFont *MonoFont();                     // JetBrains Mono, for numbers (used from M1b on)
  }
  ```
- Design tokens:

  | Token | Dark | Light |
  |---|---|---|
  | `bg` | `#16181D` | `#F4F5F7` |
  | `panel` | `#1E2128` (alpha 0.96) | `#FFFFFF` (alpha 0.97) |
  | `panelBorder` | `#2C313A` | `#D9DCE2` |
  | `text` | `#E6E8EC` | `#1C1F24` |
  | `textMuted` | `#8B93A1` | `#6A7280` |
  | `accent` | `#4F8EF7` | `#2F6FE0` |
  | `accentHover` | `#6AA0FA` | `#4A82E8` |
  | `danger` | `#F26D6D` | `#D64545` |

  Metrics: window rounding 10, frame rounding 6, popup rounding 8, window padding 12×10, item spacing 8×6, borders 1px. The base font is 15px Inter; scale by `SDL_GetWindowDisplayScale`.

- [ ] **Step 1: Fetch the fonts (OFL) into a scratch dir, then copy the exact files**

```bash
SCR=$(mktemp -d)
curl -L -o $SCR/inter.zip https://github.com/rsms/inter/releases/download/v4.1/Inter-4.1.zip
curl -L -o $SCR/jbm.zip https://github.com/JetBrains/JetBrainsMono/releases/download/v2.304/JetBrainsMono-2.304.zip
(cd $SCR && unzip -q inter.zip -d inter && unzip -q jbm.zip -d jbm)
find $SCR -name "Inter-Regular.ttf" -o -name "JetBrainsMono-Regular.ttf" -o -name "LICENSE*.txt" -o -name "OFL.txt"
mkdir -p res/fonts/ng
cp "$(find $SCR/inter -name Inter-Regular.ttf | head -1)" res/fonts/ng/
cp "$(find $SCR/jbm -name JetBrainsMono-Regular.ttf | head -1)" res/fonts/ng/
cp "$(find $SCR/inter -iname 'LICENSE*' | head -1)" res/fonts/ng/OFL-Inter.txt
cp "$(find $SCR/jbm -iname 'OFL.txt' | head -1)" res/fonts/ng/OFL-JetBrainsMono.txt
ls -la res/fonts/ng
```
Expected: four files. If a release URL 404s, open the project's releases page, take the newest release at least two weeks old, and record the version in `THIRD_PARTIES.txt`.

Add to `res/CMakeLists.txt` `add_resources(` list:
```cmake
    fonts/ng/Inter-Regular.ttf
    fonts/ng/JetBrainsMono-Regular.ttf
```

- [ ] **Step 2: Implement `theme.cpp`**

```cpp
#include "solvespace.h"
#include "imgui.h"
#include <SDL3/SDL.h>
#include "ui/theme.h"

namespace SolveSpace {
namespace Ui {

static ImFont *gUiFont = nullptr, *gMonoFont = nullptr;

static ImVec4 Hex(uint32_t rgb, float a = 1.0f) {
    return ImVec4(((rgb >> 16) & 0xFF) / 255.0f, ((rgb >> 8) & 0xFF) / 255.0f,
                  (rgb & 0xFF) / 255.0f, a);
}

struct Tokens { uint32_t bg, panel, panelBorder, text, textMuted, accent, accentHover, danger;
                float panelAlpha; };
static const Tokens Dark  = { 0x16181D, 0x1E2128, 0x2C313A, 0xE6E8EC, 0x8B93A1,
                              0x4F8EF7, 0x6AA0FA, 0xF26D6D, 0.96f };
static const Tokens Light = { 0xF4F5F7, 0xFFFFFF, 0xD9DCE2, 0x1C1F24, 0x6A7280,
                              0x2F6FE0, 0x4A82E8, 0xD64545, 0.97f };

static void LoadFontsOnce() {
    if(gUiFont) return;
    ImGuiIO &io = ImGui::GetIO();
    float scale = SDL_GetWindowDisplayScale(SDL_GL_GetCurrentWindow());
    if(scale <= 0) scale = 1.0f;
    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false;   // data lives in the embedded resource
    size_t size;
    void *inter = (void *)Platform::LoadResource("fonts/ng/Inter-Regular.ttf", &size);
    gUiFont = io.Fonts->AddFontFromMemoryTTF(inter, (int)size, 15.0f * scale, &cfg);
    void *mono = (void *)Platform::LoadResource("fonts/ng/JetBrainsMono-Regular.ttf", &size);
    gMonoFont = io.Fonts->AddFontFromMemoryTTF(mono, (int)size, 14.0f * scale, &cfg);
    io.FontDefault = gUiFont;
    ImGui::GetStyle().FontScaleMain = 1.0f / scale; // keep logical sizes; glyphs rasterised sharp
}

ThemeChoice LoadThemeChoice() {
    return Platform::GetSettings()->ThawString("NG_Theme", "dark") == "light" ? ThemeChoice::LIGHT
                                                                              : ThemeChoice::DARK;
}

void ApplyTheme(ThemeChoice choice) {
    LoadFontsOnce();
    const Tokens &t = choice == ThemeChoice::DARK ? Dark : Light;
    ImGuiStyle &s = ImGui::GetStyle();
    s.WindowRounding = 10; s.ChildRounding = 8; s.FrameRounding = 6; s.PopupRounding = 8;
    s.GrabRounding = 6;    s.ScrollbarRounding = 6;
    s.WindowPadding = ImVec2(12, 10); s.FramePadding = ImVec2(8, 5);
    s.ItemSpacing = ImVec2(8, 6);     s.WindowBorderSize = 1; s.PopupBorderSize = 1;
    ImVec4 *c = s.Colors;
    c[ImGuiCol_Text]                 = Hex(t.text);
    c[ImGuiCol_TextDisabled]         = Hex(t.textMuted);
    c[ImGuiCol_WindowBg]             = Hex(t.panel, t.panelAlpha);
    c[ImGuiCol_PopupBg]              = Hex(t.panel, 0.98f);
    c[ImGuiCol_Border]               = Hex(t.panelBorder);
    c[ImGuiCol_FrameBg]              = Hex(t.bg);
    c[ImGuiCol_FrameBgHovered]       = Hex(t.panelBorder);
    c[ImGuiCol_FrameBgActive]        = Hex(t.panelBorder);
    c[ImGuiCol_TitleBg]              = Hex(t.panel);
    c[ImGuiCol_TitleBgActive]        = Hex(t.panel);
    c[ImGuiCol_MenuBarBg]            = Hex(t.panel, t.panelAlpha);
    c[ImGuiCol_Header]               = Hex(t.accent, 0.25f);
    c[ImGuiCol_HeaderHovered]        = Hex(t.accent, 0.35f);
    c[ImGuiCol_HeaderActive]         = Hex(t.accent, 0.50f);
    c[ImGuiCol_Button]               = Hex(t.accent, 0.20f);
    c[ImGuiCol_ButtonHovered]        = Hex(t.accentHover, 0.35f);
    c[ImGuiCol_ButtonActive]         = Hex(t.accent, 0.55f);
    c[ImGuiCol_CheckMark]            = Hex(t.accent);
    c[ImGuiCol_SliderGrab]           = Hex(t.accent);
    c[ImGuiCol_SliderGrabActive]     = Hex(t.accentHover);
    c[ImGuiCol_Separator]            = Hex(t.panelBorder);
    c[ImGuiCol_NavHighlight]         = Hex(t.accent);
    c[ImGuiCol_TextSelectedBg]       = Hex(t.accent, 0.35f);
}

void ToggleTheme() {
    ThemeChoice next = LoadThemeChoice() == ThemeChoice::DARK ? ThemeChoice::LIGHT
                                                              : ThemeChoice::DARK;
    Platform::GetSettings()->FreezeString("NG_Theme", next == ThemeChoice::LIGHT ? "light" : "dark");
    ApplyTheme(next);
}

ImFont *MonoFont() { return gMonoFont; }

}
}
```
In ImGui 1.92, `ImGuiCol_NavHighlight` was renamed `ImGuiCol_NavCursor`, and `FontScaleMain` replaced the older global scale field. If either name fails to compile under `IMGUI_DISABLE_OBSOLETE_FUNCTIONS`, use the 1.92 name from `extlib/imgui/imgui.h` (grep `ImGuiCol_Nav` and `FontScale`).

Wire the theme into `src/ui/ngui.cpp`: add `#include "ui/theme.h"`, make `InitNgUi()` call `ApplyTheme(LoadThemeChoice());`, and add this branch to `HandleUiAction` before the `else dbp(...)`:
```cpp
    else if(action == "ui.toggle-theme") ToggleTheme();
```

- [ ] **Step 3: Third-party notices**

Append to `THIRD_PARTIES.txt`, following the file's existing entry format: SDL3 (zlib licence), Dear ImGui (MIT), Inter (SIL OFL 1.1), JetBrains Mono (SIL OFL 1.1), each with the version and upstream URL.

- [ ] **Step 4: Manual verification**

1. Panels, menus and the palette use Inter on a dark theme with rounded corners. The property browser panel is framed the same way.
2. `Ctrl+Alt+T` switches to light and back. The choice persists across restarts.
3. On a HiDPI display (or `SDL_VIDEODRIVER=x11 GDK_SCALE=2` on Linux), the text is sharp.
4. Take screenshots of both themes, with the palette open, a context menu and the property browser, and attach them for the owner's review. `AGENTS.md` §5 requires the theme to be reviewed on screenshots.

- [ ] **Step 5: Commit**

```bash
make test_solvespace
git add res/fonts/ng res/CMakeLists.txt src/ui/theme.* src/ui/ngui.cpp src/CMakeLists.txt THIRD_PARTIES.txt
git commit -m "NG: Inter/JetBrains Mono fonts and dark/light theme from tokens."
```

---

### Task 13: Windows and macOS builds, CI, docs

**Files:**
- Modify: `CMakeLists.txt` (Windows: ANGLE as shared libraries when `ENABLE_NG_GUI`)
- Modify: `src/CMakeLists.txt` (Windows GLES defines for `imgui-sdl3`; copy ANGLE DLLs next to `solvespace-ng.exe`)
- Modify: `.github/scripts/install-ubuntu.sh`, `build-ubuntu.sh`, `build-windows.sh`, `build-macos.sh`
- Modify: `AGENTS.md` §9 (NG build instructions)

**Interfaces:**
- Consumes: everything above.
- Produces: `solvespace-ng` built and the test suite (with NG tests) run on all three CI platforms. This is the M1a done-state.

- [ ] **Step 1: Linux CI**

`install-ubuntu.sh`: append the X11/Wayland development packages needed to build vendored SDL3 (the `ubuntu-latest` runner has no `libsdl3-dev`):
```sh
sudo apt-get install -q -y \
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxss-dev libxfixes-dev \
  libxkbcommon-dev libwayland-dev wayland-protocols libegl-dev libdecor-0-dev
git submodule update --init extlib/SDL extlib/imgui
```
`build-ubuntu.sh`: add `-DENABLE_NG_GUI="ON"` to the cmake line. The test run already uses `make test_solvespace`, which now includes `test/ng/*`.

- [ ] **Step 2: Windows via ANGLE (GLES 3)**

SDL loads EGL/GLES dynamically, so ANGLE must be built as DLLs for `solvespace-ng`. In `CMakeLists.txt`'s Windows `OPENGL STREQUAL "3"` block:
```cmake
            if(ENABLE_NG_GUI)
                set(ANGLE_STATIC OFF CACHE INTERNAL "")
            else()
                set(ANGLE_STATIC ON  CACHE INTERNAL "")
            endif()
```
(replacing the unconditional `set(ANGLE_STATIC ON ...)`).

In `src/CMakeLists.txt` `imgui-sdl3`:
```cmake
    if(WIN32)
        target_compile_definitions(imgui-sdl3 PUBLIC IMGUI_IMPL_OPENGL_ES3)
        target_include_directories(imgui-sdl3 PUBLIC ${CMAKE_SOURCE_DIR}/extlib/angle/include)
        target_link_libraries(imgui-sdl3 PUBLIC GLESv2)
    endif()
```
After `add_executable(solvespace-ng ...)`:
```cmake
    if(WIN32)
        add_custom_command(TARGET solvespace-ng POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:EGL> $<TARGET_FILE:GLESv2> $<TARGET_FILE_DIR:solvespace-ng>)
    endif()
```
`gl3shader.h` already compiles the renderer against `GLES2/gl2.h` with `GL_APICALL` emptied for static linkage. With shared ANGLE, `GL_APICALL` must be `__declspec(dllimport)`. Change the `WIN32` branch in `src/render/gl3shader.h` to:
```cpp
#if defined(WIN32) || defined(__EMSCRIPTEN__)
#   if !defined(SOLVESPACE_ANGLE_SHARED)
#       define GL_APICALL /*static linkage*/
#   endif
```
and add `target_compile_definitions(solvespace-ng PRIVATE SOLVESPACE_ANGLE_SHARED)` on Windows only. The old `solvespace` target keeps static ANGLE, because it never defines that macro, and `ANGLE_STATIC` is only OFF when NG is enabled. **But both targets share one ANGLE build.** With `ENABLE_NG_GUI=ON` on Windows, the old Win32 `solvespace` target must also define `SOLVESPACE_ANGLE_SHARED` and get the DLLs copied. Apply the same `target_compile_definitions` and POST_BUILD copy to `solvespace` when `WIN32 AND ENABLE_NG_GUI`.

`build-windows.sh`: add `-DENABLE_NG_GUI=ON` and `git submodule update --init extlib/SDL extlib/imgui` (in `install-windows.sh`).

Verify in Windows CI that `solvespace-ng.exe` and `solvespace.exe` both build and the test suite passes. A smoke run isn't possible in CI. Ask the owner to run the artifact on a Windows machine with the Task 7 checklist, if one is available.

- [ ] **Step 3: macOS**

`build-macos.sh`: add `-DENABLE_NG_GUI=ON`. `install-macos.sh`: init `extlib/SDL extlib/imgui`. A 2.1 compatibility context on macOS is the legacy profile, and `<OpenGL/gl.h>` provides the `EXT` framebuffer functions mapped in Task 8. If the build reports missing `glGenFramebuffersEXT`, add `#include <OpenGL/glext.h>` in the Apple branch of `gl3shader.h`.

- [ ] **Step 4: Fix any Windows test-path issue**

If `ng/settingsfile` fails on Windows because of `/tmp`, change `TempSettingsPath` to:
```cpp
    return Path::From(std::filesystem::temp_directory_path().string())
               .Join(std::string("ssng-test-") + name + ".txt");
```
with `#include <filesystem>`.

- [ ] **Step 5: Document in AGENTS.md**

In §9 "Build (Linux)" add:
```sh
sudo apt install libsdl3-dev              # or let CMake build the vendored extlib/SDL
git submodule update --init extlib/SDL extlib/imgui
cmake .. -DENABLE_NG_GUI=ON && make -j$(nproc) solvespace-ng && ./bin/solvespace-ng
```
plus one line each for the NG settings and keymap locations (`SDL_GetPrefPath("solvespace","solvespace-ng")`: Linux `~/.local/share/solvespace/solvespace-ng/`, Windows `%APPDATA%\solvespace\solvespace-ng\`, macOS `~/Library/Application Support/solvespace/solvespace-ng/`), with `settings.txt` and `keymap.txt`.

- [ ] **Step 6: Push and watch CI**

```bash
git add .github/scripts CMakeLists.txt src/CMakeLists.txt src/render/gl3shader.h AGENTS.md test/ng
git commit -m "NG: build solvespace-ng on Linux, Windows (ANGLE GLES3) and macOS in CI."
git push
gh run watch $(gh run list --limit 1 --json databaseId -q '.[0].databaseId')
```
Expected: Ubuntu, Windows and macOS jobs are green. M1a is done when this passes and the owner has reviewed the theme screenshots from Task 12.

---

## M1 Roadmap: phases after M1a

M1b and M1c each get their **own detailed plan**, written when the previous phase merges. Their code depends on the panel/overlay framework, the keymap and the theme that M1a creates, and on what using M1a daily reveals. The scope below is fixed by `AGENTS.md` §5 and §8.

**M1b — Interaction model (Modern keymap complete).** New UI commands in `src/ui/`, each a registered `ui.*` action with headless tests:
- **Pie menus** (`src/ui/piemenu.{h,cpp}`): `Shift+A` add, `F` constrain (shows only constraints valid for the current selection), `` ` `` view.
- **`Tab` sketch mode**: enter/exit the active sketch group.
- **`G` grab**: drag under the solver, `X`/`Y`/`Z` axis locks.
- **`H` / `Alt+H` / `Shift+H`**: hide / unhide all / isolate.
- **Numpad views** (1/3/7, `Ctrl` for the opposite side) and **`.` frame selected**.
- **Smart dimension `D`**: infers distance, diameter or angle from the selection.
- **Status/HUD bar**: active tool, the keys available now, DOF count, solver state.
- **Typed values become constraints** while drawing (`25` `Tab` `30` `Enter` → length + angle constraints). Needs the `Pending` tool state in `src/mouse.cpp` exposed through a small `ToolHud` interface in core, the first real `cmd/` extraction.

**M1c — Panels replace the text window, then delete the old frontends.**
- **Outliner** (groups, then later Parts), **Properties** (one ImGui panel per text-window screen, ported one screen per task from `src/textscreens.cpp`, `confscreen.cpp`, `describescreen.cpp`, `style.cpp`), and **Problems**.
- A **parity checklist** generated from `GraphicsWindow::MenuCommands()` plus the list of `TextWindow::Show*` screens. Each entry is ticked when it has a new home.
- Constraint glyph restyle and Lucide icons (adds `lunasvg` v3.5.0, already allowed in `AGENTS.md` §2).
- When the checklist is complete: delete `guigtk.cpp`, `guiwin.cpp`, `guimac.mm`, `guiqt.*`, the Qt/GTK CMake branches, the floating property-browser FBO panel and `src/toolbar.cpp`. The HTML/web frontend stays until NG targets the web.
