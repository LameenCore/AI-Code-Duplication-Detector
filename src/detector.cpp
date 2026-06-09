#include "detector.h"
#include <algorithm>
#include <iostream>

// Calculates Levenshtein distance between two strings
int levenshteinDistance(const std::string& s1, const std::string& s2) {
    int m = s1.size();
    int n = s2.size();

    // Create a 2D table
    std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1));

    // Fill base cases
    for (int i = 0; i <= m; i++) dp[i][0] = i;
    for (int j = 0; j <= n; j++) dp[0][j] = j;

    // Fill the rest of the table
    for (int i = 1; i <= m; i++) {
        for (int j = 1; j <= n; j++) {
            if (s1[i-1] == s2[j-1]) {
                dp[i][j] = dp[i-1][j-1]; // characters match, no change needed
            } else {
                dp[i][j] = 1 + std::min({
                    dp[i-1][j],   // deletion
                    dp[i][j-1],   // insertion
                    dp[i-1][j-1]  // substitution
                });
            }
        }
    }

    return dp[m][n];
}

// Calculates similarity percentage between two strings
double calculateSimilarity(const std::string& s1, const std::string& s2) {
    if (s1.empty() && s2.empty()) return 100.0;
    if (s1.empty() || s2.empty()) return 0.0;

    // For large functions, only compare first 500 chars for performance
    std::string a = s1.substr(0, std::min((int)s1.size(), 500));
    std::string b = s2.substr(0, std::min((int)s2.size(), 500));

    int distance = levenshteinDistance(a, b);
    int maxLen = std::max(a.size(), b.size());

    return (1.0 - (double)distance / maxLen) * 100.0;
}

std::vector<DuplicatePair> detectDuplicates(const std::vector<Function>& functions, double threshold) {
    std::vector<DuplicatePair> duplicates;

    std::hash<std::string> hasher;

    for (int i = 0; i < (int)functions.size(); i++) {
        for (int j = i + 1; j < (int)functions.size(); j++) {
            const Function& f1 = functions[i];
            const Function& f2 = functions[j];

            // Skip if same file and same name
            if (f1.filename == f2.filename && f1.name == f2.name) continue;

            // Quick hash check first — if identical, similarity is 100%
            size_t hash1 = hasher(f1.body);
            size_t hash2 = hasher(f2.body);

            double similarity = 0.0;

            if (hash1 == hash2) {
                similarity = 100.0;
            } else {
                similarity = calculateSimilarity(f1.body, f2.body);
            }

            if (similarity >= threshold) {
                DuplicatePair pair;
                pair.func1 = f1;
                pair.func2 = f2;
                pair.similarity = similarity;
                duplicates.push_back(pair);
            }
        }
    }

    // Sort by similarity, highest first
    std::sort(duplicates.begin(), duplicates.end(), [](const DuplicatePair& a, const DuplicatePair& b) {
        return a.similarity > b.similarity;
    });

    return duplicates;
}