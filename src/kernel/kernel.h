//-----------------------------------------------------------------------------
// The geometry kernel behind a group's solid (AGENTS.md section 6). A group captures what its
// solid is made from in a KernelJob, its inputs as values (no reads of the sketch while the
// kernel works), and the kernel returns the group's own body and the running body (the
// previous groups' solid combined with it). SolveSpaceKernel (solvespacekernel.cpp) is
// SolveSpace's own NURBS and mesh code; OcctKernel (occtkernel.cpp, desktop builds) makes
// Open CASCADE B-reps and hands the rest of SolveSpace their tessellation.
//
// Included by sketch.h after the handle types; uses the srf/ and polygon types.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_KERNEL_KERNEL_H
#define SOLVESPACE_KERNEL_KERNEL_H

#include <memory>
#include <vector>

// A B-rep that only the kernel that made it reads (the OCCT kernel's). Immutable once made, so
// bodies, groups and the worker thread share it.
struct KernelShape;

// A solid: a NURBS shell, a triangle mesh (imported, or a group forced to a mesh), or both.
struct Body {
    SShell shell         = {};
    SMesh  mesh          = {};
    std::shared_ptr<const KernelShape> shape;
    bool   tessellated   = false; // mesh is shape's tessellation: true normals, faces named
    bool   booleanFailed = false;

    void Clear() {
        shell.Clear();
        mesh.Clear();
        shape.reset();
        tessellated   = false;
        booleanFailed = false;
    }
};

enum class KernelCombine { UNION, DIFFERENCE, INTERSECTION, ASSEMBLE };

enum class KernelFeature {
    EXTRUDE, LATHE, REVOLVE, HELIX, PATTERN, LINK, BOOLEAN,
    FILLET, CHAMFER, SHELL, DRAFT, HOLE, LOFT, SWEEP,
};

// A hole's shape (AGENTS.md section 6): plain, counterbored or countersunk; through all (depth
// 0) or to a depth; optionally a modelled ISO metric thread (nominal diameter and pitch), whose
// bore is then the minor diameter. Lengths in mm, the angle in degrees. Zero is "unset" so
// groups that aren't holes save nothing.
enum class HoleKind : uint32_t { SIMPLE = 0, COUNTERBORE = 1, COUNTERSINK = 2 };
struct HoleSpec {
    HoleKind kind         = HoleKind::SIMPLE;
    double   diameter     = 0;
    double   depth        = 0;
    double   headDiameter = 0;
    double   headDepth    = 0;
    double   headAngle    = 0;
    double   thread       = 0;
    double   pitch        = 0;
    int      flip         = 0; // 1: along +normal (an int: zero isn't saved)
    double   clearance    = 0; // added to every diameter, for printing (review 5)

    // The bore's diameter: the thread's minor diameter when threaded; with the clearance.
    double Bore() const {
        return (thread > 0 ? thread - 1.0825 * pitch : diameter) + clearance;
    }
    double Head() const { return headDiameter + clearance; }
    double Major() const { return thread + clearance; } // a thread's major diameter
    // How deep the counterbore's floor or the countersink's narrow end is.
    double SeatDepth() const {
        switch(kind) {
            case HoleKind::COUNTERBORE: return headDepth;
            case HoleKind::COUNTERSINK:
                return std::max(0.0, (Head() - Bore()) / 2 / tan(headAngle * PI / 360));
            case HoleKind::SIMPLE: break;
        }
        return 0;
    }
};

// Which side of the faces a shell's wall goes (the faces' outward side is outside).
enum class ShellSide : uint32_t { INSIDE = 0, OUTSIDE = 1, BOTH = 2 };

struct KernelJob {
    enum class Op {
        NONE, EXTRUDE, LATHE, REVOLVE, HELIX, STEP_TRANSLATE, STEP_ROTATE, LINKED, FILLET, CHAMFER,
        SHELL, DRAFT, HOLE, LOFT, SWEEP
    };
    Op        op    = Op::NONE;
    RgbaColor color = {};

    // EXTRUDE, LATHE, REVOLVE, HELIX: the source sketch's closed loops.
    SBezierLoopSetSet *loops = nullptr;
    // EXTRUDE: the offsets of the bottom and top caps, and the source sketch's line segments
    // (in sketch order), whose extruded sides are named after them.
    Vector tbot = {}, ttop = {};
    struct SideLine {
        hEntity entity;
        Vector  a, b;
    };
    std::vector<SideLine> sideLines;
    // LATHE, REVOLVE, HELIX: the axis, and the angles and distances swept along it.
    Vector axisPoint = {}, axis = {};
    double angles = 0, anglef = 0, dists = 0, distf = 0;

    // STEP_TRANSLATE, STEP_ROTATE: copies of the source group's own body.
    int     steps = 0;
    bool    oneSided = false, skipFirst = false, sourceSuppressed = false;
    Vector  stepTrans = {}, stepAxis = {};
    double  stepTheta = 0;
    SShell *sourceShell = nullptr;
    SMesh  *sourceMesh  = nullptr;
    std::shared_ptr<const KernelShape> sourceShape;
    KernelCombine sourceCombine = KernelCombine::UNION; // how the copies join each other

    // LINKED: the imported solid, placed.
    SShell    *impShell = nullptr;
    SMesh     *impMesh  = nullptr;
    std::shared_ptr<const KernelShape> impShape;
    Vector     offset   = {};
    Quaternion q        = {};
    double     scale    = 1;

