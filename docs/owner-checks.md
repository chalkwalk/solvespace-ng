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

### Theme (2026-10-17, `46874f16`, `88e0dfe3`)
- [ ] `Ctrl+Alt+T` opens your terminal again, not the theme; the theme is in the palette
      ("toggle theme") and Preferences > SolveSpace NG.
- [ ] Light theme: the drawing area is light, lines dark, hover orange; everything readable
      (dimensions, construction lines, errors, the selection).
- [ ] A line style colour you changed yourself stays when switching theme.
- [ ] The theme and canvas colours survive a restart.

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
| Right-drag pan during a grab cancels it | open |
| Zoom or orbit during a grab makes points jump | open |
| Pie: a click in the centre dead zone, or the pie's key again, doesn't close it | open |
| Keys pressed while a menu-bar dropdown is open still run commands | open |
| Hover stays on the canvas item when the pointer moves onto a panel | open |
| Cancelling a grab clears the redo stack | open |
| `Z` axis lock does nothing in a workplane | open |
| Isolate (`Shift+H`) can hide the active group | open |
| Grab's hover fallback leaves the item selected | open |
| `Tab` over `#references` hides your groups; `Tab` while drawing an arc/spline switches group | open |
| A typed angle outside a workplane is silently ignored | open |
| Debug script (`SOLVESPACE_NG_SCRIPT`) drops typed characters | open |
| Mouse wheel ignores "natural scrolling" (flipped direction) | open |
| A triple-click counts as a second double-click | open |
| `#` can't be bound in `keymap.txt`; a UTF-8 BOM breaks its first line | open |
