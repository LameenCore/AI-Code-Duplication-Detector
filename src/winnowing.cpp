#include "winnowing.h"
#include <regex>
#include <functional>
#include <algorithm>

std::vector<std::string> tokenize(const std::string& code) {
    std::vector<std::string> tokens;
    std::regex tokenRegex("[a-zA-Z_][a-zA-Z0-9_]*|[0-9]+|[+\\-*/=<>!&|]+|[{}();,\\[\\].]");

    auto begin = std::sregex_iterator(code.begin(), code.end(), tokenRegex);
    auto end = std::sregex_iterator();

    for (auto it = begin; it != end; ++it) {
        tokens.push_back(it->str());
    }
    return tokens;
}

std::set<size_t> getFingerprints(const std::vector<std::string>& tokens, int k, int w) {
    std::set<size_t> fingerprints;
    std::hash<std::string> hasher;

    if ((int)tokens.size() < k) return fingerprints;

    // Step 1: hash every k-gram
    std::vector<size_t> hashes;
    for (size_t i = 0; i + k <= tokens.size(); i++) {
        std::string gram;
        for (int j = 0; j < k; j++) gram += tokens[i + j];
        hashes.push_back(hasher(gram));
    }

    // Step 2: winnow - slide a window of size w over the hashes, keep the minimum from each
    if ((int)hashes.size() < w) {
        if (!hashes.empty()) {
            fingerprints.insert(*std::min_element(hashes.begin(), hashes.end()));
        }
        return fingerprints;
    }

    for (size_t i = 0; i + w <= hashes.size(); i++) {
        size_t minHash = hashes[i];
        for (int j = 1; j < w; j++) {
            if (hashes[i + j] < minHash) minHash = hashes[i + j];
        }
        fingerprints.insert(minHash);
    }

    return fingerprints;
}

double fingerprintSimilarity(const std::set<size_t>& fp1, const std::set<size_t>& fp2) {
    if (fp1.empty() && fp2.empty()) return 100.0;
    if (fp1.empty() || fp2.empty()) return 0.0;

    int intersectionCount = 0;
    for (const auto& h : fp1) {
        if (fp2.count(h)) intersectionCount++;
    }

    int unionCount = (int)fp1.size() + (int)fp2.size() - intersectionCount;
    return ((double)intersectionCount / unionCount) * 100.0;
}