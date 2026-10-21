# Owner checks and open issues

Kept by the agents while the owner is away (started 2026-10-17). Every item has automated tests;
the hands-on list is what tests can't judge: how it feels. Tick items, or note what felt wrong
and it becomes a test and a fix.

Build: `cd build-ng && make -j$(nproc) solvespace-ng && ./bin/solvespace-ng [file.slvs]`
(branch `ng/m1c-feedback`). Watch the X11 tests: `NGX11_SHOW=:0 python3 ../test/x11/run.py
bin/solvespace-ng`.

## Hands-on checks

### Line chains (2026-10-17, `9bdd5553`)
- [ ] Right-click ends a chain without a segment to the cursor; so do Esc and Enter (nothing
      typed).
- [ ] Double-click ends the chain at that point; so does clicking the last point again.
- [ ] Backspace (nothing typed) takes back the last segment and keeps drawing; with a value
      typed it edits the value. Backspace before the second point cancels the line.
- [ ] Quick flicks then a click: horizontal/vertical constraints match the segment you see.
- [ ] In a 3D sketch (New Group > Sketch In 3d) the line tool offers only a length field.

### Theme (2026-10-17, `46874f16`, `88e0dfe3`)
- [ ] `Ctrl+Alt+T` opens your terminal again, not the theme; the theme is in the palette
      ("toggle theme") and Preferences > SolveSpace NG.
- [ ] Light theme: the drawing area is light, lines dark, hover orange; everything readable
      (dimensions, construction lines, errors, the selection).
- [ ] A line style colour you changed yourself stays when switching theme.
- [ ] The theme and canvas colours survive a restart.

### Grab (2026-10-17)
- [ ] While grabbing (`G`), pan with a right-drag, zoom with the wheel and orbit with the
      middle button: the grab carries on, and the points stay put until you move the mouse.
- [ ] A right *click* still cancels a grab.
- [ ] Undo something, grab, cancel with Esc: Redo still brings back what you undid.
- [ ] In a workplane sketch, `Z` does nothing while grabbing and the hint shows only `X Y`.
- [ ] `G` with nothing selected grabs what's under the mouse and leaves nothing selected.

