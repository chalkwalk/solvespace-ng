# AGENTS.md — SolveSpace NG

Instructions and agreed design for AI coding agents (Claude, Gemini, Codex, …) working in this
repository. `CLAUDE.md` and `GEMINI.md` are symlinks to this file — edit only `AGENTS.md`.

This repository is a **long-lived fork of SolveSpace** (working name **SolveSpace NG**, binary
`solvespace-ng`) that keeps SolveSpace's constraint-based modelling core and gives it a modern,
keyboard-first UI, a pluggable geometry kernel (SolveSpace → Open CASCADE), and a multi-body
assembly workflow for 3D-printed parts and mechanisms. It is primarily a personal tool and may
later be promoted to an independently named project.

Design agreed 2026-10-15. Sections marked **Decision** are settled; change them only with the
owner's approval.

---

## 1. Goals and non-goals

**Goals**
- Keep what makes SolveSpace good: constraint-based parametric modelling, a fast solver, live
  dragging, and heavy keyboard use.
- A modern look and feel in the spirit of **Plasticity / Shapr3D** (minimal chrome, big viewport,
  floating panels), with **Blender-style** keyboard speed (command palette, pie menus, modal tools
  with typed values).
- Fillets, chamfers and other Open CASCADE (OCCT) features **without** giving up the constraint
  workflow: features can sit anywhere in the feature chain and still expose constrainable
  geometry.
- Multi-part design in one file: parts that reference each other, interlocking fit joints with
  printer-tuned clearances, and mechanisms that can be dragged live.
- Platforms: Linux, Windows, macOS. Web (WASM) stays supported where practical, but the web
  build may lack OCCT-only features (the owner accepts losing web for fillets/chamfers).

**Non-goals**
- Direct modelling (push/pull, face offsets, defeaturing). These edit topology instead of
  parameters and break the constraint paradigm.
- Upstream contribution. Upstream's review rules (`.github/copilot-instructions.md`) don't govern
  this fork, but we still merge upstream fixes (see §9).

## 2. Licensing — Decision

- SolveSpace, including the solver (`libslvs`), is **GPLv3**. All fork and app code stays
  GPLv3. Keep existing copyright headers and attributions.
- Code derived from SolveSpace can **not** be relicensed MIT. (The `MIT License` classifier in
  `pyproject.toml` is upstream metadata and should not be relied on. The solver compiled into the
  Python package is GPLv3.)
- New, independent modules written from scratch (keymap engine, palette and pie-menu widgets,
  theme tokens) may be **MIT** and must live under `libs/<name>/` with their own `LICENSE`. They
  must not include or link SolveSpace code.
- Allowed dependencies: SDL3 (zlib), Dear ImGui (MIT), lunasvg (MIT), Lucide icons (ISC), Inter
  and JetBrains Mono fonts (OFL), OCCT (LGPL-2.1 + exception), Eigen (MPL2). Code borrowed from
  Dune 3D (GPLv3) is fine with attribution. Avoid non-OSI or "non-commercial" licences, e.g. the
  Dear ImGui Test Engine.

## 3. Background facts (verified in this repo)

- `include/slvs.h` / `src/slvs/` is **only the 2D/3D constraint solver**. Groups,
  extrude/lathe/boolean, the NURBS and mesh kernel (`src/srf/`, `src/mesh.cpp`), the `.slvs` file
  format and export all live in `solvespace-core` (`src/CMakeLists.txt`, ~41k lines).
- The UI is tightly coupled to the core through the global `SS` and its `SS.GW` (graphics window)
  and `SS.TW` (text window), with 600+ references across `src/*.cpp`.
- The toolbar (`src/toolbar.cpp`) and the property browser ("text window", `src/textwin.cpp`,
  `src/textscreens.cpp`, `src/confscreen.cpp`, `src/describescreen.cpp`) are custom-drawn in
  OpenGL. The Qt6 frontend (`src/platform/guiqt.cpp`, retired 2026-10-21) only wrapped these. That is
  why a toolkit swap alone doesn't modernise the look.
- Commands: `enum class Command` in `src/ui.h` and the `MenuEntry` table with accelerators in
  `src/graphicswin.cpp`. Modal tool state lives in `GraphicsWindow::pending` (`enum class Pending`,
  `src/ui.h`).
- Solid generation: `Group::Generate()` (`src/group.cpp`) creates sketch-derived entities;
  `Group::GenerateShellAndMesh()` (`src/groupmesh.cpp`) builds solids. The second is the kernel
  seam.
- Assemblies today: `Group::Type::LINKED` (`src/sketch.h`) imports another `.slvs` file as a
  frozen rigid body with 6 DOF placement.
