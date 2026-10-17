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

## Decisions waiting for the owner
- Merge order: `ng/m1a-shell` > `ng/m1b-interaction` > `ng/m1-ui-harness` > `ng/m1c-panels` >
  `ng/m1c-feedback` > `master`.
- `pr/` branches to review: `undo-grouporder-leak`, `headless-null-guards`,
  `diameter-label-leader`, `hover-list-leak`, `paste-scale-crash`, `line-chain-ending`
  (stacked on `headless-null-guards`).
- M1c proposals (AGENTS.md §5): `O` / `N` / `Shift+P` panel keys, the hidden old browser, no
  keyboard navigation in panels, Problems opening itself.
- M1c task 10 (delete the old frontends): needs your go-ahead, after deciding what replaces
  the left toolbar and the icon / constraint-glyph restyle.

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
