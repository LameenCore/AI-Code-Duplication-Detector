#pragma once

#include <vector>
#include "extractor.h"

struct DuplicatePair {
    Function func1;
    Function func2;
    double similarity;
};

// Returns all duplicate/similar function pairs
std::vector<DuplicatePair> detectDuplicates(const std::vector<Function>& functions, double threshold = 80.0);