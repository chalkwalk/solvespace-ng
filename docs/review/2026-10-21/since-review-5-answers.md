# SolveSpace NG review 6: since review 5

Your review-5 changes, loft and sweep, sketching on a face, feature sizes as dimensions, materials, and the quad-mesh-for-Blender spike. App screens are dark (left) and light (right); the last three are Blender renders. For every row give a verdict (keep, tweak or rethink); where there is a PROPOSAL, answer yes or no in the proposal column; notes are optional.

| sheet | question | verdict [keep/tweak/rethink] | proposal [yes/no] | note |
|---|---|---|---|---|
| 1 | The tool shelf: Fillet, Chamfer, Shell, Draft and Hole now have their own Features block, at the bottom. Fillet and chamfer are drawn front-on, so the round and the straight cut tell apart. The hole has its bolt. Loft and Sweep (new) sit with the groups that make a solid. |  | yes |  |
| 2 | A 3 mm fillet. Its radius is now a dimension (R3.00), like an extrusion's depth: double-click it to edit; delete it, or make it reference, and dragging the fillet's lines or constraining them sets the radius. Its derived lines are the new dashed Reference style. PROPOSAL: show a feature's size dimension only while its group is active, as Fusion shows a feature's dimensions while you edit it. |  | yes |  |
| 3 | Fillet in Properties: the radius (the dimension's value), then its edges, each clickable to select, x to remove. |  | yes |  |
| 4 | Shell in Properties: thickness, the wall's side (inside, outside or both sides), and the faces left open. |  | yes |  |
| 5 | Hole in Properties: kind, thread, depth, counterbore size, and the new print clearance (added to every diameter); the hole centres listed below. |  | yes |  |
| 6 | Sketch on a face: click the top face, Shift+W, and draw. The sketch follows the face when the model changes; it works on any face, a drafted, shell or loft face included. |  | yes |  |
| 7 | A loft from a 10 mm square to a 4 mm one, 20 above: pick a curve in each profile sketch, in order, then New Group > Loft. Its end faces can be sketched on. |  | yes |  |
| 8 | Loft in Properties: the profiles in order (removable), and how the solid combines, as for an extrusion. |  | yes |  |
| 9 | A circle swept along three connected lines: pick the profile's curve first, then the path's. Sharp corners in the path are mitred. PROPOSAL: an option to round the path's corners instead (a transition radius), as Fusion's and Onshape's sweeps can. |  | yes |  |
| 10 | Preferences: the material for mass properties, PLA, PETG, ABS, ASA, TPU, nylon or polycarbonate, beside the density field, which is still there for any other material. |  | yes |  |
| 11 | The quad mesh spike, in Blender: Export Mesh to .usda. Left, the exported cage, a box with its four top edges filleted: quads that follow each face's flow, the sharp edges creased. Right, that cage subdivided twice (Catmull-Clark): sharp where creased, round where filleted. Is this the kind of mesh you were after? |  | yes |  |
| 12 | A shelled box: the rim, a face with a hole, is rings of quads, so it subdivides cleanly too. |  | yes |  |
| 13 | The hard case: a counterbore cutting into the fillets leaves curved faces with more than four sides. Those become n-gons and pucker when subdivided (the top right). Details: docs/research/quad-mesh-spike.md. PROPOSAL: invest in splitting such faces into four-sided patches, the research part, to make the export production quality. |  | yes | Can we subdivide into a radial set of triangles? I know I asked for quads, but this seems appropriate. |
