// SPDX-License-Identifier: MIT
// Fuzzy subsequence matching for command palettes. Independent of SolveSpace.
#ifndef PALETTE_FUZZY_H
#define PALETTE_FUZZY_H

#include <string>
#include <string_view>
#include <vector>

namespace palette {

// Returns -1 if `query` is not a case-insensitive subsequence of `text`; otherwise a score where
// higher is better. An empty query scores 0.
int FuzzyScore(std::string_view query, std::string_view text);
// Indices of matching items, best first; ties keep their original order.
std::vector<size_t> Rank(std::string_view query, const std::vector<std::string> &items,
                         size_t limit);

}

#endif
