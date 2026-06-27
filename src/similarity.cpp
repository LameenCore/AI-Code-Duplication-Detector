#include "similarity.h"
#include <cmath>
#include <algorithm>

double cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size() || a.empty()) return 0.0;

    double dot = 0.0, normA = 0.0, normB = 0.0;
    for (size_t i = 0; i < a.size(); i++) {
        dot   += (double)a[i] * (double)b[i];
        normA += (double)a[i] * (double)a[i];
        normB += (double)b[i] * (double)b[i];
    }
    if (normA == 0.0 || normB == 0.0) return 0.0;

    double cos = dot / (std::sqrt(normA) * std::sqrt(normB)); // ranges -1..1
    return ((cos + 1.0) / 2.0) * 100.0; // rescale to 0-100, like --threshold
}

std::vector<Match> rankBySimilarity(const std::vector<float>& query,
                                     const std::vector<std::vector<float>>& candidates) {
    std::vector<Match> matches;
    matches.reserve(candidates.size());
    for (size_t i = 0; i < candidates.size(); i++) {
        matches.push_back({i, cosineSimilarity(query, candidates[i])});
    }
    std::sort(matches.begin(), matches.end(), [](const Match& x, const Match& y) {
        return x.similarity > y.similarity;
    });
    return matches;
}
