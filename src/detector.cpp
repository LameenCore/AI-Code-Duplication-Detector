#include "detector.h"
#include "normalizer.h"
#include <algorithm>
#include <iostream>
#include <unordered_map>
#include <chrono>

// Calculates Levenshtein distance between two strings
int levenshteinDistance(const std::string& s1, const std::string& s2) {
    int m = s1.size();
    int n = s2.size();

    std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1));

    for (int i = 0; i <= m; i++) dp[i][0] = i;
    for (int j = 0; j <= n; j++) dp[0][j] = j;

    for (int i = 1; i <= m; i++) {
        for (int j = 1; j <= n; j++) {
            if (s1[i-1] == s2[j-1]) {
                dp[i][j] = dp[i-1][j-1];
            } else {
                dp[i][j] = 1 + std::min({
                    dp[i-1][j],
                    dp[i][j-1],
                    dp[i-1][j-1]
                });
            }
        }
    }
    return dp[m][n];
}

// Caps and normalizes a function body ONCE. Pass 2 used to call
// normalizeCode/normalizeVariables on functions[i]'s body again every time
// it showed up in a new pair -- for 1964 functions that's the same body
// re-normalized up to ~1964 times instead of once. Precomputing this per
// function (O(n) total) instead of per pair (O(n^2) total) is what
// actually fixes the slowdown, not just the Levenshtein pruning below.
std::string prepareForComparison(const std::string& body) {
    std::string capped = body.substr(0, std::min((int)body.size(), 500));
    return normalizeVariables(normalizeCode(capped));
}

// Calculates similarity percentage between two ALREADY-normalized strings
// (see prepareForComparison above). threshold lets the caller skip the
// expensive Levenshtein DP table entirely when there's no way the result
// could reach it -- see the length-bound check below.
double calculateSimilarity(const std::string& varNormA, const std::string& varNormB, double threshold = 0.0) {
    if (varNormA.empty() && varNormB.empty()) return 100.0;
    if (varNormA.empty() || varNormB.empty()) return 0.0;

    int maxLen = std::max(varNormA.size(), varNormB.size());
    if (maxLen == 0) return 100.0;

    // Levenshtein distance can never be smaller than the difference in
    // length between the two strings -- you need at least that many
    // insertions/deletions no matter how well the rest lines up. So the
    // best-case similarity (assuming distance == lengthDiff) is an upper
    // bound on the real answer. If even that best case can't reach
    // threshold, running the full O(n*m) DP table would be wasted work --
    // skip it.
    int lengthDiff = std::abs((int)varNormA.size() - (int)varNormB.size());
    double bestCaseSimilarity = (1.0 - (double)lengthDiff / maxLen) * 100.0;
    if (bestCaseSimilarity < threshold) {
        return bestCaseSimilarity;
    }

    int distance = levenshteinDistance(varNormA, varNormB);
    return (1.0 - (double)distance / maxLen) * 100.0;
}

std::vector<DuplicatePair> detectDuplicates(const std::vector<Function>& functions, double threshold) {
    std::vector<DuplicatePair> duplicates;
    std::hash<std::string> hasher;

    // --- PASS 1: Fingerprinting (exact match detection) ---
    // Group functions by their normalized body hash
    std::unordered_map<size_t, std::vector<int>> fingerprints;

    for (int i = 0; i < (int)functions.size(); i++) {
        std::string normalized = normalizeCode(functions[i].body);
        size_t hash = hasher(normalized);
        fingerprints[hash].push_back(i);
    }

    // Track which function indices already have an exact match
    std::vector<bool> exactMatched(functions.size(), false);

    // Any group with more than 1 function = exact duplicates
    for (const auto& entry : fingerprints) {
        const std::vector<int>& group = entry.second;

        if (group.size() > 1) {
            // Compare all pairs within this group
            for (int i = 0; i < (int)group.size(); i++) {
                for (int j = i + 1; j < (int)group.size(); j++) {
                    int idxA = group[i];
                    int idxB = group[j];

                    // Skip same function in same file
                    if (functions[idxA].filename == functions[idxB].filename &&
                        functions[idxA].name == functions[idxB].name) continue;

                    // Double check with direct string comparison (handle hash collisions)
                    std::string normA = normalizeCode(functions[idxA].body);
                    std::string normB = normalizeCode(functions[idxB].body);

                    if (normA == normB) {
                        DuplicatePair pair;
                        pair.func1 = functions[idxA];
                        pair.func2 = functions[idxB];
                        pair.similarity = 100.0;
                        duplicates.push_back(pair);

                        exactMatched[idxA] = true;
                        exactMatched[idxB] = true;
                    }
                }
            }
        }
    }

    // --- PASS 2: Levenshtein (near-duplicate detection) ---
    // Only run on functions that didn't get an exact match.
    // Normalize every body exactly once up front instead of once per pair.
    std::vector<std::string> prepared(functions.size());
    for (int i = 0; i < (int)functions.size(); i++) {
        if (!exactMatched[i]) {
            prepared[i] = prepareForComparison(functions[i].body);
        }
    }

    // Near-duplicate detection on very short functions is noisy: with only
    // a handful of normalized tokens, any tiny difference in body length is
    // a huge percentage of the total, so unrelated one-liners (e.g. ImAbs
    // vs ImTrunc, or two single-line getters like GetHoveredID vs
    // GetCursorStartPos) end up looking like 80%+ matches purely because
    // they're both short -- not because either was copy-pasted from the
    // other. 40 characters wasn't enough to catch single-statement getters;
    // 80 covers most one- and two-line utility functions while still being
    // short enough to leave real near-duplicates (which tend to span
    // several statements) untouched. Exact duplicates of any length are
    // already caught in Pass 1, so skipping short bodies here only drops
    // noisy near-matches, never real copy-paste hits.
    const size_t MIN_COMPARISON_LENGTH = 80;

    auto lastPrint = std::chrono::steady_clock::now();

    for (int i = 0; i < (int)functions.size(); i++) {
        if (exactMatched[i]) continue;
        if (prepared[i].size() < MIN_COMPARISON_LENGTH) continue;

        for (int j = i + 1; j < (int)functions.size(); j++) {
            if (exactMatched[j]) continue;
            if (prepared[j].size() < MIN_COMPARISON_LENGTH) continue;

            if (functions[i].filename == functions[j].filename &&
                functions[i].name == functions[j].name) continue;

            double similarity = calculateSimilarity(prepared[i], prepared[j], threshold);

            if (similarity >= threshold) {
                DuplicatePair pair;
                pair.func1 = functions[i];
                pair.func2 = functions[j];
                pair.similarity = similarity;
                duplicates.push_back(pair);
            }
        }

        // On a large real-world codebase, Pass 2 can take a while -- print
        // a heartbeat every couple seconds so it's clear the scan is still
        // working and hasn't silently hung.
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - lastPrint).count() >= 2) {
            std::cout << "  Pass 2: compared function " << (i + 1) << "/" << functions.size() << "...\n";
            lastPrint = now;
        }
    }

    // Sort by similarity highest first
    std::sort(duplicates.begin(), duplicates.end(), [](const DuplicatePair& a, const DuplicatePair& b) {
        return a.similarity > b.similarity;
    });

    return duplicates;
}