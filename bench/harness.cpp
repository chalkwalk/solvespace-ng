//-----------------------------------------------------------------------------
// Our harness for running benchmarks.
//
// Copyright 2016 whitequark
//-----------------------------------------------------------------------------
#include <algorithm>
#include <chrono>
#include <functional>
#include <thread>
#include <string>
#include <vector>

#include "solvespace.h"

using namespace SolveSpace;

static bool RunBenchmark(std::function<void()> setupFn,
                         std::function<bool()> benchFn,
                         std::function<void()> teardownFn,
                         size_t minIter = 5, double minTime = 5.0) {
    // Warmup
    setupFn();
    if(!benchFn()) {
        fprintf(stderr, "Benchmark failed\n");
        return false;
    }
    teardownFn();

    // Benchmark
    size_t iter = 0;
    double time = 0.0;
    while(iter < minIter || time < minTime) {
        setupFn();
        auto testStartTime = std::chrono::steady_clock::now();
        benchFn();
        auto testEndTime = std::chrono::steady_clock::now();
        teardownFn();

        std::chrono::duration<double> testTime = testEndTime - testStartTime;
        time += testTime.count();
        iter += 1;
    }

    // Report
    fprintf(stdout, "Iterations: %zd\n", iter);
    fprintf(stdout, "Time:       %.3f s\n", time);
    fprintf(stdout, "Per iter.:  %.3f s\n", time / (double)iter);

    return true;
}

// Frame times in ms: mean, 95th percentile and worst, against a 60 fps frame (16.7 ms).
static void ReportFrames(const char *what, std::vector<double> ms) {
    std::sort(ms.begin(), ms.end());
    double sum = 0;
    for(double t : ms) sum += t;
    double mean = sum / ms.size(), p95 = ms[std::min(ms.size() - 1, ms.size() * 95 / 100)];
    fprintf(stdout, "%s: %zu frames, mean %.2f ms, p95 %.2f ms, worst %.2f ms (%s 60 fps)\n",
            what, ms.size(), mean, p95, ms.back(), p95 <= 1000.0 / 60 ? "holds" : "misses");
}

static double Ms(std::chrono::steady_clock::time_point a, std::chrono::steady_clock::time_point b) {
    return std::chrono::duration<double, std::milli>(b - a).count();
}

static bool Load(const Platform::Path &filename) {
    SS.Init();
#if defined(HAVE_OCCT)
    SS.kernelKind = KernelKind::OCCT; // as desktop NG
#endif
    if(!SS.LoadFromFile(filename)) return false;
    SS.AfterNewFile();
    SS.GW.activeGroup = SK.groupOrder[SK.groupOrder.n - 1];
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    return true;
}

// The model's side of a frame while a point is dragged (AGENTS.md section 8), as NG does it:
// the point moved, the sketch solved, the solids handed to the worker (worker: false makes
// them on the main thread, as stock SolveSpace does); rendering is left out. Frames are paced
// at 60 fps, so the worker has a real frame's time; then how often the solids were renewed,
// and how long the worker takes to catch up once the drag ends.
static bool BenchDrag(const Platform::Path &filename, hEntity point, int frames, bool worker) {
    if(!Load(filename)) return false;
    Entity *e = SK.entity.FindByIdNoOops(point);
    if(e == nullptr || !e->IsPoint()) {
        fprintf(stderr, "No point %08x to drag\n", point.v);
        return false;
    }
    SS.backgroundRegenEnabled = worker;
    Vector start   = e->PointGetNum();
    int    applied = SS.regenChainsApplied;
    auto   begin   = std::chrono::steady_clock::now();
    std::vector<double> ms;
    for(int i = 0; i < frames; i++) {
        auto t0 = std::chrono::steady_clock::now();
        std::this_thread::sleep_until(begin + std::chrono::microseconds(16667 * i));
        t0 = std::chrono::steady_clock::now();
        // Round a small circle, a turn every 60 frames.
        double a = 2 * PI * i / 60;
        SS.GW.pending.operation = GraphicsWindow::Pending::DRAGGING_POINTS;
        SK.GetEntity(point)->PointForceTo(start.Plus(Vector::From(cos(a), sin(a), 0).ScaledBy(0.5)));
        SS.MarkGroupDirty(SK.GetEntity(point)->group);
        SS.GenerateAll(SolveSpaceUI::Generate::DIRTY);
        SS.PollRegeneration();
        ms.push_back(Ms(t0, std::chrono::steady_clock::now()));
    }
    ReportFrames(worker ? "drag" : "drag without the worker", ms);
    if(worker) {
        double seconds = Ms(begin, std::chrono::steady_clock::now()) / 1000;
        auto   t0      = std::chrono::steady_clock::now();
        SS.GW.pending.operation = GraphicsWindow::Pending::NONE;
        SS.FlushRegeneration();
        fprintf(stdout, "solids renewed %.1f times a second; caught up %.2f ms after the drag\n",
                (SS.regenChainsApplied - applied) / seconds,
                Ms(t0, std::chrono::steady_clock::now()));
    }
    return true;
}

