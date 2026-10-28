//-----------------------------------------------------------------------------
// Regenerating solids on a worker thread while dragging (AGENTS.md section 6, M2): the sketch
// solves every frame and its lines move at once; the groups' solids are made on the kernel's
// worker from KernelJobs whose inputs are copied here, and the newest finished chain is applied
// on the main thread. Until then the old solids are drawn as a ghost. A face the worker can't
// name (a new one) sends the work back to the main thread. Saving, exporting, undo and the end
// of a drag wait for the result (FlushRegeneration).
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "kernel/worker.h"

namespace SolveSpace {

namespace {

struct RegenItem {
    hGroup            group;
    KernelJob         job;
    SnapshotFaceNamer namer;
    KernelResult      result;
    // Copies of what the job reads, or the index of an earlier item whose result it reads.
    SBezierLoopSetSet loops      = {};
    SShell            prevShell  = {}, sourceShell = {}, impShell = {};
    SMesh             prevMesh   = {}, sourceMesh  = {}, impMesh  = {};
    int               prevIndex  = -1, sourceIndex = -1;

    explicit RegenItem(SnapshotFaceNamer namer) : namer(std::move(namer)) {}
    ~RegenItem() {
        loops.Clear();
        prevShell.Clear(); sourceShell.Clear(); impShell.Clear();
        prevMesh.Clear(); sourceMesh.Clear(); impMesh.Clear();
        result.Clear();
    }
};

struct RegenChain {
    uint64_t                                generation = 0, epoch = 0;
    KernelKind                              kernel     = KernelKind::SOLVESPACE;
    double                                  chordTol   = 0;
    int                                     maxSegments = 0;
    std::vector<std::unique_ptr<RegenItem>> items;
    bool                                    missed = false;
    SEdgeList                               naked  = {};

    ~RegenChain() { naked.Clear(); }

