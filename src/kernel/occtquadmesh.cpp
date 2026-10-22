//-----------------------------------------------------------------------------
// A quad mesh of the OCCT kernel's B-rep for Blender (a spike, after review 5): every
// four-sided face is a structured quad grid following the face's own (u, v) flow, a flat face
// with one hole rings of quads, any other face without a hole rings of quads round a fan of
// triangles at its centre (when it can see its whole edge from there), the rest triangles; each B-rep edge is cut into points once, so faces share them and the mesh is
// watertight; the sharp edges are creased, so Catmull-Clark subdivision keeps the shape.
// Written as USD (.usda text), which Blender imports with its creases.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "kernel/occtshape.h"

#include <map>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepTools.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <BRep_Tool.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <GCPnts_UniformAbscissa.hxx>
#include <Geom2d_Curve.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>

namespace SolveSpace {
namespace {

Vector Vec(const gp_Pnt &p) {
    return Vector::From(p.X(), p.Y(), p.Z());
}

// A point along a wire as a face sees it: the mesh vertex, and where it is on the face.
struct WirePoint {
    int      id;
    gp_Pnt2d uv;
};

struct QuadMesher {
    std::vector<Vector>           points;
    std::vector<std::vector<int>> polys;
    std::vector<std::vector<int>> creases;
    TopTools_IndexedMapOfShape    vertices, edges;
    std::vector<int>              count;  // segments per edge
    std::vector<std::vector<double>> params; // per edge, in its own direction
    std::vector<std::vector<int>>    ids;

    // How many pieces an edge is cut into at first: no longer than target, turning at most
    // 30 degrees each, a closed edge into three at least.
    int FirstCount(const TopoDS_Edge &edge, double target) {
        if(BRep_Tool::Degenerated(edge)) return 1;
        BRepAdaptor_Curve curve(edge);
        double            length = GCPnts_AbscissaPoint::Length(curve);
        int               n      = (int)ceil(length / target - 1e-9);
        if(curve.GetType() != GeomAbs_Line) {
            double f = curve.FirstParameter(), l = curve.LastParameter(), turn = 0;
            gp_Pnt p;
            gp_Vec prev, d;
            curve.D1(f, p, prev);
            for(int i = 1; i <= 32; i++) {
                curve.D1(f + (l - f) * i / 32, p, d);
                if(prev.Magnitude() > 1e-12 && d.Magnitude() > 1e-12) turn += prev.Angle(d);
                prev = d;
            }
            n = std::max(n, (int)ceil(turn / (PI / 6) - 1e-9));
        }
        if(TopExp::FirstVertex(edge).IsSame(TopExp::LastVertex(edge))) n = std::max(n, 3);
        return std::max(n, 1);
    }

    std::vector<TopoDS_Edge> WireEdges(const TopoDS_Wire &wire, const TopoDS_Face &face) {
        std::vector<TopoDS_Edge> list;
        for(BRepTools_WireExplorer we(wire, face); we.More(); we.Next()) list.push_back(we.Current());
        return list;
    }

    // An edge as the wire runs along it, from its start to its end.
    std::vector<WirePoint> Along(const TopoDS_Edge &edge, const TopoDS_Face &face) {
        int                        e = edges.FindIndex(edge) - 1;
        double                     f, l;
        Handle(Geom2d_Curve)       pc = BRep_Tool::CurveOnSurface(edge, face, f, l);
        std::vector<WirePoint>     out;
        for(size_t k = 0; k < ids[e].size(); k++) {
            gp_Pnt2d uv = pc.IsNull() ? gp_Pnt2d() : pc->Value(params[e][k]);
            out.push_back({ ids[e][k], uv });
        }
        if(edge.Orientation() == TopAbs_REVERSED) std::reverse(out.begin(), out.end());
        return out;
    }

