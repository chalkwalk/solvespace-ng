# Icon conventions in CAD and modelling software

Research for the NG tool shelf icons (2026-10-17, owner: "the more familiar this can all be to
users of other CAD and modelling software, the easier"). Sources: the constraint and tool names
and docs of Fusion 360, Onshape and SOLIDWORKS, and the actual icon sets of FreeCAD (Sketcher
and PartDesign, LGPL; rendered here only to look at, nothing copied) and Dune 3D (GPLv3; its
in-canvas constraint glyphs). The renders and screenshots used are kept out of the repo (some
are not freely licensed); the agent that did this research kept them in its scratch space.

- Fusion 360: [Constraints in sketches](https://help.autodesk.com/cloudhelp/ENU/Fusion-Sketch/files/SKT-CONSTRAINTS.htm),
  [drawing sketch constraints](https://help.autodesk.com/cloudhelp/ENU/Fusion-Drawing/files/DWG-SKETCH-CONSTRAIN.htm)
- Onshape: [Sketch tools](https://cad.onshape.com/help/Content/sketch-tools.htm)
- SOLIDWORKS: [Sketch relations icons](https://help.solidworks.com/2017/English/solidworks/sldworks/c_Sketch_Relations_Icons.htm)
- FreeCAD: `src/Mod/Sketcher/Gui/Resources/icons`, `src/Mod/PartDesign/Gui/Resources/icons`
- Dune 3D: `src/texture-icons.svg`

## What the tools agree on

**Sketch geometry is drawn with its defining points as dots.** Line: a diagonal with both end
points. Rectangle: corner points. Circle: centre dot. Arc: end points and centre. Spline: a curve
with its control points off the curve. Point: one dot. This tells a sketch tool apart from a
constraint or a view icon at a glance.

**Text is a letter A** (FreeCAD, Fusion, SOLIDWORKS), not the word-processor "T".

**Construction is a dashed line** (FreeCAD shows a solid line becoming dashed; Onshape and Fusion
toggle a dashed line).

**Constraint icons are the glyph drawn on the sketch** (FreeCAD toolbar = its in-canvas marks;
Dune 3D's canvas glyphs; SOLIDWORKS relation icons). So learning one teaches the other:

| Constraint | Common symbol |
|---|---|
| Horizontal | a horizontal bar `—` |
| Vertical | a vertical bar `|` |
| Parallel | `//` |
| Perpendicular | `⊥` |
| Equal | `=` |
| Symmetric | two chevrons facing a centre line `>¦<` |
| Point on curve | a dot on a line |
| Midpoint | a dot (or triangle) in the middle of a line |
| Coincident | two points made one |
| Tangent | a circle touching a line |
| Concentric | two circles, one centre |
| Fix / lock | a padlock |
| Same orientation | two parallel arrows `⇈` |
| Dimension (smart) | a dimension line with arrows between extension lines `|←→|` |
| Angle | two rays with an arc |
| Reference (driven) dimension | the value in parentheses `( )` (SOLIDWORKS shows driven dimensions so) |

**Features:** extrude is a profile with a solid rising from it and an up arrow; revolve is a
profile swept around a dashed axis with a turning arrow (full turn for SolveSpace's lathe, part
of a turn for revolve); helix is a coil around an axis; linear pattern is copies in a row with an
arrow; circular pattern is copies on a circle; a new sketch is a pencil on a plane; inserting a
part is a box with an arrow into it.

**Views:** isometric is a cube (the ViewCube); "normal to" / "look at" is a plane with an arrow
square onto it.

## Applied to NG

Second pass (owner review, 2026-10-17): SolveSpace's own toolbar icons already follow these
conventions *and* look like the drawing area (white lines, green square points, magenta
constraint marks such as `H`, `V`, `|<-10->|`, `REF`), which ties each tool to what it draws;
and FreeCAD's shaded feature icons read as 3D far better than outlines. So the NG icons
(`res/icons/ng/generate.py`) are vector redraws in that spirit: sketch tools and constraints are
miniatures of the canvas, coloured at run time from the drawing's line styles (Active group,
Datum, Constraint, Construction), and features are small solids shaded in the theme's accent
colour. Changing the theme or a line-style colour recolours the icons. SolveSpace-only commands
get the nearest convention (Split Curves at Intersection: crossing lines with the split point;
Other Supplementary Angle: crossing lines with the angle on the other side).

## Panel layouts

From Onshape's help (its Part Studio screenshot) and freely licensed Wikimedia Commons
screenshots of Shapr3D, Fusion 360, FreeCAD 1.0 and Blender 4.3 (no usable interface
screenshots of SketchUp, SOLIDWORKS, Tinkercad's 3D editor or Plasticity were freely available;
their help pages describe them: [SOLIDWORKS UI overview](https://help.solidworks.com/2024/english/SolidWorks/sldworks/c_user_interface_overview.htm),
[Plasticity UI overview](https://doc.plasticity.xyz/plasticity-essentials/plasticity-interface/user-interface-overview),
[Shapr3D Items Manager](https://support.shapr3d.com/hc/en-us/articles/7873936188956),
[Onshape UI basics](https://cad.onshape.com/help/Content/Home/user_interface_basics.htm)):

| App | Model tree / history | Tools | View cube | Properties |
|---|---|---|---|---|
| Onshape | left feature list | top toolbar (Sketch first) | top right | feature dialog over the list |
| Fusion 360 | top-left browser, timeline at the bottom | top ribbon in labelled groups | top right | floating dialog |
| SOLIDWORKS | left FeatureManager tree | top CommandManager | (heads-up toolbar top) | PropertyManager in the left pane |
| Shapr3D | right History / Items sidebar | slim left icon column | top right | contextual |
| Plasticity | left Outliner | left tool column, command palette | top right | right sidebar |
| SketchUp | Outliner in the right tray | top / left toolbars | — | Entity Info in the right tray |
| Tinkercad | — | top toolbar; shapes panel right | top left | inspector over the shape |
| FreeCAD 1.0 | left model tree | top toolbars | top right | left (property view) |
| Blender | right Outliner | left tool column | top right gizmo | right Properties |

So: tree on the left (Onshape, Fusion, SOLIDWORKS, Plasticity, FreeCAD) or right (Shapr3D,
SketchUp, Blender); tools along the top in CAD (Fusion, Onshape, SOLIDWORKS, FreeCAD), a slim
left column in the modelling-first apps (Shapr3D, Plasticity, Blender); a view cube top right in
almost all. NG today: Outliner left, tool shelf top, Properties right, no view cube.
