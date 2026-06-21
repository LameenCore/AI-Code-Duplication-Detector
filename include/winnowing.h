#ifndef WINNOWING_H
#define WINNOWING_H

#include <string>
#include <vector>
#include <set>

std::vector<std::string> tokenize(const std::string& code);
std::set<size_t> getFingerprints(const std::vector<std::string>& tokens, int k, int w);
double fingerprintSimilarity(const std::set<size_t>& fp1, const std::set<size_t>& fp2);

#endif