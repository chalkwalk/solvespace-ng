# M3e Loft and Sweep Implementation Plan

> **For agentic workers:** superpowers:executing-plans, inline (the owner asked to continue
> with the rest of M3, 2026-10-21).

**Goal:** the two remaining shape-making features of M3 (AGENTS.md section 6: "Loft (two or
more sketches) / sweep (profile + path sketch) — derived: input sketch entities, start/end
profile frames").

**Architecture:**
- **New groups.** `LOFT` = 5405 and `SWEEP` = 5406 are solid groups like extrude: they make
  `thisBody` and combine it with the previous solid (union, difference, intersection,
  assemble), so they appear in the combine options and colours.
- **Inputs:** `std::vector<hEntity> profiles`, saved `Group.profiles`.
  - Loft: one picked curve per profile sketch, in loft order. The profile is that curve's
    sketch's closed outer loop, so each profile sketch holds one closed shape.
  - Sweep: the first is a curve of the profile sketch, and the rest are the path's curves,
    connected end to end in any order.
- **Kernel jobs** carry values: each profile's outer loop as `std::vector<SBezier>`, and the
  path's curves.
  - Loft: `BRepOffsetAPI_ThruSections` (solid, smooth).
  - Sweep: `BRepOffsetAPI_MakePipeShell` (corrected Frenet, as SolveSpace's own helix uses
    the axis binormal), `MakeSolid`.
  - The end caps are named with the derived end faces; sides are unnamed.
  - Holes inside a profile are not lofted or swept in M3e: only the outer loop is, and the
    Problems panel says so.
- **Derived entities:**
  - Loft: `Remap(first profile, REMAP_LOFT_START)` and `Remap(last profile, REMAP_LOFT_END)`,
    face entities in the end profiles' planes, facing out.
  - Sweep: `Remap(profile, REMAP_SWEEP_START)` at the path's start, facing back along it, and
    `REMAP_SWEEP_END` at its end, facing along it.

  They are numeric from the current sketches, so they follow drags, and they always exist.
  The input sketch entities are already entities.
- **UI:** New Group ▸ Loft / Sweep, shelf and icons, Properties (the profile count, the
  combine options), the Outliner.

## Global constraints
As before (green on every commit; old files unchanged; OCCT-only code guarded; values-only
jobs; derived entities always exist).

## Review focus
1. Profiles whose loops have different numbers of edges (square to circle): the loft works.
2. A path that isn't connected: refused with a message, or a problem.
3. A loft through three profiles: smooth.
4. A profile with a hole: the outer loop only, with a note.
5. Moving a profile sketch's point: the solid and the end faces follow.

## Tasks
1. **Loft** (`test/group/loft/test.cpp`):
   - two equal squares 20 apart: volume 2000;
   - a 10-square to a 4-square: the frustum, 1040;
   - a square to a circle: volume between the two prisms;
   - start/end faces;
   - refused with fewer than two profiles or with SolveSpace's kernel;
   - save/reload.
2. **Sweep** (`test/group/sweep/test.cpp`):
   - a circle of radius 1 along a 10 line: π·10;
   - along a quarter arc of radius 10: π·5π by Pappus;
   - along a line and a line joined: watertight;
   - start/end faces;
   - a disconnected path is refused.
3. **UI**: menu, shelf, icons, Properties, `test/ngui` flows, AGENTS.md.