- Prior art: **Dune 3D** (GPLv3) = SolveSpace solver + OCCT + GTK4 UI. Learn from it; we chose
  not to fork it (GTK4-bound, and its UI is the layer we'd replace).

## 4. Architecture — Decision (Approach A: staged fork and refactor)

```
app/      SDL3 shell: window, GL ES3 context, input, HiDPI, file dialogs
ui/       Dear ImGui panels (custom theme) + viewport overlays: command palette,
          pie menus, modal-tool HUD, outliner, properties, problems, keymap editor
cmd/      Command registry, Keymap (presets: Modern, SolveSpace Classic),
          Tool state machine (from Pending), Selection model, Undo transactions
doc/      solvespace-core: sketch, groups, requests, constraints, solver, file I/O, export
kernel/   IKernel interface; SolveSpaceKernel (SShell/SMesh) and OcctKernel (desktop)
render/   existing Canvas abstraction + GL renderer, targeting GL ES 3.0 / WebGL2
```

Rules:
1. **Dependencies only point downward.** Nothing in `doc/`, `kernel/` or `render/` may reference
   `SS.GW`, `SS.TW`, or `ui/`. Move UI-coupled code upward one screen or command at a time. Never
   do a big-bang rewrite.
2. **Every user action is a registered command** (`id`, name, description, `enabled(ctx)`,
   `run(ctx)`). Menus, palette, pies, toolbar and keymaps are views of that registry. Every
   command must be runnable headlessly from a script.
3. **The solver and the `.slvs` format stay backward compatible.** Old files must open unchanged.
   New data (parts, joints, features, fit table) goes in new, versioned records.
4. **The old frontends (GTK, Win32, Cocoa, Qt) stay buildable until M1 parity**, then get deleted.
   (Done 2026-10-21 with the owner's go-ahead: the desktop builds only `solvespace-ng`; the web
   build keeps `guihtml`; `guinone` stays for headless use. The core's TextWindow remains for now.)
5. **C++20** across the project. Graphics: OpenGL, restricted to the GL ES 3.0 / WebGL2 subset
   (ANGLE on Windows as today). A WebGPU backend is a later option behind `Canvas`.

## 5. UI/UX — Decision

**Layout.** The viewport fills the window and everything else floats over it.
- **Placement** (decided by the owner in review 4, 2026-10-17, layout c, as Blender, SketchUp
  and Plasticity): the Outliner over Properties on the right, the tool shelf as a column on the
  left, Problems at the bottom beside the stack. Panels remember where they were left, per
  layout.
- **Outliner** (top right, collapses to icons): Skeleton → Parts → Groups/Features → Sketches.
  Shows cross-part dependencies.
- **Properties** (right, below the Outliner, follows the selection): editable fields for the selected group, entity,
  constraint or joint. Fields accept expressions. Replaces every text-window screen.
- **Problems**: redundant or conflicting constraints, failed solves, unstable edge references,
  each with one click to highlight the culprits.
- **Status/HUD bar** (bottom): active tool, the keys available right now (like Blender's status
  bar), DOF count, solver and regeneration state.
- **Command palette** and **pie menus** float over the viewport. The tool shelf is optional and
  hidden by default. (Built 2026-10-17: View ▸ Tool Shelf shows a "Tools" column of icon buttons
  for every old-toolbar tool, two wide, three in a short window; NG never draws the custom
  toolbar.)
- **View cube** (added 2026-10-17, as in Fusion 360, Onshape, Shapr3D and FreeCAD, which all
  show axes with their cube): bottom left of the viewport, where SolveSpace has always drawn its
  X/Y/Z axes, which now start at the cube's -X -Y -Z corner and run along its edges, as in
  Onshape and Fusion (decided with the owner, 2026-10-17: in keeping
  with SolveSpace, and the corner never has to dodge panels). The axes are still the reference
  planes' normals, selectable for constraints (`GraphicsWindow::referenceAxesAnchor`); the cube
  takes the mouse only over its faces. Click a face to look at it, an edge or a corner to look
  between its faces (Z up, as the numpad views); drag it to orbit. 80% opaque until the mouse
  is on it.

**Typed values become constraints.** While a modal tool is active, typed numbers fill HUD fields
(`Tab` cycles, `Enter` confirms, `Esc` or right-click cancels). Unlike Blender, a typed value
creates a real constraint. Example: drawing a line, type `25`, `Tab`, `30`, `Enter` and you get a
length constraint and an angle constraint.

**Keymap — "Modern" preset.** Blender's core (palette, pies, grab, sketch focus, numpad
views) with the sketch, constraint and group letters of the industry CAD apps, Onshape's where
they differ (decided by the owner in review 4, 2026-10-17, replacing the Blender-only draft:
"as many unqualified controls as possible, overlapping with industry standard";
`docs/research/keymap-and-layout.md`). A **"SolveSpace Classic"** preset reproduces today's
bindings. All bindings can be remapped and are stored in a keymap file
(`res/keymaps/modern.keymap` is the preset).

| Key | Action |
|---|---|
| `Space` / `F3` | Command palette (searches every command, shows its binding) |
| `Tab` | Sketch focus: activate the group of the selection and look onto its workplane; `Tab` again returns (while drawing, `Tab` cycles the typed-value fields) |
| `Shift+A` | Add pie: line, rectangle, circle, arc, spline, point, text, workplane |
| `L` `R` `C` `A` `S` `P` | Line, rectangle, circle, arc, spline, point; `Alt+C` tangent arc, `Shift+T` text |
| `D` | Smart dimension (distance, diameter or angle, inferred from the selection); `Shift+D` reference |
| `F` | Constrain pie: only the constraints valid for the selection |
| `H` `V` `I` `B`/`T` `Shift+L` `Q` `Shift+S` `M` `Shift+J` `U` | Horizontal, vertical, on point/curve, parallel/tangent, perpendicular, equal, symmetric, midpoint, lock, other angle |
| `X` | Toggle construction (`Delete`/`Backspace` delete) |
| `G` | Grab: drag under the solver; `X`/`Y`/`Z` locks an axis; type a value |
| `E` / `Shift+R` / `Alt+R` / `Alt+E` | Extrude / revolve / lathe / helix; `Shift+W` sketch in a new workplane; `Shift+F` fillet the selected edges (as Onshape); `Shift+H` hole at the selected points (review 5) |
| `Y` / `Shift+Y` / `Shift+I` | Hide / show all / isolate **groups** (SolveSpace has no per-entity hide) |
| `Ctrl+A` / `Alt+A`, `Esc` | Select all / none |
| `` ` `` pie, numpad `1` `3` `7`, `Shift+1`..`7` | View pie; front/right/top (`Ctrl` = opposite side); Onshape's Shift+digits (front, back, left, right, top, bottom, isometric) for keyboards without a numpad; `W` looks at the sketch; `.` frames selection |
| MMB / `Shift`+MMB / wheel | Orbit / pan / zoom (the mouse scheme is configurable) |
| `Ctrl+Z` / `Ctrl+Shift+Z` | Undo / redo |

**Implied selection scope** (decided by the owner after M4a, 2026-10-22, as in Blender): a
selection stands for what the operation needs.
- **Edges for faces:** edges that bound a face whole stand for the face, for the operations on
  faces: shell, draft, sketch on a face, new part (`GraphicsWindow::ImplyFacesFromEdges`).
- **Faces for edges:** faces stand for their edges for fillet and chamfer. That means all
  their edges, or with the option, only the outline of the picked region.
- **Rule for new operations:** every new operation states its scope and accepts the other
  scopes through these conversions.

**Feature sizes are free** (owner's feedback after M4a, 2026-10-22): a fillet's, chamfer's,
shell's or draft's size is a free parameter.
- **Setting it:** dragging the feature's handles or constraining them sets it; Properties sets
  the value.
- **Holding it:** "hold this size" adds a dimension (`FEATURE_SIZE`).
- **Matching:** Equal between handles of two features of a kind ties their sizes
  (`FEATURE_EQUAL`).

**Keymap pass after the features land** (decided by the owner in review 5, 2026-10-21): once
most M3–M5 features exist, rethink every binding by relative frequency of use and industry
standards. Frequently used commands get unqualified keys, unless a qualified chord is very
widely used across the industry. Explore a grammar for qualifiers: the unqualified key `N` is
the command, and `Alt+N`, `Shift+N`, `Ctrl+N` are its relatives along a named dimension each
(e.g. Shift = the reverse or the reference form, Alt = the variant), so the keys can be
discovered. Articulate those dimensions before rebinding.

In a text field, `Ctrl+A`, the clipboard and undo chords edit the text; they reach the keymap
only when no field has the keyboard.

With nothing selected, commands act on the item under the cursor.

Panels in M1c (proposed 2026-10-16 while the owner was away, to confirm): `O` Outliner, `N`
Properties (the old browser, briefly on `Shift+N`, was retired 2026-10-21), `Shift+P` Problems, which
opens itself on a new solve problem but not for an open contour. Properties shows the selection,
else the group picked in the Outliner, else the active group. Preferences, Styles, View and the
Paste Transformed / Step Dimension / Tangent Arc panels open where the browser screen did.
Panels take no ImGui keyboard navigation (no Tab between fields), so Tab, Space and Enter always
reach the keymap unless a field is being typed in. `docs/m1-parity.md` lists every screen's new
home.

Typed angles that are not multiples of 90° add a construction reference line plus an angle
constraint; multiples of 90° add horizontal/vertical (decided 2026-10-16: SolveSpace has no
line-to-axis angle constraint). Ending a line chain (decided 2026-10-17, after comparing Inkscape, FreeCAD, Fusion 360, Onshape
and AutoCAD): the segment following the cursor is only a preview. Right-click, Esc, or Enter
with nothing typed end the chain without it. A double-click, or clicking the last point again,
ends the chain at that point. Backspace with nothing typed removes the last placed segment
(with the constraints its click or typed values added) and keeps drawing. For future UX calls
like this, prefer the modern, industry-standard behaviour.
`D` replaced `constrain-distance` and `.`/`Num.` replaced
`center-view` in the Modern preset (both remain in the menus and the palette).

**Visual style.**
- Dark (default) and light themes, built from design tokens.
- Inter for the UI and JetBrains Mono for numbers.
- Lucide-based icons rendered with lunasvg.
- Constraint glyphs coloured by state (built 2026-10-21, `Constraint::StateColor`):
  under-constrained in the Constraints style's colour, fully constrained halfway to the
  background (as SOLIDWORKS and Fusion turn fully defined sketches neutral), culprits of a failed
  solve in the Errors red lightened against the failed solve's dark red canvas. On by default in
  NG (Preferences ▸ SolveSpace NG), off in stock SolveSpace so its render tests and old
  frontends look as before. The glyph shapes stay SolveSpace's: the shelf icons mirror them.
- Optional matcap shading, clean edge lines, a grid on the active sketch plane.
- Design the theme tokens first in M1, and review them on screenshots before building panels.
  ImGui must not look like a developer tool.

## 6. Kernel and features — Decision

**The invariant.** Constraints reference only **solver entities**:
- sketch entities;
- entities that groups generate as equations of upstream parameters (as extrude does today);
- the **derived entities** declared by new features (below).

Constraints never reference raw B-rep topology parametrically. This keeps SolveSpace immune to
the topological naming problem.

**`IKernel`** (in `kernel/`). `Group::GenerateShellAndMesh()` calls it instead of `SShell`
directly:

```cpp
struct IKernel {
  BodyId  extrude(const Profile&, Vector dir, double depth, /*draft, …*/);
  BodyId  revolve(...);  BodyId helix(...);  BodyId loft(...);  BodyId sweep(...);
  BodyId  boolean(BodyId a, BodyId b, BoolOp);
  BodyId  fillet(BodyId, std::span<const EdgeRef>, double r);
  BodyId  chamfer(BodyId, std::span<const EdgeRef>, double d);
  BodyId  shell(BodyId, std::span<const FaceRef>, double t);
  BodyId  split(BodyId, const Plane&/*or sketch*/);
  Mesh    tessellate(BodyId, double chordTol);
  EdgeRef pickEdge(BodyId, Ray);   FaceRef pickFace(BodyId, Ray);
  MassProps massProps(BodyId);     double minDistance(BodyId, BodyId);
  bool    exportStep(...);         BodyId importStep(...);
};
```

- **SolveSpaceKernel** wraps today's `SShell`/`SMesh` and behaves exactly as now. OCCT-only ops
  report "unsupported" and the UI greys them out. Used in web builds.
- **OcctKernel** is desktop only. It becomes the default when the kernel parity suite passes
  (§8).
- Mesh-mode bodies (imported STL etc.) keep the existing mesh-boolean path.

**Features are ordinary groups that can sit anywhere in the chain.** Each declares the derived
entities it creates, as solver expressions, so later sketches and constraints can use them and
they move live during drags:

| Feature | Derived entities (constrainable) |
|---|---|
| Shell (thickness `t`) | Inner faces = source faces offset by `t` |
| Draft / taper on extrude (angle `α`) | Side faces = base edge + normal rotated by `α` |
| Hole (plain, counterbore, countersink, modelled ISO threads) | Axis, centre, top/bottom points, counterbore plane |
| Fillet `r` between two flat faces | Fillet axis line, both tangent lines |
| Chamfer | Both chamfer edge lines |
| Loft (two or more sketches) / sweep (profile + path sketch) | Input sketch entities, start/end profile frames |
| Split body (plane or sketch) | Cutting plane, frame of each resulting Part |
| Joint (§7) | Joint frame (origin + axes) on each Part |

Also in M3: **mass properties** (volume, mass, centre of mass) and **STEP export/import**.

**Naming edges and faces stably.** An `EdgeRef`/`FaceRef` is stored by **where it came from**, e.g.
"extrude G3 × sketch line h7 × top cap". It is propagated through later operations using OCCT
history (`BRepTools_History`, `Modified()`/`Generated()`). A geometric fallback (nearest edge to a
saved midpoint and direction) covers what history can't resolve, and every fallback use raises a
Problems entry.

**Explicit limit.** Geometry with no closed-form equation, such as a fillet between curved faces
or an edge from intersecting curved surfaces, can be **snapped to** but not parametrically
constrained. The Problems list says so.

**Regeneration.** Kernel regeneration runs on a worker thread, keeping only the newest request,
with a tessellation cache per group. While dragging, only the sketch solve runs per frame. The
stale mesh shows as a ghost until the new one arrives.

**Built in M2 (2026-10-21, branch `ng/m2-kernel`):** `src/kernel/` holds `IKernel`
(`kernel.h`), `SolveSpaceKernel` (`solvespacekernel.cpp`: the shell and mesh code moved from
`groupmesh.cpp`, unchanged), `FaceNamer` (`facenamer.h`) and the worker thread (`worker.h`). A
group captures its solid's inputs in a `KernelJob` (`Group::BuildKernelJob`), so the kernel
never reads the sketch. While dragging (NG only), `GenerateAll` hands the groups to the worker
as one chain (`src/regen.cpp`); the newest finished chain is applied between frames, older
ones are dropped, and the solids are drawn dimmed until then. Faces are named from a snapshot
of each group's remap table; a name the snapshot lacks sends that regeneration back to the
main thread, so files stay byte-identical. Save, export, undo and the end of a drag wait for
the worker (`FlushRegeneration`). Tests: `test/kernel/` (also clean under ThreadSanitizer:
configure a build with `-DCMAKE_CXX_FLAGS=-fsanitize=thread`), `test/ngui/regen/`.

**Built in M3a (2026-10-21, branch `ng/m3-occt`):** Open CASCADE V7_9_3 is a shallow
submodule (`extlib/occt`), built once per build directory as a Release static external
project with only the toolkits used (`cmake/OcctVendored.cmake`; `ENABLE_OCCT`, off for the
web; `OCCT_PREFIX` reuses another build's install, e.g. for the ThreadSanitizer build).
`OcctKernel` (`src/kernel/occtkernel.cpp`) makes extrude, lathe, revolve, helix, step and
repeat and the four combines as B-reps (`KernelShape`, kept in each `Body` and group beside
the shell and mesh) and hands the rest of SolveSpace their tessellation: true normals and
each triangle's face name. Faces get the same names as SolveSpace's kernel gives them (same
`FaceNamer` calls, same order), carried through booleans and copies by OCCT's history.
Links, meshes and groups forced to a mesh take SolveSpace's mesh path. STEP export writes the
B-rep. The kernel is `SS.kernelKind` (`Kernel` in settings.txt). **Decision (owner,
2026-10-21, after comparing the two kernels' costs):** desktop NG makes solids with Open
CASCADE only, with no choice in Preferences (an older choice of SolveSpace's is dropped).
SolveSpace's kernel stays in the code for meshes and links, the web build, stock SolveSpace,
the core tests and the parity suite. OCCT tessellates lazily, only when a mesh is asked for,
and makes the drawn outlines from its triangulation's topology (`Tessellate`), not by
searching the mesh; booleans and meshing run in parallel.
`test/kernel/parity` regenerates every model under `test/` with both kernels and compares
volume, area, bounding box and watertightness (all 102 solid groups of 287 models match).
Files saved with OCCT store the solid as triangles, as mesh groups do, not NURBS surfaces.

**Built in M3b (2026-10-21, branch `ng/m3b-features`):** fillet and chamfer groups
(`Group::Type::FILLET`/`CHAMFER`, New Group ▸ Fillet Edges / Chamfer Edges, `Shift+F`). A
group holds its edges as the entities solid groups make (an extrusion's copies of its sketch
and its side lines; saved as `Group.edges`) and its radius or distance (`valA`, edited in
Properties). The OCCT kernel rounds every B-rep edge of the previous solid lying on those
entities' curves (`BRepFilletAPI`), so an edge split by a boolean rounds whole; a failure or a
missing edge is a Problems entry (`Group::featureProblem`). Derived entities: along each
straight edge between two planar faces, a fillet makes its two tangent lines and its axis, a
chamfer its two edge lines (`REMAP_FILLET_*`, `REMAP_CHAMFER_*`). The solid gives only which
faces meet at the edge (`KernelEdgeFaces`, cached per edge); the lines' positions come from
those faces' entities, so they follow a drag at once, and they always exist (on the edge when
no solid is known, e.g. with SolveSpace's kernel), so constraints on them are never pruned.
When an edge's faces change on the fresh solid, `GenerateAll` solves the later groups once
more. An edge rounded away is hidden while its fillet is shown. Not yet: edges no entity
describes (two bodies' intersection), derived entities for curved edges, a Problems note for
a straight edge on a curved face.

**Built in M3c (2026-10-21, branch `ng/m3c-features`):** Inspect ▸ Mass Properties (volume,
surface area, mass at a density set in Preferences, centre of mass; exact from the B-rep via
`KernelMassProperties`, else from the mesh). Shell groups (`SHELL`: faces left open, saved as
`Group.faces`, and a wall thickness; `BRepOffsetAPI_MakeThickSolid`, or with no face open the
solid less its inward offset) declare an inner face `Remap(face, REMAP_SHELL_INNER)` for every
face entity before them. Draft groups (`DRAFT`: side faces of extrusions, an angle in degrees,
positive narrowing along the extrusion, about its sketch plane; `BRepOffsetAPI_DraftAngle`)
declare each drafted face `Remap(face, REMAP_DRAFT_FACE)` through its base line with its
normal turned by the angle. Which way each face entity faces the solid's outside is cached
from the solid (`KernelFaceNormals`, topology only); planes come from the entities. Draft is
a group after the extrusion rather than an extrusion option, so the extrusion's own entities
(which its constraints solve) keep their meaning; its top edges stay untapered.

**Built in M3d (2026-10-21, branch `ng/m3d-holes`):** hole groups (`HOLE`, New Group ▸ Hole,
`Shift+H`, bound by the owner in review 5): centres are points in a workplane (`Group.points`), and each hole goes against
the workplane's normal (into the face sketched on), or flipped. `HoleSpec` (`Group.hole.*`):
plain, counterbore or countersink; through all (depth 0) or a depth; an optional modelled ISO
metric internal thread ("M4", coarse pitch from ISO 261, or "M4x0.5"), whose bore is the
minor diameter and whose 60° groove is swept along a helix from below the head. Derived
entities per centre: `REMAP_HOLE_TOP`, `_BOTTOM`, `_AXIS`, `_SEAT` (the seat face names a
counterbore's floor). Hole booleans run OCCT in parallel with oriented boxes: a thread's
helical cut is the slowest feature (about 0.4 s per 12 mm M4 thread).

**Built in M3e (2026-10-21, branch `ng/m3e-loft-sweep`):** loft and sweep groups (`LOFT`,
`SWEEP`; New Group ▸ Loft / Sweep), solid makers combined like an extrusion. Inputs are
`Group.profiles`: for a loft, one picked curve per profile sketch, in order, each profile
being the closed outer loop holding it; for a sweep, a curve of the profile sketch, then the
path's curves (chained end to end from the end nearest the profile, picked in any order).
`BRepOffsetAPI_ThruSections` (smooth) and `BRepOffsetAPI_MakePipeShell` (corrected Frenet,
mitred corners). Derived entities: the end faces (`REMAP_LOFT_START/END`,
`REMAP_SWEEP_START/END`), facing out, naming the end caps. Holes inside profiles are left out
(with a note).

**Built in M3f (2026-10-21, branch `ng/m3f-step-import`):** STEP import: New Group ▸ Link
Part takes `.step`/`.stp` files (`SolveSpaceUI::LinkStep`). The B-rep (`Group::impShape`, read
again on load, not saved) is placed by the link's position, rotation and scale and combined as
a B-rep by the OCCT kernel; its tessellation (`impMesh`) is what SolveSpace's kernel and
builds without OCCT use. Imported faces are unnamed: they can be picked and measured, not yet
sketched on.

**Sketch on a face (2026-10-21, after review 5):** Sketch in New Workplane (`Shift+W`) takes a
face of the solid, an extrusion's cap or side or any feature's derived face, and optionally a
point for the origin (`WORKPLANE_BY_FACE`, stock's long-stubbed subtype). The workplane follows
the face's plane every regeneration; its normal points to the viewer as the face was picked
(`predef.negateV`), its axes keep the view's (`predef.q`, re-projected).

**Feature sizes as solver parameters (M3h, 2026-10-21, after review 5):** a fillet's radius, a
chamfer's distance, a shell's thickness and a draft's angle (as its tangent) are their
group's parameter `h.param(0)`, like an extrusion's depth. A new feature starts with a
`FEATURE_SIZE` dimension holding it (drawn off the solid at its first moving derived point;
edited on the canvas or in Properties). Derived points that the size moves are
`POINT_ALONG_PARAM` (base + direction × parameter), so with the dimension made reference or
deleted, constraints on derived entities or drags set the size; a draft's derived top edge
(`REMAP_DRAFT_TOP`) sets its angle. Kernel jobs build the solved size. Hole sizes are still
plain values. Feature groups saved before M3h load without the dimension (one free DOF).

**Quad mesh export, a spike (2026-10-21, owner's question in review 5):** Export Mesh to a
`.usda` file writes the B-rep as quads for Blender, with creases for Catmull-Clark
(`src/kernel/occtquadmesh.cpp`; findings in `docs/research/quad-mesh-spike.md`): quad grids
on four-sided faces, rings on flat one-hole faces, rings round a centre fan of triangles on
other faces without holes (review 6), ear-clipped triangles on the rest, shared edge points.
Faces the centre can't see whole (a notched strip) are slivers: the open problem.

**Later:** variable-radius fillets, STEP import as a reference body (snappable, warned),
XCAF colours and names, exact hidden-line drawing export (`HLRBRep`).

## 7. Assemblies — Decision (multi-body in one file)

- **Part:** a named body with its own ordered list of groups, plus material, colour and print
  settings. A document holds many Parts.
- **Skeleton:** shared sketches and workplanes at the top of the document that any Part can
  reference. This is the main co-design tool: one skeleton profile drives both the tab on Part A
  and the slot on Part B.
- **Cross-part references:** a Part's sketch may reference skeleton entities or groups that come
  earlier in the document in other Parts. The dependency graph must have no cycles. Reject
  cycles with a clear message.
- **Placement:** each Part has a rigid transform (7 solver parameters: position + quaternion, as
  `LINKED` groups have today), solved by assembly constraints: coincident, concentric, planar
  contact, distance, angle, lock.
- **Design pose vs. motion pose:**
  - Cross-part references are resolved in the **design pose** (the authored layout).
  - Motion joints move Parts *away* from the design pose **without rebuilding their geometry**.
  - Dragging a mechanism is therefore a pure solve plus a mesh transform. That is real-time
    with no kernel work.
- **Fit joints (3D printing):** press fit, sliding fit, snap fit, dovetail, dowel pin,
  pin-hole.
  - Each takes its clearance from a document-wide **fit table** (e.g. `press = -0.1`,
    `slide = 0.2`, `loose = 0.4` mm, tuned per printer).
  - Each generates matching geometry on both Parts from one shared profile: the male side as
    drawn, the female side offset by the clearance.
- **Motion joints (mechanisms):** revolute, slider, cylindrical, planar, ball, and gear/rack
  coupling (as a ratio constraint).
  - Each leaves exactly the right DOF free and takes optional limits.
  - `G` drags a Part within its joints, solving live.
  - A joint value can be scrubbed or animated, with a **clearance sweep** across the range
    (e.g. "minimum gap 0.12 mm at 47°").
- **Split + automatic joints:** split a large body into printable Parts and add dowel or
  dovetail fit joints at the cut.
- **Output:**
  - per-Part STL / 3MF / STEP, with optional "lay flat on largest face";
  - whole assembly as STEP, or 3MF with separate objects;
  - a simple parts list (BOM).
- **Compatibility:** existing `LINKED` groups load as Parts whose source is an external file (for
  reusable standard parts). An old single-body document opens as one Part.

**Built in M4a (2026-10-22, branch `ng/m4a-parts`):**
- **A part is a tag on groups.** `Group::part` is saved as `Group.part.v`, where 0 is the
  first part, so old files are unchanged. `SK.part` holds the named `AddPart` records,
  written once a file has a second part. The global group order stays as the dependency
  order.
- **Each part has its own solid,** carried from the previous group of the same part
  (`RunningMeshGroup`). Cross-part references need nothing new: any group uses any earlier
  entity, so a lid sketched on an enclosure's rim follows it.
- **New Group ▸ New Part** (`part-new`) starts the next part with a sketch, on the picked
  face if there is one. New groups join the active group's part.
- **Display and panels:**
  - The viewport shows every part's solid as of the active group, and their faces can be
    picked.
  - The Outliner heads each run of a part's groups with its name.
  - Properties renames the part.
- **Export:** File ▸ Export Parts (`export-parts`, `SolveSpaceUI::ExportParts`) writes
  `<stem>-<part>.stl` or `.step` per part. This meets M4's "done when"
  (`test/group/partsexport`: an enclosure and lid, two closed files).

**Built in M4b (2026-10-22, branch `ng/m4b-assembly`):**
- **Material:** a part's density (`Part.density`, 0 = Preferences'), chosen in Properties
  of any of its solid groups (`SetPartMaterial`).
- **Parts list:** Inspect ▸ Parts List and File ▸ Export ▸ Parts List (CSV)
  (`PartsList`, `ExportPartsList`): each part's material, volume, mass, bounding box and
  linked file.
- **3MF:** Export Parts to a `.3mf` name writes one file with an object per part, triangles
  in their colours (`src/export3mf.cpp`: a stored zip, no new dependency).
- **Lay flat** (Preferences, off): STL and 3MF parts are turned onto their largest flat
  face, standing on z = 0, and set side by side in a 3MF (`SolveSpaceUI::LayFlat`).
- **Linked files are parts:** Link Part starts a part named after the file. An older file's
  link keeps its part, and Properties offers "own part" (`MoveToNewPart`).
- **Skeleton:** with parts, the Outliner heads the sketches before the first solid
  "Skeleton".
- **Placement and the design pose (ruling, 2026-10-22):** in-file parts are authored in
  place, so the authored layout is the design pose and placements are the identity. The
  part transform, the assembly constraints that move it and the motion pose come with M5's
  joints, the first thing that moves a part. Links keep their own 6-DOF placement.

## 8. Milestones and testing

Each milestone leaves a working, daily-usable app. Work in milestone order. Don't start
M(n+1) features before M(n)'s "done when" is met, unless the owner says otherwise.

| # | Scope | Done when |
|---|---|---|
| **M0** | C++20 bump; CI on Linux/Windows/macOS; scripted headless command runner; baseline green | Existing `test/` suite passes on all three platforms in CI |
| **M1** | New UI at parity: `solvespace-ng` target (SDL3 + ImGui) next to the old frontends; command registry; Modern + Classic keymaps; outliner, properties and problems panels replacing every text-window screen; palette, pies, HUD with typed constraints; themes | Parity checklist complete (every `Command` and every text-window screen has a new home); then delete the GTK/Win32/Cocoa/Qt frontends |
| **M2** | `IKernel` + `SolveSpaceKernel`; background regeneration; ghost preview | Regression output unchanged |
| **M3** | `OcctKernel`; derived-entity framework; fillet, chamfer, shell, draft, hole/threads, loft, sweep, mass properties, STEP | Kernel parity suite passes; OCCT is the desktop default |
| **M4** | Parts, skeleton, cross-part references, design pose, per-part export (STL/3MF/STEP), BOM, `LINKED` compatibility | An enclosure + lid co-designed in one file exports two printable parts |
| **M5** | Fit table, fit joints, motion joints + limits, live dragging, scrubbing, clearance sweep, split with automatic joints | A printed-hinge model drags live and reports correct clearance |
| Later | Hidden-line drawings, STEP reference import, variable fillets, WebGPU | — |

**Testing requirements:**
- The existing `test/` suite (`test/harness.cpp`, target `test_solvespace`) **stays green on
  every commit**.
- **Headless command tests:** UI behaviour is tested by driving registered commands from scripts,
  not by pixel-poking ImGui.
- **UI interaction tests** (added 2026-10-16, so whole user flows are tested, not only their
  parts): `test/ngui/` runs the real `solvespace-ng` app in-process on SDL's offscreen video
  driver and drives it with real key, text and mouse events. Find ImGui items by label and model
  geometry by projecting it; never hard-code pixels.
  `test/ngui/sessions/` chains many features on one model, the way a person works, and ends
  each session with undo to the start, redo to the end, and a save/reopen round trip.
  `test/x11/run.py` (added 2026-10-17) runs the real binary on Xvfb, types and clicks through
  xdotool (the X11 input path a keyboard and mouse take) and checks the saved `.slvs` file.
  Every bug the owner reports gets a test in one of these tiers first.
- **Kernel parity tests:** every model under `test/` is compared across kernels on volume, area,
  bounding box and watertightness, within a set tolerance.
- **Format golden files:** old `.slvs` files load, and saving then reloading gives the same file.
  New records have version tags.
- **Performance:** benchmarks in `bench/`. Dragging a reference assembly must hold 60 fps.
- Use test-driven development for new logic (commands, keymap, derived entities, joints).

## 9. Working in this repo

**Build (Linux)**: the default build is SolveSpace NG; see below. Stock SolveSpace is built
only from `github-master` (worktrees for the `pr/` branches), never from NG branches.
```sh
./bin/solvespace-testsuite <regex>   # run only matching tests, e.g. core/commands
```

**Build SolveSpace NG (SDL3 + Dear ImGui frontend)**
```sh
git submodule update --init extlib/SDL extlib/imgui extlib/lunasvg extlib/occt   # SDL3 is vendored (or install libsdl3-dev)
mkdir -p build-ng && cd build-ng
cmake .. -DCMAKE_BUILD_TYPE=Debug     # ENABLE_NG_GUI is on by default
make -j$(nproc) solvespace-ng solvespace-testsuite && make test_solvespace   # includes test/ng/*
./bin/solvespace-ng [file.slvs]
SOLVESPACE_NG_SCREENSHOT=/tmp/shot.png ./bin/solvespace-ng f.slvs   # save a frame and exit
# Drive the UI with real SDL events (steps: action, keys, key [Ctrl+|Shift+|Alt+]Name, wait N,
# shot PATH, quit). Use `timeout -s KILL`: a modal dialog blocks the event loop.
SOLVESPACE_NG_SCRIPT="action ui.command-palette; keys extrude; wait 3; shot /tmp/p.png; quit" ...
# UI interaction tests (also run by `make test_solvespace`); needs EGL (Mesa) but no display.
make -j$(nproc) solvespace-ng-uitest && ./bin/solvespace-ng-uitest [regex [seed]]
SDL_VIDEO_DRIVER=x11 ./bin/solvespace-ng-uitest ngui/grab/   # watch them run on screen
# Black-box tests of the real binary on Xvfb, driven with xdotool and checked through the saved
# file (also run by test_solvespace when Xvfb, xdotool and python3 are installed).
make test_ng_x11        # or: python3 ../test/x11/run.py bin/solvespace-ng [filter]
NGX11_SHOW=:0 python3 ../test/x11/run.py bin/solvespace-ng   # watch on your own display
# Owner review rounds (blindweb, $BLINDWEB; its server on
# <the review host> serves ~/*-blind): capture every menu, panel and overlay in both themes,
# put them side by side, and write rounds from a spec (docs/review/<date>/*.json).
NG_CAPTURE_DIR=/tmp/cap ./bin/solvespace-ng-uitest ngui/capture/
NG_CAPTURE_DIR=/tmp/cap NG_CAPTURE_THEME=light ./bin/solvespace-ng-uitest ngui/capture/
python3 ../tools/review/combine.py /tmp/cap /tmp/sheets
python3 $BLINDWEB/make_round.py SPEC.json ~/solvespace-ng-blind/<date>-<name>
```
The owner's answers land in each round's `scores.md`.
In-tree builds are rejected; always build in `build/`. NG settings and the user keymap live in
`SDL_GetPrefPath("solvespace","solvespace-ng")`: Linux `~/.local/share/solvespace/solvespace-ng/`,
Windows `%APPDATA%\solvespace\solvespace-ng\`, macOS
`~/Library/Application Support/solvespace/solvespace-ng/` (`settings.txt`, `keymap.txt`; setting
`NG_KeymapPreset=modern|classic`). See `README.md` for Windows (MinGW/MSVC),
macOS and Emscripten builds.

**Code style**
- Follow `.clang-format`: 4 spaces, no tabs, 100 columns, attached braces, no space before
  parentheses.
- Match the surrounding code's naming and comment density. Don't reformat code you aren't
  changing.

**Process**
- Make small, staged changes. Each commit builds and passes tests.
- Refactors that untangle `SS.GW`/`SS.TW` move one screen or command per commit.
- Don't add dependencies beyond §2 without the owner's approval.
- Update this file whenever a **Decision** changes. Record the date and the reason.
- Branches: feature work on `ng/<milestone>-<topic>` branches, merged into `master`.
- Remotes (decided 2026-10-16): the only remote is `origin` =
  `<the owner's LAN repo>`, a private bare repo on the owner's LAN. The
  GitHub remote (`solvespace/solvespace`) was removed on purpose: nothing goes to the official
  repo unless the owner decides the work is excellent enough to offer, and a public GitHub fork
  is only being considered. Never add a GitHub remote or push anywhere else without the owner's
  say-so. GitHub Actions CI therefore does not run; verify Windows/macOS manually or once a fork
  exists.
- Upstream sync: when wanted, fetch without keeping a remote, e.g.
  `git fetch https://github.com/solvespace/solvespace.git master` then merge `FETCH_HEAD`.
  Merge upstream quarterly until M1 lands, then cherry-pick solver/kernel fixes only.
- `github-master` mirrors the official GitHub `master` head (created 2026-10-16 at `8ef723e1`).
  Never commit NG work to it; only fast-forward it to upstream:
  `git fetch https://github.com/solvespace/solvespace.git master && git branch -f github-master
  FETCH_HEAD && git push origin github-master`.
- Upstreamable fixes (bug fixes, `wishlist.txt` items, anything useful to stock SolveSpace) go on
  `pr/<topic>` branches cut from `github-master`, ported so they build and pass `test/` there with
  no NG code or dependencies, following upstream's review rules in
  `.github/copilot-instructions.md` (small, no new dependencies, human-tested; upstream
  discourages AI-attributed commits, so the owner decides authorship before anything is offered).
  Keep them on the LAN remote; the owner reviews, tests, rewrites and pushes them upstream
  personally. Agents never open PRs, push upstream, or rewrite a `pr/` branch after handing it
  over.
- **Standing duty for agents:** whenever NG work fixes or improves stock SolveSpace behaviour
  (a core bug, a leak, a crash, a `wishlist.txt` item), also cut a `pr/<topic>` branch from
  `github-master` with just that change (`git cherry-pick -x`, or the relevant hunks), verify it
  builds and passes `test/` as stock SolveSpace (C++11, no NG), push it to `origin`, and tell the
  owner. Existing `pr/` branches:
  - `pr/undo-grouporder-leak`: UndoState `groupOrder` leaked on every undo step (from `e220b5d8`).
  - `pr/headless-null-guards`: null window/menu-item guards for headless runs (from `978602ca`).
  - `pr/diameter-label-leader`: a diameter label at the centre had no leader and logged a
    zero-vector warning every frame (from `3827b2be`).
  - `pr/hover-list-leak`: every hit test leaked the previous hover list (from `60b82650`).
  - `pr/paste-scale-crash`: a non-expression in Paste Transformed's scale crashed (from
    `0868c8bd`).
  - `pr/line-chain-ending`: right-click (and a second click on the last point) kept the line
    segment following the cursor; a stale horizontal/vertical suggestion after a quick move
    (from `9bdd5553`; sits on `pr/headless-null-guards`).
  - `pr/volume-mesh-group`: Analyze ▸ Volume left a mesh group's own triangles out of the
    current group's volume (from `ca419d95`).
