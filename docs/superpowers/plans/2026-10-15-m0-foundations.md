# M0 — Foundations Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Get the fork building and testing on C++20, with green CI on Linux, Windows and macOS. Give every `Command` a stable string name, and add a way to run commands headlessly from tests. M1's keymap, palette and command tests are built on those last two.

**Architecture:** No new layers yet. We set up the fork's GitHub plumbing and bump the language standard. A small `commandnames` module maps `Command` ↔ stable kebab-case ids. The existing test harness (`test/harness.{h,cpp}`) gains `RUN_COMMAND(name)`. It calls `SS.GW.ActivateCommand()` and then flushes deferred work with `SS.Refresh()`, the same path a keypress takes in the real app.

**Tech Stack:** C++20, CMake ≥ 3.18, existing SolveSpace test harness, GitHub Actions (existing `.github/workflows/test.yml`).

**Spec:** `AGENTS.md` (§4 Architecture rules, §8 Milestones/M0, §9 Working in this repo)

## Global Constraints

- License: all fork code GPLv3; keep existing copyright headers (`AGENTS.md` §2).
- C++ standard: **C++20** across the project (`AGENTS.md` §4 rule 5).
- The existing `test/` suite (`test/harness.cpp`, target `test_solvespace`) **stays green on every commit**.
- `.slvs` format and solver stay backward compatible; old files must open unchanged.
- Code style: follow `.clang-format`: 4 spaces, no tabs, 100 columns, attached braces, no space before parentheses. Match surrounding naming.
- No new dependencies in M0.
- Builds are out-of-tree only (`build/`).

## Review Focus

1. **Unknown, empty or wrong-case command names** (`""`, `"no-such"`, `"SELECT-ALL"`) must return `Command::NONE` and never assert. `RUN_COMMAND` with such a name must record a test failure, not crash. Covered in Task 3 and Task 4.
2. **A new `Command` added to the menu table without a stable name** must fail a test, not silently drop out of keymaps. Covered in Task 3 (`every_menu_command_has_a_name`).
3. **Running a command headlessly with no file loaded or an empty selection** (e.g. `delete` with nothing selected) must not crash. Covered in Task 4 (`delete_with_empty_selection_is_harmless`).
4. **Old-version fixtures still load after the C++20 bump.** The existing `*_v20.slvs` / `*_v22.slvs` migrate tests cover this. Task 2 must run the full suite, not a filtered subset.
5. **MSVC C++20 build.** Windows CI must build with the bumped standard. Task 5 verifies it on the fork's CI.

---

### Task 1: Fork plumbing and local baseline

This task changes no code. It establishes the starting state. **Steps 1 and 3 change things outside this machine. Confirm each with the owner before running it.**

**Files:** none.

**Interfaces:**
- Consumes: nothing.
- Produces: `origin` = owner's fork, `upstream` = `solvespace/solvespace`. A local `build/` directory where the baseline `make test_solvespace` passes.

- [ ] **Step 1: Create the fork on GitHub (owner confirms first)**

Today `origin` points at `https://github.com/solvespace/solvespace.git`. Run:

```bash
cd solvespace
gh repo fork solvespace/solvespace --remote --remote-name origin
git remote -v
```
Expected: `origin` → `https://github.com/ChalkWalk/solvespace.git`, `upstream` → `https://github.com/solvespace/solvespace.git`. (`gh` renames the old `origin` to `upstream` automatically.)

- [ ] **Step 2: Install build dependencies (owner runs; needs sudo)**

Ask the owner to run this in the session prompt:
```
! sudo .github/scripts/install-ubuntu.sh
```
Expected: the apt install succeeds and the `extlib/libdxfrw extlib/mimalloc extlib/eigen` submodules are initialised.

- [ ] **Step 3: Enable GitHub Actions on the fork (owner, in the browser)**

Forks have Actions disabled by default. The owner opens `https://github.com/ChalkWalk/solvespace/actions` and clicks "I understand my workflows, go ahead and enable them".

