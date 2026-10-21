# M3a Open CASCADE Kernel at Parity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans (chosen: inline;
> the owner asked for M3 to start, 2026-10-21, and for roadmap work to continue without
> intervention).

**Goal:** an `OcctKernel` behind `IKernel` that makes every solid SolveSpace's own kernel
makes (extrude, lathe, revolve, helix, step-and-repeat, link, the four combines), passes a
kernel parity suite over every model in `test/`, and becomes the desktop (NG) default. This
is the first part of M3 (AGENTS.md section 8); derived entities and the OCCT-only features
(fillet, chamfer, shell, draft, hole, loft, sweep) follow in M3b and later plans.

**Architecture:**
- OCCT is a shallow submodule (`extlib/occt`, V7_9_3), built once per build directory as a
  Release static external project with only the toolkits used (`cmake/OcctVendored.cmake`,
  `ENABLE_OCCT`, default on except Emscripten; `-DOCCT_PREFIX=` reuses another install).
- A `Body` gains `std::shared_ptr<const KernelShape> shape`, an immutable B-rep that only
  the OCCT kernel reads (`src/kernel/occtkernel.cpp` defines it). Groups keep `thisShape` and
  `runningShape` next to their shells and meshes; jobs carry the previous, source and
  imported shapes, and the background chain shares them (immutable, so no copies).
- The rest of SolveSpace keeps reading meshes: the OCCT kernel tessellates each body into
  its `SMesh` (chord tolerance from `SS.ChordTolMm()`, the thread-local override on the
  worker), with every triangle's `meta.face` and colour from its B-rep face and true vertex
  normals. `Body::tessellated` tells `GenerateDisplayItems` to keep those normals and find
  edges as it does for a NURBS shell. Saving writes those triangles, so linking a file made
  with OCCT imports a mesh, as for mesh groups today.
- Faces are named exactly as SolveSpace names them (the same `FaceNamer` calls with the same
  keys: caps `REMAP_TOP`/`REMAP_BOTTOM`, extruded sides `REMAP_LINE_TO_FACE`, revolve ends
  `REMAP_LATHE_START`/`END`, copies remapped by copy number), so files, constraints on faces
  and sketches on faces carry over between kernels. Each B-rep face's name and colour live in
  a map inside `KernelShape`, carried through booleans and transforms by OCCT's history
  (`Modified`, `Generated`).
- Inputs with no B-rep (an imported mesh, a linked file's surfaces or triangles, a group
  forced to a mesh) take SolveSpace's mesh path (`CombineMeshes`, shared with
  `SolveSpaceKernel`), as AGENTS.md section 6 says for mesh bodies.