- Tests: `RUN_COMMAND("<command-name>")` in `test/` runs a menu command headlessly (names in
  `src/commandnames.cpp`). Headless code paths have null windows/menu items: guard them.
  Not headless-safe yet (null FileDialog/Window under `guinone.cpp`): `open`, `save`, `save-as`,
  `import`, all `export-*`, `group-link`, `image`, `exit`, `zoom-in`, `zoom-out`, `full-screen`.
- NG UI actions (`ui.*`) are listed in `src/ui/uiactions.cpp`; core-backed ones are bound in
  `src/ui/coreactions.cpp` and implemented as `GraphicsWindow` methods so tests can call them.
  Modal tools (pie, grab, typed drawing) see chords before the keymap via `KeyRouter::modal`;
  test them through `Ui::Grab()`, `Ui::Draw()` and the core `Grab*` / `CommitTyped*` methods.
  Headless tests can click: `SS.GW.MouseLeftDown/Up` tolerate a null window (set
  `SS.showToolbar = false` first). Debug-script keys use SDL key names (`Keypad 1`, `Escape`,
  `Return`); `move x y` and `click x y` drive the mouse. An unknown key opens a blocking error
  dialog, which shows up as a timeout.
- UI interaction tests (`test/ngui/<area>/test.cpp`, listed in `ngui_SOURCES` in
  `test/CMakeLists.txt`) are ordinary `TEST_CASE`s in `solvespace-ng-uitest`. Include
  `../uitest.h` before `harness.h`. Drive the app with `Platform::Driver`
  (`src/platform/sdl/uidriver.h`): `Key("Shift+A")`, `Type("25")`, `Move`, `Click`,
  `DoubleClick`, `Drag`, `Wheel`, `KeyDown("Shift")`, `ClickItem("label")`,
  `ClickMenu({"Sketch", "Line Segment"})`. Each call renders until the app is idle (timers
  included, so animations finish). `uitest.h` adds `At(Vector)`, `AtMiddle(line)`,
  `AtLabel(constraint)`, `DrawLine`, `ShiftClickAt`, `PickPie("Circle")`,
  `ClickBrowserText("hide all")` and `ContextMenu(p, "Delete")`. Dialogs never block:
  `Driver::Dialogs()` records them, `OnMessage(policy)` answers message boxes (default: the
  default button), `QueueFile(path)` answers the next file dialog, `DuringModal(step)` runs
  inside a context menu; an unanswered nested loop fails the case. Each case starts with
  default settings in a temporary config dir; a failing case saves a screenshot to
  `ngui-failures/` in the working directory. Fixtures are copied per test directory.
  Only visible items are found (scrolled out of view = not there): `RevealIn(panel, item)`
  scrolls a panel with the wheel, and clicks land on the visible part of an item. ImGui combo
  boxes don't report their label to the hooks; prefer radio buttons or selectables in panels.
  Panel logic lives in core methods (`SolveSpaceUI::ChangeGroupOption`, `SetSetting`,
  `Style::SetValue`, ...) shared with the old browser, plus `src/ui/*model.cpp` view models;
  test those headlessly in `test/ng/` and the panels end to end in `test/ngui/`.

**Developer docs worth reading first:** `developer_docs/IdLists_Entities_and_Remap.txt`,
`developer_docs/Solver_Transforms.txt`.
