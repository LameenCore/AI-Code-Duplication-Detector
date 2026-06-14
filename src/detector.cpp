#include "detector.h"
#include "normalizer.h"
#include <algorithm>
#include <iostream>
#include <unordered_map>

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

// Calculates similarity percentage between two normalized strings
double calculateSimilarity(const std::string& s1, const std::string& s2) {
    if (s1.empty() && s2.empty()) return 100.0;
    if (s1.empty() || s2.empty()) return 0.0;

    std::string a = normalizeCode(s1.substr(0, std::min((int)s1.size(), 500)));
    std::string b = normalizeCode(s2.substr(0, std::min((int)s2.size(), 500)));

    int distance = levenshteinDistance(a, b);
    int maxLen = std::max(a.size(), b.size());

    if (maxLen == 0) return 100.0;
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
    // Only run on functions that didn't get an exact match
    for (int i = 0; i < (int)functions.size(); i++) {
        if (exactMatched[i]) continue;

        for (int j = i + 1; j < (int)functions.size(); j++) {
            if (exactMatched[j]) continue;

            if (functions[i].filename == functions[j].filename &&
                functions[i].name == functions[j].name) continue;

            double similarity = calculateSimilarity(functions[i].body, functions[j].body);

            if (similarity >= threshold) {
                DuplicatePair pair;
                pair.func1 = functions[i];
                pair.func2 = functions[j];
                pair.similarity = similarity;
                duplicates.push_back(pair);
            }
        }
    }

    // Sort by similarity highest first
    std::sort(duplicates.begin(), duplicates.end(), [](const DuplicatePair& a, const DuplicatePair& b) {
        return a.similarity > b.similarity;
    });

    return duplicates;
}