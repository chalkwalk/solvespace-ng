#include "harness.h"
#include "fuzzy.h"

TEST_CASE(subsequence_matching) {
    CHECK_TRUE(palette::FuzzyScore("ls", "Line Segment") > 0);
    CHECK_TRUE(palette::FuzzyScore("xyz", "Line Segment") == -1);
    CHECK_TRUE(palette::FuzzyScore("LINE", "line segment") > 0);
    CHECK_TRUE(palette::FuzzyScore("", "anything") == 0);
}

TEST_CASE(word_starts_and_runs_rank_higher) {
    std::vector<std::string> items = { "Export Image...", "Extrude" };
    auto r = palette::Rank("ext", items, 10);
    CHECK_TRUE(r.size() == 2 && r[0] == 1);
    items = { "Sketch › Circle", "Constrain › Equal Length / Radius", "Arc of a Circle" };
    r = palette::Rank("circ", items, 10);
    CHECK_TRUE(r.size() == 2);
    CHECK_TRUE(r[0] == 0);
}

TEST_CASE(rank_limit_and_stability) {
    std::vector<std::string> items = { "aa", "ab", "ac", "zz" };
    auto r = palette::Rank("a", items, 2);
    CHECK_TRUE(r.size() == 2 && r[0] == 0 && r[1] == 1);
    r = palette::Rank("", items, 10);
    CHECK_TRUE(r.size() == 4 && r[3] == 3);
}