    void Run() {
        SolveSpaceUI::SetThreadKernelOverrides(chordTol, maxSegments, &naked);
        for(auto &item : items) {
            KernelJob &job = item->job;
            if(job.loops) job.loops = &item->loops;
            if(item->prevIndex >= 0) {
                job.prevShell = &items[item->prevIndex]->result.running.shell;
                job.prevMesh  = &items[item->prevIndex]->result.running.mesh;
                job.prevShape = items[item->prevIndex]->result.running.shape;
            } else {
                job.prevShell = &item->prevShell;
                job.prevMesh  = &item->prevMesh;
            }
            if(job.sourceShell) {
                if(item->sourceIndex >= 0) {
                    job.sourceShell = &items[item->sourceIndex]->result.thisBody.shell;
                    job.sourceMesh  = &items[item->sourceIndex]->result.thisBody.mesh;
                    job.sourceShape = items[item->sourceIndex]->result.thisBody.shape;
                } else {
                    job.sourceShell = &item->sourceShell;
                    job.sourceMesh  = &item->sourceMesh;
                }
            }
            if(job.impShell) {
                job.impShell = &item->impShell;
                job.impMesh  = &item->impMesh;
            }
            Kernel(kernel).Run(job, item->namer, &item->result);
            if(item->namer.missed) {
                missed = true;
                break;
            }
        }
        SolveSpaceUI::SetThreadKernelOverrides(0, 0, nullptr);
        Platform::FreeAllTemporary(); // this thread's arena (kd-trees, BSPs) is done with
    }
};

void CopyLoops(SBezierLoopSetSet *from, SBezierLoopSetSet *to) {
    to->Clear();
    for(SBezierLoopSet &set : from->l) {
        SBezierLoopSet copy = set;
        copy.l = {};
        for(SBezierLoop &loop : set.l) {
            SBezierLoop lc = loop;
            lc.l = {};
            for(SBezier &b : loop.l) lc.l.Add(&b);
            copy.l.Add(&lc);
        }
        to->l.Add(&copy);
    }
}

LatestTaskWorker &Worker() {
    // Never destroyed: the thread may still be running a chain when the app exits.
    static LatestTaskWorker *worker = new LatestTaskWorker();
    return *worker;
}

std::mutex                  gFinishedMutex;
std::shared_ptr<RegenChain> gFinished; // the last chain the worker finished
// Every chain submitted gets the next generation. A synchronous regeneration (undo, say) starts
// a new epoch, and chains of an older epoch are dropped; a chain of this epoch is applied when
// it finishes even if newer ones are on the way, so a long drag still renews the solids.
uint64_t                    gGeneration = 0, gEpoch = 0;

}

bool SolveSpaceUI::BackgroundRegenAllowed() {
    if(!backgroundRegenEnabled || exportMode) return false;
    switch(GW.pending.operation) {
        case GraphicsWindow::Pending::DRAGGING_POINTS:
        case GraphicsWindow::Pending::DRAGGING_NEW_POINT:
        case GraphicsWindow::Pending::DRAGGING_NEW_LINE_POINT:
        case GraphicsWindow::Pending::DRAGGING_NEW_CUBIC_POINT:
        case GraphicsWindow::Pending::DRAGGING_NEW_ARC_POINT:
        case GraphicsWindow::Pending::DRAGGING_NEW_RADIUS:
        case GraphicsWindow::Pending::DRAGGING_RADIUS:
        case GraphicsWindow::Pending::DRAGGING_NORMAL:
        case GraphicsWindow::Pending::GRABBING:
            return true;
        default:
            return false;
    }
}

void SolveSpaceUI::SubmitBackgroundRegen(const std::vector<hGroup> &groups) {
    auto chain         = std::make_shared<RegenChain>();
    chain->generation  = ++gGeneration;
    chain->epoch       = gEpoch;
    chain->kernel      = kernelKind;
    chain->chordTol    = ChordTolMm();
    chain->maxSegments = GetMaxSegments();
    for(hGroup hg : groups) {
        Group *g    = SK.GetGroup(hg);
        auto   item = std::make_unique<RegenItem>(g->FaceSnapshot());
        item->group = hg;
        item->job   = g->BuildKernelJob();
        KernelJob &job = item->job;
        if(job.loops) CopyLoops(job.loops, &item->loops);
        for(size_t i = 0; i < chain->items.size(); i++) {
            Group *earlier = SK.GetGroup(chain->items[i]->group);
            if(job.prevShell == &earlier->runningShell) item->prevIndex = (int)i;
            if(job.sourceShell == &earlier->thisShell) item->sourceIndex = (int)i;
        }
        if(item->prevIndex < 0) {
            item->prevShell.MakeFromCopyOf(job.prevShell);
            item->prevMesh.MakeFromCopyOf(job.prevMesh);
        }
        if(job.sourceShell && item->sourceIndex < 0) {
            item->sourceShell.MakeFromCopyOf(job.sourceShell);
            item->sourceMesh.MakeFromCopyOf(job.sourceMesh);
        }
        if(job.impShell) {
            item->impShell.MakeFromCopyOf(job.impShell);
            item->impMesh.MakeFromCopyOf(job.impMesh);
        }
        g->regenPending = true;
        chain->items.push_back(std::move(item));
    }
    GW.Invalidate(/*clearPersistent=*/true); // the solids turn to ghosts
    std::function<void()> wake = onRegenFinished;
    Worker().Submit([chain, wake] {
        chain->Run();
        {
            std::lock_guard<std::mutex> lock(gFinishedMutex);
            gFinished = chain;
        }
        if(wake) wake();
    });
}

void SolveSpaceUI::DropBackgroundRegen() {
    gEpoch++;
    for(Group &g : SK.group) g.regenPending = false;
    GW.Invalidate(/*clearPersistent=*/true);
}

bool SolveSpaceUI::RegenerationPending() {
    if(Worker().Busy()) return true;
    std::lock_guard<std::mutex> lock(gFinishedMutex);
    return gFinished != nullptr;
}

bool SolveSpaceUI::PollRegeneration() {
    if(!BackgroundRegenAllowed() && Worker().Busy()) Worker().Wait(); // the drag has ended
    std::shared_ptr<RegenChain> chain;
    {
        std::lock_guard<std::mutex> lock(gFinishedMutex);
        chain.swap(gFinished);
    }
    if(!chain || chain->epoch != gEpoch) return false; // the model was made again since
    if(chain->missed) {
        // A face the snapshot couldn't name: make this regeneration on the main thread.
        regenMisses++;
        for(auto &item : chain->items) {
            if(Group *g = SK.group.FindByIdNoOops(item->group)) g->regenPending = false;
        }
        bool allowed           = backgroundRegenEnabled;
        backgroundRegenEnabled = false;
        GenerateAll(Generate::DIRTY);
        backgroundRegenEnabled = allowed;
        return true;
    }
    for(auto &item : chain->items) {
        if(!SK.group.FindByIdNoOops(item->group)) return false; // the model changed under it
    }
    // A newer chain still on the way: these solids are drawn, as ghosts until it lands.
    bool newest = chain->generation == gGeneration;
    for(auto &item : chain->items) {
        Group *g = SK.GetGroup(item->group);
        g->ApplyKernelResult(&item->result);
        g->regenPending = !newest;
        if(newest) g->clean = true;
    }
    nakedEdges.Clear();
    for(const SEdge &e : chain->naked.l) nakedEdges.AddEdge(e.a, e.b);
    regenChainsApplied++;
    GW.Invalidate(/*clearPersistent=*/true);
    return true;
}

void SolveSpaceUI::FlushRegeneration() {
    if(!backgroundRegenEnabled) return;
    Worker().Wait();
    PollRegeneration();
}

}
