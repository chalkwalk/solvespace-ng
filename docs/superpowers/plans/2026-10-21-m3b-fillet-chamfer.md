# M3b Fillet, Chamfer and Derived Entities Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans (chosen: inline;
> the owner asked to merge M3a and continue with M3b, 2026-10-21, and for roadmap work to
> continue without intervention).

**Goal:** fillet and chamfer groups that round or bevel chosen edges of the solid with Open
CASCADE, anywhere in the group list, and that declare derived entities (a fillet's two tangent
lines and its axis, a chamfer's two edge lines) that later sketches and constraints use and
that move live while dragging. This is the second part of M3 (AGENTS.md sections 6 and 8).
Shell, draft, hole, loft, sweep, mass properties and STEP import follow in later M3 plans.

**Architecture:**
- **Groups.** `Group::Type::FILLET` (5400) and `CHAMFER` (5401) are ordinary groups.
  - `valA` holds the radius or distance in mm.
  - `std::vector<hEntity> edges` holds the chosen edges, saved as `Group.edges`, a
    space-separated list of hex handles in a new table format `'E'`.
  - The group adds no body of its own: `thisBody` stays empty and `running` is the previous
    solid with the edges rounded. A pattern of a fillet group therefore copies nothing; pattern
    the solid, then fillet it.
- **Edges are solver entities**, which a solid group already makes:
  - an extrusion's top and bottom copies of its sketch's curves;
  - its side lines (`REMAP_PT_TO_LINE`);
  - a lathe's circles.

  An edge is stored by its handle, which says where it came from (`group × sketch entity ×
  remap`). The kernel finds the B-rep edges lying on the entity's curve at regeneration time,
  so an edge split by a boolean still rounds every piece. Edges no entity describes (the
  intersection of two bodies) are a later plan.
- **Kernel.**
  - `KernelJob::Op::FILLET` and `CHAMFER` carry each edge's curves as `SBezier` values, plus
    the radius or distance.
  - `OcctKernel` runs `BRepFilletAPI_MakeFillet` or `MakeChamfer` on `prevShape`. Face names
    carry over through the operation's history (`CarryNames`); the new rounded and bevelled
    faces are unnamed (0).
  - `SolveSpaceKernel` passes the previous solid through.
  - In either kernel, a failure or a missing edge becomes `KernelResult::problem`. The group
    keeps it as `featureProblem`, and the Problems panel and Properties show it.
- **Derived entities.**
  - For each straight edge between two planar faces, the group makes line segments with
    numeric points (`POINT_N_COPY`, as a lathe's circles), handles `Remap(edge, REMAP_…)`:
    - fillet: tangent line on face 1, tangent line on face 2, axis;
    - chamfer: edge line on face 1 and on face 2.
  - **Positions.** Each comes from the edge's endpoints `a`,`b` and the directions `u1`,`u2`
    into the two faces, perpendicular to the edge (α is the angle between `u1` and `u2`):
    - fillet tangent lines: `a + s·uᵢ`, with setback `s = r / tan(α/2)`;
    - fillet axis: `a + r/sin(α/2) · unit(u1+u2)`;
    - chamfer edge lines: `a + d·uᵢ`;
    - and the same from `b`.
  - **Topology from the B-rep, geometry from the solver.** The B-rep supplies only
    topology: which two faces meet at the edge (their face entity handles) and the sign that
    points `n × e` into each. The kernel query is `KernelEdgeFaces`, cached per edge in the
    group and not saved. The directions themselves come from the face entities' current
    normals, so the lines follow a drag at once.
    - A face with no name falls back to the cached numeric direction.
    - Without any adjacency (the SolveSpace kernel, or a curved face), the lines lie on the
      edge and the Problems panel says why.
  - **The entities always exist,** so constraints on them are never pruned. Only their
    positions depend on the B-rep.
- **Regeneration.**
  - `GenerateAll`'s first (bounding box) pass solves later groups against the cached
    adjacency. The second pass updates it from the fresh solid of the previous group.
  - If an adjacency changed, `GenerateAll` runs once more, so constraints on derived entities
    solve against current topology. This happens only when topology changes (load, a new
    fillet, a face flipping).
- **Edges filleted away** are hidden: the fillet group marks the edge entity
  (`Entity::roundedBy`), and `Entity::IsVisible` hides it while that group is at or before the
  active one.
- **UI:**
  - New Group ▸ Fillet / Chamfer;
  - `Shift+F` fillet in the Modern keymap (as Onshape);
  - the palette;
  - Properties has a radius / distance field (expressions, in the document's units) and the
    edge count;
  - the Outliner has type names;
  - the Problems panel has feature problems.

  Making a fillet with the SolveSpace kernel says it needs Open CASCADE (Preferences ▸
  SolveSpace NG).

**Tech stack:** C++20, Open CASCADE 7.9.3 (`TKFillet`, already built).

**Spec:** AGENTS.md sections 3 (groups), 6 (features as groups, derived entities, naming,
explicit limit), 7 (n/a), 8 (M3 row, TDD for derived entities).

## Global constraints
- `test/` stays green on every commit; stock references are never edited.
- Old files open unchanged; files without fillet or chamfer groups save byte for byte as before.
- `src/kernel/` never references `SS.GW`, `SS.TW` or `ui/`; OCCT headers only in
  `src/kernel/occt*.cpp` and tests.
- `-DENABLE_OCCT=OFF` and the web build keep building (`HAVE_OCCT` guards; stubs).
- Kernel jobs stay values-only (the worker may run them while dragging).

## Review focus
1. A fillet whose radius is too big for the edge: OCCT fails. The previous solid shows, the
   Problems panel names the group, and nothing crashes.
2. An edge entity the solid no longer has (cut away by a later change): "edge not found", the
   other edges still round.
3. Deleting the extrusion that made a chosen edge: the fillet group's edge list loses it
   (`PruneGroups` treats the edges like operands), with no dangling handle.
4. Undo/redo and save/reopen of a filleted model: same edges, same radius, the derived
   entities keep their handles (a constraint on a tangent line survives the round trip).
5. Dragging a sketch point upstream of a fillet while a later sketch is constrained to the
   fillet's tangent line: that sketch follows during the drag (worker path).

---

### Task 1: Fillet and chamfer groups, saved and made from a selection

**Files:**
- Modify: `src/sketch.h` (Type values, `edges`, `featureProblem`, `edgeFaces` cache,
  `REMAP_` values, `Entity::roundedBy`), `src/file.cpp` (`'E'` format, `Group.edges`),
  `src/ui.h` + `src/commandnames.cpp` + `src/graphicswin.cpp` (commands `GROUP_FILLET`
  "group-fillet", `GROUP_CHAMFER` "group-chamfer"), `src/group.cpp` (`MenuGroup`,
  `DescriptionString`), `src/generate.cpp` (`PruneGroups` drops edges that no longer exist),
  `src/groupprops.cpp` (`SetFeatureSize`).
- Test: `test/group/fillet/test.cpp` (core, both kernels' builds).

**Interfaces — produces:**
- `std::vector<hEntity> Group::edges`; `double Group::valA` = radius / distance (mm).
- `bool SolveSpaceUI::SetFeatureSize(hGroup, const std::string &expr)` (positive only).
- `static bool Group::IsEdgeEntity(const Entity *e)`: a line, arc, circle or cubic made by a
  solid group (`EXTRUDE`, `LATHE`, `REVOLVE`, `HELIX`).

- [ ] Step 1: test `fillet_group_is_made_from_selected_edges`. Load
  `test/group/fillet/box.slvs` (a 10×20 rectangle extruded 30, made in this task with
  the app and committed). Select the extrusion's top-front line entity and run
  `RUN_COMMAND("group-fillet")`. Expect:
  - a new active FILLET group whose `edges` is that handle;
  - `valA > 0`.
- [ ] Step 2: test `fillet_edges_survive_save_and_reload`. Expect that it saves, reloads, has
  the same `edges` and `valA`, and that a file without fillets is byte-identical (covered by
  the existing format tests).
- [ ] Step 3: test `fillet_needs_an_edge`. With nothing selected, the command shows an error
  and makes no group.
- [ ] Step 4: run (RED: unknown command / no group). Implement:
  - the types, the `'E'` format (write `%08x` joined by spaces; parse with `strtoul` until
    the end);
  - `MenuGroup` cases: every selected entity must pass `IsEdgeEntity`;
    `valA` = 10% of the shortest selected edge;
  - `PruneGroups`: drop edges whose entity is gone, and delete the group when none are
    left, as for operands;
  - `SetFeatureSize`.
- [ ] Step 5: GREEN; full core suite; commit.

### Task 2: The kernel rounds and bevels the edges

**Files:**
- Modify: `src/kernel/kernel.h` (`Op::FILLET`, `Op::CHAMFER`, `edgeCurves`, `size`,
  `KernelResult::problem`), `src/groupmesh.cpp` (`BuildKernelJob`, `ApplyKernelResult`,
  `GenerateDisplayItems` shows a feature group's own running solid, `IsMeshGroup` includes
  the new types), `src/kernel/occtkernel.cpp`, `src/kernel/solvespacekernel.cpp`,
  `src/ui/problemsmodel.cpp` (feature problem), `cmake/OcctVendored.cmake` (`TKFillet` is
  already in; check that the `BRepFilletAPI` headers install).
- Test: `test/kernel/occt/test.cpp` (HAVE_OCCT), `test/group/fillet/test.cpp`.

**Interfaces — produces:**
- `std::vector<std::vector<SBezier>> KernelJob::edgeCurves`; `double KernelJob::size`.
- `std::string KernelResult::problem`; `std::string Group::featureProblem`.
- `bool KernelSupports(KernelFeature)` = `Kernel().Supports(...)`, for the UI.

- [ ] Step 1: tests (HAVE_OCCT):
  - `fillet_rounds_a_box_edge`: OCCT kernel, fillet radius 2 on a 30-long edge; volume =
    box − (1 − π/4)·4·30 within 0.5%; the mesh is closed. The faces named before (top, the
    sides) are still named.
  - `chamfer_bevels_a_box_edge`: d = 2; volume = box − 2·30 within 0.5%.
  - `a_fillet_too_big_leaves_the_solid_and_reports_it`: r = 50; running volume = box; the
    group's `featureProblem` is not empty.
  - `a_missing_edge_is_reported_and_the_rest_round`: two edges, one moved off the solid by
    editing the extrusion's sketch so its entity no longer lies on a B-rep edge. Implement as
    a fillet of an edge from a group suppressed away. Expect `featureProblem` to mention
    "not found", with the other edge rounded.
- [ ] Step 2: test (core, any build): `fillet_with_the_solvespace_kernel_keeps_the_solid`.
  The volume is unchanged, and `featureProblem` mentions Open CASCADE.
- [ ] Step 3: RED. Implement:
  - The edge match: a B-rep edge matches when its two ends and its midpoint lie within
    `LENGTH_EPS·10` of one of the entity's beziers (`SBezier::ClosestPointTo`, distance).
  - `MakeFillet.Add(r, edge)` for each matching edge, then `Build`, guarded with
    `try/catch(Standard_Failure)`.
  - `CarryNames(op, {&prev.faces}, &out.faces)`.
  - `Supports(FILLET|CHAMFER)` is true in OCCT.
- [ ] Step 4: test (HAVE_OCCT, `test/kernel/background`):
  `fillets_made_on_the_worker_match_a_full_regeneration`.
- [ ] Step 5: GREEN; core + UI suites; commit.

### Task 3: Derived entities that follow the solid

**Files:**
- Modify: `src/kernel/kernel.h` + `occtkernel.cpp` (`KernelEdgeFaces`, stub without OCCT),
  `src/group.cpp` (`Generate` cases, `MakeFeatureLines`), `src/generate.cpp` (one more
  pass when an adjacency changed), `src/entity.cpp` / `src/drawentity.cpp` (`roundedBy`
  hides), `src/sketch.h`.
- Test: `test/group/fillet/test.cpp`.

**Interfaces — produces:**
- `struct KernelEdgeFace { uint32_t face; Vector into; };`
  `bool KernelEdgeFaces(const std::shared_ptr<const KernelShape> &, Vector a, Vector b,
  KernelEdgeFace out[2])`: the two planar faces at the straight B-rep edge lying on ab.
- `Group::REMAP_FILLET_TANGENT_1 = 1020`, `…_TANGENT_2 = 1021`, `REMAP_FILLET_AXIS = 1022`,
  `REMAP_CHAMFER_EDGE_1 = 1023`, `REMAP_CHAMFER_EDGE_2 = 1024`. Line `n`'s handle is
  `Remap(edge, n)`, its points `Remap(edge, n + 10)` and `Remap(edge, n + 20)`.

- [ ] Step 1: tests (HAVE_OCCT):
  - `a_fillet_declares_its_tangent_lines_and_axis`: on the box's top-front edge with r = 2,
    the tangent lines lie 2 mm in from the edge on the top face and on the front face, and
    the axis lies 2 mm in along both.
  - `a_chamfer_declares_its_edge_lines`: d = 2, the same two lines.
  - `derived_lines_follow_a_change_upstream`: change the extrusion depth from 30 to 40. The
    top tangent line moves with the top face; checked after one `GenerateAll`, with no stale
    pass.
  - `a_constraint_on_a_tangent_line_survives`: a later 3D sketch with a point constrained on
    the tangent line keeps it there after a depth change, undo, save and reload.
- [ ] Step 2: test (any build): `derived_lines_exist_without_occt`: with the SolveSpace
  kernel, the entities exist (on the edge), so the constraint above is not pruned.
- [ ] Step 3: test: `a_rounded_edge_is_hidden_while_the_fillet_is_active`: the edge entity
  is not visible with the fillet group active, and is visible with the extrusion active.
- [ ] Step 4: RED. Implement:
  - `Generate` cases;
  - the adjacency cache with signs;
  - `SS.derivedTopologyChanged` (set in the non-bbox pass, consumed at the end of
    `GenerateAll` with one guarded repeat);
  - `roundedBy` hiding.
- [ ] Step 5: GREEN; all suites; commit.

### Task 4: The UI: menu, key, Properties, Outliner, Problems

**Files:**
- Modify: `src/ui/menulayout.cpp` (New Group ▸ Fillet, Chamfer), `res/keymaps/modern.keymap`
  (`group-fillet: Shift+F`), `src/ui/properties.cpp` (radius / distance field, edge count),
  `src/ui/outlinermodel.cpp` (type names and icons), `src/ui/problemsmodel.cpp` (if not
  done), `AGENTS.md` (§5 key table row, §6 "Built in M3b"), `docs/m1-parity.md` if it lists
  commands.
- Test: `test/ngui/fillet/test.cpp` (`ngui_SOURCES`), and the keymap preset tests where they
  enumerate bindings.

- [ ] Step 1: test `ng_fillets_a_selected_edge`. Open the box, click its top-front edge,
  press `Shift+F`, type `3` in Properties' radius field, then Enter. Expect a FILLET group
  with valA 3 and a smaller volume than the box. Undo returns to the box.
- [ ] Step 2: test `ng_chamfer_from_the_menu`: New Group ▸ Chamfer with an edge selected.
- [ ] Step 3: RED; implement; GREEN; all suites (core, UI, X11); commit.

### Final
- Self-review against this plan's review focus (no subagents unless asked); fix pass with
  tests.
- AGENTS.md updated; push `ng/m3b-features`; report.
- No stock change is expected, because fillets are NG-only. If a stock bug turns up, cut a
  `pr/` branch.
