# M3c Mass Properties, Shell and Draft Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans (chosen: inline;
> the owner asked to merge M3b and continue with the rest of M3, 2026-10-21).

**Goal:** three more parts of M3 (AGENTS.md sections 6 and 8):
- mass properties (volume, surface area, mass, centre of mass), exact from the B-rep;
- a shell group (hollow the solid, optionally opening chosen faces), whose inner faces are
  constrainable;
- a draft group (taper chosen side faces of an extrusion about its sketch plane), whose
  drafted faces are constrainable.

Holes and threads (M3d), loft and sweep (M3e), and STEP import (M3f) follow.

**Architecture** (as in M3b, docs/superpowers/plans/2026-10-21-m3b-fillet-chamfer.md):
- **Mass properties.**
  - `KernelMassProperties(shape, &MassProps)` uses `BRepGProp`, exact.
  - Without a B-rep, the running mesh (`SMesh::CalculateVolume`, area, the centroid of
    signed tetrahedra) is used, as today.
  - `SS.density` (g/cm³; setting `Density`, default 1.24, PLA; Preferences ▸ SolveSpace NG)
    gives the mass.
  - The command Inspect ▸ Mass Properties (`mass-properties`, `Command::MASS_PROPERTIES`)
    shows `SolveSpaceUI::MassPropertiesText()`.
  - Show Center of Mass uses the exact centre when there is a B-rep.
- **Faces as references.** Shell and draft groups keep `std::vector<hEntity> faces`, saved
  as `Group.faces` in format `'E'`. These are face entities: an extrusion's side
  (`REMAP_LINE_TO_FACE`), its caps, or an earlier feature's derived faces.
- **Kernel query.** `KernelFaceNormals(shape)` returns each named planar B-rep face's
  outward normal. A group caches, per face entity, whether its entity's normal points out of
  the solid. That is topology only, as with M3b's edge faces; a change triggers
  `derivedTopologyChanged`.
- **Shell** (`Group::Type::SHELL` = 5402).
  - The thickness `valA` (mm) is taken inward:
    `BRepOffsetAPI_MakeThickSolid::MakeThickSolidByJoin` on the previous solid, without the
    chosen faces.
  - Derived entities: for every face entity of the groups before it, an inner face
    `Remap(face, REMAP_SHELL_INNER)` (`FACE_NORMAL_PT` with a numeric point
    `Remap(face, REMAP_SHELL_INNER + 10)`), offset by `t` against the outward normal.
  - The kernel names each inner B-rep face after its outer face's inner entity, through
    `FaceNamer::ExistingFace`, so faces can be sketched on and picked.
- **Draft** (`Group::Type::DRAFT` = 5403).
  - Takes side faces of extrusions (`FACE_XPROD`). The neutral plane is the extrusion's
    sketch plane, through the face entity's point; the pull direction is the extrusion
    vector. The angle `valA` is in degrees, positive narrowing along the pull.
  - Uses `BRepOffsetAPI_DraftAngle`.
  - Derived entity: the drafted face `Remap(face, REMAP_DRAFT_FACE)` (`FACE_NORMAL_PT`).
    It passes through the base line (the face entity's point and the sketch line's
    direction), and its normal is the outward normal turned towards the pull by α. The
    kernel names the drafted B-rep face with it.
  - The extrusion's own top edges keep their untapered positions: a later sketch should
    use the drafted faces.
- **UI and fallbacks.**
  - New Group ▸ Shell / Draft;
  - Properties: thickness / angle (expressions), face count, problem;
  - Outliner icons and Problems, as in M3b;
  - both need Open CASCADE; with SolveSpace's kernel the solid passes through with a
    problem.