    // FILLET, CHAMFER: the previous body with these edges rounded or bevelled: each the
    // curves of an edge entity, matching the B-rep edges that lie on them; the radius or
    // distance.
    std::vector<std::vector<SBezier>> edgeCurves;
    double size = 0;
    // SHELL: the faces left open (face entity handles); the wall thickness is size.
    std::vector<uint32_t> faceNames;
    ShellSide             shellSide = ShellSide::INSIDE;
    // DRAFT: each face (a face entity's handle) tapered by size (radians) about the plane
    // through point normal to pull, along pull; the drafted face is named name.
    struct DraftFace {
        uint32_t face = 0, name = 0;
        Vector   point = {}, pull = {};
    };
    std::vector<DraftFace> draftFaces;
    // HOLE: each hole's centre on the face and direction into the solid (unit), and the name
    // of its seat face; the shape is hole.
    struct HoleAt {
        Vector   point = {}, dir = {};
        uint32_t seat  = 0;
    };
    std::vector<HoleAt> holes;
    HoleSpec            hole;
    // LOFT: the sections' outer loops, in order; SWEEP: the profile's (one). sectionHoles: a
    // profile had holes, which are left out. The end caps are named startName and endName.
    std::vector<std::vector<SBezier>> sections;
    bool                              sectionHoles = false;
    uint32_t                          startName = 0, endName = 0;
    // SWEEP: the path's curves, connected end to end in any order.
    std::vector<SBezier> path;

    // Combining with the previous groups' running body.
    bool          forcedToMesh = false; // combine as triangle meshes
    bool          suppress     = false; // contributes nothing
    KernelCombine combine      = KernelCombine::UNION;
    SShell       *prevShell    = nullptr;
    SMesh        *prevMesh     = nullptr;
    std::shared_ptr<const KernelShape> prevShape;
};

struct KernelResult {
    Body thisBody; // the group's own solid
    Body running;  // the previous groups' solid combined with it
    std::string problem; // why the solid is not as asked (a fillet that failed), or empty

    void Clear() {
        thisBody.Clear();
        running.Clear();
        problem.clear();
    }
};

class IKernel {
public:
    virtual ~IKernel() = default;
    // Makes the job's bodies into *result, naming faces through namer.
    virtual void Run(const KernelJob &job, FaceNamer &namer, KernelResult *result) = 0;
    // Whether the kernel can make that feature; the UI greys out what it can't.
    virtual bool Supports(KernelFeature feature) const = 0;
};

enum class KernelKind : uint32_t { SOLVESPACE = 0, OCCT = 1 };

// The kernel in use (SS.kernelKind), or a given one. Asking for OCCT in a build without it
// gives SolveSpace's.
IKernel &Kernel();
IKernel &Kernel(KernelKind kind);
IKernel &SolveSpaceKernelInstance();
bool KernelAvailable(KernelKind kind);

// Writes a B-rep as a STEP file, scaled by scale; false if there is none or writing failed.
bool KernelExportStep(const std::shared_ptr<const KernelShape> &shape,
                      const Platform::Path &filename, double scale);

// One of the two faces at a straight edge of a B-rep: its name (a face entity's handle, or 0)
// and the direction from the edge into the face, perpendicular to the edge.
struct KernelEdgeFace {
    uint32_t face = 0;
    Vector   into = {};
};
enum class KernelEdgeFacesResult { NOT_FOUND, NOT_PLANAR, FOUND };
// The two faces of shape meeting at the B-rep edge that lies on the segment ab, if both are
// planar.
KernelEdgeFacesResult KernelEdgeFaces(const std::shared_ptr<const KernelShape> &shape, Vector a,
                                      Vector b, KernelEdgeFace out[2]);

// A named planar face of a B-rep: its name (a face entity's handle) and outward normal.
struct KernelFaceNormal {
    uint32_t face = 0;
    Vector   out  = {};
};
std::vector<KernelFaceNormal> KernelFaceNormals(const std::shared_ptr<const KernelShape> &shape);

// A solid's volume (mm^3), surface area (mm^2) and centre of mass; exact when from a B-rep.
struct MassProps {
    double volume = 0, area = 0;
    Vector centre = {};
    bool   exact  = false;
};
// The B-rep's mass properties, exactly; false if there is no B-rep.
bool KernelMassProperties(const std::shared_ptr<const KernelShape> &shape, MassProps *out);

// A STEP file's solid, or null if it can't be read.
std::shared_ptr<const KernelShape> KernelImportStep(const Platform::Path &filename);
// A B-rep's tessellation (as the kernel gives a body's), each triangle in color.
void KernelTessellate(const std::shared_ptr<const KernelShape> &shape, RgbaColor color,
                      SMesh *mesh);

// A quad mesh of a B-rep for Blender (occtquadmesh.cpp, a spike): quad grids on four-sided
// faces, n-gons on the rest, creases on sharp edges, written as USD text; edges cut no longer
// than target. False with no B-rep (or no OCCT).
struct QuadMeshStats {
    int    quads = 0, triangles = 0, ngons = 0, vertices = 0, creases = 0;
    int    openEdges = 0; // polygon edges not shared by exactly two polygons
    double volume    = 0;
};
bool KernelExportQuadUsd(const std::shared_ptr<const KernelShape> &shape,
                         const Platform::Path &filename, double target, QuadMeshStats *stats);

// Combines the job's previous body and result->thisBody as triangle meshes into
// result->running.mesh: the path for meshes and groups forced to a mesh.
void CombineAsMeshes(const KernelJob &job, KernelResult *result);

#endif
