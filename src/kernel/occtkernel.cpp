//-----------------------------------------------------------------------------
// The Open CASCADE kernel (AGENTS.md section 6, M3): solids are OCCT B-reps, kept in each
// Body's KernelShape, and the rest of SolveSpace gets their tessellation, every triangle
// carrying its face's name and colour. Faces are named with the same FaceNamer calls, in the
// same order, as SolveSpace's own kernel makes them, so files, and constraints and sketches on
// faces, carry over between kernels; a face's name follows it through booleans and copies by
// OCCT's history. What has no B-rep (a linked file, a mesh, a group forced to a mesh) takes
// SolveSpace's mesh path.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "kernel/occtshape.h"

#include <mutex>
#include <set>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAlgoAPI_BooleanOperation.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepGProp.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepLib.hxx>
#include <BRepLib_ToolTriangulatedShape.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepOffsetAPI_DraftAngle.hxx>
#include <BRepOffsetAPI_MakePipeShell.hxx>
#include <BRepOffsetAPI_MakeOffsetShape.hxx>
#include <BRepOffsetAPI_MakeThickSolid.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <BRepBndLib.hxx>
#include <BRepTools.hxx>
#include <Bnd_Box.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <GCE2d_MakeSegment.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <Interface_Static.hxx>
#include <STEPControl_Reader.hxx>
#include <STEPControl_Writer.hxx>
#include <StepBasic_Product.hxx>
#include <StepData_StepModel.hxx>
#include <TCollection_HAsciiString.hxx>
#include <GProp_GProps.hxx>
#include <GProp_PrincipalProps.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <Geom_BezierCurve.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <NCollection_DataMap.hxx>
#include <Poly_PolygonOnTriangulation.hxx>
#include <Poly_Triangulation.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <Standard_Failure.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopTools_MapOfShape.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Pln.hxx>
#include <gp_Quaternion.hxx>

