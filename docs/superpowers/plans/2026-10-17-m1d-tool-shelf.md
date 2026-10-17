# M1d Tool Shelf and Icons Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

Written 2026-10-17 while the owner is away ("keep on working"; UX calls lean to modern and
industry standard). Executed inline (native). The look is for the owner to review on
screenshots (AGENTS.md §5: review the visual style on screenshots); `docs/owner-checks.md`
gets the review items.

**Status (2026-10-17):** tasks 1–4 done on `ng/m1c-feedback`. Rulings: missing icon files are
caught by the model test (`LoadResource` asserts), the runtime fallback covers broken SVGs;
the shelf is capped at 560 px so it sits between the side panels, and a section that doesn't fit
whole starts a new row; the narrow-window case became "shrink the window" (the app's minimum
size is 720×636). Extra fixes found by the tests: frames continue while an auto-sized window
fits; panels are kept on screen on resize; every panel reloads its settings per main window.

**Goal:** Replace SolveSpace's custom-drawn left toolbar (`src/toolbar.cpp`) in NG with a
floating ImGui tool shelf, hidden by default, whose buttons are Lucide-style icons rendered by
lunasvg; one of the two items left before the old frontends can go (`docs/m1-parity.md`).

**Architecture:** Three layers, each testable on its own:
- `Ui::RasterizeIcon` (pure: SVG text → RGBA pixels, via lunasvg), unit-tested in `test/ng`;
- `Ui::BuildShelf` (pure model: sections of `{Command, icon, label, keys}` from the command
  registry and the keymap), unit-tested in `test/ng`, including parity with the old toolbar;
- the shelf panel view plus a GL icon-texture cache in the SDL frontend, tested end to end in
  `test/ngui/shelf`.

The shelf reuses View ▸ Show Toolbar (`SS.showToolbar`, its menu check mark and its saved
setting): in NG that flag means "shelf shown", and the old toolbar is never drawn or hit-tested.

**Tech stack:** C++20, Dear ImGui 1.92.9b, lunasvg v3.5.0 (MIT, bundles plutovg, MIT), Lucide
0.577.0 icons (ISC).

**Spec:** `AGENTS.md` §5 (layout: "The tool shelf is optional and hidden by default"; visual
style: "Lucide-based icons rendered with lunasvg"), §2 (allowed dependencies), §8 (M1 done-when).

## Global Constraints

- Allowed dependencies only (§2): lunasvg (MIT), Lucide icons (ISC). lunasvg goes in as the git
  submodule `extlib/lunasvg` at tag `v3.5.0`, built static, without examples or system fonts.
- Copied Lucide SVGs keep Lucide's ISC licence (`res/icons/ng/LICENSE-lucide.txt` names them);
  icons drawn for NG in the same style (24×24 viewBox, `stroke="currentColor"`,
  `stroke-width="2"`, round caps and joins, no fill) are part of SolveSpace NG (GPLv3).
