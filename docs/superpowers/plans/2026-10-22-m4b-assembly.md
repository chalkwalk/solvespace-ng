# M4b Assembly Output Implementation Plan

> **For agentic workers:** superpowers:executing-plans, inline. On 2026-10-22 the owner said
> "go ahead" with M4b (the rest of M4 after M4a: placement and the design pose, 3MF, the
> BOM, `LINKED` compatibility, a skeleton in the Outliner).

**Goal:** finish M4's scope (AGENTS.md section 7, "Output" and "Compatibility").
- Parts carry a material.
- The document gives a parts list (BOM).
- Parts export to 3MF as separate objects in one file, optionally laid flat.
- Linked files are parts.
- The Outliner shows the skeleton.

**Placement and the design pose (ruling):** in-file parts are authored in place, so the
authored layout *is* the design pose and every part's placement is the identity.
- A placement transform only matters once parts move away from it, which is M5's motion
  joints ("motion joints move Parts away from the design pose without rebuilding their
  geometry").
- `LINKED` parts already have their 6-DOF placement, solved by constraints on their points.
- So the part transform (7 parameters), the assembly constraints that move it, and the
  motion pose are built with the joints in M5, where the "done when" exercises them.
- Cost if wrong: the owner expected in-file parts to be movable now. That would be one
  transform per part applied to its solid and entities, the first task of M5.

**Architecture:**
- **Material.** `Part::density` (g/cm³; 0 = the document's density from Preferences), saved
  as `Part.density`.
  - The first part's record is written once a part has a name or a material, as for
    `Part.name` today.
  - Properties of a group shows its part's material radios (the Preferences list plus
    "default").
  - Colour comes from the groups' colours, as now. 3MF writes each triangle's colour.
- **Parts list.** `SolveSpaceUI::PartsList()` returns one row per part with a solid:
  - name, source (the linked file, if the part is a link);
  - material name and density;
  - volume, mass and bounding box size (exact from the B-rep, else from the mesh, as
    Mass Properties).

  Inspect ▸ Parts List (`parts-list`) shows it as a message. File ▸ Export Parts List
  (`export-parts-list`) writes it as CSV (`ExportPartsList(path)`).
- **3MF.** Export Parts to a `.3mf` name writes one file with an object per part, named
  after the part, with a colour group for the triangles' colours.
  - The package is a zip, stored without compression (CRC-32 written by hand), so no new
    dependency: `src/export3mf.cpp`, `Export3mf(path, objects)`.
- **Lay flat.** A Preferences toggle, "lay parts flat on export" (`ExportLayFlat`, off).
  - Each part is turned so its largest planar face (by area, among triangles with the same
    outward normal and plane) faces down, then moved to stand on z = 0.
  - In a 3MF file the objects are also set side by side along x, 5 mm apart.
  - Applies to STL and 3MF; STEP keeps the design pose.
- **Linked parts.** New Group ▸ Link Part starts a new part, named after the linked file.
  - Old files keep their linked groups in their parts (they open unchanged).
  - Properties of a linked group that shares its part offers "own part"
    (`SolveSpaceUI::MoveToNewPart(hg)`), which gives it a new part of its own.
- **Skeleton.** With two or more parts, the Outliner heads the groups before the first one
  that makes a solid "Skeleton": the shared sketches and workplanes.

## Global constraints
As before. Old files open and save byte-identical (golden tests); a single-part file with no
material writes no part record. No new dependencies (§2).

## Review focus
1. A part whose solid is empty (its last group deleted): left out of the list and the 3MF,
   no crash.
2. A part name with characters XML can't hold as they are (`<`, `&`, quotes): the 3MF is
   still well-formed.
3. A part with no planar face (a sphere-like lathe): lay flat leaves it upright, standing
   on z = 0.
4. Undo of a material change and of "own part".
5. A 3MF of a part with more than 65535 triangles: zip sizes are 32-bit, so this is fine
   up to 4 GB; a larger file fails with an error.

## Tasks
1. **Material and parts list** (`test/group/partslist/test.cpp`):
   - `Part::density`, saved, undoable;
   - `PartsList` rows: volumes and masses of two boxes in two parts, the default density;
   - CSV export;
   - Properties radios (`test/ngui/parts`).
2. **3MF** (`test/group/parts3mf/test.cpp`):
   - one file, two objects with the right triangle counts and volume;
   - names escaped;
   - the zip read back by Python's `zipfile` in a check, or by our own reader in the test.
3. **Lay flat** (core test): a box extruded on a tilted workplane is laid on its biggest
   face at z = 0; STL and 3MF.
4. **Linked parts and the skeleton:**
   - Link Part makes a new part named after the file;
   - "own part" moves a group;
   - the Outliner's skeleton header (`test/ngui/parts`);
   - AGENTS.md "Built in M4b".
