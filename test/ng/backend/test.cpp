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