- Every shelf button runs a command through the same path as the menu (`ActivateCommand`).
- No default key for the shelf (Blender's `T` is construction here); it is in View ▸ Show
  Toolbar and the palette.
- The old frontends keep building and keep their toolbar; only NG hides it.
- C++20; GPLv3 for app code.

## Review Focus

1. HiDPI (framebuffer scale 2): icons are rasterized at logical size × scale, so they are crisp,
   not upscaled. (Task 1 tests the rasterizer at 2×; Task 3 requests `size * scale`.)
2. Theme switch: icons are rasterized white and tinted with the theme's text colour every frame,
   so they stay visible on light and dark. (Task 3 screenshot test in the light theme.)
3. The harness makes a new ImGui context per case: icon textures are freed with ImGui and made
   again, never reused stale. (Task 3: the cache is cleared in `ShutdownImGui`; the suite runs
   several shelf cases in one process.)
4. A missing or broken icon resource: the button falls back to its text label, no crash.
   (Task 3 test: `IconTextureFromSvg("broken", "not an svg", 24) == 0`.) Ruling (Task 3):
   `LoadResource` asserts on a missing name, so missing files are caught by the Task 2 model
   test (it loads every icon) and the runtime fallback covers broken SVGs.
5. A narrow window: the shelf wraps into rows and every button stays reachable. (Task 3 test at
   640×480.)

---

### Task 1: lunasvg and the icon rasterizer

**Files:**
- Create: `extlib/lunasvg` (submodule, tag `v3.5.0`), `src/ui/iconraster.h`,
  `src/ui/iconraster.cpp`, `test/ng/icons/test.cpp`
- Modify: `.gitmodules`, `CMakeLists.txt` (add the subdirectory with options off),
  `src/CMakeLists.txt` (`ui/iconraster.cpp` in `solvespace-ng-support`, link `lunasvg`),
  `test/CMakeLists.txt` (`ng/icons/test.cpp`), `AGENTS.md` §9 (submodule init line)

**Interfaces:**
- Produces:
```cpp
namespace SolveSpace { namespace Ui {
struct IconImage {
    int                  width = 0, height = 0;
    std::vector<uint8_t> rgba; // straight alpha, rows top to bottom
};
// Renders a Lucide-style SVG (stroke="currentColor") white on transparent, px × px.
std::optional<IconImage> RasterizeIcon(std::string_view svg, int px);
}}
```

- [ ] **Step 1: Write the failing test** (`test/ng/icons/test.cpp`)

```cpp
#include "solvespace.h"
#include "ui/iconraster.h"
#include "harness.h"

static const char *Circle =
    "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"24\" height=\"24\" viewBox=\"0 0 24 24\""
    " fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\"><circle cx=\"12\" cy=\"12\""
    " r=\"10\"/></svg>";

static uint8_t Alpha(const SolveSpace::Ui::IconImage &im, int x, int y) {
    return im.rgba[(y * im.width + x) * 4 + 3];
}

TEST_CASE(icon_is_white_on_transparent) {
    auto im = SolveSpace::Ui::RasterizeIcon(Circle, 24);
    CHECK_TRUE(im && im->width == 24 && im->height == 24);
    CHECK_TRUE(Alpha(*im, 12, 2) > 128);  // on the stroke, top of the circle
    CHECK_TRUE(im->rgba[(2 * 24 + 12) * 4] > 200); // white, not black
    CHECK_TRUE(Alpha(*im, 12, 12) == 0);  // the middle is empty
}

TEST_CASE(icon_scales_crisply) {
    auto im = SolveSpace::Ui::RasterizeIcon(Circle, 48); // a 2x display
    CHECK_TRUE(im && im->width == 48);
    CHECK_TRUE(Alpha(*im, 24, 4) > 128);
    CHECK_TRUE(Alpha(*im, 24, 24) == 0);
}

TEST_CASE(broken_svg_gives_nothing) {
    CHECK_FALSE(SolveSpace::Ui::RasterizeIcon("not an svg", 24).has_value());
    CHECK_FALSE(SolveSpace::Ui::RasterizeIcon(Circle, 0).has_value());
}
```

- [ ] **Step 2: Run it and watch it fail.** `make solvespace-testsuite` fails: `ui/iconraster.h`
  not found.

- [ ] **Step 3: Add the submodule and build options**

```sh
git submodule add https://github.com/sammycage/lunasvg.git extlib/lunasvg
git -C extlib/lunasvg checkout v3.5.0
```
Top-level `CMakeLists.txt`, where the NG GUI deps are added (next to `extlib/imgui`):
```cmake
    set(LUNASVG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(LUNASVG_DISABLE_LOAD_SYSTEM_FONTS ON CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS_SAVED ${BUILD_SHARED_LIBS})
    set(BUILD_SHARED_LIBS OFF)
    add_subdirectory(extlib/lunasvg EXCLUDE_FROM_ALL)
    set(BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS_SAVED})
```

- [ ] **Step 4: Implement** (`src/ui/iconraster.cpp`)

```cpp
#include <lunasvg.h>
#include "ui/iconraster.h"

namespace SolveSpace { namespace Ui {

std::optional<IconImage> RasterizeIcon(std::string_view svg, int px) {
    if(px <= 0) return std::nullopt;
    // Lucide strokes with currentColor; draw white so the UI can tint it with any colour.
    std::string text(svg);
    for(size_t at; (at = text.find("currentColor")) != std::string::npos;) {
        text.replace(at, strlen("currentColor"), "#ffffff");
    }
    auto doc = lunasvg::Document::loadFromData(text);
    if(!doc) return std::nullopt;
    lunasvg::Bitmap bmp = doc->renderToBitmap(px, px, 0x00000000);
    if(bmp.isNull()) return std::nullopt;
    bmp.convertToRGBA(); // lunasvg renders premultiplied BGRA
    IconImage im;
    im.width  = bmp.width();
    im.height = bmp.height();
    im.rgba.resize((size_t)im.width * im.height * 4);
    for(int y = 0; y < im.height; y++) {
        memcpy(&im.rgba[(size_t)y * im.width * 4], bmp.data() + (size_t)y * bmp.stride(),
               (size_t)im.width * 4);
    }
    return im;
}

}}
```

- [ ] **Step 5: Run the tests**: `./bin/solvespace-testsuite ng/icons` → 3 pass; whole suite green.
- [ ] **Step 6: Commit** (`.gitmodules`, `extlib/lunasvg`, CMake files, the new files, AGENTS.md).

---

### Task 2: Icons and the shelf model

**Files:**
- Create: `res/icons/ng/*.svg`, `res/icons/ng/LICENSE-lucide.txt`, `src/ui/shelfmodel.h`,
  `src/ui/shelfmodel.cpp`, `test/ng/shelf/test.cpp`
- Modify: `res/CMakeLists.txt` (every SVG), `src/CMakeLists.txt` (`ui/shelfmodel.cpp` in
  `solvespace-ng-support`), `test/CMakeLists.txt`

**Interfaces:**
- Consumes: `Ui::RasterizeIcon` (Task 1), `CommandName(Command)`,
  `GraphicsWindow::MenuLabel(Command)`, `Platform::StripMnemonic`, `keymap::Keymap::ChordsFor`.
- Produces:
```cpp
namespace SolveSpace { namespace Ui {
struct ShelfItem {
    Command     command;
    std::string icon;  // resource "icons/ng/<icon>.svg"
    std::string label; // the menu label without '&', e.g. "Line Segment"
    std::string keys;  // the first chord bound to the command, e.g. "L"; "" if none
};
struct ShelfSection {
    std::string            title; // "Sketch", "Constrain", "Groups", "View"
    std::vector<ShelfItem> items;
};
std::vector<ShelfSection> BuildShelf(const keymap::Keymap &keymap);
}}
```

Sections and icons (L = copied from Lucide 0.577.0, N = drawn for NG in Lucide's style):

| Section | Command | Icon |
|---|---|---|
| Sketch | LINE_SEGMENT | `slash` (L) |
| | RECTANGLE | `rectangle-horizontal` (L) |
| | CIRCLE | `circle` (L) |
| | ARC | `arc` (N: a 3-point arc with end dots) |
| | TANGENT_ARC | `tangent-arc` (N: a line turning into an arc) |
| | CUBIC | `spline` (L) |
| | DATUM_POINT | `dot-point` (N: a filled-look small circle with a cross) |
| | TTF_TEXT | `type` (L) |
| | IMAGE | `image` (L) |
| | CONSTRUCTION | `construction-line` (N: a dashed diagonal) |
| | SPLIT_CURVES | `scissors` (L) |
| Constrain | DISTANCE_DIA | `ruler-dimension-line` (L) |
| | ANGLE | `angle` (N: two rays and an arc) |
| | HORIZONTAL | `horizontal` (N: a horizontal line with end ticks) |
| | VERTICAL | `vertical` (N: the same, vertical) |
| | PARALLEL | `parallel` (N: two slanted parallel lines) |
| | PERPENDICULAR | `perpendicular` (N: ⊥) |
| | ON_ENTITY | `point-on` (N: a line with a circled point on it) |
| | SYMMETRIC | `symmetric` (N: two dots about a dashed axis) |
| | EQUAL | `equal` (L) |
| | ORIENTED_SAME | `same-orientation` (N: two parallel arrows) |
| | OTHER_ANGLE | `other-angle` (N: crossing lines, arc on the other side) |
| | REFERENCE | `parentheses` (L) |
| Groups | GROUP_EXTRUDE | `arrow-up-from-line` (L) |
| | GROUP_LATHE | `rotate-3d` (L) |
| | GROUP_HELIX | `helix` (N: a coil) |
| | GROUP_REVOLVE | `rotate-ccw` (L) |
| | GROUP_ROT | `rotate-cw-square` (L) |
| | GROUP_TRANS | `copy` (L) |
| | GROUP_WRKPL | `layers-plus` (L) |
| | GROUP_3D | `axis-3d` (L) |
| | GROUP_LINK | `link` (L) |
| View | NEAREST_ISO | `box` (L) |
| | ONTO_WORKPLANE | `scan` (L) |

- [ ] **Step 1: Write the failing tests** (`test/ng/shelf/test.cpp`)

```cpp
#include "solvespace.h"
#include "ui/keypresets.h"
#include "ui/iconraster.h"
#include "ui/shelfmodel.h"
#include "harness.h"

using namespace SolveSpace::Ui;

static std::vector<ShelfItem> AllItems() {
    std::string err;
    std::vector<ShelfItem> all;
    for(auto &s : BuildShelf(*ModernPreset(&err))) {
        all.insert(all.end(), s.items.begin(), s.items.end());
    }
    return all;
}

TEST_CASE(shelf_has_every_old_toolbar_tool) {
    // src/toolbar.cpp's buttons, in order.
    const Command toolbar[] = {
        Command::LINE_SEGMENT, Command::RECTANGLE, Command::CIRCLE, Command::ARC,
        Command::TTF_TEXT, Command::IMAGE, Command::TANGENT_ARC, Command::CUBIC,
        Command::DATUM_POINT, Command::CONSTRUCTION, Command::SPLIT_CURVES,
        Command::DISTANCE_DIA, Command::ANGLE, Command::HORIZONTAL, Command::VERTICAL,
        Command::PARALLEL, Command::PERPENDICULAR, Command::ON_ENTITY, Command::SYMMETRIC,
        Command::EQUAL, Command::ORIENTED_SAME, Command::OTHER_ANGLE, Command::REFERENCE,
        Command::GROUP_EXTRUDE, Command::GROUP_LATHE, Command::GROUP_HELIX,
        Command::GROUP_REVOLVE, Command::GROUP_ROT, Command::GROUP_TRANS, Command::GROUP_WRKPL,
        Command::GROUP_3D, Command::GROUP_LINK, Command::NEAREST_ISO, Command::ONTO_WORKPLANE };
    std::vector<ShelfItem> all = AllItems();
    for(Command c : toolbar) {
        bool found = false;
        for(const ShelfItem &i : all) found |= i.command == c;
        CHECK_TRUE(found);
    }
}

TEST_CASE(shelf_items_have_labels_keys_and_icons) {
    for(const ShelfItem &i : AllItems()) {
        CHECK_FALSE(i.label.empty());
        CHECK_TRUE(i.label.find('&') == std::string::npos);
        size_t size;
        const void *svg = Platform::LoadResource("icons/ng/" + i.icon + ".svg", &size);
        auto im = RasterizeIcon(std::string_view((const char *)svg, size), 24);
        CHECK_TRUE(im.has_value());
        int ink = 0;
        for(size_t p = 3; im && p < im->rgba.size(); p += 4) ink += im->rgba[p] > 128;
        CHECK_TRUE(ink > 10); // the icon draws something
    }
    for(const ShelfItem &i : AllItems()) {
        if(i.command == Command::LINE_SEGMENT) CHECK_EQ_STR(i.keys, "L");
        if(i.command == Command::GROUP_EXTRUDE) CHECK_EQ_STR(i.keys, "E");
    }
}
```

- [ ] **Step 2: Run them and watch them fail** (header missing).
- [ ] **Step 3: Copy the Lucide icons** (from a sparse clone of `lucide-icons/lucide` at
  `0.577.0`, `icons/<name>.svg`) into `res/icons/ng/`, write `LICENSE-lucide.txt` (the ISC
  text and the list of copied files), and draw the N icons. Example, `horizontal.svg`:
```xml
<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none"
     stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
  <path d="M4 12h16" /><path d="M4 9v6" /><path d="M20 9v6" />
</svg>
```
  Add every file to `res/CMakeLists.txt` after `fonts/ng/JetBrainsMono-Regular.ttf`.
- [ ] **Step 4: Implement `BuildShelf`** from the table above as a static
  `{section, Command, icon}` list; label = `Platform::StripMnemonic(GraphicsWindow::MenuLabel(c))`
  (fallback `CommandName(c)`), keys = `keymap::FormatChord(ChordsFor(CommandName(c))[0])`.
- [ ] **Step 5: Run the tests** → pass; whole suite green.
- [ ] **Step 6: Commit.**

---

### Task 3: The shelf panel

**Files:**
- Create: `src/ui/shelf.h`, `src/ui/shelf.cpp`, `src/platform/sdl/icontextures.h`,
  `test/ngui/shelf/test.cpp`, `test/ngui/shelf/line.slvs`
- Modify: `src/ui.h` (`GraphicsWindow::classicToolbar`), `src/toolbar.cpp` (the old toolbar's
  draw / hover / click functions return early unless `classicToolbar`), `src/platform/guisdl.cpp`
  (icon texture cache; freed in `ShutdownImGui` and `ClearGui`; NG's `ShowToolbar` default is
  false), `src/ui/panels.{h,cpp}` (`PanelEdge::TOP`; left panels start at x = 12 now that the
  old toolbar is gone), `src/ui/ngui.cpp` (draw the shelf; `classicToolbar = false`),
  `src/CMakeLists.txt`, `test/CMakeLists.txt`, `test/ngui/menus/test.cpp`
  (`toolbar_icon_click_starts_its_tool` becomes a shelf test)

**Interfaces:**
- Consumes: `BuildShelf` (Task 2), `RasterizeIcon` (Task 1), `Panel` (M1c).
- Produces:
```cpp
namespace SolveSpace { namespace Platform {
// A GL texture with the icon drawn white at px × px (cached); 0 if the resource is missing or
// broken. Valid until the ImGui context is shut down.
ImTextureID IconTexture(const std::string &name, int px);
}}
namespace SolveSpace { namespace Ui {
Panel &ShelfPanel();          // "Tools", PanelEdge::TOP, shown while SS.showToolbar
void   DrawShelf(const keymap::Keymap &km, std::function<void()> *run);
int    ShelfTextFallbacks();  // buttons drawn as text last frame (missing icons)
}}
```
Each button is `ImGui::ImageButton("##shelf-<command name>", tex, size, …, tint = text colour)`
(text `SmallButton` when the texture is 0), followed by
`IMGUI_TEST_ENGINE_ITEM_INFO(GetItemID(), "<label>##shelf-<name>", GetItemFlags())` so the UI
tests find it by label; its tooltip is "<label>  <keys>". Buttons flow left to right and wrap
at the panel's width (at most 640 px, never wider than the window less 24 px); sections are
separated by a vertical gap. A click posts `SS.GW.ActivateCommand(command)`.

- [ ] **Step 1: Write the failing UI tests** (`test/ngui/shelf/test.cpp`)

```cpp
// The tool shelf: NG's replacement for the old left toolbar, hidden until asked for.
#include "../uitest.h"
#include "ui/shelf.h"
#include "harness.h"

using namespace SolveSpace::UiTest;

static bool ShelfShown() { return Driver::FindItem("Tools").has_value(); }

TEST_CASE(shelf_starts_hidden_and_the_old_toolbar_is_gone) {
    CHECK_LOAD("line.slvs");
    CHECK_FALSE(ShelfShown());
    Driver::Click(20, 60); // where the old toolbar's line button was
    CHECK_TRUE(SS.GW.pending.operation == GraphicsWindow::Pending::NONE);
}

TEST_CASE(view_menu_shows_and_hides_the_shelf) {
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(Driver::ClickMenu({ "View", "Show Toolbar" }));
    CHECK_TRUE(ShelfShown());
    CHECK_TRUE(SS.showToolbar);
    CHECK_TRUE(ClosePanel("Tools"));
    CHECK_FALSE(SS.showToolbar);
}

TEST_CASE(a_shelf_button_starts_its_tool) {
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(Driver::ClickMenu({ "View", "Show Toolbar" }));
    CHECK_TRUE(Driver::ClickItem("Line Segment"));
    CHECK_TRUE(SS.GW.pending.command == Command::LINE_SEGMENT);
    CHECK_TRUE(Ui::ShelfTextFallbacks() == 0); // every button had its icon
}

TEST_CASE(a_constraint_button_with_nothing_selected_explains) {
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(Driver::ClickMenu({ "View", "Show Toolbar" }));
    CHECK_TRUE(Driver::ClickItem("Parallel"));
    CHECK_TRUE(Driver::Dialogs().size() == 1 && Driver::Dialogs()[0].kind == "message");
}

TEST_CASE(shelf_wraps_in_a_narrow_window) {
    CHECK_LOAD("line.slvs");
    Driver::Resize(640, 480);
    CHECK_TRUE(Driver::ClickMenu({ "View", "Show Toolbar" }));
    for(const char *l : { "Line Segment", "Link / Assemble...", "Align View to Workplane" }) {
        auto it = Driver::FindItem(l);
        CHECK_TRUE(it && it->fullyVisible);
    }
}

TEST_CASE(missing_icon_falls_back_to_text) {
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(Platform::IconTexture("no-such-icon", 24) == 0);
}

TEST_CASE(icons_follow_the_light_theme) {
    CHECK_LOAD("line.slvs");
    CHECK_TRUE(Driver::ClickMenu({ "View", "Show Toolbar" }));
    Driver::Key("Space");
    Driver::Type("toggle theme");
    Driver::Key("Return");
    auto b = Driver::FindItem("Circle");
    CHECK_TRUE(b.has_value());
    Platform::Path shot = Driver::ConfigDir().Join("shelf.png");
    CHECK_TRUE(Driver::Screenshot(shot));
    auto px = Pixmap::ReadPng(shot);
    // Some pixel of the circle icon is dark (tinted with the light theme's text colour).
    int dark = 0;
    for(int y = (int)b->y0; px && y < (int)b->y1; y++) {
        for(int x = (int)b->x0; x < (int)b->x1; x++) dark += px->GetPixel(x, y).red < 90;
    }
    CHECK_TRUE(dark > 5);
}
```
(Labels: use the exact menu labels; check them with `GraphicsWindow::MenuLabel` while writing.)

- [ ] **Step 2: Run them and watch them fail** (`ui/shelf.h` missing).
- [ ] **Step 3: Implement** the cache, the panel, the TOP edge, `classicToolbar`, the
  `ShowToolbar` default (in NG's `InitGui`: if the setting is absent, i.e.
  `ThawBool("ShowToolbar", true) != ThawBool("ShowToolbar", false)`, freeze it false), and
  sync: `ShelfPanel().SetShown(SS.showToolbar)` before `Begin`; if the panel's close button
  hid it, `SS.showToolbar = false; SS.GW.EnsureValidActives();` (the menu check mark).
- [ ] **Step 4: Update `test/ngui/menus` `toolbar_icon_click_starts_its_tool`** to show the
  shelf and click "Rectangle"; remove the pixel-coordinate toolbar click.
- [ ] **Step 5: Run** the UI suite, the core suite and the X11 suite → all green.
- [ ] **Step 6: Commit.**

---

### Task 4: Docs and owner review

**Files:** `docs/m1-parity.md` (toolbar item: replaced by the shelf), `AGENTS.md` (§5 note that
the shelf replaces the toolbar and reuses View ▸ Show Toolbar; §9 harness note: no more
hard-coded toolbar pixels), `docs/owner-checks.md` (screenshots of the shelf in dark and light
for review; hands-on items), `docs/superpowers/plans/2026-10-17-m1d-tool-shelf.md` (status).

- [ ] **Step 1:** Take the screenshots with the debug script
  (`action ui.command-palette; keys show toolbar; key Return; wait 5; shot …`) in both themes,
  into `docs/owner-review/` (PNG, committed, small).
- [ ] **Step 2:** Update the docs above.
- [ ] **Step 3:** Commit and push `ng/m1c-feedback`.
