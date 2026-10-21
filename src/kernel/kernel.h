//-----------------------------------------------------------------------------
// The geometry kernel behind a group's solid (AGENTS.md section 6). A group captures what its
// solid is made from in a KernelJob, its inputs as values (no reads of the sketch while the
// kernel works), and the kernel returns the group's own body and the running body (the
// previous groups' solid combined with it). SolveSpaceKernel (solvespacekernel.cpp) is
// SolveSpace's own NURBS and mesh code; an OCCT kernel joins it in M3.
//
// Included by sketch.h after the handle types; uses the srf/ and polygon types.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_KERNEL_KERNEL_H
#define SOLVESPACE_KERNEL_KERNEL_H

#include <vector>

// A solid: a NURBS shell, a triangle mesh (imported, or a group forced to a mesh), or both.
struct Body {
    SShell shell         = {};
    SMesh  mesh          = {};
    bool   booleanFailed = false;

    void Clear() {
        shell.Clear();
        mesh.Clear();
        booleanFailed = false;
    }
};

enum class KernelCombine { UNION, DIFFERENCE, INTERSECTION, ASSEMBLE };

enum class KernelFeature {
    EXTRUDE, LATHE, REVOLVE, HELIX, PATTERN, LINK, BOOLEAN,
    FILLET, CHAMFER, SHELL, LOFT, SWEEP,
};

struct KernelJob {
    enum class Op { NONE, EXTRUDE, LATHE, REVOLVE, HELIX, STEP_TRANSLATE, STEP_ROTATE, LINKED };
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
    KernelCombine sourceCombine = KernelCombine::UNION; // how the copies join each other

    // LINKED: the imported solid, placed.
    SShell    *impShell = nullptr;
    SMesh     *impMesh  = nullptr;
    Vector     offset   = {};
    Quaternion q        = {};
    double     scale    = 1;

    // Combining with the previous groups' running body.
    bool          forcedToMesh = false; // combine as triangle meshes
    bool          suppress     = false; // contributes nothing
    KernelCombine combine      = KernelCombine::UNION;
    SShell       *prevShell    = nullptr;
    SMesh        *prevMesh     = nullptr;
};

struct KernelResult {
    Body thisBody; // the group's own solid
    Body running;  // the previous groups' solid combined with it

    void Clear() {
        thisBody.Clear();
        running.Clear();
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

// The kernel in use.
IKernel &Kernel();

#endif
