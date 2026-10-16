#include "solvespace.h"

#include "harness.h"

TEST_CASE(select_all_delete_undo_roundtrip) {
    CHECK_LOAD("line.slvs");
    int before = SK.request.n;
    CHECK_TRUE(before > 0);
    RUN_COMMAND("select-all");
    RUN_COMMAND("delete");
    CHECK_TRUE(SK.request.n < before);
    RUN_COMMAND("undo");
    CHECK_TRUE(SK.request.n == before);
    CHECK_SAVE("line.slvs");
}

TEST_CASE(delete_with_empty_selection_is_harmless) {
    CHECK_LOAD("line.slvs");
    int before = SK.request.n;
    RUN_COMMAND("unselect-all");
    RUN_COMMAND("delete");
    CHECK_TRUE(SK.request.n == before);
}

TEST_CASE(unknown_command_fails_the_check) {
    // RunCommand is called directly (not via the macro) so this test can assert on
    // the failure without aborting itself.
    size_t failsBefore = helper->failCount;
    bool ok = helper->RunCommand(__FILE__, __LINE__, "no-such-command");
    CHECK_FALSE(ok);
    CHECK_TRUE(helper->failCount == failsBefore + 1);
    helper->failCount = failsBefore; // the expected failure must not fail the suite
}
