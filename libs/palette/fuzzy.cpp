// SPDX-License-Identifier: MIT
#include "fuzzy.h"

#include <algorithm>
#include <cctype>

namespace palette {

static bool IsWordStart(std::string_view text, size_t i) {
    if(i == 0) return true;
    unsigned char p = (unsigned char)text[i - 1];
    // Bytes >= 0x80 (e.g. the UTF-8 "›" path separator) also count as separators.
    return p == ' ' || p == '-' || p == '_' || p == '/' || p == '(' || p >= 0x80;
}

int FuzzyScore(std::string_view query, std::string_view text) {
    if(query.empty()) return 0;
    int score = 0;
    size_t qi = 0;
    size_t lastMatch = std::string_view::npos;
    for(size_t ti = 0; ti < text.size() && qi < query.size(); ti++) {
        if(std::tolower((unsigned char)text[ti]) != std::tolower((unsigned char)query[qi])) {
            continue;
        }
        score += 10;
        if(IsWordStart(text, ti)) score += 15;
        if(lastMatch != std::string_view::npos) {
            if(ti == lastMatch + 1) score += 8;
            else score -= (int)(ti - lastMatch - 1);
        }
        lastMatch = ti;
        qi++;
    }
    return qi == query.size() ? std::max(score, 1) : -1;
}

std::vector<size_t> Rank(std::string_view query, const std::vector<std::string> &items,
                         size_t limit) {
    std::vector<std::pair<int, size_t>> scored;
    for(size_t i = 0; i < items.size(); i++) {
        int s = FuzzyScore(query, items[i]);
        if(s >= 0) scored.push_back({ s, i });
    }
    std::stable_sort(scored.begin(), scored.end(),
                     [](const auto &a, const auto &b) { return a.first > b.first; });
    std::vector<size_t> out;
    for(size_t i = 0; i < scored.size() && i < limit; i++) out.push_back(scored[i].second);
    return out;
}

}
