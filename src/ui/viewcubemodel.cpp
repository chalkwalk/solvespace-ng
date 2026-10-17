//-----------------------------------------------------------------------------
// The view cube's geometry and picking.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "ui/viewcubemodel.h"

namespace SolveSpace {
namespace Ui {

// Beyond this fraction of a face's half-width, a point picks the edge (or corner) there.
static const double EdgeBand = 0.6;

static const struct {
    const char *label;
    Vector      normal, a, b; // a and b span the face
} Faces[] = {
    { "Front",  { 0, -1, 0 }, { 1, 0, 0 }, { 0, 0, 1 } },
    { "Back",   { 0, 1, 0 },  { 1, 0, 0 }, { 0, 0, 1 } },
    { "Right",  { 1, 0, 0 },  { 0, 1, 0 }, { 0, 0, 1 } },
    { "Left",   { -1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } },
    { "Top",    { 0, 0, 1 },  { 1, 0, 0 }, { 0, 1, 0 } },
    { "Bottom", { 0, 0, -1 }, { 1, 0, 0 }, { 0, 1, 0 } },
};

static Point2d Project(Vector p, Vector right, Vector up, Point2d center, double size) {
    return Point2d::From(center.x + size * p.Dot(right), center.y - size * p.Dot(up));
}

std::vector<CubeFace> ViewCubeFaces(Vector right, Vector up, Point2d center, double size) {
    Vector                toViewer = right.Cross(up);
    std::vector<CubeFace> out;
    for(const auto &f : Faces) {
        CubeFace face;
        face.label   = f.label;
        face.normal  = f.normal;
        face.facing  = f.normal.Dot(toViewer);
        face.visible = face.facing > 1e-6;
        const double su[4] = { -1, 1, 1, -1 }, sv[4] = { -1, -1, 1, 1 };
        for(int i = 0; i < 4; i++) {
            Vector p = f.normal.Plus(f.a.ScaledBy(su[i])).Plus(f.b.ScaledBy(sv[i]));
            face.corners[i] = Project(p, right, up, center, size);
        }
        out.push_back(face);
    }
    return out;
}

// Where p falls on a visible face, in the face's (a, b) coordinates; nothing if outside.
static std::optional<std::pair<double, double>> FaceCoords(const Vector &n, const Vector &a,
                                                           const Vector &b, Vector right,
                                                           Vector up, Point2d center,
                                                           double size, Point2d p) {
    // Screen offset of the face point n + u a + v b, solved for u and v.
    Point2d o  = Project(n, right, up, center, size);
    Point2d ea = Project(n.Plus(a), right, up, center, size).Minus(o);
    Point2d eb = Project(n.Plus(b), right, up, center, size).Minus(o);
    Point2d d  = p.Minus(o);
    double det = ea.x * eb.y - ea.y * eb.x;
    if(fabs(det) < 1e-9) return std::nullopt;
    double u = (d.x * eb.y - d.y * eb.x) / det;
    double v = (ea.x * d.y - ea.y * d.x) / det;
    if(fabs(u) > 1 || fabs(v) > 1) return std::nullopt;
    return std::make_pair(u, v);
}

std::optional<Vector> ViewCubePick(Vector right, Vector up, Point2d center, double size,
                                   Point2d p) {
    Vector toViewer = right.Cross(up);
    for(const auto &f : Faces) {
        if(f.normal.Dot(toViewer) <= 1e-6) continue;
        auto uv = FaceCoords(f.normal, f.a, f.b, right, up, center, size, p);
        if(!uv) continue;
        Vector dir = f.normal;
        if(fabs(uv->first) > EdgeBand) dir = dir.Plus(f.a.ScaledBy(uv->first > 0 ? 1 : -1));
        if(fabs(uv->second) > EdgeBand) dir = dir.Plus(f.b.ScaledBy(uv->second > 0 ? 1 : -1));
        return dir;
    }
    return std::nullopt;
}

std::vector<std::array<Point2d, 4>> ViewCubeRegion(Vector right, Vector up, Point2d center,
                                                   double size, Vector pick) {
    Vector toViewer = right.Cross(up);
    std::vector<std::array<Point2d, 4>> out;
    for(const auto &f : Faces) {
        if(f.normal.Dot(toViewer) <= 1e-6 || pick.Dot(f.normal) < 0.5) continue;
        // Along each face axis: the band at the picked side, or the middle if not picked.
        auto range = [](double c, double *lo, double *hi) {
            if(c > 0.5)       { *lo = EdgeBand; *hi = 1; }
            else if(c < -0.5) { *lo = -1;       *hi = -EdgeBand; }
            else              { *lo = -EdgeBand; *hi = EdgeBand; }
        };
        double u0, u1, v0, v1;
        range(pick.Dot(f.a), &u0, &u1);
        range(pick.Dot(f.b), &v0, &v1);
        const double us[4] = { u0, u1, u1, u0 }, vs[4] = { v0, v0, v1, v1 };
        std::array<Point2d, 4> quad;
        for(int i = 0; i < 4; i++) {
            Vector q = f.normal.Plus(f.a.ScaledBy(us[i])).Plus(f.b.ScaledBy(vs[i]));
            quad[i]  = Project(q, right, up, center, size);
        }
        out.push_back(quad);
    }
    return out;
}
}
}