### Pie menus (2026-10-17)
- [ ] A click in a pie's centre closes it; so does its own key pressed again (`Shift+A`,
      `F`, `` ` ``). Holding the key down keeps it open.

### Menus (2026-10-17)
- [ ] With a menu-bar dropdown open, letters don't start tools; arrows move, Esc closes it;
      `Ctrl` shortcuts still work.
- [ ] Moving the pointer from a highlighted line onto a panel un-highlights the line.

### Sketch focus (2026-10-17)
- [ ] `Tab` over the XY/YZ/ZX planes doesn't hide your groups; `Tab` while drawing an arc or
      spline does nothing.

### Tool shelf (2026-10-17, M1d)
Screenshots: `docs/owner-review/shelf-dark.png`, `shelf-light.png`, and every icon in
`docs/owner-review/icons.png`. Redrawn 2026-10-17 after researching other CAD apps
(`docs/research/cad-icon-conventions.md`): sketch tools show their points, constraints are their
on-sketch glyphs (`—` `|` `//` `⊥` `=` `>¦<`), features follow Fusion/Onshape/FreeCAD.
Third pass (v3): miniatures of the drawing area in its own line-style colours (as stock
SolveSpace's toolbar), features as shaded solids in the theme accent; `generate.py` writes them.
- [ ] View ▸ Show Toolbar shows the "Tools" shelf at the top; the old left toolbar is gone.
- [ ] Each button starts its tool; hovering shows its name and key.
- [ ] The icons read clearly in both themes and look like what you know from other CAD.
- [ ] Placement: centred at the top between the Outliner and Properties. Alternative: a
      vertical strip at the left edge (where the old toolbar was), with the Outliner moved right.
- [ ] Shrinking the window keeps every panel on screen.

### View cube (2026-10-17)
Screenshots: `docs/owner-review/viewcube-dark.png`, `viewcube-light.png`.
- [ ] The cube sits in the bottom left with the X/Y/Z axes coming out of it (labelled), and
      turns with the view.
- [ ] Clicking a face looks straight at it (same as the numpad views); an edge or corner looks
      between its faces; the part under the mouse is highlighted first.
- [ ] Dragging the cube orbits the view; the drag never selects or moves anything.
- [ ] The axes beside the cube can still be selected for constraints (e.g. parallel to Z),
      and clicks just off the cube reach the drawing.
- [ ] It is 80% opaque until the mouse is on it, then solid.
- [ ] In a small window it moves right of the Outliner when there is room.

### Review answers P1-P14 (2026-10-17, `f20994bb`..`a8b41809`)
From `docs/review/2026-10-17/answers.md`; the next review round shows them again.
- [ ] Menus: File has an Export submenu and Quit; View has Standard Views, the panels with
      check marks, and Light Theme; Analyze is Inspect; Help lists the keyboard shortcuts. Each
      item shows the key of the current keymap (switch to Classic and look again).
- [ ] Outliner: the first row is "Origin"; every group has its type icon and a DOF column
      that shows 0 when fully constrained.
- [ ] Properties: plain labels, lengths with their unit at the display precision; a
      constraint's type reads "Distance", "Vertical"...
- [ ] Styles and Preferences: readable style names; the SolveSpace NG section is first.
- [ ] Pies (`Shift+A`, `F`): each item has its icon.
- [ ] Right-click a selection: Grab, Dimension, Constrain..., Hide Group come first, with their
      keys; "Group Properties" / "Style Properties" replace the Info items.
- [ ] Problems: the group by its name; culprits as "Vertical (c003)" with their icon.

### Keys and layout from review 4 (2026-10-21, `e549b980`, `1cd86823`)
- [ ] Modern keys: `L R C A S P` draw, `H V I B/T Q M` constrain, `X` toggles construction,
      `Y`/`Shift+Y`/`Shift+I` hide/show/isolate groups, `Ctrl+A` selects all, `Shift+1..7`
      views, `W` looks at the sketch, `Alt+R` lathe, `Alt+E` helix. The menus show these keys.
- [ ] In a text field (a group name, the palette), `Ctrl+A` selects the text, not the drawing.
- [ ] Outliner over Properties on the right; View ▸ Tool Shelf is a column on the left (three
      wide in a short window); Problems opens at the bottom beside them, not over Properties.
      Panels you moved before start from the new places once.

### Constraint glyphs by state (2026-10-21)
Screenshots: `docs/owner-review/glyphs.png` (under, fully constrained, failed; dark and light).
- [ ] An under-constrained sketch's glyphs are the usual magenta; once fully constrained they
      go quieter; a failed solve's culprits turn light red on the red canvas.
- [ ] Preferences ▸ SolveSpace NG ▸ "colour constraints by state" turns it off.

### M2: solids made in the background while dragging (2026-10-21, branch `ng/m2-kernel`)
- [ ] Drag a point of an extrusion (or grab it with `G`) in a heavier model: the sketch
      follows the mouse at once; the solid follows a moment later, drawn dimmed meanwhile.
- [ ] Let go: the solid is exact; saving and exporting right after give the final shape.

## Decisions waiting for the owner
- Merge order: agreed 2026-10-21; `master` fast-forwarded through `ng/m1a-shell` ... `ng/m2-kernel`
  > `ng/m1-remove-frontends`.
- `pr/` branches to review: `undo-grouporder-leak`, `headless-null-guards`,
  `diameter-label-leader`, `hover-list-leak`, `paste-scale-crash`, `line-chain-ending`
  (stacked on `headless-null-guards`).
- M1c proposals (AGENTS.md §5): `O` / `N` / `Shift+P` panel keys, no keyboard navigation in
  panels, Problems opening itself.
- M1c task 10 (delete the old frontends): done 2026-10-21 with your go-ahead. Still to do: a
  macOS bundle and Windows resources for `solvespace-ng`, checked on those platforms.
- M3: Open CASCADE as a submodule (measured: see `docs/research/occt-vendoring.md`).

## Open issues (known, being worked through)

Found by reviews and tests; fixed ones move to the hands-on list above with their commit.

| Issue | Status |
|---|---|
| Right-drag pan during a grab cancels it | fixed |
| Zoom or orbit during a grab makes points jump | fixed |
| Pie: a click in the centre dead zone, or the pie's key again, doesn't close it | fixed |
| Keys pressed while a menu-bar dropdown is open still run commands | fixed |
| Hover stays on the canvas item when the pointer moves onto a panel | fixed |
| Cancelling a grab clears the redo stack | fixed |
| `Z` axis lock does nothing in a workplane | fixed |
| Isolate (`Shift+H`) can hide the active group | as intended: isolate shows only that group (as in Blender and Onshape), and drawing in the hidden active group shows it again (tested) |
| Grab's hover fallback leaves the item selected | fixed |
| `Tab` over `#references` hides your groups; `Tab` while drawing an arc/spline switches group | fixed |
| A typed angle outside a workplane is silently ignored | fixed: no angle field outside a workplane |
| Debug script (`SOLVESPACE_NG_SCRIPT`) drops typed characters | fixed |
| Mouse wheel ignores "natural scrolling" (flipped direction) | as intended: SDL already gives the direction the OS asks for, as stock SolveSpace gets it from GTK |
| A triple-click counts as a second double-click | as intended: it only re-opens what the double-click opened; a plain press would close a dimension's editor instead |
| `#` can't be bound in `keymap.txt`; a UTF-8 BOM breaks its first line | fixed |

### Internal (no visible effect; for developers)

| Issue | Status |
|---|---|
| GL objects deleted after the GL context at exit (canvases, window framebuffers) | fixed: freed in `ClearGui` first (no automated test: the harness never exits) |
| C++20 deprecation warnings: enum/float arithmetic in `textwin.cpp` (the old browser, deleted in M1c task 10) and a `libdxfrw` copy assignment in `exportvector.cpp` | harmless; left |
| `settingsfile` remove/rename use narrow paths: non-ASCII Windows profile paths fail | open (needs a Windows check) |
| Test nits: `every_menu_command_has_a_name` covers only menu commands; the expected-failure test prints `FAILED` on every run | open |