namespace SolveSpace {

namespace {

gp_Pnt Pnt(Vector v) { return gp_Pnt(v.x, v.y, v.z); }
gp_Dir Dir(Vector v) { return gp_Dir(v.x, v.y, v.z); }
gp_Vec Vec(Vector v) { return gp_Vec(v.x, v.y, v.z); }
Vector Vec(const gp_XYZ &p) { return Vector::From(p.X(), p.Y(), p.Z()); }

// BRepMesh stores triangulations in the shapes, which bodies share across threads.
std::mutex gMeshMutex;

bool IsEmpty(const std::shared_ptr<const KernelShape> &s) {
    return !s || s->shape.IsNull();
}

// A loop of the profile as a wire; each edge made from a sketch entity remembers it.
TopoDS_Wire MakeWire(const SBezierLoop &loop, NCollection_DataMap<TopoDS_Shape, uint32_t,
                                                                  TopTools_ShapeMapHasher> *ents) {
    int n = loop.l.n;
    std::vector<TopoDS_Vertex> verts(n);
    for(int i = 0; i < n; i++) {
        verts[i] = BRepBuilderAPI_MakeVertex(Pnt(loop.l[i].Start()));
    }
    BRepBuilderAPI_MakeWire wire;
    for(int i = 0; i < n; i++) {
        const SBezier &sb = loop.l[i];
        const TopoDS_Vertex &va = verts[i], &vb = verts[(i + 1) % n];
        TopoDS_Edge edge;
        if(sb.deg == 1) {
            edge = BRepBuilderAPI_MakeEdge(va, vb);
        } else {
            TColgp_Array1OfPnt   poles(1, sb.deg + 1);
            TColStd_Array1OfReal weights(1, sb.deg + 1);
            for(int j = 0; j <= sb.deg; j++) {
                poles.SetValue(j + 1, Pnt(sb.ctrl[j]));
                weights.SetValue(j + 1, sb.weight[j]);
            }
            Handle(Geom_BezierCurve) curve = new Geom_BezierCurve(poles, weights);
            edge = BRepBuilderAPI_MakeEdge(curve, va, vb, 0, 1);
        }
        if(sb.entity != 0) ents->Bind(edge, sb.entity);
        wire.Add(edge);
    }
    return wire.Wire();
}

double SignedArea(const SBezierLoop &loop, Vector normal) {
    SContour sc = {};
    loop.MakePwlInto(&sc);
    double area = 0;
    for(int i = 0; i < sc.l.n; i++) {
        Vector a = sc.l[i].p, b = sc.l[WRAP(i + 1, sc.l.n)].p;
        area += a.Cross(b).Dot(normal);
    }
    sc.l.Clear();
    return area / 2;
}

// A loop set (one outer loop and its holes) as a planar face whose normal is the set's.
TopoDS_Face MakeFace(const SBezierLoopSet &sbls,
                     NCollection_DataMap<TopoDS_Shape, uint32_t, TopTools_ShapeMapHasher> *ents) {
    Vector normal = sbls.normal.WithMagnitude(1);
    int    outer  = 0;
    std::vector<double> areas;
    for(int i = 0; i < sbls.l.n; i++) {
        areas.push_back(SignedArea(sbls.l[i], normal));
        if(fabs(areas[i]) > fabs(areas[outer])) outer = i;
    }
    auto oriented = [&](int i, bool ccw) {
        TopoDS_Wire w = MakeWire(sbls.l[i], ents);
        if((areas[i] > 0) != ccw) w.Reverse();
        return w;
    };
    BRepBuilderAPI_MakeFace face(gp_Pln(Pnt(sbls.point), Dir(normal)), oriented(outer, true));
    for(int i = 0; i < sbls.l.n; i++) {
        if(i != outer) face.Add(oriented(i, false));
    }
    return face.Face();
}

void Orient(TopoDS_Shape *shape) {
    for(TopExp_Explorer ex(*shape, TopAbs_SOLID); ex.More(); ex.Next()) {
        TopoDS_Solid solid = TopoDS::Solid(ex.Current());
        BRepLib::OrientClosedSolid(solid);
    }
}

TopoDS_Shape MakeCompound(const std::vector<TopoDS_Shape> &shapes) {
    if(shapes.size() == 1) return shapes[0];
    TopoDS_Compound compound;
    BRep_Builder    builder;
    builder.MakeCompound(compound);
    for(const TopoDS_Shape &s : shapes) builder.Add(compound, s);
    return compound;
}

// The face of solid that a sweep's end section (a wire, or a face) bounds.
TopoDS_Shape CapOf(const TopoDS_Shape &solid, const TopoDS_Shape &section) {
    if(section.IsNull() || section.ShapeType() == TopAbs_FACE) return section;
    TopTools_IndexedMapOfShape sectionEdges;
    TopExp::MapShapes(section, TopAbs_EDGE, sectionEdges);
    for(TopExp_Explorer ex(solid, TopAbs_FACE); ex.More(); ex.Next()) {
        TopExp_Explorer edges(ex.Current(), TopAbs_EDGE);
        bool all = edges.More();
        for(; edges.More() && all; edges.Next()) {
            all = sectionEdges.Contains(edges.Current());
        }
        if(all) return ex.Current();
    }
    return TopoDS_Shape();
}

// What the sweep made from a profile edge, asked of the sweep itself: a full revolution's
// history leaves out the flat faces.
template<class Sweep>
TopTools_ListOfShape Swept(Sweep &, const TopoDS_Shape &) {
    return {};
}
TopTools_ListOfShape Swept(BRepPrimAPI_MakeRevol &revol, const TopoDS_Shape &edge) {
    TopTools_ListOfShape out;
    TopoDS_Shape made = const_cast<BRepSweep_Revol &>(revol.Revol()).Shape(edge);
    if(!made.IsNull()) out.Append(made);
    return out;
}

// Gives every face of a sweep its name: the start and end caps, and each side by the sketch
// entity whose edge made it.
template<class Sweep>
void NameSweep(Sweep &sweep, const TopoDS_Shape &first, const TopoDS_Shape &last,
               const NCollection_DataMap<TopoDS_Shape, uint32_t, TopTools_ShapeMapHasher> &ents,
               const std::function<uint32_t(int which, uint32_t entity)> &name,
               RgbaColor color, FaceMap *faces) {
    if(!first.IsNull()) {
        for(TopExp_Explorer ex(first, TopAbs_FACE); ex.More(); ex.Next()) {
            faces->Bind(ex.Current(), { name(0, 0), color });
        }
    }
    if(!last.IsNull()) {
        for(TopExp_Explorer ex(last, TopAbs_FACE); ex.More(); ex.Next()) {
            faces->Bind(ex.Current(), { name(1, 0), color });
        }
    }
    for(NCollection_DataMap<TopoDS_Shape, uint32_t, TopTools_ShapeMapHasher>::Iterator it(ents);
        it.More(); it.Next()) {
        TopTools_ListOfShape sides = sweep.Generated(it.Key());
        if(sides.IsEmpty()) sides = Swept(sweep, it.Key());
        for(const TopoDS_Shape &side : sides) {
            if(side.ShapeType() != TopAbs_FACE) continue;
            faces->Bind(side, { name(2, it.Value()), color });
        }
    }
}

// The faces of result, named after the faces of the inputs they came from.
template<class Op>
void CarryNames(Op &op, const std::vector<const FaceMap *> &inputs, FaceMap *out) {
    for(const FaceMap *in : inputs) {
        for(FaceMap::Iterator it(*in); it.More(); it.Next()) {
            const TopTools_ListOfShape &modified = op.Modified(it.Key());
            if(modified.IsEmpty()) {
                if(!op.IsDeleted(it.Key())) out->Bind(it.Key(), it.Value());
            } else {
                for(const TopoDS_Shape &m : modified) out->Bind(m, it.Value());
            }
        }
    }
}

std::shared_ptr<KernelShape> Transformed(const KernelShape &from, const gp_Trsf &trsf,
                                         FaceNamer *namer, int remap) {
    BRepBuilderAPI_Transform transform(from.shape, trsf, /*copy=*/true);
    auto out   = std::make_shared<KernelShape>();
    out->shape = transform.Shape();
    for(FaceMap::Iterator it(from.faces); it.More(); it.Next()) {
        FaceInfo info = it.Value();
        if(info.face != 0) info.face = namer->Name(hEntity{ info.face }, remap).v;
        out->faces.Bind(transform.ModifiedShape(it.Key()), info);
    }
    return out;
}

// Combines a and b; false if OCCT fails.
bool Boolean(KernelCombine how, const KernelShape &a, const KernelShape &b, KernelShape *out) {
    if(how == KernelCombine::ASSEMBLE) {
        out->shape = MakeCompound({ a.shape, b.shape });
        for(const FaceMap *in : { &a.faces, &b.faces }) {
            for(FaceMap::Iterator it(*in); it.More(); it.Next()) {
                out->faces.Bind(it.Key(), it.Value());
            }
        }
        return true;
    }
    std::unique_ptr<BRepAlgoAPI_BooleanOperation> op;
    switch(how) {
        case KernelCombine::UNION:        op.reset(new BRepAlgoAPI_Fuse()); break;
        case KernelCombine::DIFFERENCE:   op.reset(new BRepAlgoAPI_Cut()); break;
        case KernelCombine::INTERSECTION: op.reset(new BRepAlgoAPI_Common()); break;
        case KernelCombine::ASSEMBLE:     break;
    }
    TopTools_ListOfShape args, tools;
    args.Append(a.shape);
    tools.Append(b.shape);
    op->SetArguments(args);
    op->SetTools(tools);
    op->SetNonDestructive(true);
    // OCCT's own threads, and oriented boxes for the interference checks.
    op->SetRunParallel(true);
    op->SetUseOBB(true);
    op->Build();
    if(!op->IsDone() || op->HasErrors()) return false;
    op->SimplifyResult();
    out->shape = op->Shape();
    CarryNames(*op, { &a.faces, &b.faces }, &out->faces);
    return true;
}

// The shape's faces as triangles, with true normals, each carrying its face's name and colour.
// One side of a segment of a B-rep edge, as a face's triangulation has it.
struct EdgeSide {
    Vector a, b, facet, na, nb; // the ends, the facet beside it, the surface normals at the ends
};

// The shape's faces as triangles, with true normals, each carrying its face's name and colour;
// and, if asked, the outlines SolveSpace draws from them, straight from the triangulation's
// topology (a triangle edge inside a face where the facets turn; each B-rep edge between its
// two faces, tagged sharp where their surfaces' normals differ), instead of searching the mesh.
void Tessellate(const KernelShape &ks, RgbaColor color, SMesh *mesh,
                SOutlineList *outlines = nullptr) {
    std::lock_guard<std::mutex> lock(gMeshMutex);
    double tol = SS.ChordTolMm();
    // Faces carried over from earlier bodies keep the triangulations made for those, whose
    // edges needn't match their new neighbours'; mesh the whole shape afresh.
    BRepTools::Clean(ks.shape);
    BRepMesh_IncrementalMesh(ks.shape, tol, /*isRelative=*/false, /*angle=*/0.35,
                             /*parallel=*/true);
    // Each B-rep edge's segments, with every face's side of them. Edges are told apart by
    // shape and place: the copies of a step and repeat share their edges' shapes.
    TopTools_IndexedMapOfShape edgeIndex;
    if(outlines) TopExp::MapShapes(ks.shape, TopAbs_EDGE, edgeIndex);
    std::map<int, std::vector<std::vector<EdgeSide>>> edgeSides;
    for(TopExp_Explorer ex(ks.shape, TopAbs_FACE); ex.More(); ex.Next()) {
        const TopoDS_Face &face = TopoDS::Face(ex.Current());
        TopLoc_Location    loc;
        Handle(Poly_Triangulation) tri = BRep_Tool::Triangulation(face, loc);
        if(tri.IsNull()) continue;
        if(!tri->HasNormals()) BRepLib_ToolTriangulatedShape::ComputeNormals(face, tri);
        const gp_Trsf &trsf     = loc.Transformation();
        bool           reversed = face.Orientation() == TopAbs_REVERSED;

        STriMeta meta = { 0, color };
        if(const FaceInfo *info = ks.faces.Seek(face)) meta = { info->face, info->color };

        // For the outlines: each triangle edge (by its nodes) and the facets beside it.
        std::map<std::pair<int, int>, std::vector<Vector>> facetsBeside;
        for(int i = 1; i <= tri->NbTriangles(); i++) {
            int n[3];
            tri->Triangle(i).Get(n[0], n[1], n[2]);
            if(reversed) std::swap(n[1], n[2]);
            STriangle st = {};
            st.meta = meta;
            for(int j = 0; j < 3; j++) {
                gp_Pnt p = tri->Node(n[j]).Transformed(trsf);
                gp_Dir d = tri->Normal(n[j]).Transformed(trsf);
                if(reversed) d.Reverse();
                st.vertices[j] = Vec(p.XYZ());
                st.normals[j]  = Vec(d.XYZ());
            }
            mesh->AddTriangle(&st);
            if(outlines) {
                Vector facet = st.Normal().WithMagnitude(1);
                for(int j = 0; j < 3; j++) {
                    int a = n[j], b = n[(j + 1) % 3];
                    facetsBeside[{ std::min(a, b), std::max(a, b) }].push_back(facet);
                }
            }
        }
        if(!outlines) continue;
        auto node = [&](int k) { return Vec(tri->Node(k).Transformed(trsf).XYZ()); };
        auto normal = [&](int k) {
            gp_Dir d = tri->Normal(k).Transformed(trsf);
            if(reversed) d.Reverse();
            return Vec(d.XYZ());
        };
        // Inside the face: where its facets turn (for silhouettes).
        for(const auto &fb : facetsBeside) {
            if(fb.second.size() != 2 || fb.second[0].Equals(fb.second[1])) continue;
            outlines->AddEdge(node(fb.first.first), node(fb.first.second), fb.second[0],
                              fb.second[1], 0);
        }
        // Along its B-rep edges: each segment kept until the face on the other side comes.
        for(TopExp_Explorer ee(face, TopAbs_EDGE); ee.More(); ee.Next()) {
            const TopoDS_Edge &edge = TopoDS::Edge(ee.Current());
            if(BRep_Tool::Degenerated(edge) || BRep_Tool::IsClosed(edge, face)) continue;
            Handle(Poly_PolygonOnTriangulation) poly =
                BRep_Tool::PolygonOnTriangulation(edge, tri, loc);
            if(poly.IsNull()) continue;
            const TColStd_Array1OfInteger &nodes = poly->Nodes();
            std::vector<std::vector<EdgeSide>> &segments = edgeSides[edgeIndex.FindIndex(edge)];
            segments.resize(std::max((int)segments.size(), nodes.Length() - 1));
            for(int k = nodes.Lower(); k < nodes.Upper(); k++) {
                int a = nodes(k), b = nodes(k + 1);
                auto it = facetsBeside.find({ std::min(a, b), std::max(a, b) });
                if(it == facetsBeside.end() || it->second.empty()) continue;
                segments[k - nodes.Lower()].push_back(
                    { node(a), node(b), it->second[0], normal(a), normal(b) });
            }
        }
    }
    if(!outlines) return;
    // Each segment once: sharp if any two of its faces' surfaces turn there (where solids
    // touch, more than two faces meet), else drawn only where its facets turn.
    for(const auto &es : edgeSides) {
        for(const std::vector<EdgeSide> &sides : es.second) {
            if(sides.size() < 2) continue;
            const EdgeSide *l = &sides[0], *r = &sides[1];
            bool            sharp = false;
            for(size_t i = 0; i < sides.size() && !sharp; i++) {
                for(size_t j = i + 1; j < sides.size() && !sharp; j++) {
                    const EdgeSide &p = sides[i], &q = sides[j];
                    sharp = !((p.na.Equals(q.na) && p.nb.Equals(q.nb)) ||
                              (p.na.Equals(q.nb) && p.nb.Equals(q.na)));
                    if(sharp) l = &p, r = &q;
                }
            }
            if(!sharp && l->facet.Equals(r->facet)) continue;
            outlines->AddEdge(l->a, l->b, l->facet, r->facet, sharp ? 1 : 0);
        }
    }
}

std::shared_ptr<KernelShape> Extrude(const KernelJob &job, FaceNamer *namer) {
    auto                      ks = std::make_shared<KernelShape>();
    std::vector<TopoDS_Shape> solids;
    Vector                    along = job.ttop.Minus(job.tbot);
    for(SBezierLoopSet &sbls : job.loops->l) {
        NCollection_DataMap<TopoDS_Shape, uint32_t, TopTools_ShapeMapHasher> ents;
        TopoDS_Face face = MakeFace(sbls, &ents);
        gp_Trsf     toBottom;
        toBottom.SetTranslation(Vec(job.tbot));
        BRepBuilderAPI_Transform moved(face, toBottom, /*copy=*/true);
        NCollection_DataMap<TopoDS_Shape, uint32_t, TopTools_ShapeMapHasher> movedEnts;
        for(decltype(ents)::Iterator it(ents); it.More(); it.Next()) {
            movedEnts.Bind(moved.ModifiedShape(it.Key()), it.Value());
        }
        BRepPrimAPI_MakePrism prism(moved.Shape(), Vec(along));
        TopoDS_Shape solid = prism.Shape();

        // Named in SolveSpace's order: the bottom cap, the top, then the sides made from line
        // segments, in loop order.
        hEntity bottom = namer->Name(Entity::NO_ENTITY, Group::REMAP_BOTTOM),
                top    = namer->Name(Entity::NO_ENTITY, Group::REMAP_TOP);
        std::unordered_map<uint32_t, uint32_t> sideNames;
        for(const SBezierLoop &loop : sbls.l) {
            for(const SBezier &sb : loop.l) {
                if(sb.deg != 1 || sb.entity == 0 || sideNames.count(sb.entity)) continue;
                for(const KernelJob::SideLine &line : job.sideLines) {
                    if(line.entity.v != sb.entity) continue;
                    sideNames[sb.entity] = namer->Name(line.entity, Group::REMAP_LINE_TO_FACE).v;
                }
            }
        }
        NameSweep(prism, prism.FirstShape(), prism.LastShape(), movedEnts,
                  [&](int which, uint32_t entity) -> uint32_t {
                      if(which == 0) return bottom.v;
                      if(which == 1) return top.v;
                      auto it = sideNames.find(entity);
                      return it == sideNames.end() ? 0 : it->second;
                  },
                  job.color, &ks->faces);
        solids.push_back(solid);
    }
    ks->shape = MakeCompound(solids);
    Orient(&ks->shape);
    return ks;
}

bool IsGoodSolid(const TopoDS_Shape &shape);

// A fit joint's peg or pocket (M5b): the profile extruded, then offset by the clearance.
std::shared_ptr<KernelShape> FitShape(const KernelJob &job, KernelProblem *problem) {
    auto                      ks = std::make_shared<KernelShape>();
    std::vector<TopoDS_Shape> solids;
    for(SBezierLoopSet &sbls : job.loops->l) {
        NCollection_DataMap<TopoDS_Shape, uint32_t, TopTools_ShapeMapHasher> ents;
        TopoDS_Face face = MakeFace(sbls, &ents);
        gp_Trsf     toBottom;
        toBottom.SetTranslation(Vec(job.tbot));
        BRepBuilderAPI_Transform moved(face, toBottom, /*copy=*/true);
        TopoDS_Shape solid =
            BRepPrimAPI_MakePrism(moved.Shape(), Vec(job.ttop.Minus(job.tbot))).Shape();
        if(fabs(job.size) > LENGTH_EPS) {
            BRepOffsetAPI_MakeOffsetShape offset;
            try {
                offset.PerformByJoin(solid, job.size, 1e-5, BRepOffset_Skin,
                                     /*intersection=*/false, /*selfInter=*/false, GeomAbs_Arc);
            } catch(const Standard_Failure &) {
            }
            if(!offset.IsDone() || !IsGoodSolid(offset.Shape())) {
                *problem = N_("the fit's clearance couldn't be added to the profile");
                continue;
            }
            solid = offset.Shape();
        }
        solids.push_back(solid);
    }
    if(solids.empty()) return nullptr;
    ks->shape = MakeCompound(solids);
    Orient(&ks->shape);
    return ks;
}

std::shared_ptr<KernelShape> Revolve(const KernelJob &job, FaceNamer *namer, bool full) {
    auto                      ks = std::make_shared<KernelShape>();
    std::vector<TopoDS_Shape> solids;
    double angles = full ? 0 : job.angles, anglef = full ? 2 * PI : job.anglef;
    double dists = job.dists, distf = job.distf;
    for(SBezierLoopSet &sbls : job.loops->l) {
        NCollection_DataMap<TopoDS_Shape, uint32_t, TopTools_ShapeMapHasher> ents;
        TopoDS_Face face = MakeFace(sbls, &ents);
        // The profile moved to where the sweep starts.
        gp_Trsf start;
        start.SetRotation(gp_Ax1(Pnt(job.axisPoint), Dir(job.axis)), angles);
        gp_Trsf lift;
        lift.SetTranslation(Vec(job.axis.ScaledBy(dists)));
        BRepBuilderAPI_Transform moved(face, lift * start, /*copy=*/true);
        NCollection_DataMap<TopoDS_Shape, uint32_t, TopTools_ShapeMapHasher> movedEnts;
        for(decltype(ents)::Iterator it(ents); it.More(); it.Next()) {
            movedEnts.Bind(moved.ModifiedShape(it.Key()), it.Value());
        }

        hEntity startFace, endFace;
        if(!full) {
            startFace = namer->Name(Entity::NO_ENTITY, Group::REMAP_LATHE_START);
            endFace   = namer->Name(Entity::NO_ENTITY, Group::REMAP_LATHE_END);
        }
        auto name = [&](int which, uint32_t entity) -> uint32_t {
            if(which == 0) return startFace.v;
            if(which == 1) return endFace.v;
            return namer->ExistingFace(hEntity{ entity }, Group::REMAP_LINE_TO_FACE).v;
        };

        if(fabs(distf - dists) < LENGTH_EPS) {
            gp_Ax1 axis(Pnt(job.axisPoint.Plus(job.axis.ScaledBy(dists))), Dir(job.axis));
            BRepPrimAPI_MakeRevol revol(moved.Shape(), axis, anglef - angles);
            if(!revol.IsDone()) continue;
            NameSweep(revol, revol.FirstShape(), revol.LastShape(), movedEnts, name, job.color,
                      &ks->faces);
            solids.push_back(revol.Shape());
        } else {
            // A helix: each wire of the profile swept along a helical spine that keeps the
            // axis as its binormal, so the profile turns about the axis as it rises; the holes'
            // sweeps are then cut from the outer one's.
            gp_Pnt ref  = Pnt(sbls.point).Transformed(lift * start);
            Vector refv = Vec(ref.XYZ());
            Vector foot = job.axisPoint.Plus(
                job.axis.ScaledBy(refv.Minus(job.axisPoint).Dot(job.axis)));
            Vector out    = refv.Minus(foot);
            double radius = out.Magnitude();
            if(radius < LENGTH_EPS) {
                out    = job.axis.Normal(0);
                radius = 1;
            }
            gp_Ax3 frame(Pnt(foot), Dir(job.axis), Dir(out));
            Handle(Geom_CylindricalSurface) cylinder = new Geom_CylindricalSurface(frame, radius);
            Handle(Geom2d_TrimmedCurve) line =
                GCE2d_MakeSegment(gp_Pnt2d(0, 0), gp_Pnt2d(anglef - angles, distf - dists));
            TopoDS_Edge spineEdge = BRepBuilderAPI_MakeEdge(line, cylinder);
            BRepLib::BuildCurves3d(spineEdge);
            TopoDS_Wire spine = BRepBuilderAPI_MakeWire(spineEdge);

            TopoDS_Face  profile = TopoDS::Face(moved.Shape());
            TopoDS_Wire  outerWire = BRepTools::OuterWire(profile);
            TopoDS_Shape swept;
            KernelShape  holes;
            bool         ok = true;
            for(TopExp_Explorer ex(profile, TopAbs_WIRE); ex.More() && ok; ex.Next()) {
                BRepOffsetAPI_MakePipeShell pipe(spine);
                pipe.SetMode(Dir(job.axis));
                pipe.Add(ex.Current(), /*withContact=*/false, /*withCorrection=*/false);
                pipe.Build();
                if(!pipe.IsDone() || !pipe.MakeSolid()) {
                    ok = false;
                    break;
                }
                TopoDS_Shape solid = pipe.Shape();
                Orient(&solid);
                if(ex.Current().IsSame(outerWire)) {
                    NameSweep(pipe, CapOf(solid, pipe.FirstShape()),
                              CapOf(solid, pipe.LastShape()), movedEnts, name, job.color,
                              &ks->faces);
                    swept = solid;
                } else {
                    NameSweep(pipe, TopoDS_Shape(), TopoDS_Shape(), movedEnts, name, job.color,
                              &holes.faces);
                    holes.shape = holes.shape.IsNull() ? solid : MakeCompound({ holes.shape, solid });
                }
            }
            if(!ok || swept.IsNull()) continue;
            if(!holes.shape.IsNull()) {
                KernelShape outer, cut;
                outer.shape = swept;
                outer.faces = ks->faces;
                if(!Boolean(KernelCombine::DIFFERENCE, outer, holes, &cut)) continue;
                swept     = cut.shape;
                ks->faces = cut.faces;
            }
            solids.push_back(swept);
        }
    }
    ks->shape = MakeCompound(solids);
    Orient(&ks->shape);
    return ks;
}

std::shared_ptr<KernelShape> StepAndRepeat(const KernelJob &job, FaceNamer *namer) {
    int n = job.steps, a0 = 0;
    if(job.oneSided && job.skipFirst) {
        a0++; n++;
    }
    std::vector<std::shared_ptr<KernelShape>> copies;
    for(int a = a0; a < n; a++) {
        int     ap = a * 2 - (job.oneSided ? 0 : (n - 1));
        gp_Trsf trsf;
        if(job.op == KernelJob::Op::STEP_TRANSLATE) {
            trsf.SetTranslation(Vec(job.stepTrans.ScaledBy(ap)));
        } else {
            double     theta = ap * job.stepTheta;
            double     c = cos(theta), s = sin(theta);
            Vector     axis  = job.stepAxis;
            Quaternion q     = Quaternion::From(c, s * axis.x, s * axis.y, s * axis.z);
            trsf.SetRotation(gp_Quaternion(q.vx, q.vy, q.vz, q.w));
            trsf.SetTranslationPart(Vec(job.stepTrans.Minus(q.Rotate(job.stepTrans))));
        }
        int remap = (a == (n - 1)) ? Group::REMAP_LAST : a;
        copies.push_back(Transformed(*job.sourceShape, trsf, namer, remap));
    }
    // All the copies fused in one boolean: much faster than in pairs, as SolveSpace's kernel
    // combines them; in pairs only if that fails.
    if(copies.size() > 1 && job.sourceCombine != KernelCombine::ASSEMBLE) {
        BRepAlgoAPI_Fuse     fuse;
        TopTools_ListOfShape args, tools;
        args.Append(copies[0]->shape);
        for(size_t i = 1; i < copies.size(); i++) tools.Append(copies[i]->shape);
        fuse.SetArguments(args);
        fuse.SetTools(tools);
        fuse.SetNonDestructive(true);
        fuse.SetRunParallel(true);
        fuse.SetUseOBB(true);
        fuse.Build();
        if(fuse.IsDone() && !fuse.HasErrors()) {
            fuse.SimplifyResult();
            auto out   = std::make_shared<KernelShape>();
            out->shape = fuse.Shape();
            std::vector<const FaceMap *> names;
            for(const auto &c : copies) names.push_back(&c->faces);
            CarryNames(fuse, names, &out->faces);
            return out;
        }
    }
    while(copies.size() > 1) {
        std::vector<std::shared_ptr<KernelShape>> next;
        for(size_t i = 0; i < copies.size(); i += 2) {
            if(i + 1 == copies.size()) {
                next.push_back(copies[i]);
                continue;
            }
            auto out = std::make_shared<KernelShape>();
            KernelCombine how = job.sourceCombine == KernelCombine::ASSEMBLE
                                    ? KernelCombine::ASSEMBLE : KernelCombine::UNION;
            if(!Boolean(how, *copies[i], *copies[i + 1], out.get())) {
                Boolean(KernelCombine::ASSEMBLE, *copies[i], *copies[i + 1], out.get());
            }
            next.push_back(out);
        }
        copies = std::move(next);
    }
    return copies.empty() ? std::make_shared<KernelShape>() : copies[0];
}

// Whether a B-rep edge lies on an edge entity's curves: its ends and its middle do.
bool OnCurves(const TopoDS_Edge &edge, const std::vector<SBezier> &curves) {
    BRepAdaptor_Curve c(edge);
    double f = c.FirstParameter(), l = c.LastParameter();
    for(double t : { f, (f + l) / 2, l }) {
        Vector p       = Vec(c.Value(t).XYZ());
        bool   onCurve = false; // ("near" is a macro on Windows)
        for(const SBezier &sb : curves) {
            double u;
            sb.ClosestPointTo(p, &u, /*mustConverge=*/false);
            if(sb.PointAt(u).Equals(p, 1e-4)) {
                onCurve = true;
                break;
            }
        }
        if(!onCurve) return false;
    }
    return true;
}

// The previous body with the job's edges rounded or bevelled, or null if nothing was;
// *problem says what went wrong.
std::shared_ptr<KernelShape> RoundEdges(const KernelJob &job, KernelProblem *problem) {
    const KernelShape        &prev   = *job.prevShape;
    bool                      fillet = job.op == KernelJob::Op::FILLET;
    BRepFilletAPI_MakeFillet  makeFillet(prev.shape);
    BRepFilletAPI_MakeChamfer makeChamfer(prev.shape);
    BRepBuilderAPI_MakeShape &op = fillet ? (BRepBuilderAPI_MakeShape &)makeFillet
                                          : (BRepBuilderAPI_MakeShape &)makeChamfer;

    TopTools_IndexedMapOfShape edges;
    TopExp::MapShapes(prev.shape, TopAbs_EDGE, edges);
    TopTools_MapOfShape added;
    int                 missing = 0;
    for(const std::vector<SBezier> &curves : job.edgeCurves) {
        bool found = false;
        for(int i = 1; i <= edges.Extent(); i++) {
            const TopoDS_Edge &edge = TopoDS::Edge(edges(i));
            if(BRep_Tool::Degenerated(edge) || !OnCurves(edge, curves)) continue;
            found = true;
            if(!added.Add(edge)) continue;
            if(fillet) {
                makeFillet.Add(job.size, edge);
            } else {
                makeChamfer.Add(job.size, edge);
            }
        }
        if(!found) missing++;
    }
    if(missing > 0) {
        *problem = N_("%d of the %d edges were not found on the solid");
        problem->count[0] = missing;
        problem->count[1] = (int)job.edgeCurves.size();
    }
    // The picked faces' edges: all, or those with only one picked face beside them.
    if(!job.faceNames.empty()) {
        auto picked = [&](const TopoDS_Shape &face) {
            const FaceInfo *info = prev.faces.Seek(face);
            return info && std::find(job.faceNames.begin(), job.faceNames.end(), info->face) !=
                               job.faceNames.end();
        };
        TopTools_IndexedDataMapOfShapeListOfShape facesOf;
        TopExp::MapShapesAndAncestors(prev.shape, TopAbs_EDGE, TopAbs_FACE, facesOf);
        for(int i = 1; i <= facesOf.Extent(); i++) {
            const TopoDS_Edge &edge = TopoDS::Edge(facesOf.FindKey(i));
            if(BRep_Tool::Degenerated(edge)) continue;
            int beside = 0, faces = 0;
            TopTools_MapOfShape seen;
            for(const TopoDS_Shape &f : facesOf(i)) {
                if(!seen.Add(f)) continue; // a seam's face, twice
                faces++;
                if(picked(f)) beside++;
            }
            if(beside == 0 || faces < 2) continue;
            if(job.outlineOnly && beside != 1) continue;
            if(!added.Add(edge)) continue;
            if(fillet) {
                makeFillet.Add(job.size, edge);
            } else {
                makeChamfer.Add(job.size, edge);
            }
        }
    }
    if(added.IsEmpty()) return nullptr;

    try {
        op.Build();
    } catch(const Standard_Failure &) {
    }
    if(!op.IsDone() || op.Shape().IsNull()) {
        *problem = fillet ? "the fillet failed; the radius may be too big for the edges"
                          : "the chamfer failed; the distance may be too big for the edges";
        return nullptr;
    }
    auto out   = std::make_shared<KernelShape>();
    out->shape = op.Shape();
    CarryNames(op, { &prev.faces }, &out->faces);
    return out;
}

// A planar face's outward normal.
bool OutwardNormal(const TopoDS_Face &face, gp_Dir *out) {
    BRepAdaptor_Surface surface(face, /*restriction=*/false);
    if(surface.GetType() != GeomAbs_Plane) return false;
    *out = surface.Plane().Axis().Direction();
    if(face.Orientation() == TopAbs_REVERSED) out->Reverse();
    return true;
}

double Volume(const TopoDS_Shape &shape) {
    GProp_GProps props;
    BRepGProp::VolumeProperties(shape, props);
    return props.Mass();
}

// Whether a result is a usable solid: valid, and with some volume.
bool IsGoodSolid(const TopoDS_Shape &shape) {
    return !shape.IsNull() && BRepCheck_Analyzer(shape).IsValid() && Volume(shape) > 0;
}

// A solid offset by distance (outwards when positive) with sharp corners, and which face of the
// solid each of its faces was made from; null if OCCT fails.
TopoDS_Shape OffsetSolid(const TopoDS_Shape &solid, double distance,
                         std::vector<std::pair<TopoDS_Shape, TopoDS_Shape>> *madeFrom) {
    BRepOffsetAPI_MakeOffsetShape offset;
    try {
        offset.PerformByJoin(solid, distance, 1e-4, BRepOffset_Skin, /*intersection=*/false,
                             /*selfInter=*/false, GeomAbs_Intersection);
        if(!offset.IsDone() || !IsGoodSolid(offset.Shape())) return {};
    } catch(const Standard_Failure &) {
        return {};
    }
    for(TopExp_Explorer ex(solid, TopAbs_FACE); ex.More(); ex.Next()) {
        for(const TopoDS_Shape &made : offset.Generated(ex.Current())) {
            if(made.ShapeType() == TopAbs_FACE) madeFrom->push_back({ made, ex.Current() });
        }
    }
    return offset.Shape();
}

// The previous body hollowed to the job's thickness on the job's side of its faces, leaving
// the job's faces open (none: a sealed void). Each new wall face is named with the inner or
// outer face entity of the face it was made from (Group::REMAP_SHELL_INNER, _OUTER).
std::shared_ptr<KernelShape> Shell(const KernelJob &job, FaceNamer *namer, KernelProblem *problem) {
    const KernelShape &prev   = *job.prevShape;
    const char        *failed = "the shell failed; the thickness may be too big for the solid";
    double             t      = job.size;
    // Names: the face of prev each face came from, and the face entity it is named with.
    FaceMap origin;
    auto    nameMade = [&](const TopoDS_Shape &made, const TopoDS_Shape &from, int remap,
                        KernelShape *ks) {
        const FaceInfo *info = origin.Seek(from);
        if(info == nullptr || info->face == 0) return;
        hEntity name = namer->ExistingFace(hEntity{ info->face }, remap);
        if(name.v != 0) ks->faces.Bind(made, FaceInfo{ name.v, info->color });
        origin.Bind(made, *info);
    };

    for(FaceMap::Iterator it(prev.faces); it.More(); it.Next()) origin.Bind(it.Key(), it.Value());
    // The faces left open, as the solid has them.
    TopTools_ListOfShape open;
    std::set<uint32_t>   found;
    for(TopExp_Explorer ex(prev.shape, TopAbs_FACE); ex.More(); ex.Next()) {
        const FaceInfo *info = origin.Seek(ex.Current());
        if(info == nullptr || std::find(job.faceNames.begin(), job.faceNames.end(),
                                        info->face) == job.faceNames.end()) {
            continue;
        }
        open.Append(ex.Current());
        found.insert(info->face);
    }

    bool   outwards = job.shellSide == ShellSide::OUTSIDE;
    int    wall     = outwards ? Group::REMAP_SHELL_OUTER : Group::REMAP_SHELL_INNER;
    double offset   = outwards ? t : -t;

    auto out = std::make_shared<KernelShape>();
    if(open.IsEmpty()) {
        // Both sides: the solid grows by half first, its new faces the outer walls.
        KernelShape base;
        if(job.shellSide == ShellSide::BOTH) {
            std::vector<std::pair<TopoDS_Shape, TopoDS_Shape>> madeFrom;
            base.shape = OffsetSolid(prev.shape, t / 2, &madeFrom);
            if(base.shape.IsNull()) {
                *problem = failed;
                return nullptr;
            }
            for(const auto &[made, from] : madeFrom) {
                nameMade(made, from, Group::REMAP_SHELL_OUTER, &base);
            }
        } else {
            base.shape = prev.shape;
            base.faces = prev.faces;
        }
        // Sealed: the bigger solid less the smaller.
        std::vector<std::pair<TopoDS_Shape, TopoDS_Shape>> madeFrom;
        KernelShape other;
        other.shape = OffsetSolid(base.shape, offset, &madeFrom);
        if(other.shape.IsNull()) {
            *problem = failed;
            return nullptr;
        }
        for(const auto &[made, from] : madeFrom) nameMade(made, from, wall, &other);
        const KernelShape &outer = outwards ? other : base, &inner = outwards ? base : other;
        BRepAlgoAPI_Cut      cut;
        TopTools_ListOfShape args, tools;
        args.Append(outer.shape);
        tools.Append(inner.shape);
        cut.SetArguments(args);
        cut.SetTools(tools);
        cut.SetRunParallel(true);
        cut.SetUseOBB(true);
        cut.Build();
        if(!cut.IsDone() || cut.HasErrors() || !IsGoodSolid(cut.Shape()) ||
           Volume(cut.Shape()) >= Volume(outer.shape) * (1 - 1e-9)) {
            *problem = failed;
            return nullptr;
        }
        out->shape = cut.Shape();
        CarryNames(cut, { &outer.faces, &inner.faces }, &out->faces);
        return out;
    }

    // Open: the solid thickened from its faces, as one wall, or (both sides) a half wall out
    // and a half wall in, joined, so the open faces' rims stay in their planes.
    auto thicken = [&](double by, int remap, KernelShape *ks) {
        BRepOffsetAPI_MakeThickSolid make;
        try {
            make.MakeThickSolidByJoin(prev.shape, open, by, 1e-4, BRepOffset_Skin,
                                      /*intersection=*/false, /*selfInter=*/false,
                                      GeomAbs_Intersection);
            make.Build();
            if(!make.IsDone() || !IsGoodSolid(make.Shape())) return false;
        } catch(const Standard_Failure &) {
            return false;
        }
        ks->shape = make.Shape();
        CarryNames(make, { &prev.faces }, &ks->faces);
        for(TopExp_Explorer ex(prev.shape, TopAbs_FACE); ex.More(); ex.Next()) {
            const FaceInfo *info = origin.Seek(ex.Current());
            if(info == nullptr || found.count(info->face)) continue;
            for(const TopoDS_Shape &made : make.Generated(ex.Current())) {
                if(made.ShapeType() == TopAbs_FACE) nameMade(made, ex.Current(), remap, ks);
            }
        }
        return true;
    };
    bool done;
    if(job.shellSide == ShellSide::BOTH) {
        KernelShape out1, in1;
        done = thicken(t / 2, Group::REMAP_SHELL_OUTER, &out1) &&
               thicken(-t / 2, Group::REMAP_SHELL_INNER, &in1) &&
               Volume(in1.shape) < Volume(prev.shape) * (1 - 1e-9) &&
               Boolean(KernelCombine::UNION, out1, in1, out.get());
    } else {
        done = thicken(offset, wall, out.get()) &&
               (outwards || Volume(out->shape) < Volume(prev.shape) * (1 - 1e-9));
    }
    if(!done || !IsGoodSolid(out->shape)) {
        *problem = failed;
        return nullptr;
    }
    if(found.size() < job.faceNames.size()) {
        *problem = N_("%d of the %d faces were not found on the solid");
        problem->count[0] = (int)(job.faceNames.size() - found.size());
        problem->count[1] = (int)job.faceNames.size();
    }
    return out;
}

// The previous body with the job's faces tapered; each drafted face is named as the job says.
std::shared_ptr<KernelShape> Draft(const KernelJob &job, FaceNamer *, KernelProblem *problem) {
    const KernelShape       &prev = *job.prevShape;
    BRepOffsetAPI_DraftAngle draft(prev.shape);
    std::vector<std::pair<TopoDS_Shape, uint32_t>> drafted;
    int                                            missing = 0;
    try {
        for(const KernelJob::DraftFace &df : job.draftFaces) {
            bool found = false;
            for(TopExp_Explorer ex(prev.shape, TopAbs_FACE); ex.More(); ex.Next()) {
                const FaceInfo *info = prev.faces.Seek(ex.Current());
                if(info == nullptr || info->face != df.face) continue;
                found = true;
                draft.Add(TopoDS::Face(ex.Current()), Dir(df.pull), job.size,
                          gp_Pln(Pnt(df.point), Dir(df.pull)));
                if(!draft.AddDone()) {
                    *problem = N_("the draft failed on a face it cannot taper");
                    return nullptr;
                }
                drafted.push_back({ ex.Current(), df.name });
            }
            if(!found) missing++;
        }
        if(drafted.empty()) {
            *problem = N_("none of the faces were found on the solid");
            return nullptr;
        }
        draft.Build();
    } catch(const Standard_Failure &) {
    }
    if(!draft.IsDone() || !IsGoodSolid(draft.Shape())) {
        *problem = N_("the draft failed; the angle may be too big for the solid");
        return nullptr;
    }
    if(missing > 0) {
        *problem = N_("%d of the %d faces were not found on the solid");
        problem->count[0] = missing;
        problem->count[1] = (int)job.draftFaces.size();
    }
    auto out   = std::make_shared<KernelShape>();
    out->shape = draft.Shape();
    // A modification's history is its ModifiedShape; a drafted face takes its new name.
    for(FaceMap::Iterator it(prev.faces); it.More(); it.Next()) {
        TopoDS_Shape made = draft.ModifiedShape(it.Key());
        if(!made.IsNull()) out->faces.Bind(made, it.Value());
    }
    for(const auto &[face, name] : drafted) {
        TopoDS_Shape made = draft.ModifiedShape(face);
        if(made.IsNull()) continue;
        RgbaColor color = prev.faces.Seek(face) ? prev.faces.Find(face).color : RgbaColor{};
        out->faces.UnBind(made);
        out->faces.Bind(made, FaceInfo{ name, color });
    }
    return out;
}

// An ISO metric internal thread's groove (where the screw's thread goes): a 60 degree profile
// from just inside the bore out to the major diameter, swept along a helix of the pitch about
// the axis from start along dir for length; null if OCCT fails.
TopoDS_Shape ThreadGroove(gp_Pnt start, gp_Dir dir, double minor, double major, double pitch,
                          double length) {
    gp_Ax3 frame(start, dir);
    gp_Dir out   = frame.XDirection();
    double rIn   = minor / 2 - 0.1 * pitch, rMaj = major / 2;
    auto   width = [&](double r) { return pitch / 8 + 2 * (rMaj - r) * tan(PI / 6); };
    auto   at    = [&](double r, double z) {
        return start.Translated(gp_Vec(out) * r + gp_Vec(dir) * z);
    };
    BRepBuilderAPI_MakePolygon polygon(at(rIn, -width(rIn) / 2), at(rMaj, -pitch / 16),
                                       at(rMaj, pitch / 16), at(rIn, width(rIn) / 2),
                                       /*close=*/true);
    if(!polygon.IsDone()) return {};

    double turns = length / pitch;
    Handle(Geom_CylindricalSurface) cylinder = new Geom_CylindricalSurface(frame, rIn);
    Handle(Geom2d_TrimmedCurve) line =
        GCE2d_MakeSegment(gp_Pnt2d(0, 0), gp_Pnt2d(2 * PI * turns, length));
    TopoDS_Edge spineEdge = BRepBuilderAPI_MakeEdge(line, cylinder);
    BRepLib::BuildCurves3d(spineEdge);
    TopoDS_Wire                 spine = BRepBuilderAPI_MakeWire(spineEdge);
    BRepOffsetAPI_MakePipeShell pipe(spine);
    pipe.SetMode(dir);
    pipe.Add(polygon.Wire(), /*withContact=*/false, /*withCorrection=*/false);
    pipe.Build();
    if(!pipe.IsDone() || !pipe.MakeSolid()) return {};
    TopoDS_Shape groove = pipe.Shape();
    Orient(&groove);
    return groove;
}

// The previous body with the job's holes cut; each counterbore's floor named with its seat.
// Where dowels go across a cut (M5b): on each region of solid's section by the plane, two on
// the region's long axis, else one at its centre, each clear of the region's edges and of the
// other by clear.
std::vector<Vector> DowelPoints(const TopoDS_Shape &solid, const gp_Pln &pln, double reach,
                                double clear) {
    std::vector<Vector> points;
    TopoDS_Face         big = BRepBuilderAPI_MakeFace(pln, -reach, reach, -reach, reach);
    BRepAlgoAPI_Common  section(solid, big);
    if(!section.IsDone()) return points;
    gp_Dir n = pln.Axis().Direction();
    for(TopExp_Explorer ex(section.Shape(), TopAbs_FACE); ex.More(); ex.Next()) {
        const TopoDS_Face &face = TopoDS::Face(ex.Current());
        GProp_GProps       props;
        BRepGProp::SurfaceProperties(face, props);
        gp_Pnt               c  = props.CentreOfMass();
        GProp_PrincipalProps pp = props.PrincipalProperties();
        double               m[3];
        pp.Moments(m[0], m[1], m[2]);
        gp_Vec axes[3] = { pp.FirstAxisOfInertia(), pp.SecondAxisOfInertia(),
                           pp.ThirdAxisOfInertia() };
        // The long axis: in the plane, with the smaller moment.
        int long_ = -1;
        for(int i = 0; i < 3; i++) {
            if(fabs(gp_Vec(n).Dot(axes[i].Normalized())) > 0.5) continue;
            if(long_ < 0 || m[i] < m[long_]) long_ = i;
        }
        if(long_ < 0) continue;
        gp_Vec a = axes[long_].Normalized();
        // How far a point is inside the region from its edges (negative: outside).
        TopoDS_Compound edges;
        BRep_Builder    builder;
        builder.MakeCompound(edges);
        for(TopExp_Explorer ee(face, TopAbs_EDGE); ee.More(); ee.Next()) {
            builder.Add(edges, ee.Current());
        }
        auto inside = [&](const gp_Pnt &q) {
            BRepClass_FaceClassifier in(face, q, 1e-7);
            if(in.State() != TopAbs_IN) return -1.0;
            BRepExtrema_DistShapeShape dist(BRepBuilderAPI_MakeVertex(q).Vertex(), edges);
            return dist.IsDone() ? dist.Value() : -1.0;
        };
        // The region's half-length along the axis, each way from the centre.
        double step = std::max(clear / 16, 1e-3), l[2] = { 0, 0 };
        for(int s = 0; s < 2; s++) {
            gp_Vec dir = s == 0 ? a : a.Reversed();
            while(l[s] < reach && inside(c.Translated(dir * (l[s] + step))) >= 0) l[s] += step;
        }
        double along = std::min(l[0], l[1]) - clear;
        gp_Pnt p0 = c.Translated(a * along), p1 = c.Translated(a * -along);
        if(along >= clear && inside(p0) >= clear - 1e-6 && inside(p1) >= clear - 1e-6) {
            points.push_back(Vec(p0.XYZ()));
            points.push_back(Vec(p1.XYZ()));
        } else if(inside(c) >= clear - 1e-6) {
            points.push_back(Vec(c.XYZ()));
        }
    }
    return points;
}

// A split's half (M5b): the previous solid on the side splitNormal points to, with pockets for
// dowels across the cut.
std::shared_ptr<KernelShape> SplitHalf(const KernelJob &job, KernelProblem *problem) {
    const KernelShape &prev = *job.prevShape;
    if(job.splitNormal.Magnitude() < LENGTH_EPS) {
        *problem = N_("the split's plane is gone");
        return nullptr;
    }
    Bnd_Box box;
    BRepBndLib::Add(prev.shape, box);
    double xmin, ymin, zmin, xmax, ymax, zmax;
    box.Get(xmin, ymin, zmin, xmax, ymax, zmax);
    Vector lo = Vector::From(xmin, ymin, zmin), hi = Vector::From(xmax, ymax, zmax);
    double reach = lo.Plus(hi).ScaledBy(0.5).Minus(job.splitPoint).Magnitude() +
                   hi.Minus(lo).Magnitude() + 1;
    gp_Pln      pln(Pnt(job.splitPoint), Dir(job.splitNormal));
    TopoDS_Face big = BRepBuilderAPI_MakeFace(pln, -reach, reach, -reach, reach);
    KernelShape half;
    half.shape = BRepPrimAPI_MakePrism(big, Vec(job.splitNormal.ScaledBy(2 * reach))).Shape();
    auto out   = std::make_shared<KernelShape>();
    if(!Boolean(KernelCombine::INTERSECTION, prev, half, out.get())) {
        *problem = N_("the solid couldn't be split");
        return nullptr;
    }
    if(job.dowelDiameter <= 0) return out;
    double c = job.dowelClearance, d = job.dowelDiameter;
    std::vector<Vector> at = DowelPoints(prev.shape, pln, reach, d + std::max(c, 0.0));
    if(at.empty()) {
        *problem = N_("the cut is too small for a dowel this size");
        return out;
    }
    std::vector<TopoDS_Shape> pockets;
    for(const Vector &q : at) {
        gp_Ax2 axis(Pnt(q.Minus(job.splitNormal.ScaledBy(0.1))), Dir(job.splitNormal));
        pockets.push_back(BRepPrimAPI_MakeCylinder(axis, d / 2 + c, job.dowelDepth + 0.1).Shape());
    }
    KernelShape tools;
    tools.shape = MakeCompound(pockets);
    auto cut    = std::make_shared<KernelShape>();
    if(!Boolean(KernelCombine::DIFFERENCE, *out, tools, cut.get())) {
        *problem = N_("the dowel pockets couldn't be cut");
        return out;
    }
    return cut;
}

std::shared_ptr<KernelShape> Holes(const KernelJob &job, KernelProblem *problem) {
    const KernelShape &prev = *job.prevShape;
    const HoleSpec    &spec = job.hole;
    Bnd_Box            box;
    BRepBndLib::Add(prev.shape, box);
    double xmin, ymin, zmin, xmax, ymax, zmax;
    box.Get(xmin, ymin, zmin, xmax, ymax, zmax);

    double               bore = spec.Bore(), seat = spec.SeatDepth();
    TopTools_ListOfShape tools;
    try {
        for(const KernelJob::HoleAt &h : job.holes) {
            // How far the solid reaches along the hole: a through hole's length.
            double reach = 0;
            for(double x : { xmin, xmax }) for(double y : { ymin, ymax }) for(double z : { zmin, zmax }) {
                reach = std::max(reach, Vector::From(x, y, z).Minus(h.point).Dot(h.dir));
            }
            double length = spec.depth > 0 ? spec.depth : reach + bore;
            // Each tool starts on the face (OCCT handles the coplanar caps).
            gp_Ax2 axis(Pnt(h.point), Dir(h.dir));
            tools.Append(BRepPrimAPI_MakeCylinder(axis, bore / 2, length).Shape());
            if(spec.kind == HoleKind::COUNTERBORE) {
                tools.Append(
                    BRepPrimAPI_MakeCylinder(axis, spec.Head() / 2, spec.headDepth)
                        .Shape());
            } else if(spec.kind == HoleKind::COUNTERSINK && seat > 0) {
                tools.Append(
                    BRepPrimAPI_MakeCone(axis, spec.Head() / 2, bore / 2, seat).Shape());
            }
            if(spec.thread > 0) {
                // The thread starts below the head (where the bore starts) and stops half a
                // pitch short of a blind hole's floor.
                double threadLength = (spec.depth > 0 ? spec.depth - spec.pitch / 2 : reach) - seat;
                TopoDS_Shape groove = threadLength <= 0 ? TopoDS_Shape() : ThreadGroove(
                    Pnt(h.point.Plus(h.dir.ScaledBy(seat - (seat > 0 ? 0 : spec.pitch)))),
                    Dir(h.dir), bore, spec.Major(), spec.pitch,
                    threadLength + (seat > 0 ? 0 : spec.pitch));
                if(groove.IsNull()) {
                    *problem = N_("the thread could not be made");
                    return nullptr;
                }
                tools.Append(groove);
            }
        }
    } catch(const Standard_Failure &) {
        *problem = N_("the holes could not be made");
        return nullptr;
    }
    if(tools.IsEmpty()) return nullptr;

    BRepAlgoAPI_Cut      cut;
    TopTools_ListOfShape args;
    args.Append(prev.shape);
    cut.SetArguments(args);
    cut.SetTools(tools);
    cut.SetNonDestructive(true);
    // Threads make big booleans: OCCT's own threads, and oriented boxes, more than halve them.
    cut.SetRunParallel(true);
    cut.SetUseOBB(true);
    cut.Build();
    if(!cut.IsDone() || cut.HasErrors() || !IsGoodSolid(cut.Shape())) {
        *problem = N_("the holes could not be cut");
        return nullptr;
    }
    cut.SimplifyResult();
    if(Volume(cut.Shape()) >= Volume(prev.shape) * (1 - 1e-12)) {
        *problem = N_("the holes miss the solid");
        return nullptr;
    }
    auto out   = std::make_shared<KernelShape>();
    out->shape = cut.Shape();
    CarryNames(cut, { &prev.faces }, &out->faces);
    // A counterbore's floor: an unnamed plane facing out of the hole at the seat's depth.
    if(spec.kind == HoleKind::COUNTERBORE) {
        for(const KernelJob::HoleAt &h : job.holes) {
            gp_Pnt floor = Pnt(h.point.Plus(h.dir.ScaledBy(seat)));
            for(TopExp_Explorer ex(out->shape, TopAbs_FACE); ex.More(); ex.Next()) {
                const FaceInfo *info = out->faces.Seek(ex.Current());
                gp_Dir          normal;
                if((info && info->face != 0) || !OutwardNormal(TopoDS::Face(ex.Current()), &normal)) {
                    continue;
                }
                BRepAdaptor_Surface surface(TopoDS::Face(ex.Current()));
                if(!normal.IsParallel(Dir(h.dir), 1e-9) ||
                   surface.Plane().Distance(floor) > 1e-6) {
                    continue;
                }
                out->faces.Bind(ex.Current(), FaceInfo{ h.seat, job.color });
            }
        }
    }
    return out;
}

// A section's loop as a wire.
TopoDS_Wire SectionWire(const std::vector<SBezier> &curves) {
    SBezierLoop loop = {};
    for(const SBezier &sb : curves) {
        SBezier copy = sb;
        loop.l.Add(&copy);
    }
    NCollection_DataMap<TopoDS_Shape, uint32_t, TopTools_ShapeMapHasher> ents;
    TopoDS_Wire wire = MakeWire(loop, &ents);
    loop.l.Clear();
    return wire;
}

// Names a made solid's end caps: the faces at its first and last sections.
void NameCaps(const KernelJob &job, const TopoDS_Shape &solid, const TopoDS_Shape &first,
              const TopoDS_Shape &last, KernelShape *ks) {
    for(const auto &[section, name] : { std::make_pair(first, job.startName),
                                        std::make_pair(last, job.endName) }) {
        if(section.IsNull() || name == 0) continue;
        TopoDS_Shape cap = CapOf(solid, section);
        if(!cap.IsNull()) ks->faces.Bind(cap, FaceInfo{ name, job.color });
    }
}

// The holes' solids cut from a made solid, whose face names are kept.
bool CutHoles(const std::vector<TopoDS_Shape> &holes, KernelShape *ks) {
    if(holes.empty()) return true;
    KernelShape cutter, cut;
    cutter.shape = MakeCompound(holes);
    if(!Boolean(KernelCombine::DIFFERENCE, *ks, cutter, &cut)) return false;
    *ks = cut;
    return true;
}

// Each section's holes in loft order: chains[k][i] is the k-th chain's hole in section i,
// each matched to the hole of the next section nearest it about its section's centre. Empty
// if any section has a different number of holes.
std::vector<std::vector<const std::vector<SBezier> *>> HoleChains(const KernelJob &job) {
    auto centre = [](const std::vector<SBezier> &loop) {
        Vector sum = {};
        for(const SBezier &sb : loop) sum = sum.Plus(sb.Start());
        return sum.ScaledBy(1.0 / std::max<size_t>(1, loop.size()));
    };
    size_t n = job.sectionHoles.empty() ? 0 : job.sectionHoles[0].size();
    for(const auto &holes : job.sectionHoles) {
        if(holes.size() != n) return {};
    }
    std::vector<std::vector<const std::vector<SBezier> *>> chains(n);
    for(size_t k = 0; k < n; k++) chains[k].push_back(&job.sectionHoles[0][k]);
    for(size_t i = 1; i < job.sections.size(); i++) {
        Vector             was = centre(job.sections[i - 1]), now = centre(job.sections[i]);
        std::vector<bool>  taken(n, false);
        for(auto &chain : chains) {
            Vector at   = centre(*chain.back()).Minus(was);
            size_t best = n;
            for(size_t m = 0; m < n; m++) {
                if(taken[m]) continue;
                double d = centre(job.sectionHoles[i][m]).Minus(now).Minus(at).Magnitude();
                if(best == n || d < centre(job.sectionHoles[i][best]).Minus(now).Minus(at).Magnitude()) {
                    best = m;
                }
            }
            taken[best] = true;
            chain.push_back(&job.sectionHoles[i][best]);
        }
    }
    return chains;
}

std::shared_ptr<KernelShape> Loft(const KernelJob &job, KernelProblem *problem) {
    if(job.sections.size() < 2) return nullptr;
    auto ks = std::make_shared<KernelShape>();
    try {
        BRepOffsetAPI_ThruSections loft(/*isSolid=*/true, /*ruled=*/false);
        for(const std::vector<SBezier> &section : job.sections) {
            loft.AddWire(SectionWire(section));
        }
        loft.Build();
        if(!loft.IsDone() || !IsGoodSolid(loft.Shape())) {
            *problem = N_("the loft failed");
            return nullptr;
        }
        ks->shape = loft.Shape();
        Orient(&ks->shape);
        NameCaps(job, ks->shape, loft.FirstShape(), loft.LastShape(), ks.get());
        // Each chain of matching holes lofted the same way and cut.
        bool anyHoles = false;
        for(const auto &holes : job.sectionHoles) anyHoles = anyHoles || !holes.empty();
        std::vector<std::vector<const std::vector<SBezier> *>> chains = HoleChains(job);
        if(anyHoles && chains.empty()) {
            *problem = N_("holes are lofted only when every profile has as many; these are left out");
        }
        std::vector<TopoDS_Shape> cutters;
        for(const auto &chain : chains) {
            BRepOffsetAPI_ThruSections hole(/*isSolid=*/true, /*ruled=*/false);
            for(const std::vector<SBezier> *loop : chain) hole.AddWire(SectionWire(*loop));
            hole.Build();
            if(!hole.IsDone() || !IsGoodSolid(hole.Shape())) {
                *problem = N_("the holes could not be lofted");
                return ks;
            }
            TopoDS_Shape solid = hole.Shape();
            Orient(&solid);
            cutters.push_back(solid);
        }
        if(!CutHoles(cutters, ks.get())) *problem = N_("the holes could not be cut");
    } catch(const Standard_Failure &) {
        *problem = N_("the loft failed");
        return nullptr;
    }
    return ks;
}

// A profile swept along the job's path (pieces in order, end to end).
std::shared_ptr<KernelShape> Sweep(const KernelJob &job, KernelProblem *problem) {
    if(job.sections.empty() || job.path.empty()) {
        *problem = N_("the sweep has no profile or no path");
        return nullptr;
    }
    auto ks = std::make_shared<KernelShape>();
    try {
        // Each corner between two straight pieces rounded by the job's radius: the lines cut
        // back by r tan(turn / 2) and a tangent arc between them (review 6).
        std::vector<SBezier> path = job.path;
        std::vector<double>  backStart(path.size(), 0), backEnd(path.size(), 0);
        if(job.cornerRadius > 0) {
            bool fits = true;
            for(size_t i = 0; i + 1 < path.size(); i++) {
                if(path[i].deg != 1 || path[i + 1].deg != 1) continue;
                Vector u = path[i].ctrl[1].Minus(path[i].ctrl[0]).WithMagnitude(1);
                Vector v = path[i + 1].ctrl[1].Minus(path[i + 1].ctrl[0]).WithMagnitude(1);
                double turn = acos(std::max(-1.0, std::min(1.0, u.Dot(v))));
                if(turn < 1e-6 || turn > PI - 1e-6) continue;
                double back  = job.cornerRadius * tan(turn / 2);
                backEnd[i]       = back;
                backStart[i + 1] = back;
            }
            for(size_t i = 0; i < path.size(); i++) {
                if(path[i].deg != 1) continue;
                double length = path[i].ctrl[1].Minus(path[i].ctrl[0]).Magnitude();
                fits = fits && backStart[i] + backEnd[i] < length - LENGTH_EPS;
            }
            if(!fits) {
                *problem = N_("the path's corners are too tight for that radius; they stay sharp");
                std::fill(backStart.begin(), backStart.end(), 0);
                std::fill(backEnd.begin(), backEnd.end(), 0);
            }
        }
        BRepBuilderAPI_MakeWire spine;
        for(size_t i = 0; i < path.size(); i++) {
            const SBezier &sb = path[i];
            if(sb.deg == 1) {
                Vector u = sb.ctrl[1].Minus(sb.ctrl[0]).WithMagnitude(1);
                Vector a = sb.ctrl[0].Plus(u.ScaledBy(backStart[i]));
                Vector b = sb.ctrl[1].Minus(u.ScaledBy(backEnd[i]));
                spine.Add(BRepBuilderAPI_MakeEdge(Pnt(a), Pnt(b)).Edge());
                if(backEnd[i] > 0) {
                    Vector next = path[i + 1].ctrl[1].Minus(path[i + 1].ctrl[0]).WithMagnitude(1);
                    Vector c    = path[i + 1].ctrl[0].Plus(next.ScaledBy(backStart[i + 1]));
                    Handle(Geom_TrimmedCurve) arc = GC_MakeArcOfCircle(Pnt(b), Vec(u), Pnt(c));
                    spine.Add(BRepBuilderAPI_MakeEdge(arc).Edge());
                }
                continue;
            }
            TColgp_Array1OfPnt   poles(1, sb.deg + 1);
            TColStd_Array1OfReal weights(1, sb.deg + 1);
            for(int j = 0; j <= sb.deg; j++) {
                poles.SetValue(j + 1, Pnt(sb.ctrl[j]));
                weights.SetValue(j + 1, sb.weight[j]);
            }
            Handle(Geom_BezierCurve) curve = new Geom_BezierCurve(poles, weights);
            spine.Add(BRepBuilderAPI_MakeEdge(curve).Edge());
        }
        if(!spine.IsDone()) {
            *problem = N_("the sweep's path is not connected");
            return nullptr;
        }
        // The profile's outer loop, then each hole, swept the same way; the holes are cut.
        std::vector<TopoDS_Shape> cutters;
        size_t holes = job.sectionHoles.empty() ? 0 : job.sectionHoles[0].size();
        for(size_t i = 0; i <= holes; i++) {
            BRepOffsetAPI_MakePipeShell pipe(spine.Wire());
            pipe.SetMode(/*isFrenet=*/false); // corrected Frenet
            // A sharp corner in the path is mitred.
            pipe.SetTransitionMode(BRepBuilderAPI_RightCorner);
            const std::vector<SBezier> &loop =
                i == 0 ? job.sections[0] : job.sectionHoles[0][i - 1];
            pipe.Add(SectionWire(loop), /*withContact=*/false, /*withCorrection=*/false);
            pipe.Build();
            if(!pipe.IsDone() || !pipe.MakeSolid() || !IsGoodSolid(pipe.Shape())) {
                if(i == 0) {
                    *problem = N_("the sweep failed");
                    return nullptr;
                }
                *problem = N_("the holes could not be swept");
                return ks;
            }
            TopoDS_Shape solid = pipe.Shape();
            Orient(&solid);
            if(i == 0) {
                ks->shape = solid;
                NameCaps(job, ks->shape, pipe.FirstShape(), pipe.LastShape(), ks.get());
            } else {
                cutters.push_back(solid);
            }
        }
        if(!CutHoles(cutters, ks.get())) *problem = N_("the holes could not be cut");
    } catch(const Standard_Failure &) {
        *problem = N_("the sweep failed");
        return nullptr;
    }
    return ks;
}

// A linked B-rep (a STEP file) scaled, rotated and moved as the link says, as SolveSpace
// places a linked mesh.
std::shared_ptr<KernelShape> PlaceLinked(const KernelJob &job) {
    gp_Trsf scale, rotate, move;
    scale.SetScale(gp_Pnt(0, 0, 0), job.scale);
    rotate.SetRotation(gp_Quaternion(job.q.vx, job.q.vy, job.q.vz, job.q.w));
    move.SetTranslation(Vec(job.offset));
    auto out   = std::make_shared<KernelShape>();
    out->shape = BRepBuilderAPI_Transform(job.impShape->shape, move * rotate * scale,
                                          /*copy=*/true)
                     .Shape();
    return out;
}

class OcctKernel final : public IKernel {
public:
    void Run(const KernelJob &job, FaceNamer &namer, KernelResult *result) override {
        result->Clear();
        bool prevHasBody = !job.prevMesh->IsEmpty() || !job.prevShell->IsEmpty() ||
                           !IsEmpty(job.prevShape);
        bool isStep = job.op == KernelJob::Op::STEP_TRANSLATE ||
                      job.op == KernelJob::Op::STEP_ROTATE;
        bool sourceHasBody = isStep && !job.sourceSuppressed &&
                             (!job.sourceMesh->IsEmpty() || !job.sourceShell->IsEmpty() ||
                              !IsEmpty(job.sourceShape));
        bool linkedMesh = job.op == KernelJob::Op::LINKED && (IsEmpty(job.impShape) || job.scale <= 0);
        if(job.forcedToMesh || linkedMesh ||
           (prevHasBody && IsEmpty(job.prevShape)) ||
           (sourceHasBody && IsEmpty(job.sourceShape))) {
            // No B-rep to work with: SolveSpace's kernel, combining as meshes. The groups' meshes
            // are made when asked for, so mesh any B-rep it is given here.
            KernelJob meshJob    = job;
            meshJob.forcedToMesh = true;
            SMesh prevMesh = {}, sourceMesh = {};
            if(job.prevMesh->IsEmpty() && !IsEmpty(job.prevShape)) {
                Tessellate(*job.prevShape, job.color, &prevMesh);
                meshJob.prevMesh = &prevMesh;
            }
            if(job.sourceMesh && job.sourceMesh->IsEmpty() && !IsEmpty(job.sourceShape)) {
                Tessellate(*job.sourceShape, job.color, &sourceMesh);
                meshJob.sourceMesh = &sourceMesh;
            }
            SolveSpaceKernelInstance().Run(meshJob, namer, result);
            prevMesh.Clear();
            sourceMesh.Clear();
            return;
        }

        std::shared_ptr<const KernelShape> running;
        if(job.op == KernelJob::Op::FILLET || job.op == KernelJob::Op::CHAMFER ||
           job.op == KernelJob::Op::SHELL || job.op == KernelJob::Op::DRAFT ||
           job.op == KernelJob::Op::HOLE || job.op == KernelJob::Op::SPLIT) {
            // These reshape the previous solid.
            running = job.prevShape;
            if(!job.suppress && !IsEmpty(job.prevShape)) {
                std::shared_ptr<KernelShape> reshaped;
                switch(job.op) {
                    case KernelJob::Op::SHELL: reshaped = Shell(job, &namer, &result->problem); break;
                    case KernelJob::Op::DRAFT: reshaped = Draft(job, &namer, &result->problem); break;
                    case KernelJob::Op::HOLE:  reshaped = Holes(job, &result->problem); break;
                    case KernelJob::Op::SPLIT: reshaped = SplitHalf(job, &result->problem); break;
                    default:                   reshaped = RoundEdges(job, &result->problem); break;
                }
                if(reshaped) running = reshaped;
            }
            Finish(job, running, result);
            return;
        }

        std::shared_ptr<KernelShape> made;
        switch(job.op) {
            case KernelJob::Op::LINKED: made = PlaceLinked(job); break;
            case KernelJob::Op::NONE:
            case KernelJob::Op::FILLET:
            case KernelJob::Op::CHAMFER:
            case KernelJob::Op::SHELL:
            case KernelJob::Op::DRAFT:
            case KernelJob::Op::HOLE:
            case KernelJob::Op::SPLIT: break;
            case KernelJob::Op::LOFT:  made = Loft(job, &result->problem); break;
            case KernelJob::Op::SWEEP: made = Sweep(job, &result->problem); break;
            case KernelJob::Op::EXTRUDE: made = Extrude(job, &namer); break;
            case KernelJob::Op::FIT:     made = FitShape(job, &result->problem); break;
            case KernelJob::Op::LATHE:   made = Revolve(job, &namer, /*full=*/true); break;
            case KernelJob::Op::REVOLVE:
                made = Revolve(job, &namer, /*full=*/fabs(job.anglef - job.angles) >= 2 * PI);
                break;
            case KernelJob::Op::HELIX: made = Revolve(job, &namer, /*full=*/false); break;
            case KernelJob::Op::STEP_TRANSLATE:
            case KernelJob::Op::STEP_ROTATE:
                if(sourceHasBody) made = StepAndRepeat(job, &namer);
                break;
        }
        if(made && !made->shape.IsNull()) {
            result->thisBody.shape       = made;
            result->thisBody.tessellated = true;
        }

        if(IsEmpty(result->thisBody.shape) || job.suppress) {
            running = job.prevShape;
        } else if(IsEmpty(job.prevShape)) {
            if(job.combine == KernelCombine::UNION || job.combine == KernelCombine::ASSEMBLE) {
                running = result->thisBody.shape;
            }
        } else {
            auto out = std::make_shared<KernelShape>();
            if(Boolean(job.combine, *job.prevShape, *result->thisBody.shape, out.get())) {
                running = out;
            } else {
                running                       = job.prevShape;
                result->running.booleanFailed = true;
            }
        }
        Finish(job, running, result);
    }

