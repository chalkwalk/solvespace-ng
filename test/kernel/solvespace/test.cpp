// SolveSpaceKernel (src/kernel/kernel.h): a group's solid made from a KernelJob, its inputs
// captured as values, must be the solid the group makes itself.
#include "solvespace.h"
#include "kernel/kernel.h"

#include "harness.h"

static double Volume(SShell *shell, SMesh *mesh) {
    SMesh m = {};
    m.MakeFromCopyOf(mesh);
    shell->TriangulateInto(&m);
    double v = m.CalculateVolume();
    m.Clear();
    return v;
}

TEST_CASE(a_kernel_job_makes_the_groups_own_solid) {
    CHECK_LOAD("extrudes.slvs");
    int compared = 0;
    for(hGroup hg : SK.groupOrder) {
        Group *g = SK.GetGroup(hg);
        if(g->type != Group::Type::EXTRUDE) continue;
        KernelJob    job = g->BuildKernelJob();
        SnapshotFaceNamer namer = g->FaceSnapshot();
        KernelResult r;
        Kernel().Run(job, namer, &r);
        CHECK_FALSE(namer.missed);
        CHECK_TRUE(r.running.shell.surface.n == g->runningShell.surface.n);
        CHECK_TRUE(r.thisBody.shell.surface.n == g->thisShell.surface.n);
        double want = Volume(&g->runningShell, &g->runningMesh);
        CHECK_EQ_EPS(Volume(&r.running.shell, &r.running.mesh), want);
        for(int i = 0; i < g->thisShell.surface.n; i++) {
            CHECK_TRUE(r.thisBody.shell.surface[i].face == g->thisShell.surface[i].face);
        }
        r.Clear();
        compared++;
    }
    CHECK_TRUE(compared == 4);
}

TEST_CASE(features_the_solvespace_kernel_lacks_are_unsupported) {
    CHECK_TRUE(Kernel().Supports(KernelFeature::EXTRUDE));
    CHECK_TRUE(Kernel().Supports(KernelFeature::BOOLEAN));
    for(KernelFeature f : { KernelFeature::FILLET, KernelFeature::CHAMFER, KernelFeature::SHELL,
                            KernelFeature::LOFT, KernelFeature::SWEEP }) {
        CHECK_FALSE(Kernel().Supports(f));
    }
}
