#include "solvespace.h"
#include "platform/sdl/timerregistry.h"
#include "platform/sdl/dialoghelpers.h"

#include "harness.h"

using namespace SolveSpace::Platform;

class DummyTimer final : public Timer {
public:
    void RunAfter(unsigned) override {}
};

TEST_CASE(timer_registry_rearm_and_cancel) {
    TimerRegistry reg;
    auto t = std::make_shared<DummyTimer>();
    uint64_t first = reg.Arm(t);
    uint64_t second = reg.Arm(t);         // re-armed before first fired
    CHECK_TRUE(first != second);
    reg.Cancel(first);
    CHECK_TRUE(reg.Take(first) == nullptr);
    CHECK_TRUE(reg.Take(second) == t);
    CHECK_TRUE(reg.Take(second) == nullptr); // fires once
    uint64_t third = reg.Arm(t);
    t.reset();                               // owner destroyed before firing
    CHECK_TRUE(reg.Take(third) == nullptr);
}

TEST_CASE(ensure_extension) {
    std::vector<FileFilter> f = { { "SolveSpace models", { "slvs" } },
                                  { "Meshes", { "stl", "obj" } } };
    CHECK_EQ_STR(EnsureExtension("/a/b", f, 1), "/a/b.stl");
    CHECK_EQ_STR(EnsureExtension("/a/b", f, -1), "/a/b.slvs");
    CHECK_EQ_STR(EnsureExtension("/a/b.slvs", f, 1), "/a/b.slvs");
    CHECK_EQ_STR(EnsureExtension("/a.dir/b", f, 0), "/a.dir/b.slvs");
    CHECK_EQ_STR(EnsureExtension("/a/b", {}, 0), "/a/b");
    CHECK_EQ_STR(SdlFilterPattern(f[1]), "stl;obj");
}

#include "platform/sdl/deferredqueue.h"

TEST_CASE(deferred_queue_runs_in_order_and_defers_reposts) {
    DeferredQueue q;
    std::vector<int> order;
    q.Post([&] { order.push_back(1); });
    q.Post([&] {
        order.push_back(2);
        q.Post([&] { order.push_back(3); }); // posted while running: next RunAll
    });
    CHECK_TRUE(order.empty());
    q.RunAll();
    CHECK_TRUE(order.size() == 2 && order[0] == 1 && order[1] == 2);
    CHECK_FALSE(q.Empty());
    q.RunAll();
    CHECK_TRUE(order.size() == 3 && order[2] == 3);
    CHECK_TRUE(q.Empty());
}

TEST_CASE(suggested_save_path_uses_stem_folder_and_filter_extension) {
    std::vector<FileFilter> png = { { "PNG", { "png" } } };
    CHECK_EQ_STR(SuggestedSavePath("/work/model.slvs", "/exports", png), "/exports/model.png");
    CHECK_EQ_STR(SuggestedSavePath("/work/model.slvs", "", png), "/work/model.png");
    CHECK_EQ_STR(SuggestedSavePath("", "/exports", png), "/exports/untitled.png");
    CHECK_EQ_STR(SuggestedSavePath("/work/model.slvs", "", {}), "/work/model");
}
