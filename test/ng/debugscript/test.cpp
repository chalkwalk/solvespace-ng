#include "solvespace.h"
#include "platform/sdl/debugscript.h"

#include "harness.h"

using namespace SolveSpace::Platform;

TEST_CASE(parse_steps) {
    std::string err;
    auto steps = ParseDebugScript("action ui.command-palette; keys line; key Return;"
                                  " wait 5; shot /tmp/a.png; quit", &err);
    CHECK_TRUE(steps.has_value());
    CHECK_TRUE(steps->size() == 6);
    CHECK_TRUE((*steps)[0].kind == DebugStep::Kind::ACTION);
    CHECK_EQ_STR((*steps)[0].arg, "ui.command-palette");
    CHECK_TRUE((*steps)[1].kind == DebugStep::Kind::KEYS);
    CHECK_EQ_STR((*steps)[1].arg, "line");
    CHECK_TRUE((*steps)[2].kind == DebugStep::Kind::KEY);
    CHECK_TRUE((*steps)[3].kind == DebugStep::Kind::WAIT && (*steps)[3].frames == 5);
    CHECK_TRUE((*steps)[4].kind == DebugStep::Kind::SHOT);
    CHECK_EQ_STR((*steps)[4].arg, "/tmp/a.png");
    CHECK_TRUE((*steps)[5].kind == DebugStep::Kind::QUIT);
}

TEST_CASE(reject_bad_steps) {
    std::string err;
    CHECK_FALSE(ParseDebugScript("frobnicate", &err).has_value());
    CHECK_TRUE(err.find("step 1") != std::string::npos);
    CHECK_FALSE(ParseDebugScript("wait; quit", &err).has_value());
    CHECK_FALSE(ParseDebugScript("wait x", &err).has_value());
    CHECK_TRUE(ParseDebugScript("", &err).has_value());
}