    // Where a face's outside is at uv.
    static gp_Dir Outward(const TopoDS_Face &face, const BRepAdaptor_Surface &s, gp_Pnt2d uv) {
        gp_Pnt p;
        gp_Vec du, dv;
        s.D1(uv.X(), uv.Y(), p, du, dv);
        gp_Vec n = du.Crossed(dv);
        if(n.Magnitude() < 1e-12) return gp_Dir(0, 0, 1);
        if(face.Orientation() == TopAbs_REVERSED) n.Reverse();
        return gp_Dir(n);
    }

    // The polygons from first on, flipped if they face into the solid.
    void FaceOut(size_t first, gp_Dir out) {
        Vector sum = {};
        for(size_t i = first; i < polys.size(); i++) {
            const std::vector<int> &p = polys[i];
            for(size_t k = 0; k < p.size(); k++) {
                Vector a = points[p[k]], b = points[p[(k + 1) % p.size()]];
                sum      = sum.Plus(Vector::From((a.y - b.y) * (a.z + b.z),
                                                 (a.z - b.z) * (a.x + b.x),
                                                 (a.x - b.x) * (a.y + b.y)));
            }
        }
        if(sum.Dot(Vector::From(out.X(), out.Y(), out.Z())) >= 0) return;
        for(size_t i = first; i < polys.size(); i++) std::reverse(polys[i].begin(), polys[i].end());
    }

    void AddPoly(std::vector<int> p) {
        // Corners where a side shrinks to a point (a cone's tip) repeat; drop the repeats.
        std::vector<int> q;
        for(int id : p) {
            if(q.empty() || q.back() != id) q.push_back(id);
        }
        while(q.size() > 1 && q.front() == q.back()) q.pop_back();
        if(q.size() >= 3) polys.push_back(q);
    }

    bool QuadGrid(const TopoDS_Face &face, const std::vector<TopoDS_Edge> &sides) {
        BRepAdaptor_Surface    surface(face);
        std::vector<WirePoint> s[4];
        for(int k = 0; k < 4; k++) s[k] = Along(sides[k], face);
        int nu = (int)s[0].size() - 1, nv = (int)s[1].size() - 1;
        if(nu < 1 || nv < 1 || (int)s[2].size() != nu + 1 || (int)s[3].size() != nv + 1) {
            return false;
        }
        auto B = [&](int i) { return s[0][i]; };
        auto R = [&](int j) { return s[1][j]; };
        auto T = [&](int i) { return s[2][nu - i]; };
        auto L = [&](int j) { return s[3][nv - j]; };
        std::vector<std::vector<int>> grid(nu + 1, std::vector<int>(nv + 1));
        for(int i = 0; i <= nu; i++) {
            for(int j = 0; j <= nv; j++) {
                if(j == 0) { grid[i][j] = B(i).id; continue; }
                if(j == nv) { grid[i][j] = T(i).id; continue; }
                if(i == 0) { grid[i][j] = L(j).id; continue; }
                if(i == nu) { grid[i][j] = R(j).id; continue; }
                // Inside: the boundary's blend (a Coons patch) in the face's (u, v).
                double s1 = (double)i / nu, t = (double)j / nv;
                gp_XY  uv = B(i).uv.XY() * (1 - t) + T(i).uv.XY() * t + L(j).uv.XY() * (1 - s1) +
                           R(j).uv.XY() * s1 -
                           (B(0).uv.XY() * ((1 - s1) * (1 - t)) + B(nu).uv.XY() * (s1 * (1 - t)) +
                            T(0).uv.XY() * ((1 - s1) * t) + T(nu).uv.XY() * (s1 * t));
                grid[i][j] = (int)points.size();
                points.push_back(Vec(surface.Value(uv.X(), uv.Y())));
            }
        }
        size_t first = polys.size();
        for(int i = 0; i < nu; i++) {
            for(int j = 0; j < nv; j++) {
                AddPoly({ grid[i][j], grid[i + 1][j], grid[i + 1][j + 1], grid[i][j + 1] });
            }
        }
        gp_Pnt2d mid((B(0).uv.X() + T(nu).uv.X()) / 2, (B(0).uv.Y() + T(nu).uv.Y()) / 2);
        FaceOut(first, Outward(face, surface, mid));
        return true;
    }

