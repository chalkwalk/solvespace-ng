# M3h Feature Sizes as Solver Parameters Implementation Plan

> **For agentic workers:** superpowers:executing-plans, inline. The owner asked for this in
> review 5 (2026-10-21):
> - a handle for the fillet radius, or matching another fillet's;
> - the shell's thickness constrained like other elements;
> - pulling a drafted face's top and having the angle follow.
>
> On "proceed with the next work items" it is the second of the two proposals.

**Goal:** a feature's size works like an extrusion's depth.
- Fillet radius, chamfer distance, shell thickness and draft angle become solver parameters
  of their groups, held by a dimension the group starts with.
- The dimension is drawn in the viewport, editable there and in Properties, and can be made
  reference or deleted.
- When the dimension doesn't hold the size, constraints on the feature's derived entities or
  a drag of them set it.

**Architecture:**
- **A new point entity type, `POINT_ALONG_PARAM`.** `numPoint` is the base, `numNormal`'s
  vector part is the direction per unit, and `param[0]` is the parameter. The point is
  `base + direction × param`, as expressions (constraints in the group drive the
  parameter) and numerically. `PointForceTo` (dragging) sets the parameter from the point's
  projection on the direction.
- **Feature groups get `h.param(0)`,** the size, initialised from `valA`:
  - fillet radius, chamfer distance and shell thickness in mm;
  - draft: tan of the angle, which keeps the derived top edge linear.

  Kernel jobs read the parameter's solved value.
- **Derived entities** become `POINT_ALONG_PARAM` where they depend on the size:
  - fillet tangent points: direction `uᵢ / tan(α/2)`;
  - fillet axis: `unit(u1+u2) / sin(α/2)`;
  - chamfer edge points: `uᵢ`;
  - shell inner and outer face points: ∓ outward normal, by the side;
  - draft: a new derived line, the drafted face's top edge `REMAP_DRAFT_TOP`: the base line
    raised by the extrusion's height, direction `−outward × height` per unit tan α.

  Face entities whose point is such a point follow the parameter too.
- **A new constraint type, `FEATURE_SIZE`.**
  - Equation: `param(0) − valA`; for a draft, `param(0) − tan(valA°)`.
  - Label: "R 3", "3" or "5°" for a draft. It is anchored at a derived entity (the first
    derived point, else the first edge or face), with the usual draggable label offset, and
    drawn with a leader.
  - A feature group starts with one, so it has no degrees of freedom.
  - Editing it is `SetConstraintValue`; the Properties size field edits it (or the
    parameter, when there is none); reference and delete work as for any dimension.

**Spec:** AGENTS.md section 6 ("features declare derived entities as solver expressions, so
later sketches and constraints can use them and they move live"), section 3 (constraints
reference solver entities), review 5 answers (docs/review/2026-10-21/answers.md, sheets 4, 6
and 8).

## Global constraints
As before. Old files open: a feature group saved before this has `valA` but no parameter or
dimension. It loads with the parameter at `valA` and no dimension, so it gains one degree of
freedom; the owner's files are the only ones affected.

## Review focus
1. A fillet whose dimension is deleted and nothing else constrains it: it keeps its radius
   (the solver leaves a free parameter where it is) and shows 1 DOF.
2. Dragging a tangent line with the dimension present: refused, as for a dimensioned sketch.
3. A constraint on a derived point that asks for a negative radius: the solver fails, with
   the usual Problems entry, not a crash in OCCT.
4. Undo/redo of editing the dimension, and save/reload: the same size.
5. The worker thread (dragging upstream): the jobs carry the parameter's value.

## Tasks
1. **`POINT_ALONG_PARAM` and fillet/chamfer parameters** (`test/group/fillet`):
   - a constraint in the fillet group (tangent point a set distance from the edge, with the
     size dimension deleted) sets the radius and the solid follows;
   - the parameter drives the kernel;
   - dragging a tangent point (forced) changes the radius.
2. **`FEATURE_SIZE`**:
   - made with every feature group; holds the size;
   - edit, reference and delete (1 DOF), save/reload, undo;
   - label text;
   - Properties edits it.
3. **Shell and draft parameters**, and the draft's top edge: a point constrained on the
   draft top edge at a set inset sets the angle; a constraint on the shell's inner face sets
   the thickness.
4. **UI** (`test/ngui`): the radius dimension shows in the viewport; double-clicking it edits
   it; deleting it and dragging a tangent line changes the radius. AGENTS.md.
