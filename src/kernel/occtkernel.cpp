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

#include <mutex>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAlgoAPI_BooleanOperation.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepLib.hxx>
#include <BRepLib_ToolTriangulatedShape.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepOffsetAPI_MakePipeShell.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <GCE2d_MakeSegment.hxx>
#include <Interface_Static.hxx>
#include <STEPControl_Writer.hxx>
#include <Geom_BezierCurve.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <NCollection_DataMap.hxx>
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

struct FaceInfo {
    uint32_t  face  = 0; // the face entity's handle, or 0
    RgbaColor color = {};
};

using FaceMap = NCollection_DataMap<TopoDS_Shape, FaceInfo, TopTools_ShapeMapHasher>;

}

struct KernelShape {
    TopoDS_Shape shape;
    FaceMap      faces;
};

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
    op->SetRunParallel(false);
    op->Build();
    if(!op->IsDone() || op->HasErrors()) return false;
    op->SimplifyResult();
    out->shape = op->Shape();
    CarryNames(*op, { &a.faces, &b.faces }, &out->faces);
    return true;
}

// The shape's faces as triangles, with true normals, each carrying its face's name and colour.
void Tessellate(const KernelShape &ks, RgbaColor color, SMesh *mesh) {
    std::lock_guard<std::mutex> lock(gMeshMutex);
    double tol = SS.ChordTolMm();
    // Faces carried over from earlier bodies keep the triangulations made for those, whose
    // edges needn't match their new neighbours'; mesh the whole shape afresh.
    BRepTools::Clean(ks.shape);
    BRepMesh_IncrementalMesh(ks.shape, tol, /*isRelative=*/false, /*angle=*/0.35,
                             /*parallel=*/false);
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
    // Combined in pairs, as SolveSpace's kernel does.
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
        Vector p    = Vec(c.Value(t).XYZ());
        bool   near = false;
        for(const SBezier &sb : curves) {
            double u;
            sb.ClosestPointTo(p, &u, /*mustConverge=*/false);
            if(sb.PointAt(u).Equals(p, 1e-4)) {
                near = true;
                break;
            }
        }
        if(!near) return false;
    }
    return true;
}

// The previous body with the job's edges rounded or bevelled, or null if nothing was;
// *problem says what went wrong.
std::shared_ptr<KernelShape> RoundEdges(const KernelJob &job, std::string *problem) {
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
        *problem = ssprintf("%d of the %d edges were not found on the solid", missing,
                            (int)job.edgeCurves.size());
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

class OcctKernel final : public IKernel {
public:
    void Run(const KernelJob &job, FaceNamer &namer, KernelResult *result) override {
        result->Clear();
        bool prevHasBody = !job.prevMesh->IsEmpty() || !job.prevShell->IsEmpty();
        bool isStep = job.op == KernelJob::Op::STEP_TRANSLATE ||
                      job.op == KernelJob::Op::STEP_ROTATE;
        bool sourceHasBody = isStep && !job.sourceSuppressed &&
                             (!job.sourceMesh->IsEmpty() || !job.sourceShell->IsEmpty());
        if(job.forcedToMesh || job.op == KernelJob::Op::LINKED ||
           (prevHasBody && IsEmpty(job.prevShape)) ||
           (sourceHasBody && IsEmpty(job.sourceShape))) {
            // No B-rep to work with: SolveSpace's kernel, combining as meshes.
            KernelJob meshJob    = job;
            meshJob.forcedToMesh = true;
            SolveSpaceKernelInstance().Run(meshJob, namer, result);
            return;
        }

        std::shared_ptr<const KernelShape> running;
        if(job.op == KernelJob::Op::FILLET || job.op == KernelJob::Op::CHAMFER) {
            running = job.prevShape;
            if(!job.suppress && !IsEmpty(job.prevShape)) {
                if(auto rounded = RoundEdges(job, &result->problem)) running = rounded;
            }
            Finish(job, running, result);
            return;
        }

        std::shared_ptr<KernelShape> made;
        switch(job.op) {
            case KernelJob::Op::NONE:
            case KernelJob::Op::LINKED:
            case KernelJob::Op::FILLET:
            case KernelJob::Op::CHAMFER: break;
            case KernelJob::Op::EXTRUDE: made = Extrude(job, &namer); break;
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
            Tessellate(*made, job.color, &result->thisBody.mesh);
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
        if(running == job.prevShape) {
            result->running.mesh.MakeFromCopyOf(job.prevMesh);
        } else if(!IsEmpty(running)) {
            Tessellate(*running, job.color, &result->running.mesh);
        }
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
            case KernelFeature::CHAMFER: return true;
            case KernelFeature::SHELL:
            case KernelFeature::LOFT:
            case KernelFeature::SWEEP: return false;
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

}