    // A face as the wire runs round it: each mesh point once.
    std::vector<WirePoint> Round(const std::vector<TopoDS_Edge> &wire, const TopoDS_Face &face) {
        std::vector<WirePoint> out;
        for(const TopoDS_Edge &e : wire) {
            std::vector<WirePoint> a = Along(e, face);
            out.insert(out.end(), a.begin(), a.end() - 1);
        }
        return out;
    }

    // A flat face with one hole: rings of quads from the outer wire in to the hole's, the two
    // cut into as many points, lined up, the rings no wider than target.
    bool Rings(const TopoDS_Face &face, const std::vector<TopoDS_Edge> &outerWire,
               const std::vector<TopoDS_Edge> &innerWire, double target) {
        BRepAdaptor_Surface    surface(face);
        std::vector<WirePoint> outer = Round(outerWire, face), inner = Round(innerWire, face);
        size_t                 n     = outer.size();
        if(n < 3 || inner.size() != n) return false;
        // The hole runs the other way round: turn it, then start it where it is nearest.
        std::reverse(inner.begin(), inner.end());
        size_t best = 0;
        double least = VERY_POSITIVE;
        for(size_t shift = 0; shift < n; shift++) {
            double sum = 0;
            for(size_t i = 0; i < n; i++) {
                sum += points[outer[i].id].Minus(points[inner[(i + shift) % n].id]).Magnitude();
            }
            if(sum < least) least = sum, best = shift;
        }
        std::rotate(inner.begin(), inner.begin() + best, inner.end());
        int rings = std::max(1, (int)ceil(least / n / target - 1e-9));
        std::vector<std::vector<int>> row(rings + 1, std::vector<int>(n));
        for(size_t i = 0; i < n; i++) {
            row[0][i]     = outer[i].id;
            row[rings][i] = inner[i].id;
            for(int k = 1; k < rings; k++) {
                gp_XY uv  = outer[i].uv.XY() * (1.0 - (double)k / rings) +
                            inner[i].uv.XY() * ((double)k / rings);
                row[k][i] = (int)points.size();
                points.push_back(Vec(surface.Value(uv.X(), uv.Y())));
            }
        }
        size_t first = polys.size();
        for(int k = 0; k < rings; k++) {
            for(size_t i = 0; i < n; i++) {
                size_t j = (i + 1) % n;
                AddPoly({ row[k][i], row[k][j], row[k + 1][j], row[k + 1][i] });
            }
        }
        FaceOut(first, Outward(face, surface, outer[0].uv));
        return true;
    }