// A mechanism's frame (AGENTS.md section 7): each joint turned or slid through its range, the
// parts' meshes and outlines posed; no kernel work.
static bool BenchPose(const Platform::Path &filename, int frames) {
    if(!Load(filename)) return false;
    if(SK.joint.IsEmpty()) {
        fprintf(stderr, "No joint to move\n");
        return false;
    }
    for(Group &g : SK.group) g.GenerateDisplayItems();
    std::vector<double> ms;
    for(int i = 0; i < frames; i++) {
        auto t0 = std::chrono::steady_clock::now();
        for(Joint &j : SK.joint) {
            if(j.type == Joint::Type::GEAR) continue;
            double lo = j.limited[0] ? j.lo[0] : -180, hi = j.limited[0] ? j.hi[0] : 180;
            SS.SetJointValue(j.h, lo + (hi - lo) * (0.5 + 0.5 * sin(2 * PI * i / 120)),
                             /*remember=*/false);
        }
        size_t triangles = 0;
        for(Group &g : SK.group) triangles += g.ShownMesh().l.n + g.ShownOutlines().l.n;
        if(triangles == 0) return false;
        ms.push_back(Ms(t0, std::chrono::steady_clock::now()));
    }
    ReportFrames("pose", ms);
    return true;
}

int main(int argc, char **argv) {
    std::vector<std::string> args = Platform::InitCli(argc, argv);

    std::string mode;
    Platform::Path filename;
    std::string point = "00090001"; // bench/hinge.slvs: the hole's centre
    if(args.size() == 3 || args.size() == 4) {
        mode = args[1];
        filename = Platform::Path::From(args[2]);
        if(args.size() == 4) point = args[3];
    } else {
        fprintf(stderr, "Usage: %s [mode] [filename] [point]\n", args[0].c_str());
        fprintf(stderr, "Mode can be one of: load, drag (the point, an entity handle in hex), "
                        "pose.\n");
        return 1;
    }

    bool result = false;
    if(mode == "load") {
        result = RunBenchmark(
            [] {
                SS.Init();
            },
            [&] {
                if(!SS.LoadFromFile(filename))
                    return false;
                SS.AfterNewFile();
                return true;
            },
            [] {
                SK.Clear();
                SS.Clear();
            });
    } else if(mode == "drag") {
        hEntity he = { (uint32_t)strtoul(point.c_str(), nullptr, 16) };
        result     = BenchDrag(filename, he, 300, /*worker=*/true) &&
                     BenchDrag(filename, he, 60, /*worker=*/false);
    } else if(mode == "pose") {
        result = BenchPose(filename, 600);
    } else {
        fprintf(stderr, "Unknown mode \"%s\"\n", mode.c_str());
    }

    return (result == true ? 0 : 1);
}
