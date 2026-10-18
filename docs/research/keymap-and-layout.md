# Keymap and layout research (2026-10-17)

For the owner's notes in `docs/review/2026-10-17/answers.md`: "inspired by the likes of vim: as
many unqualified controls as possible, but also overlapping with industry standard as much as we
can muster", reasonable Helix and Lathe keys, and "could Outliner and Properties stack on the
left, with the tool shelf vertical on the left?". Nothing here is built yet; the review round
`~/solvespace-ng-blind/2026-10-17-keys-layout` asks for the decisions (K1-K6, L1).

## Sources

- Onshape: the official list, [Keyboard Shortcuts and Hotkeys](https://cad.onshape.com/help/Content/shortcut_keys.htm) (complete; the most thorough sketch set of any app).
- Shapr3D: the Windows list on [DefKey](https://defkey.com/shapr3d-windows-shortcuts), consistent with
  Shapr3D's [help article](https://support.shapr3d.com/hc/en-us/articles/7873906073884) ("Single Key Action" hotkeys are the default).
- Plasticity: the official manual, [Essentials](https://doc.plasticity.xyz/all-commands/plasticity-essentials),
  [Common commands](https://doc.plasticity.xyz/all-commands/common-commands) and
  [Tool commands](https://doc.plasticity.xyz/all-commands/tool-commands) (G move, R rotate, S scale, T trim, H hide family, numpad views).
- Fusion: Autodesk's [shortcut page](https://www.autodesk.com/shortcuts/fusion-360) (L, R, C, D, T, E confirmed through a search; the page itself refuses direct fetches).
- FreeCAD: the wiki ([ConstrainDistance](https://wiki.freecad.org/Sketcher_ConstrainDistance) K,D; [ConstrainVertical](https://wiki.freecad.org/Sketcher_ConstrainVertical) V) and
  [scan2cad's list](https://www.scan2cad.com/blog/cad/freecad-keyboard-shortcuts/) (0.18, partly outdated). FreeCAD 1.0 uses two-key families: G,x for geometry and K,x for constraints.
- SOLIDWORKS binds almost nothing by default (S opens a shortcut bar), so it isn't a column.
- Blender and SolveSpace Classic: Blender's default keymap; Classic from `src/graphicswin.cpp`.

Entries with `*` come from memory or a third-party list and weren't re-checked against the app.

## What the apps agree on

- **Sketch tools on bare letters**: L line, R rectangle, C circle (Fusion, Onshape, Shapr3D;
  Classic has R and C), A arc (Onshape, Shapr3D, Classic), D dimension (all of them).
- **E extrudes** in Fusion, Shapr3D, Plasticity and Blender; Onshape uses Shift+E.
- **Constraints on letters** in the sketch-centred apps: H horizontal and V vertical (Onshape,
  Classic, FreeCAD), plus I coincident, T tangent, B parallel, M midpoint (Onshape).
- **Hide** is H in Blender and Plasticity, Y in Onshape: the one real clash with H horizontal (K2).
- **Select all** is Ctrl+A everywhere except Blender and Plasticity (A).
- **Views**: the numpad in Blender and Plasticity, Shift+1..7 in Onshape, Ctrl+digits in Shapr3D,
  bare digits in FreeCAD. Laptops lack a numpad, so a digit row binding helps.
- **Grab/move** is G in Blender and Plasticity (Fusion and Shapr3D use M for move).
- **Lathe and helix have no default key in any app.**

## The proposal

The Modern preset keeps its Blender core (Space palette, Shift+A add pie, F constrain pie, G grab,
Tab sketch focus, numpad views, N and O panels) and moves the sketch and constraint keys to the
industry letters, Onshape's where the apps differ, since its sketch set is the most complete.

### K1. Sketch tools on the industry letters

| Action | Classic | Fusion | Onshape | Shapr3D | Plasticity | FreeCAD | Blender | NG now | Proposed |
|---|---|---|---|---|---|---|---|---|---|
| Line | S | L | L | L | Shift+A | G,L* | - | L | L |
| Rectangle | R | R | G / R | R | - | G,R* | - | B | R |
| Circle | C | C | C | C | - | G,C* | - | C | C |
| Arc | A | - | A | A | - | G,A* | - | Shift+C | A |
| Tangent arc | Shift+A | - | Shift+A (1) | - | - | - | - | Alt+C | Alt+C |
| Point | P | - | Shift+S | - | - | - | - | P | P |
| Spline | B | - | - | I | - | G,B* | - | Shift+B | S |
| Text | T | - | - | - | - | - | - | Shift+T | Shift+T |
| Select all | Ctrl+A | Ctrl+A* | - | Ctrl+A | A | Ctrl+A* | A | A | Ctrl+A |

(1) Onshape: switches line / tangent arc while drawing. A frees Ctrl+A select-all as everywhere but Blender.

### K2. Horizontal / vertical against hide

| Action | Classic | Fusion | Onshape | Shapr3D | Plasticity | FreeCAD | Blender | NG now | Proposed |
|---|---|---|---|---|---|---|---|---|---|
| Horizontal | H | - | H | Shift+V (2) | - | K,H* | - | F, 1 | a: H  / b: F, 1 |
| Vertical | V | - | V | Shift+V (2) | - | V | - | F, 2 | a: V  / b: F, 2 |
| Hide | - | V* | Y | - | H | Space* | H | H | a: Y  / b: H |
| Show all | - | - | Shift+Y | - | Alt+H | - | Alt+H | Alt+H | a: Shift+Y / b: Alt+H |
| Isolate | - | - | Shift+I | - | . | - | / | Shift+H | a: Shift+I / b: Shift+H |

(2) Shapr3D: one Horizontal/Vertical tool. a = Onshape's keys (H/V constraints, Y hides); b = keep Blender's H family.

### K3. The other constraints on letters

| Action | Classic | Fusion | Onshape | Shapr3D | Plasticity | FreeCAD | Blender | NG now | Proposed |
|---|---|---|---|---|---|---|---|---|---|
| Coincident / on | O | - | I | Shift+N | - | K,C* | - | F pie | I |
| Parallel / tangent | L | - | B / T | Shift+A / Shift+T | - | K,P* / K,T* | - | F pie | B, T |
| Perpendicular | [ | - | Shift+L | Shift+P | - | K,N* | - | F pie | Shift+L |
| Equal | Q | - | E | Shift+E | - | K,E* | - | F pie | Q |
| Symmetric | Y | - | Shift+Q | Shift+S | - | K,S* | - | F pie | Shift+S |
| Midpoint | M | - | Shift+M | Shift+M | - | - | - | F pie | M |
| Lock where dragged | ] | - | Shift+J | Shift+L | - | K,L* | - | - | Shift+J |
| Dimension / angle | D / N | D | D | - | = | K,D | - | D | D (unchanged) |

Onshape's E (equal) is our extrude, so equal keeps Classic's Q. B and T both run Parallel / Tangent.

### K4. Construction and delete

| Action | Classic | Fusion | Onshape | Shapr3D | Plasticity | FreeCAD | Blender | NG now | Proposed |
|---|---|---|---|---|---|---|---|---|---|
| Toggle construction | G | X | Q | - | - | G,N* | - | T | X |
| Delete | Delete | Delete | Delete, Backspace | Backspace | Delete* | Delete | X, Delete | Delete, X | Delete, Backspace |
| Unselect all | Esc | Esc* | Space | Esc | Esc | Esc* | Alt+A | Alt+A, Esc | Alt+A, Esc |

X toggles construction as in Fusion; X no longer deletes (Blender's habit). Backspace deletes as in Onshape/Shapr3D.

### K5. Groups: extrude, revolve, lathe, helix

| Action | Classic | Fusion | Onshape | Shapr3D | Plasticity | FreeCAD | Blender | NG now | Proposed |
|---|---|---|---|---|---|---|---|---|---|
| Extrude | Shift+X | E | Shift+E | E | E* | - | E | E | E |
| Revolve | Shift+V | - | Shift+W | V | - | - | - | Shift+R | Shift+R |
| Lathe (full turn) | Shift+L | - | - | - | - | - | - | - | Alt+R |
| Helix | Shift+H | - | - | - | - | - | - | - | Alt+E |
| Sketch in workplane | Shift+W | - | Shift+S | - | - | - | - | Shift+W | Shift+W |
| Patterns | Shift+T / Shift+R | - | - | - | - | - | - | - | palette only |

No app binds lathe or helix by default; Alt+R (revolve, all the way) and Alt+E (extrude, turning) are mnemonic proposals.

### K6. Views without a numpad

| Action | Classic | Fusion | Onshape | Shapr3D | Plasticity | FreeCAD | Blender | NG now | Proposed |
|---|---|---|---|---|---|---|---|---|---|
| Front | F2 (3) | -* | Shift+1 | Ctrl+2 | Num1 | 1 | Num1 | Num1 | Num1, Shift+1 |
| Back | - | -* | Shift+2 | Ctrl+3 | Ctrl+Num1 | 4 | Ctrl+Num1 | Ctrl+Num1 | Ctrl+Num1, Shift+2 |
| Left / right | - | -* | Shift+3 / 4 | Ctrl+7 / 6 | Ctrl+Num3 / Num3 | 6 / 3 | Ctrl+Num3 / Num3 | Ctrl+Num3 / Num3 | + Shift+3 / 4 |
| Top / bottom | - | -* | Shift+5 / 6 | Ctrl+4 / 5 | Num7 / Ctrl+Num7 | 2 / 5 | Num7 / Ctrl+Num7 | Num7 / Ctrl+Num7 | + Shift+5 / 6 |
| Isometric | F3 (3) | -* | Shift+7 | Ctrl+1 | - | 0 | - | View pie | Shift+7 |
| Look at sketch | W | -* | N | - | - | V,... * | Num7 (4) | Tab (focus) | W, Tab |

(3) Classic's nearest ortho / iso. (4) Blender's top view. Shift+digits are free while typing values (only bare digits go to the fields).

### Letters after K1-K6 (option a everywhere)

| | Bare | Shift | Alt |
|---|---|---|---|
| A | arc | add pie | unselect all |
| B | parallel / tangent | | |
| C | circle | | tangent arc |
| D | dimension | reference dimension | angle |
| E | extrude | | helix |
| F | constrain pie | | |
| G | grab | grid | |
| H | horizontal | | |
| I | coincident / on | isolate | |
| J | | lock where dragged | |
| L | line | perpendicular | |
| M | midpoint | | |
| N | Properties | classic browser | |
| O | Outliner | | |
| P | point | Problems | |
| Q | equal | | |
| R | rectangle | revolve | lathe |
| S | spline | symmetric | |
| T | parallel / tangent | text | |
| U | other supplementary angle | | |
| V | vertical | | |
| W | look at sketch | sketch in new workplane | |
| X | toggle construction | | |
| Y | hide group | show all | |
| 1-7 | (typed values) | standard views | |

Free for later: J, K, Z and most Shift/Alt letters. When fillet and chamfer arrive (M3), F is
Fusion's and Shapr3D's fillet key; the constrain pie could then move (decide then).

## Layout

How the apps arrange the tree, the properties and the tools (their default layouts, from memory and the screenshots gathered for `cad-icon-conventions.md`; not re-checked app by app):

| App | Tree | Properties | Tools |
|---|---|---|---|
| Fusion | left | dialog over the canvas, top right | ribbon, top |
| Onshape | left | takes the tree's place while editing | toolbar, top |
| SOLIDWORKS | left | a tab beside the tree (same pane) | CommandManager, top |
| FreeCAD | left, top half | left, bottom half (stacked) | toolbars, top |
| Blender | right, top | right, below the tree (stacked) | vertical column, left |
| SketchUp | right tray (stacked panels) | right tray | vertical tool set, left |
| Plasticity | right | dialog, top left | vertical column, left |
| Shapr3D | left sidebar | right | vertical column, right |
| NG now | left | right | optional shelf, top |

Stacking tree and properties on one side is common (FreeCAD and SOLIDWORKS left; Blender and
SketchUp right); a vertical tool column is too (Blender, SketchUp, Plasticity, Shapr3D), always on
the side away from the tree. Mock-ups (`tools/review/layout_mockups.py`, made from a capture of
the app): A now; B the owner's idea, tools and both panels on the left; C tools left, panels
stacked right (Blender, SketchUp, Plasticity); D panels stacked left, tools along the top (FreeCAD,
SOLIDWORKS). Stacked panels are ~260 px wide, so Properties wraps or clips long option names
(visible in the mock-ups) unless the stack is wider. The view cube stays bottom left and moves
right of anything there, as it does now.