- `Kernel()` returns the kernel chosen by `SS.kernelKind`: SolveSpace in stock and in the
  core test suite (their references are SolveSpace's exact output), OCCT in NG once parity
  passes (Preferences ▸ SolveSpace NG ▸ Geometry kernel).
- STEP export writes the OCCT B-rep when there is one (`STEPControl_Writer`).

**Tech stack:** C++20, Open CASCADE 7.9.3 (LGPL-2.1 + exception, allowed by section 2).

**Spec:** AGENTS.md sections 2 (OCCT allowed), 4 (kernel/ layer), 6 (IKernel, OcctKernel,
naming, regeneration), 8 (M3 row, kernel parity tests).

## Global constraints
- `test/` stays green on every commit; stock references are never edited.
- Files made with the SolveSpace kernel are unchanged byte for byte.
- `src/kernel/` never references `SS.GW`, `SS.TW` or `ui/`; OCCT headers appear only in
  `src/kernel/occt*.cpp` and tests.
- The web build and `-DENABLE_OCCT=OFF` builds keep working (`HAVE_OCCT` guards).
- Kernel work stays thread-safe: the worker may run the OCCT kernel while dragging.

## Review focus
1. A profile with arcs and splines (rational Beziers) and holes: the B-rep face must be valid
   and the extrusion closed.
2. A difference that removes a whole body or leaves nothing: an empty result, not a crash.
3. A boolean OCCT fails: `booleanFailed` set and the Problems panel says so, as today.
4. A model mixing a linked mesh with OCCT solids: falls back to the mesh path, still closed.
5. Undo, save and reopen of an OCCT model: the same faces keep the same handles.

---

### Task 1: OCCT builds and links
- `extlib/occt` submodule (shallow, V7_9_3); `cmake/OcctVendored.cmake`; `ENABLE_OCCT`;
  `solvespace-core` links `occt` and defines `HAVE_OCCT`.
- Test `test/kernel/occt/test.cpp`: a 10×20×30 box measures 6000.

### Task 2: Shapes ride along with bodies; the kernel is selectable
- `kernel.h`: `struct KernelShape;`, `Body::shape`, `Body::tessellated`; `KernelJob`
  `prevShape`, `sourceShape`, `impShape`; `enum class KernelKind { SOLVESPACE, OCCT }`,
  `Kernel(KernelKind)`, and `Kernel()` = the chosen one.
- Group: `thisShape`, `runningShape`, `tessellated`; cleared in `Clear`, dropped in undo
  copies; `BuildKernelJob` and `ApplyKernelResult` carry them; regen.cpp passes them along
  the chain.
- `GenerateDisplayItems`: a tessellated running mesh keeps its normals, edges found as SHARP.
- `CombineMeshes` (the forced-to-mesh path) moves to a shared function.
- Tests: switching `SS.kernelKind` picks the kernel; `test/kernel/solvespace` unchanged.

### Task 3: Extrude, combine and tessellate
- `occtkernel.cpp`: SBezierLoopSet → planar `TopoDS_Face` (degree 1 → line, else rational
  Bezier edges; outer loop + holes), each edge remembering its `SBezier::entity`;
  `BRepPrimAPI_MakePrism` along `ttop - tbot` from the profile moved by `tbot`; caps named
  `REMAP_BOTTOM`/`REMAP_TOP` from `FirstShape`/`LastShape`; a side made from a line segment
  named `REMAP_LINE_TO_FACE` of its entity (as SolveSpace names only planar sides).
- Combine: `BRepAlgoAPI_Fuse`/`Cut`/`Common`; ASSEMBLE = compound; failure =
  `booleanFailed`, result = the previous body. Face names follow `Modified`.
- Tessellate with `BRepMesh_IncrementalMesh` into the body's mesh, triangles oriented by
  face orientation, normals from the triangulation.
- Tests `test/kernel/occt`: a box sketch extruded measures exactly; the extruded rectangle's
  faces carry the same handles under both kernels; union, difference, intersection of two
  extrusions give the expected volumes; a profile with an arc and a hole stays closed.

### Task 4: Lathe, revolve, helix
- Lathe: `BRepPrimAPI_MakeRevol` 2π; revolve: angle `anglef - angles` from the profile
  rotated by `angles`; helix: `BRepOffsetAPI_MakePipeShell` of the profile along a helical
  spine with the axis as binormal; ends `REMAP_LATHE_START`/`END` (swapped as
  `CheckNormalAxisRelationship` swaps them), sides `ExistingFace(entity, LINE_TO_FACE)`.
- Tests: volumes against `test/kernel/revolved`'s recorded numbers (lathe 9409.5476,
  revolve 3141.5905, helix 3124.1269, within tessellation tolerance); end faces named alike.

### Task 5: Step and repeat, link, mesh fallback
- Copies by `gp_Trsf`, faces renamed per copy (`REMAP_LAST` for the last), fused pairwise
  (or assembled) as `StepAndRepeat` does.
- LINKED and forced-to-mesh, or any input without a shape: SolveSpace's mesh path.
- Tests: translate and rotate patterns measure n × the source; a link combines as a mesh.

### Task 6: Kernel parity suite and the NG default
- `test/kernel/parity`: every `.slvs` under `test/` regenerated with both kernels; volume and
  area within 1%, bounding box within 0.1% of its size (plus the chord tolerance), watertight
  when SolveSpace's is. Models SolveSpace itself fails on are skipped and listed.
- `SS.kernelKind` setting (`Kernel`), Preferences radio; NG seeds OCCT, stock and core tests
  keep SolveSpace; the UI test suite runs with OCCT.
- STEP export from `runningShape` when there is one.
- AGENTS.md section 6/9 updated (build notes, `OCCT_PREFIX`).