    // A face with no hole that is neither four-sided nor a ring (review 6): rings of quads
    // from its edge in towards a centre, then a fan of triangles round the centre; false if
    // the centre, or the way to it from the edge, leaves the face.
    bool Radial(const TopoDS_Face &face, double target) {
        BRepAdaptor_Surface    surface(face);
        std::vector<WirePoint> edge = Round(WireEdges(BRepTools::OuterWire(face), face), face);
        size_t                 n    = edge.size();
        if(n < 3) return false;
        gp_XY c(0, 0);
        for(const WirePoint &p : edge) c += p.uv.XY();
        c /= (double)n;
        auto inside = [&](gp_XY uv) {
            BRepClass_FaceClassifier where(face, gp_Pnt2d(uv), 1e-7);
            return where.State() == TopAbs_IN || where.State() == TopAbs_ON;
        };
        if(!inside(c)) return false;
        double reach = 0;
        Vector centre = Vec(surface.Value(c.X(), c.Y()));
        for(const WirePoint &p : edge) reach += points[p.id].Minus(centre).Magnitude() / n;
        int rings = std::max(1, (int)ceil(reach / target - 1e-9));
        // Every ring point, and the way between them, must be on the face itself (the
        // surface goes on past it).
        int steps = std::max(4, 2 * rings);
        for(const WirePoint &p : edge) {
            for(int k = 1; k < steps; k++) {
                if(!inside(p.uv.XY() * (1 - (double)k / steps) + c * ((double)k / steps))) {
                    return false;
                }
            }
        }
        std::vector<std::vector<int>> row(rings, std::vector<int>(n));
        for(size_t i = 0; i < n; i++) {
            row[0][i] = edge[i].id;
            for(int k = 1; k < rings; k++) {
                gp_XY uv  = edge[i].uv.XY() * (1.0 - (double)k / rings) + c * ((double)k / rings);
                row[k][i] = (int)points.size();
                points.push_back(Vec(surface.Value(uv.X(), uv.Y())));
            }
        }
        int    mid   = (int)points.size();
        points.push_back(centre);
        size_t first = polys.size();
        for(size_t i = 0; i < n; i++) {
            size_t j = (i + 1) % n;
            for(int k = 0; k + 1 < rings; k++) {
                AddPoly({ row[k][i], row[k][j], row[k + 1][j], row[k + 1][i] });
            }
            AddPoly({ row[rings - 1][i], row[rings - 1][j], mid });
        }
        FaceOut(first, Outward(face, surface, gp_Pnt2d(c)));
        return true;
    }

    // A simple polygon (uv) as triangles, by clipping ears; false if it won't go.
    bool EarClip(std::vector<WirePoint> poly) {
        auto cross = [](gp_XY a, gp_XY b, gp_XY c) { return (b - a) ^ (c - b); };
        double area = 0;
        for(size_t i = 0; i < poly.size(); i++) {
            area += poly[i].uv.XY() ^ poly[(i + 1) % poly.size()].uv.XY();
        }
        if(area < 0) std::reverse(poly.begin(), poly.end());
        while(poly.size() > 3) {
            bool clipped = false;
            for(size_t i = 0; i < poly.size() && !clipped; i++) {
                const WirePoint &a = poly[(i + poly.size() - 1) % poly.size()], &b = poly[i],
                                &c = poly[(i + 1) % poly.size()];
                if(cross(a.uv.XY(), b.uv.XY(), c.uv.XY()) <= 1e-14) continue;
                bool empty = true;
                for(const WirePoint &p : poly) {
                    if(p.id == a.id || p.id == b.id || p.id == c.id) continue;
                    gp_XY q = p.uv.XY();
                    if(cross(a.uv.XY(), b.uv.XY(), q) > 0 && cross(b.uv.XY(), c.uv.XY(), q) > 0 &&
                       cross(c.uv.XY(), a.uv.XY(), q) > 0) {
                        empty = false;
                        break;
                    }
                }
                if(!empty) continue;
                AddPoly({ a.id, b.id, c.id });
                poly.erase(poly.begin() + i);
                clipped = true;
            }
            if(!clipped) return false;
        }
        AddPoly({ poly[0].id, poly[1].id, poly[2].id });
        return true;
    }