**Spec:** AGENTS.md sections 6 (features as groups; derived entities: "Shell: inner faces =
source faces offset by t"; "Draft / taper on extrude: side faces = base edge + normal rotated
by α"; mass properties), 8.

## Global constraints
- `test/` stays green on every commit; files without the new groups save as before.
- `src/kernel/` has no UI references; OCCT headers only in `occt*.cpp` and tests;
  `-DENABLE_OCCT=OFF` builds (stubs).
- Derived entities always exist (constraints never pruned); kernel jobs stay values-only.

## Review focus
1. Shell thicker than the part: OCCT fails. The previous solid shows, with a problem.
2. Opening a curved face (a cylinder's side): the shell works, and no inner entity is made
   for a face with no entity.
3. Draft on a face that is not an extrusion side: refused when the group is made.
4. A drafted face used by a later sketch's constraint follows a depth change in one
   regeneration.
5. Mass properties of an empty model: "no solid", not a division by zero.

---

### Task 1: Mass properties
- Files: `src/kernel/kernel.h` (`struct MassProps { double volume, area; Vector centre; }`,
  `bool KernelMassProperties`), `occtkernel.cpp`, `solvespacekernel.cpp` (stub);
  `src/solvespace.{h,cpp}` (`density`, `MassPropertiesText`, the command); `src/ui.h`,
  `commandnames.cpp`, `graphicswin.cpp`, `ui/menulayout.cpp`; `ui/prefspanel.cpp` (density);
  `src/solvespace.cpp` `UpdateCenterOfMass`.
- Tests (`test/analysis/massprops/test.cpp`):
  - `mass_properties_of_a_box`, with box.slvs and OCCT: volume 6000, area 2200, mass at
    1.24 g/cm³ 7.44 g, centre (5, 10, 15);
  - the same with SolveSpace's kernel (mesh, any build);
  - `mass_properties_without_a_solid` says so.

### Task 2: Shell
- Files: `src/sketch.h` (`SHELL`, `faces`, `faceOut` cache, `REMAP_SHELL_INNER = 1030`),
  `file.cpp` (`Group.faces`), `ui.h`/`commandnames.cpp`/`graphicswin.cpp` (`GROUP_SHELL`
  "group-shell"), `group.cpp` (`MenuGroup`, `Generate`), `generate.cpp` (prune faces),
  `groupmesh.cpp` (job), `kernel.h` (`Op::SHELL`, `faceNames`, `KernelFaceNormals`),
  `occtkernel.cpp`, `solvespacekernel.cpp`.
- Tests (`test/group/shell/test.cpp`, using box.slvs):
  - `shell_opens_the_top_and_hollows_the_box`: t = 1, top face open, volume
    6000 − 8·18·29 = 1824;
  - `a_closed_shell_keeps_the_outside` (no faces): volume 6000 − 8·18·28;
  - `shell_too_thick_leaves_the_solid_and_says_so`;
  - `the_shell_declares_inner_faces`: the inner face of the front side is the plane y = 1
    with normal ±y;
  - `a_point_on_an_inner_face_follows_a_change` (depth 30 → 40, the inner bottom stays at
    z = 1, the inner front at y = 1);
  - `inner_faces_carry_the_names`: the inner bottom's triangles are named
    `Remap(bottom, REMAP_SHELL_INNER)`;
  - `shell_needs_faces_of_the_solid` (a sketch line is refused);
  - `shell_with_the_solvespace_kernel_says_so`.

### Task 3: Draft
- Files: as task 2 (`DRAFT`, `GROUP_DRAFT` "group-draft", `REMAP_DRAFT_FACE = 1032`,
  `Op::DRAFT`, `draftFaces` with point and pull).
- Tests (`test/group/draft/test.cpp`):
  - `draft_tapers_the_four_sides` (box, 5°): volume equals the frustum's within 0.5%, and
    the top is smaller than the bottom;
  - `a_drafted_face_is_declared_and_named`: the front side drafted is the plane through
    the bottom front line tilted 5° inwards, and the triangles are named with it;
  - `draft_takes_only_extrusion_sides` (the top cap is refused);
  - `a_point_on_a_drafted_face_follows_a_change`;
  - `negative_draft_widens`.

### Task 4: UI
- Menu (New Group ▸ Shell, Draft; Inspect ▸ Mass Properties), shelf, Outliner icons
  (`shell.svg`, `draft.svg` from generate.py), Properties fields (`feature-size` for the
  thickness / angle), Preferences density, AGENTS.md "Built in M3c".
- UI tests (`test/ngui/features/test.cpp`):
  - `ng_shells_with_the_top_face_open`: click the top face, New Group ▸ Shell, type 1;
  - `ng_drafts_a_side`;
  - `ng_shows_mass_properties` (a message, answered).
