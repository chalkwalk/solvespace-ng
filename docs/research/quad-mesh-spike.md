# Quad mesh export for Blender: spike (2026-10-21)

The owner's question (review 5): can the CAD model give a clean, quad-centric mesh for
Blender, with creases defined for subdivision and loops that look made by a person, since
the B-rep understands the geometry's flow better than a triangle mesh?

**Answer: yes, for most of a typical printed part.** The prototype is
`src/kernel/occtquadmesh.cpp`, reached from Export Mesh with a `.usda` file name; it needs the
Open CASCADE kernel. Tests: `test/kernel/quadmesh`.

## How it works
- **Edges are cut once,** no longer than a target length (default: a thirtieth of the
  model's size) and turning at most 30°. Both faces share those points, so the mesh is
  watertight (every polygon edge used exactly twice).
- **A four-sided face becomes a quad grid** in its own (u, v): a Coons blend of its four
  sides, evaluated on the true surface. This covers box sides, extruded walls and caps
  (SolveSpace's circles are four arcs), fillets, chamfers, and seamed cylinders.
  - Opposite sides are cut alike; the larger count spreads through neighbouring faces
    until every grid agrees.
- **A flat face with one hole becomes rings of quads** from the outer wire in to the
  hole's. This covers a shell's rim and a plate with a hole.
  - The two wires are cut into as many points, they are lined up, and rings are added so
    none is wider than the target.
- **Any other face without a hole** (review 6, the owner's suggestion) gets rings of quads from
  its edge in towards its centre, then a fan of triangles round the centre. This applies when
  the centre and every ring point lie on the face.
- **Anything left** (a face the centre can't see all of, a curved face with holes) is
  triangulated by clipping ears in its (u, v), its holes bridged by a cut. N-gons remain only if
  that fails.
- **Creases:** every edge where faces meet at more than 20° is creased at full sharpness
  (USD 10). Tangent edges, such as a fillet's boundaries, stay smooth.
- **Output:** USD text: `faceVertexCounts`, `faceVertexIndices`, `points`, `creaseIndices`,
  `creaseLengths` and `creaseSharpnesses`, with `subdivisionScheme = "catmullClark"`, in
  millimetres (`metersPerUnit = 0.001`), Z up.

## What we saw (Blender 5.0.1, Catmull-Clark level 2)
- **A box with its top edges filleted:** all quads. It subdivides into a crisp box with
  round fillets. The fillet corners, where three fillets meet, gather lines but stay quads.
- **A shelled box (open top):** all quads, rim included. It subdivides cleanly.
- **A box with a counterbore cutting into the fillets:** at first the notched fillet faces
  (curved, more than four sides) were n-gons and puckered when subdivided. Since review 6 the
  rounded box's five-sided end faces are rings round a centre fan, and the notched strips are
  ear-clipped triangles: no n-gons (2075 quads, 134 triangles), and it subdivides cleanly.
  The notched strips' triangles are long slivers (up to the strip's length), which Blender's
  wireframe overlay draws as spikes. Splitting those strips at the notch into four-sided
  patches would give quads there.

The local Blender is a distro build without USD support. The check loaded the file with a
small parser and set the `crease_edge` attribute itself. Official Blender builds import USD,
creases included; the owner should confirm with theirs.

## What it would take to go further
1. **Curved faces with more than four sides or with holes** (a fillet cut by a hole, a
   boolean's patchwork faces): split them into four-sided patches (quad layout by
   T-junction-free patch decomposition) or fall back to a feature-aligned quad remesher.
   This is the research part.
2. **Density control** in the export dialog (target length; detail on curves).
3. **Face colours and names** as USD material bindings or face sets, so groups stay
   selectable in Blender.
4. **An OBJ variant** for other tools. It loses the creases, which OBJ cannot carry.
