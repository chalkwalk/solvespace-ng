# Keymap pass after M3-M5 (2026-10-22)

AGENTS.md section 5, decided by the owner in review 5: once most M3-M5 features exist, rethink
every binding by how often it is used and by industry standards; frequent commands get bare
keys unless a qualified chord is very widely used; explore a grammar for the qualifiers (the
bare key `N` is the command, `Shift+N`, `Alt+N`, `Ctrl+N` its relatives along a named dimension
each) and articulate those dimensions before rebinding. Nothing here is built; the review round
`~/solvespace-ng-blind/2026-10-22-keymap-pass` (spec `docs/review/2026-10-22/keymap-pass.json`)
asks for the decisions. The tables come from `tools/review/keymap_pass.py`; the industry
comparison they lean on is `docs/research/keymap-and-layout.md`.

## Summary

- **Tiers (P1):** four frequencies, each with a key rule: tier 1-2 bare letters, tier 3 a
  family's Shift/Alt or a pie, tier 4 the palette.
- **Grammar (P2):** bare = the family's most used command; Shift = its counterpart (inverse,
  reverse, reference); Alt = its variant (same family, another shape); Ctrl = the document and
  the app; pies for families too big for three keys.
- **The cost (P3):** today twelve Shift chords are "another command starting with this letter"
  (Onshape's habit). The grammar moves them to their family's slot or to the palette; the one
  big move is F to fillet (as Fusion and Shapr3D) and the constrain pie to K.
- **New keys (P4):** a joint pie on J, design pose on Shift+J, new part on Ctrl+Shift+N.
- **Exceptions (P5):** six qualified chords kept because they are widely known or have no slot.

### P1. How often each command is used (modelling printed parts)

| Tier | Commands | Key rule |
|---|---|---|
| 1  every few seconds | line, dimension, coincident, horizontal, vertical, construction, delete, undo, grab, Esc, palette, sketch focus, extrude | bare letter |
| 2  every minute or so | circle, rectangle, arc, point, equal, parallel/tangent, perpendicular, midpoint, symmetric, fillet, views, frame, hide, save | bare letter, or the letter's Shift/Alt |
| 3  a few times a session | revolve, chamfer, hole, reference/angle dimension, tangent arc, spline, sketch on a face, new part, joints, fit joint, design pose, isolate, show all, panels | a family's Shift/Alt, or a pie |
| 4  rarely | lathe, helix, loft, sweep, shell, draft, patterns, split part, text, gear, measure, export, units, trace, mass properties, parts list, fit table | palette and menus only |

No telemetry exists; the tiers are judged from the workflow and from which commands Fusion, Onshape and Shapr3D give bare keys.

### P2. The qualifier grammar: bare N is the command, the qualifiers its relatives

| Qualifier | Dimension | Fits it now | Breaks it now |
|---|---|---|---|
| bare | the most used command of the letter's family | L line, D dimension, E extrude, H, V | - |
| Shift | its counterpart: inverse, reverse or reference | Shift+D reference dim, Shift+Y show all | Shift+L perpendicular, Shift+S symmetric, Shift+R revolve, Shift+F fillet, Shift+H hole, Shift+I isolate, Shift+W, Shift+T, Shift+J, Shift+P, Shift+G, Shift+X |
| Alt | its variant: same family, another shape | Alt+D angle, Alt+R lathe, Alt+E helix | Alt+C tangent arc (an arc, not a circle), Alt+A unselect |
| Ctrl | the document and the app, as everywhere | Ctrl+S, Ctrl+Z, Ctrl+A, Ctrl+I link | - |
| pie | a family too big for three keys: Shift+A add, ` views, K constrain, J joints | Shift+A, `, F | - |

Today 12 Shift chords name a second command that merely starts with the letter (Onshape's habit); under the grammar each moves to its family's slot or to the palette.

### P3. What moves under the grammar (option a)

| Command | Now | Proposed | Why that slot |
|---|---|---|---|
| Fillet | Shift+F | F | tier 2; F in Fusion and Shapr3D |
| Chamfer | - | Alt+F | fillet's variant |
| Constrain pie | F | K | FreeCAD's constraint letter; frees F |
| Perpendicular | Shift+L | Alt+B | parallel's variant |
| Symmetric | Shift+S | Alt+M | midpoint's variant (about a line) |
| Tangent arc | Alt+C | Alt+A | arc's variant |
| Unselect all | Alt+A, Esc | Esc | Alt+A is the tangent arc |
| Revolve | Shift+R | Alt+E | extrude's variant: the sketch swept round |
| Extrude as a cut | - | Shift+E | extrude's counterpart (a new command) |
| Lathe / helix | Alt+R / Alt+E | palette | tier 4 |
| Isolate | Shift+I | Alt+Y | hide's variant (Shift+Y shows all) |
| Sketch in new workplane | Shift+W | Alt+W | W's variant: a new sketch plane |
| Lock where dragged | Shift+J | K pie | tier 4; J becomes joints |
| Problems | Shift+P | F8 | VS Code's next problem; it opens itself anyway |
| Grid | Shift+G | ` pie | a view setting |
| Text | Shift+T | palette | tier 4 |

Unchanged: L R C A P S, D Shift+D Alt+D, H V I B/T Q M U, X, G, Y Shift+Y, N O, W, Tab, Space, Shift+A, `, the digits and every Ctrl chord.

### P4. Keys for what M3-M5 added

| Command | Now | Proposed | Why |
|---|---|---|---|
| **Joint pie: revolute, slider, cylindrical, planar, ball, gear, fit** | **-** | **J** | **J for joint; one pie, as Shift+A** |
| **Design pose** | **-** | **Shift+J** | **the joints' counterpart: all back to 0** |
| **New part** | **-** | **Ctrl+Shift+N** | **Ctrl = the document; next to Ctrl+N** |
| Hole | Shift+H | Shift+H (exception) | no family fits; tier 3 for printing |
| Fillet / chamfer | Shift+F / - | F / Alt+F | see P3 |
| Shell, draft, loft, sweep, split part | - | palette | tier 4 |
| Mass properties, parts list, fit table | - | palette | tier 4 |

The joint pie replaces six menu-only commands; G on a jointed part still moves it.

### P5. Qualified chords kept against the grammar

| Chord | Command | Why keep it | Else |
|---|---|---|---|
| Shift+A | add pie | Blender's, known to every Blender user | - |
| Shift+1..7 | standard views | Onshape's; laptops have no numpad | - |
| Ctrl+Num1/3/7 | opposite views | Blender's | - |
| Shift+H | hole | tier 3, no family slot | palette |
| Shift+X | split curves | trimming is tier 2-3 in sketches | palette |
| Ctrl+Shift+Z, Ctrl+Y | redo | everywhere | - |

Option a keeps all six; option b sends hole and split curves to the palette.

### P6. Every letter after the pass (option a)

| Key | Bare | Shift | Alt |
|---|---|---|---|
| A | arc | add pie | **tangent arc** |
| B | parallel / tangent |   | **perpendicular** |
| C | circle |   | **-** |
| D | dimension | reference dim | angle |
| E | extrude | **extrude cut** | **revolve** |
| F | **fillet** | **-** | **chamfer** |
| G | grab | **-** |   |
| H | horizontal | hole |   |
| I | coincident / on | **-** |   |
| J | **joint pie** | **design pose** |   |
| K | **constrain pie** |   |   |
| L | line | **-** |   |
| M | midpoint |   | **symmetric** |
| N | Properties |   |   |
| O | Outliner |   |   |
| P | point | **-** |   |
| Q | equal |   |   |
| R | rectangle | **-** | **-** |
| S | spline | **-** |   |
| T | parallel / tangent | **-** |   |
| U | other angle |   |   |
| V | vertical |   |   |
| W | look at sketch | **-** | **new workplane** |
| X | construction | split curves |   |
| Y | hide group | show all | **isolate** |
| Z |   |   |   |

Highlighted: changed. Free for later: Z, C/R/S/L Shift and Alt, G/I/P/T Shift. Ctrl chords, digits, Tab, Space, ` and . are as now.

