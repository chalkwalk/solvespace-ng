# Owner review 2026-10-17: answers

From the blindweb rounds (`~/solvespace-ng-blind/2026-10-17-*`). The owner answered every
proposal and every menu, and the first five panels (the three-round layout repeated screens;
future reviews are one round, each screen once).

## Approved proposals (all yes)
P1 Export submenu · P2 Preferences..., View Projection to View · P3 View: panels group · P4
Standard Views submenu · P5 short, consistent names · P6 menu shows the key (notes: the keys
shown must always match the current keymap preset) · P7 Outliner type icons and DOF (note:
name the column and show 0 when fully constrained) · P8 values with units, display precision,
no duplicates · P9 plain labels · P10 readable style names · P11 NG preferences first · P12
pie icons · P13 context menu names and NG actions · P14 Problems names with icons.

## Menu notes
- File: Export submenu, and Quit (not Exit).
- Edit: make the proposed changes.
- View: rethink: "take the greatest hits of the other software menus and SolveSpace NG".
- New Group: are there reasonable Helix and Lathe keys?
- Sketch: minimal but unambiguous names.
- Constrain / keys: "inspired by the likes of vim: as many unqualified controls as possible,
  but also overlapping with industry standard as much as we can muster: maybe we get shortcut
  lists across the board to design our ultimate modern shortcut set".
- Analyze: would Inspect (Fusion's name) make more sense?
- Help: add the keyboard shortcut list.

## Panel notes
- Whole window: rethink: could Outliner and Properties stack on the left, with the tool shelf
  vertical on the left? Look at other software for guidance.
- Outliner: go the Fusion route (Origin, type icons).
- Properties of a group: keep. Of a line: units make sense.
- Not reviewed: the rest of the panels and overlays (covered by the proposals).

## Next
1. Done: P1-P14 with the menu notes (Quit, Inspect, Help shortcut list), commits
   `f20994bb`..`a8b41809`; hands-on list in `docs/owner-checks.md`.
2. Research shortcut sets across Blender, Fusion, Onshape, SOLIDWORKS, FreeCAD, Shapr3D,
   Plasticity (and vim-style unmodified keys) for a proposed Modern keymap, with Helix/Lathe.
3. Research panel layouts (left-stacked Outliner + Properties, vertical tool column).
4. One review round for 2 and 3, each screen once.

Items 2-4: research in `docs/research/keymap-and-layout.md`; round
`~/solvespace-ng-blind/2026-10-17-keys-layout` (spec `keys-layout.json`) asks K1-K6, L1 and V1.

## Review 4 (keys and layout): answers

L1 **c** (tool column left, Outliner over Properties on the right). K1-K6 **a** (all as proposed:
`res/keymaps/modern.keymap`, AGENTS.md section 5). V1 **a** (the View menu is good).

The owner's questions, answered:
- *Tangent arc is Shift+A elsewhere, why Alt+C?* Shift+A is Blender's Add pie, which stays;
  Alt+C keeps it beside C (circle). Onshape's Shift+A also only switches line/tangent arc while
  drawing a line.
- *S for spline isn't standardised: is it used elsewhere?* No: only Shapr3D binds spline (I),
  and I is Onshape's coincident. S was free (Blender's scale has no SolveSpace counterpart).
- *T vs Shift+T?* T is parallel/tangent (Onshape's tangent; B also works); text, rarely used,
  is Shift+T. If a trim tool arrives, T is Fusion's/Plasticity's/Shapr3D's trim: decide then.
- *Why Alt+A as well as X?* Alt+A isn't construction: it is unselect-all (Blender's), beside
  Esc. X alone toggles construction.
Unasked: Split at Intersections lost I (now coincident) and moved to Shift+X.