    // Any other face: its outline, each hole joined to it by a cut, as triangles (an n-gon if
    // that fails).
    void Triangles(const TopoDS_Face &face) {
        BRepAdaptor_Surface    surface(face);
        TopoDS_Wire            outerWire = BRepTools::OuterWire(face);
        std::vector<WirePoint> outer     = Round(WireEdges(outerWire, face), face);
        for(TopExp_Explorer ex(face, TopAbs_WIRE); ex.More(); ex.Next()) {
            if(ex.Current().IsSame(outerWire)) continue;
            std::vector<WirePoint> hole = Round(WireEdges(TopoDS::Wire(ex.Current()), face), face);
            if(hole.empty() || outer.empty()) continue;
            size_t a = 0, b = 0;
            double best = VERY_POSITIVE;
            for(size_t i = 0; i < outer.size(); i++) {
                for(size_t j = 0; j < hole.size(); j++) {
                    double d = points[outer[i].id].Minus(points[hole[j].id]).Magnitude();
                    if(d < best) best = d, a = i, b = j;
                }
            }
            std::vector<WirePoint> joined(outer.begin(), outer.begin() + a + 1);
            for(size_t k = 0; k <= hole.size(); k++) joined.push_back(hole[(b + k) % hole.size()]);
            joined.insert(joined.end(), outer.begin() + a, outer.end());
            outer = joined;
        }
        size_t first = polys.size();
        if(!EarClip(outer)) {
            polys.resize(first);
            std::vector<int> ids;
            for(const WirePoint &p : outer) ids.push_back(p.id);
            AddPoly(ids);
        }
        double u0, u1, v0, v1;
        BRepTools::UVBounds(face, u0, u1, v0, v1);
        FaceOut(first, Outward(face, surface, gp_Pnt2d((u0 + u1) / 2, (v0 + v1) / 2)));
    }
};

}

bool KernelExportQuadUsd(const std::shared_ptr<const KernelShape> &ks,
                         const Platform::Path &filename, double target, QuadMeshStats *stats) {
    if(!ks || ks->shape.IsNull() || target <= 0) return false;
    const TopoDS_Shape &shape = ks->shape;
    QuadMesher          m;
    TopExp::MapShapes(shape, TopAbs_VERTEX, m.vertices);
    TopExp::MapShapes(shape, TopAbs_EDGE, m.edges);
    for(int i = 1; i <= m.vertices.Extent(); i++) {
        m.points.push_back(Vec(BRep_Tool::Pnt(TopoDS::Vertex(m.vertices(i)))));
    }
    for(int i = 1; i <= m.edges.Extent(); i++) {
        m.count.push_back(m.FirstCount(TopoDS::Edge(m.edges(i)), target));
    }

    // The four-sided faces' opposite sides must be cut alike: take the larger count, until
    // every grid agrees.
    struct Quad {
        TopoDS_Face              face;
        std::vector<TopoDS_Edge> sides;
    };
    std::vector<Quad>        quads;
    struct Ring {
        TopoDS_Face              face;
        std::vector<TopoDS_Edge> outer, inner;
    };
    std::vector<Ring>        rings;
    std::vector<TopoDS_Face> others;
    for(TopExp_Explorer ex(shape, TopAbs_FACE); ex.More(); ex.Next()) {
        TopoDS_Face face  = TopoDS::Face(ex.Current());
        int         wires = 0;
        for(TopExp_Explorer w(face, TopAbs_WIRE); w.More(); w.Next()) wires++;
        std::vector<TopoDS_Edge> sides = m.WireEdges(BRepTools::OuterWire(face), face);
        if(wires == 1 && sides.size() == 4) {
            quads.push_back({ face, sides });
        } else if(wires == 2 && BRepAdaptor_Surface(face).GetType() == GeomAbs_Plane) {
            TopoDS_Wire outer = BRepTools::OuterWire(face), inner;
            for(TopExp_Explorer w(face, TopAbs_WIRE); w.More(); w.Next()) {
                if(!w.Current().IsSame(outer)) inner = TopoDS::Wire(w.Current());
            }
            rings.push_back({ face, sides, m.WireEdges(inner, face) });
        } else {
            others.push_back(face);
        }
    }
    std::vector<double> lengths;
    for(int i = 1; i <= m.edges.Extent(); i++) {
        const TopoDS_Edge &e = TopoDS::Edge(m.edges(i));
        lengths.push_back(BRep_Tool::Degenerated(e) ? 0
                                                    : GCPnts_AbscissaPoint::Length(BRepAdaptor_Curve(e)));
    }
    for(int pass = 0, changed = 1; changed && pass < 100; pass++) {
        changed = false;
        // A ring's two wires are cut into as many points: the shorter-cut one gets more,
        // where its pieces are longest.
        for(const Ring &r : rings) {
            auto sum = [&](const std::vector<TopoDS_Edge> &w) {
                int s = 0;
                for(const TopoDS_Edge &e : w) s += m.count[m.edges.FindIndex(e) - 1];
                return s;
            };
            for(int so = sum(r.outer), si = sum(r.inner); so != si; so = sum(r.outer), si = sum(r.inner)) {
                const std::vector<TopoDS_Edge> &w = so < si ? r.outer : r.inner;
                int    grow = -1;
                double piece = -1;
                for(const TopoDS_Edge &e : w) {
                    int i = m.edges.FindIndex(e) - 1;
                    if(lengths[i] / m.count[i] > piece) piece = lengths[i] / m.count[i], grow = i;
                }
                m.count[grow]++;
                changed = true;
            }
        }
        for(const Quad &q : quads) {
            for(int k = 0; k < 2; k++) {
                int a = m.edges.FindIndex(q.sides[k]) - 1, b = m.edges.FindIndex(q.sides[k + 2]) - 1;
                int n = std::max(m.count[a], m.count[b]);
                if(m.count[a] != n || m.count[b] != n) changed = true;
                m.count[a] = m.count[b] = n;
            }
        }
    }

    // Each edge cut once: its points shared by both its faces.
    for(int i = 1; i <= m.edges.Extent(); i++) {
        TopoDS_Edge edge = TopoDS::Edge(m.edges(i).Oriented(TopAbs_FORWARD));
        int         n    = m.count[i - 1];
        int first = m.vertices.FindIndex(TopExp::FirstVertex(edge)) - 1;
        int last  = m.vertices.FindIndex(TopExp::LastVertex(edge)) - 1;
        double f, l;
        BRep_Tool::Range(edge, f, l);
        std::vector<double> t(n + 1);
        std::vector<int>    id(n + 1, first);
        bool                degenerated = BRep_Tool::Degenerated(edge);
        if(!degenerated) {
            BRepAdaptor_Curve      curve(edge);
            GCPnts_UniformAbscissa even(curve, n + 1, f, l);
            for(int k = 0; k <= n; k++) {
                t[k] = (even.IsDone() && even.NbPoints() == n + 1) ? even.Parameter(k + 1)
                                                                   : f + (l - f) * k / n;
            }
            for(int k = 1; k < n; k++) {
                id[k] = (int)m.points.size();
                m.points.push_back(Vec(curve.Value(t[k])));
            }
            id[n] = last;
        } else {
            for(int k = 0; k <= n; k++) t[k] = f + (l - f) * k / n;
        }
        t[0] = f;
        t[n] = l;
        m.params.push_back(t);
        m.ids.push_back(id);
    }

    for(const Quad &q : quads) {
        if(!m.QuadGrid(q.face, q.sides)) others.push_back(q.face);
    }
    for(const Ring &r : rings) {
        if(!m.Rings(r.face, r.outer, r.inner, target)) others.push_back(r.face);
    }
    for(const TopoDS_Face &face : others) {
        int wires = 0;
        for(TopExp_Explorer w(face, TopAbs_WIRE); w.More(); w.Next()) wires++;
        if(wires != 1 || !m.Radial(face, target)) m.Triangles(face);
    }

    // Creases: the edges between two faces meeting at an angle (not a seam, not tangent).
    TopTools_IndexedDataMapOfShapeListOfShape facesOf;
    TopExp::MapShapesAndAncestors(shape, TopAbs_EDGE, TopAbs_FACE, facesOf);
    for(int i = 1; i <= m.edges.Extent(); i++) {
        const TopoDS_Edge &edge = TopoDS::Edge(m.edges(i));
        if(BRep_Tool::Degenerated(edge) || !facesOf.Contains(edge)) continue;
        std::vector<TopoDS_Face> faces;
        for(const TopoDS_Shape &f : facesOf.FindFromKey(edge)) {
            bool seen = false;
            for(const TopoDS_Face &g : faces) seen = seen || g.IsSame(f);
            if(!seen) faces.push_back(TopoDS::Face(f));
        }
        if(faces.size() != 2) continue;
        double f, l;
        BRep_Tool::Range(edge, f, l);
        gp_Dir n[2];
        for(int k = 0; k < 2; k++) {
            double               pf, pl;
            Handle(Geom2d_Curve) pc = BRep_Tool::CurveOnSurface(edge, faces[k], pf, pl);
            if(pc.IsNull()) continue;
            BRepAdaptor_Surface s(faces[k]);
            n[k] = QuadMesher::Outward(faces[k], s, pc->Value((f + l) / 2));
        }
        if(n[0].Angle(n[1]) > 20 * PI / 180) m.creases.push_back(m.ids[i - 1]);
    }

    // What came out, and whether it is closed.
    QuadMeshStats st;
    std::map<std::pair<int, int>, int> uses;
    for(const std::vector<int> &p : m.polys) {
        if(p.size() == 3) st.triangles++;
        else if(p.size() == 4) st.quads++;
        else st.ngons++;
        for(size_t k = 0; k < p.size(); k++) {
            int a = p[k], b = p[(k + 1) % p.size()];
            uses[{ std::min(a, b), std::max(a, b) }]++;
        }
        for(size_t k = 1; k + 1 < p.size(); k++) {
            st.volume += m.points[p[0]].Dot(m.points[p[k]].Cross(m.points[p[k + 1]])) / 6;
        }
    }
    for(const auto &u : uses) st.openEdges += u.second != 2;
    st.vertices = (int)m.points.size();
    st.creases  = (int)m.creases.size();
    if(stats) *stats = st;

    FILE *fh = OpenFile(filename, "wb");
    if(!fh) return false;
    fprintf(fh, "#usda 1.0\n(\n    defaultPrim = \"SolveSpace\"\n    metersPerUnit = 0.001\n"
                "    upAxis = \"Z\"\n)\n\ndef Xform \"SolveSpace\"\n{\n"
                "    def Mesh \"Model\"\n    {\n");
    auto ints = [&](const char *name, const std::vector<int> &v) {
        fprintf(fh, "        int[] %s = [", name);
        for(size_t k = 0; k < v.size(); k++) fprintf(fh, "%s%d", k ? ", " : "", v[k]);
        fprintf(fh, "]\n");
    };
    std::vector<int> counts, indices, creaseIds, creaseLengths;
    for(const std::vector<int> &p : m.polys) {
        counts.push_back((int)p.size());
        indices.insert(indices.end(), p.begin(), p.end());
    }
    for(const std::vector<int> &c : m.creases) {
        creaseLengths.push_back((int)c.size());
        creaseIds.insert(creaseIds.end(), c.begin(), c.end());
    }
    ints("faceVertexCounts", counts);
    ints("faceVertexIndices", indices);
    fprintf(fh, "        point3f[] points = [");
    for(size_t k = 0; k < m.points.size(); k++) {
        const Vector &p = m.points[k];
        fprintf(fh, "%s(%.9g, %.9g, %.9g)", k ? ", " : "", p.x, p.y, p.z);
    }
    fprintf(fh, "]\n");
    ints("creaseIndices", creaseIds);
    ints("creaseLengths", creaseLengths);
    fprintf(fh, "        float[] creaseSharpnesses = [");
    for(size_t k = 0; k < creaseLengths.size(); k++) fprintf(fh, "%s10", k ? ", " : "");
    fprintf(fh, "]\n        uniform token orientation = \"rightHanded\"\n"
                "        uniform token subdivisionScheme = \"catmullClark\"\n    }\n}\n");
    fclose(fh);
    return true;
}

}