- [ ] **Step 4: Build and run the baseline suite**

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_OPENMP=ON
make -j$(nproc)
make test_solvespace
```
Expected: the final line reports `0 failed`. If the baseline fails before any change, stop and report the failing test names to the owner. Don't fix upstream failures inside M0 without approval.

---

### Task 2: Bump to C++20

**Files:**
- Modify: `CMakeLists.txt:16`
- Modify (only if the compiler warns about deprecated implicit `this` capture): `src/textscreens.cpp:730`, `src/graphicswin.cpp:368`, `src/graphicswin.cpp:525`, `src/util.cpp:180`

**Interfaces:**
- Consumes: Task 1 baseline.
- Produces: all targets compile as C++20. Later tasks may use `std::string_view`, `std::optional`, `std::span`, designated initialisers.

Pre-check already done while writing this plan: no `register`, `auto_ptr`, `random_shuffle`, `binary_function`, `result_of`, dynamic exception specs, `u8""` literals, non-const comparison operators or `volatile` usage in `src/` and `test/`. The only expected diagnostics are C++20 deprecation warnings for implicit `this` capture via `[=]`.

- [ ] **Step 1: Change the standard**

In `CMakeLists.txt` replace:
```cmake
set(CMAKE_CXX_STANDARD 11)
```
with:
```cmake
set(CMAKE_CXX_STANDARD 20)
```

- [ ] **Step 2: Reconfigure and build, capturing warnings**

```bash
cd build && cmake .. && make -j$(nproc) 2>&1 | tee /tmp/cxx20-build.log | grep -E "error|warning: implicit capture of 'this'" | head -40
```
Expected: no `error:` lines. Possibly up to 4 warnings about implicit capture of `this` via `[=]`.

- [ ] **Step 3: Fix implicit-`this` warnings, if any**

For each warning in a member function, change the lambda capture from `[=]` to `[=, this]`. Example for `src/graphicswin.cpp:525`:
```cpp
    animateTimer->onTimeout = [=, this] {
```
Leave `[=]` lambdas that aren't in member functions unchanged. The compiler only warns where `this` is actually captured.

- [ ] **Step 4: Run the full suite**

```bash
make test_solvespace
```
Expected: `0 failed`, with the same check count as the Task 1 baseline.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src/
git commit -m "Build as C++20."
```

---

### Task 3: Stable command names

Keymap files (M1), the command palette (M1) and test scripts need to refer to commands by a stable string. Without one, any reordering of `enum class Command` would break saved keymaps.

**Files:**
- Create: `src/commandnames.h`
- Create: `src/commandnames.cpp`
- Modify: `src/CMakeLists.txt` (add `commandnames.cpp` to `solvespace-core` sources, after `clipboard.cpp`)
- Modify: `src/ui.h` (declare `GraphicsWindow::MenuCommands()` next to `ActivateCommand`, line ~559)
- Modify: `src/graphicswin.cpp` (define `MenuCommands()` after `ActivateCommand`)
- Create: `test/core/commandnames/test.cpp`
- Modify: `test/CMakeLists.txt` (add `core/commandnames/test.cpp` after `core/expr/test.cpp`)

**Interfaces:**
- Consumes: `enum class Command` (`src/ui.h:82`).
- Produces:
  - `const char *SolveSpace::CommandName(Command cmd)`: stable kebab-case id, or `nullptr` for `Command::NONE` or an unknown value.
  - `Command SolveSpace::CommandFromName(const std::string &name)`: exact, case-sensitive match. Returns `Command::NONE` if unknown.
  - `std::vector<Command> GraphicsWindow::MenuCommands()` (static): every non-`NONE` command in the main menu table, in menu order.

- [ ] **Step 1: Write the failing test**

Create `test/core/commandnames/test.cpp`:
```cpp
#include "harness.h"
#include "commandnames.h"

TEST_CASE(round_trip_known_names) {
    CHECK_EQ_STR(CommandName(Command::LINE_SEGMENT), "line-segment");
    CHECK_TRUE(CommandFromName("line-segment") == Command::LINE_SEGMENT);
    CHECK_EQ_STR(CommandName(Command::SELECT_ALL), "select-all");
    CHECK_TRUE(CommandFromName("select-all") == Command::SELECT_ALL);
}

TEST_CASE(unknown_names_are_none) {
    CHECK_TRUE(CommandFromName("") == Command::NONE);
    CHECK_TRUE(CommandFromName("no-such-command") == Command::NONE);
    CHECK_TRUE(CommandFromName("SELECT-ALL") == Command::NONE);
    CHECK_TRUE(CommandName(Command::NONE) == nullptr);
}

TEST_CASE(every_menu_command_has_a_name) {
    for(Command cmd : GraphicsWindow::MenuCommands()) {
        const char *name = CommandName(cmd);
        CHECK_TRUE(name != nullptr);
        CHECK_TRUE(CommandFromName(name) == cmd);
    }
}

TEST_CASE(names_are_unique) {
    std::set<std::string> seen;
    for(Command cmd : GraphicsWindow::MenuCommands()) {
        CHECK_TRUE(seen.insert(CommandName(cmd)).second);
    }
}
```
Add `#include <set>` at the top of the file if `harness.h` doesn't already pull it in.

Add to `test/CMakeLists.txt` in `testsuite_SOURCES`, after `core/expr/test.cpp`:
```cmake
    core/commandnames/test.cpp
```

- [ ] **Step 2: Run the test to verify it fails**

```bash
cd build && cmake .. && make solvespace-testsuite 2>&1 | tail -5
```
Expected: compile error, `commandnames.h: No such file or directory`.

- [ ] **Step 3: Write the header**

Create `src/commandnames.h`:
```cpp
//-----------------------------------------------------------------------------
// Stable, human-readable identifiers for menu commands, used by keymap files,
// the command palette and test scripts.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_COMMANDNAMES_H
#define SOLVESPACE_COMMANDNAMES_H

#include <string>

namespace SolveSpace {

enum class Command : uint32_t;

// Returns the stable id for `cmd`, or nullptr for Command::NONE / unknown values.
const char *CommandName(Command cmd);
// Exact, case-sensitive lookup; returns Command::NONE for unknown names.
Command CommandFromName(const std::string &name);

}

#endif
```

- [ ] **Step 4: Write the implementation**

Create `src/commandnames.cpp`:
```cpp
//-----------------------------------------------------------------------------
// Stable, human-readable identifiers for menu commands.
//
// These strings are persisted in user keymap files: never rename one, only add.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "commandnames.h"

namespace SolveSpace {

struct CommandNameEntry {
    Command     cmd;
    const char *name;
};

static const CommandNameEntry CommandNames[] = {
    // File
    { Command::NEW,               "new"                },
    { Command::OPEN,              "open"               },
    { Command::OPEN_RECENT,       "open-recent"        },
    { Command::SAVE,              "save"               },
    { Command::SAVE_AS,           "save-as"            },
    { Command::EXPORT_IMAGE,      "export-image"       },
    { Command::EXPORT_MESH,       "export-mesh"        },
    { Command::EXPORT_SURFACES,   "export-surfaces"    },
    { Command::EXPORT_VIEW,       "export-view"        },
    { Command::EXPORT_SECTION,    "export-section"     },
    { Command::EXPORT_WIREFRAME,  "export-wireframe"   },
    { Command::IMPORT,            "import"             },
    { Command::EXIT,              "exit"               },
    // View
    { Command::ZOOM_IN,           "zoom-in"            },
    { Command::ZOOM_OUT,          "zoom-out"           },
    { Command::ZOOM_TO_FIT,       "zoom-to-fit"        },
    { Command::SHOW_GRID,         "show-grid"          },
    { Command::DIM_SOLID_MODEL,   "dim-solid-model"    },
    { Command::PERSPECTIVE_PROJ,  "perspective"        },
    { Command::EXPLODE_SKETCH,    "explode-sketch"     },
    { Command::ONTO_WORKPLANE,    "align-to-workplane" },
    { Command::NEAREST_ORTHO,     "nearest-ortho"      },
    { Command::NEAREST_ISO,       "nearest-iso"        },
    { Command::CENTER_VIEW,       "center-view"        },
    { Command::SHOW_TOOLBAR,      "show-toolbar"       },
    { Command::SHOW_TEXT_WND,     "show-property-browser" },
    { Command::UNITS_INCHES,      "units-inches"       },
    { Command::UNITS_FEET_INCHES, "units-feet-inches"  },
    { Command::UNITS_MM,          "units-mm"           },
    { Command::UNITS_METERS,      "units-meters"       },
    { Command::FULL_SCREEN,       "full-screen"        },
    // Edit
    { Command::UNDO,              "undo"               },
    { Command::REDO,              "redo"               },
    { Command::CUT,               "cut"                },
    { Command::COPY,              "copy"               },
    { Command::PASTE,             "paste"              },
    { Command::PASTE_TRANSFORM,   "paste-transformed"  },
    { Command::DELETE,            "delete"             },
    { Command::SELECT_CHAIN,      "select-edge-chain"  },
    { Command::SELECT_ALL,        "select-all"         },
    { Command::SNAP_TO_GRID,      "snap-to-grid"       },
    { Command::ROTATE_90,         "rotate-imported-90" },
    { Command::UNSELECT_ALL,      "unselect-all"       },
    { Command::REGEN_ALL,         "regenerate-all"     },
    { Command::EDIT_LINE_STYLES,  "line-styles"        },
    { Command::VIEW_PROJECTION,   "view-projection"    },
    { Command::CONFIGURATION,     "configuration"      },
    // Request
    { Command::SEL_WORKPLANE,     "in-workplane"       },
    { Command::FREE_IN_3D,        "free-in-3d"         },
    { Command::DATUM_POINT,       "datum-point"        },
    { Command::WORKPLANE,         "workplane"          },
    { Command::LINE_SEGMENT,      "line-segment"       },
    { Command::CONSTR_SEGMENT,    "construction-line-segment" },
    { Command::CIRCLE,            "circle"             },
    { Command::ARC,               "arc"                },
    { Command::RECTANGLE,         "rectangle"          },
    { Command::CUBIC,             "cubic-spline"       },
    { Command::TTF_TEXT,          "ttf-text"           },
    { Command::IMAGE,             "image"              },
    { Command::SPLIT_CURVES,      "split-curves"       },
    { Command::TANGENT_ARC,       "tangent-arc"        },
    { Command::CONSTRUCTION,      "toggle-construction" },
    // Group
    { Command::GROUP_3D,          "group-sketch-3d"    },
    { Command::GROUP_WRKPL,       "group-sketch-workplane" },
    { Command::GROUP_EXTRUDE,     "group-extrude"      },
    { Command::GROUP_HELIX,       "group-helix"        },
    { Command::GROUP_LATHE,       "group-lathe"        },
    { Command::GROUP_REVOLVE,     "group-revolve"      },
    { Command::GROUP_ROT,         "group-step-rotate"  },
    { Command::GROUP_TRANS,       "group-step-translate" },
    { Command::GROUP_LINK,        "group-link"         },
    { Command::GROUP_RECENT,      "group-link-recent"  },
    // Constrain
    { Command::DISTANCE_DIA,      "constrain-distance" },
    { Command::REF_DISTANCE,      "constrain-reference-distance" },
    { Command::ANGLE,             "constrain-angle"    },
    { Command::REF_ANGLE,         "constrain-reference-angle" },
    { Command::OTHER_ANGLE,       "other-supplementary-angle" },
    { Command::REFERENCE,         "toggle-reference"   },
    { Command::EQUAL,             "constrain-equal"    },
    { Command::RATIO,             "constrain-ratio"    },
    { Command::DIFFERENCE,        "constrain-difference" },
    { Command::ON_ENTITY,         "constrain-on-entity" },
    { Command::SYMMETRIC,         "constrain-symmetric" },
    { Command::AT_MIDPOINT,       "constrain-midpoint" },
    { Command::HORIZONTAL,        "constrain-horizontal" },
    { Command::VERTICAL,          "constrain-vertical" },
    { Command::PARALLEL,          "constrain-parallel" },
    { Command::PERPENDICULAR,     "constrain-perpendicular" },
    { Command::ORIENTED_SAME,     "constrain-same-orientation" },
    { Command::WHERE_DRAGGED,     "constrain-where-dragged" },
    { Command::COMMENT,           "comment"            },
    // Analyze
    { Command::VOLUME,            "measure-volume"     },
    { Command::AREA,              "measure-area"       },
    { Command::PERIMETER,         "measure-perimeter"  },
    { Command::INTERFERENCE,      "show-interference"  },
    { Command::NAKED_EDGES,       "show-naked-edges"   },
    { Command::SHOW_DOF,          "show-dof"           },
    { Command::CENTER_OF_MASS,    "show-center-of-mass" },
    { Command::TRACE_PT,          "trace-point"        },
    { Command::STOP_TRACING,      "stop-tracing"       },
    { Command::STEP_DIM,          "step-dimension"     },
    // Help
    { Command::LOCALE,            "language"           },
    { Command::WEBSITE,           "website"            },
    { Command::GITHUB,            "github-commit"      },
    { Command::ABOUT,             "about"              },
};

const char *CommandName(Command cmd) {
    for(const CommandNameEntry &entry : CommandNames) {
        if(entry.cmd == cmd) return entry.name;
    }
    return nullptr;
}

Command CommandFromName(const std::string &name) {
    for(const CommandNameEntry &entry : CommandNames) {
        if(name == entry.name) return entry.cmd;
    }
    return Command::NONE;
}

}
```

- [ ] **Step 5: Expose the menu command list**

In `src/ui.h`, directly below `void ActivateCommand(Command id);`:
```cpp
    static std::vector<Command> MenuCommands();
```

In `src/graphicswin.cpp`, directly after the body of `GraphicsWindow::ActivateCommand`:
```cpp
std::vector<Command> GraphicsWindow::MenuCommands() {
    std::vector<Command> result;
    for(int i = 0; Menu[i].level >= 0; i++) {
        if(Menu[i].cmd != Command::NONE) result.push_back(Menu[i].cmd);
    }
    return result;
}
```

Add `commandnames.cpp` to `solvespace-core` in `src/CMakeLists.txt`, after `clipboard.cpp`:
```cmake
        clipboard.cpp
        commandnames.cpp
```

- [ ] **Step 6: Run the tests to verify they pass**

```bash
cd build && cmake .. && make -j$(nproc) solvespace-testsuite && ./bin/solvespace-testsuite commandnames
```
(The test runner takes a regex filter as its first argument; see `test/harness.cpp` `filter`.)
Expected: 4 cases run, `0 failed`. If the binary isn't at `build/bin/`, find it with `find build -name solvespace-testsuite -type f`.

- [ ] **Step 7: Run the full suite**

```bash
make test_solvespace
```
Expected: `0 failed`.

- [ ] **Step 8: Commit**

```bash
git add src/commandnames.h src/commandnames.cpp src/CMakeLists.txt src/ui.h src/graphicswin.cpp \
        test/core/commandnames/test.cpp test/CMakeLists.txt
git commit -m "Add stable string names for menu commands."
```

---

### Task 4: Headless command runner in the test harness

**Files:**
- Modify: `test/harness.h` (declare `Helper::RunCommand`, add `RUN_COMMAND` macro)
- Modify: `test/harness.cpp` (define `Helper::RunCommand`)
- Create: `test/core/commands/test.cpp`
- Create: `test/core/commands/line.slvs` (copy of `test/request/line_segment/normal.slvs`)
- Modify: `test/CMakeLists.txt` (add `core/commands/test.cpp`)

**Interfaces:**
- Consumes: `CommandFromName` (Task 3), `GraphicsWindow::ActivateCommand(Command)` (`src/ui.h:559`), `SolveSpaceUI::Refresh()` (`src/solvespace.cpp:319`).
- Produces: `bool Test::Helper::RunCommand(const char *file, int line, const char *name)` and the macro `RUN_COMMAND(name)`. Later milestones' command tests use these.

Headless safety was checked while writing this plan. `GraphicsWindow::Invalidate()` guards `if(window)`, and `TextWindow::Show()` guards `if(window) Resize();`. Under `guinone.cpp`, windows are null and timers are inert, which is why the runner must call `SS.Refresh()` itself.

- [ ] **Step 1: Copy the fixture**

```bash
mkdir -p test/core/commands
cp test/request/line_segment/normal.slvs test/core/commands/line.slvs
```

- [ ] **Step 2: Write the failing test**

Create `test/core/commands/test.cpp`:
```cpp
#include "harness.h"

TEST_CASE(select_all_delete_undo_roundtrip) {
    CHECK_LOAD("line.slvs");
    int before = SK.request.n;
    CHECK_TRUE(before > 0);
    RUN_COMMAND("select-all");
    RUN_COMMAND("delete");
    CHECK_TRUE(SK.request.n < before);
    RUN_COMMAND("undo");
    CHECK_TRUE(SK.request.n == before);
    CHECK_SAVE("line.slvs");
}

TEST_CASE(delete_with_empty_selection_is_harmless) {
    CHECK_LOAD("line.slvs");
    int before = SK.request.n;
    RUN_COMMAND("unselect-all");
    RUN_COMMAND("delete");
    CHECK_TRUE(SK.request.n == before);
}

TEST_CASE(unknown_command_fails_the_check) {
    // RunCommand is called directly (not via the macro) so this test can assert on
    // the failure without aborting itself.
    size_t failsBefore = helper->failCount;
    bool ok = helper->RunCommand(__FILE__, __LINE__, "no-such-command");
    CHECK_FALSE(ok);
    CHECK_TRUE(helper->failCount == failsBefore + 1);
    helper->failCount = failsBefore; // the expected failure must not fail the suite
}
```

Add to `test/CMakeLists.txt` `testsuite_SOURCES`, after `core/commandnames/test.cpp`:
```cmake
    core/commands/test.cpp
```

- [ ] **Step 3: Run it to verify it fails**

```bash
cd build && cmake .. && make solvespace-testsuite 2>&1 | grep -m3 error
```
Expected: `error: 'RUN_COMMAND' was not declared` (or similar).

- [ ] **Step 4: Declare the helper and macro**

In `test/harness.h`, in `class Helper`, after `CheckRenderIso`:
```cpp
    bool RunCommand(const char *file, int line, const char *name);
```
After the `CHECK_RENDER_ISO` macro:
```cpp
#define RUN_COMMAND(name) \
    do { if(!helper->RunCommand(__FILE__, __LINE__, name)) return; } while(0)
```

- [ ] **Step 5: Implement the helper**

In `test/harness.cpp`, add `#include "commandnames.h"` beside the existing includes. After `Helper::CheckRenderIso`, add:
```cpp
bool Test::Helper::RunCommand(const char *file, int line, const char *name) {
    Command cmd = CommandFromName(name);
    if(cmd == Command::NONE) {
        PrintFailure(file, line, ssprintf("unknown command '%s'", name));
        RecordCheck(false);
        return false;
    }
    SS.GW.ActivateCommand(cmd);
    // Headless timers never fire; flush the regenerate/show work a real event loop would run.
    SS.Refresh();
    return RecordCheck(true);
}
```
(`RecordCheck` increments `checkCount` and, on failure, `failCount`. Read its body in `test/harness.cpp` first to confirm, and adjust the test's arithmetic in Step 2 if it differs.)

- [ ] **Step 6: Run the tests**

```bash
make -j$(nproc) solvespace-testsuite && ./bin/solvespace-testsuite core/commands
```
Expected: 3 cases, `0 failed`.

If `select_all_delete_undo_roundtrip` fails on `SK.request.n < before`, the fixture's active group probably isn't the group that owns the line. Check by printing `SS.GW.activeGroup.v` and the line request's `group.v`. The fix is to choose a fixture whose last group owns sketch requests (e.g. `test/constraint/horizontal/line.slvs`), not to change core behaviour.

If a command dereferences a null window headlessly (segfault), add an `if(window)` guard in the core function that crashed, matching `GraphicsWindow::Invalidate()`. List each guard in the commit message.

- [ ] **Step 7: Full suite and commit**

```bash
make test_solvespace
git add test/harness.h test/harness.cpp test/core/commands test/CMakeLists.txt
git commit -m "Test harness: run menu commands headlessly by name."
```

---

### Task 5: CI green on the fork

**Files:**
- Modify: `AGENTS.md` (§9 Build: add the test-filter usage and the fork's remotes)

**Interfaces:**
- Consumes: Tasks 1–4.
- Produces: the M0 "done when" state: the `test/` suite passes on Linux, Windows and macOS in CI.

`.github/workflows/test.yml` already builds and tests on `ubuntu-latest`, `windows-2022` and `macos-latest` for pushes to non-`master` branches, so no workflow change is expected.

- [ ] **Step 1: Document the test filter and remotes in AGENTS.md**

In `AGENTS.md` §9 "Build (Linux)", after the `make test_solvespace` line, add:
```sh
./bin/solvespace-testsuite <regex>   # run only tests whose path/name matches, e.g. core/commands
```
and in §9 "Process" replace the upstream-sync bullet with:
```markdown
- Remotes: `origin` = our fork, `upstream` = `solvespace/solvespace`. Upstream sync: merge
  `upstream/master` quarterly until M1 lands, then cherry-pick solver/kernel fixes only.
```

- [ ] **Step 2: Commit and push (owner confirms the push)**

```bash
git add AGENTS.md
git commit -m "AGENTS.md: test filter usage and fork remotes."
git push -u origin ng/m0-plan
```

- [ ] **Step 3: Watch CI**

```bash
gh run list --branch ng/m0-plan --limit 5
gh run watch $(gh run list --branch ng/m0-plan --limit 1 --json databaseId -q '.[0].databaseId')
```
Expected: the "Test Ubuntu", "Test Windows" and "Test macOS" jobs all succeed. "Test Flatpak" may fail for unrelated reasons (old runtime image). Report it but don't block M0 on it.

If Windows fails on C++20, the usual cause is MSVC rejecting a construct GCC accepted, e.g. designated initialisers out of order, or `std::min`/`std::max` colliding with `<windows.h>` macros. Fix it at the reported line, push, and re-watch.

- [ ] **Step 4: Merge to master**

After CI is green and the owner approves:
```bash
git switch master && git merge --ff-only ng/m0-plan && git push origin master
```

M0 is done when this step completes.