    // The running body is running: the previous one's mesh, or running's tessellation.
    static void Finish(const KernelJob &job, std::shared_ptr<const KernelShape> running,
                       KernelResult *result) {
        // The meshes are made from the shapes when first asked for (Group::RunningMesh).
        result->running.tessellated = !IsEmpty(running);
        result->running.shape       = std::move(running);
    }

    bool Supports(KernelFeature feature) const override {
        switch(feature) {
            case KernelFeature::EXTRUDE:
            case KernelFeature::LATHE:
            case KernelFeature::REVOLVE:
            case KernelFeature::HELIX:
            case KernelFeature::PATTERN:
            case KernelFeature::LINK:
            case KernelFeature::BOOLEAN:
            case KernelFeature::FILLET:
            case KernelFeature::CHAMFER:
            case KernelFeature::SHELL:
            case KernelFeature::DRAFT:
            case KernelFeature::HOLE:
            case KernelFeature::LOFT:
            case KernelFeature::SWEEP:
            case KernelFeature::FIT:
            case KernelFeature::SPLIT: return true;
        }
        return false;
    }
};

}

IKernel &Kernel(KernelKind kind) {
    static OcctKernel occt;
    if(kind == KernelKind::OCCT) return occt;
    return SolveSpaceKernelInstance();
}

bool KernelAvailable(KernelKind) {
    return true;
}

bool KernelExportStep(const std::shared_ptr<const KernelShape> &ks,
                      const Platform::Path &filename, double scale) {
    if(IsEmpty(ks)) return false;
    TopoDS_Shape shape = ks->shape;
    if(scale != 1) {
        gp_Trsf trsf;
        trsf.SetScale(gp_Pnt(0, 0, 0), scale);
        shape = BRepBuilderAPI_Transform(shape, trsf, /*copy=*/true).Shape();
    }
    STEPControl_Writer writer;
    Interface_Static::SetCVal("write.step.unit", "MM");
    if(writer.Transfer(shape, STEPControl_AsIs) != IFSelect_RetDone) return false;
    return writer.Write(filename.raw.c_str()) == IFSelect_RetDone;
}

bool KernelExportStepBodies(const std::vector<KernelStepBody> &bodies,
                            const Platform::Path &filename, double scale) {
    STEPControl_Writer writer;
    Interface_Static::SetCVal("write.step.unit", "MM");
    int transferred = 0;
    for(const KernelStepBody &b : bodies) {
        if(IsEmpty(b.shape)) continue;
        TopoDS_Shape shape = b.shape->shape;
        if(scale != 1) {
            gp_Trsf trsf;
            trsf.SetScale(gp_Pnt(0, 0, 0), scale);
            shape = BRepBuilderAPI_Transform(shape, trsf, /*copy=*/true).Shape();
        }
        if(writer.Transfer(shape, STEPControl_AsIs) != IFSelect_RetDone) return false;
        transferred++;
        // Each transfer makes a product of its own; name the new one after its body.
        Handle(StepData_StepModel) model = writer.Model();
        for(int i = model->NbEntities(); i >= 1; i--) {
            Handle(StepBasic_Product) product =
                Handle(StepBasic_Product)::DownCast(model->Value(i));
            if(product.IsNull()) continue;
            Handle(TCollection_HAsciiString) name = new TCollection_HAsciiString(b.name.c_str());
            product->SetId(name);
            product->SetName(name);
            break;
        }
    }
    if(transferred == 0) return false;
    return writer.Write(filename.raw.c_str()) == IFSelect_RetDone;
}


KernelEdgeFacesResult KernelEdgeFaces(const std::shared_ptr<const KernelShape> &ks, Vector a,
                                      Vector b, KernelEdgeFace out[2]) {
    if(IsEmpty(ks)) return KernelEdgeFacesResult::NOT_FOUND;
    std::vector<SBezier> line = { SBezier::From(a, b) };
    TopTools_IndexedDataMapOfShapeListOfShape facesOf;
    TopExp::MapShapesAndAncestors(ks->shape, TopAbs_EDGE, TopAbs_FACE, facesOf);
    for(int i = 1; i <= facesOf.Extent(); i++) {
        const TopoDS_Edge &edge = TopoDS::Edge(facesOf.FindKey(i));
        if(BRep_Tool::Degenerated(edge) || !OnCurves(edge, line)) continue;
        const TopTools_ListOfShape &faces = facesOf(i);
        if(faces.Extent() != 2) return KernelEdgeFacesResult::NOT_PLANAR;
        int n = 0;
        for(const TopoDS_Shape &shape : faces) {
            const TopoDS_Face &face = TopoDS::Face(shape);
            BRepAdaptor_Surface surface(face, /*restriction=*/false);
            if(surface.GetType() != GeomAbs_Plane) return KernelEdgeFacesResult::NOT_PLANAR;
            gp_Dir normal = surface.Plane().Axis().Direction();
            if(face.Orientation() == TopAbs_REVERSED) normal.Reverse();
            // The face's boundary runs anticlockwise about its outward normal, so the face
            // lies to the left of the edge as the boundary runs along it.
            TopAbs_Orientation along = TopAbs_FORWARD;
            for(TopExp_Explorer ex(face, TopAbs_EDGE); ex.More(); ex.Next()) {
                if(ex.Current().IsSame(edge)) {
                    along = ex.Current().Orientation();
                    break;
                }
            }
            BRepAdaptor_Curve curve(edge);
            gp_Pnt            p;
            gp_Vec            tangent;
            curve.D1((curve.FirstParameter() + curve.LastParameter()) / 2, p, tangent);
            if(along == TopAbs_REVERSED) tangent.Reverse();
            gp_Vec into = gp_Vec(normal).Crossed(tangent);
            out[n].into = Vec(into.Normalized().XYZ());
            const FaceInfo *info = ks->faces.Seek(face);
            out[n].face = info ? info->face : 0;
            n++;
        }
        return KernelEdgeFacesResult::FOUND;
    }
    return KernelEdgeFacesResult::NOT_FOUND;
}


static TopoDS_Shape Placed(const TopoDS_Shape &shape, Quaternion q, Vector t) {
    gp_Trsf trsf;
    trsf.SetRotation(gp_Quaternion(q.vx, q.vy, q.vz, q.w));
    trsf.SetTranslationPart(gp_Vec(t.x, t.y, t.z));
    return shape.Moved(TopLoc_Location(trsf));
}

bool KernelMinDistance(const std::shared_ptr<const KernelShape> &a, Quaternion qa, Vector ta,
                       const std::shared_ptr<const KernelShape> &b, Quaternion qb, Vector tb,
                       double *distance) {
    if(IsEmpty(a) || IsEmpty(b)) return false;
    BRepExtrema_DistShapeShape dss(Placed(a->shape, qa, ta), Placed(b->shape, qb, tb));
    if(!dss.IsDone()) return false;
    // One solid inside the other has no nearest points between their faces: inside counts.
    *distance = dss.InnerSolution() ? 0 : dss.Value();
    return true;
}

bool KernelMassProperties(const std::shared_ptr<const KernelShape> &ks, MassProps *out) {
    if(IsEmpty(ks)) return false;
    GProp_GProps volume, surface;
    BRepGProp::VolumeProperties(ks->shape, volume);
    BRepGProp::SurfaceProperties(ks->shape, surface);
    out->volume = volume.Mass();
    out->area   = surface.Mass();
    out->centre = Vec(volume.CentreOfMass().XYZ());
    out->exact  = true;
    return out->volume > 0;
}


std::vector<KernelFaceNormal> KernelFaceNormals(const std::shared_ptr<const KernelShape> &ks) {
    std::vector<KernelFaceNormal> normals;
    if(IsEmpty(ks)) return normals;
    // The faces as the solid has them: the map's keys may carry another orientation.
    for(TopExp_Explorer ex(ks->shape, TopAbs_FACE); ex.More(); ex.Next()) {
        const FaceInfo *info = ks->faces.Seek(ex.Current());
        gp_Dir          out;
        if(info == nullptr || info->face == 0 || !OutwardNormal(TopoDS::Face(ex.Current()), &out)) {
            continue;
        }
        normals.push_back({ info->face, Vec(out.XYZ()) });
    }
    return normals;
}


std::shared_ptr<const KernelShape> KernelImportStep(const Platform::Path &filename) {
    STEPControl_Reader reader;
    try {
        if(reader.ReadFile(filename.raw.c_str()) != IFSelect_RetDone) return nullptr;
        if(reader.TransferRoots() == 0) return nullptr;
        auto ks   = std::make_shared<KernelShape>();
        ks->shape = reader.OneShape();
        if(ks->shape.IsNull()) return nullptr;
        return ks;
    } catch(const Standard_Failure &) {
        return nullptr;
    }
}

void KernelTessellate(const std::shared_ptr<const KernelShape> &ks, RgbaColor color, SMesh *mesh,
                      SOutlineList *outlines) {
    if(!IsEmpty(ks)) Tessellate(*ks, color, mesh, outlines);
}


std::vector<uint32_t> KernelFacesBoundedBy(const std::shared_ptr<const KernelShape> &ks,
                                           const std::vector<std::vector<SBezier>> &curves) {
    std::vector<uint32_t> faces;
    if(IsEmpty(ks) || curves.empty()) return faces;
    for(TopExp_Explorer ex(ks->shape, TopAbs_FACE); ex.More(); ex.Next()) {
        const FaceInfo *info = ks->faces.Seek(ex.Current());
        if(info == nullptr || info->face == 0) continue;
        bool whole = true, any = false;
        for(TopExp_Explorer ee(ex.Current(), TopAbs_EDGE); ee.More() && whole; ee.Next()) {
            const TopoDS_Edge &edge = TopoDS::Edge(ee.Current());
            if(BRep_Tool::Degenerated(edge)) continue;
            if(BRep_Tool::IsClosed(edge, TopoDS::Face(ex.Current()))) continue; // a seam
            bool on = false;
            for(const std::vector<SBezier> &c : curves) on = on || OnCurves(edge, c);
            whole = on;
            any   = true;
        }
        if(whole && any && std::find(faces.begin(), faces.end(), info->face) == faces.end()) {
            faces.push_back(info->face);
        }
    }
    return faces;
}

}
