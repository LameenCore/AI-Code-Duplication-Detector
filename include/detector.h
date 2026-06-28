#pragma once

#include <vector>
#include "extractor.h"

struct DuplicatePair {
    Function func1;
    Function func2;
    double similarity;
    double zscore = 0.0; // only meaningful for semantic pairs; stays 0 for token-based pairs
};

// Returns all duplicate/similar function pairs
std::vector<DuplicatePair> detectDuplicates(const std::vector<Function>& functions, double threshold = 80.0);