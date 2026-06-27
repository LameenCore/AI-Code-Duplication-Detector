#pragma once
#include <vector>
#include <cstddef>

// Cosine similarity between two equal-length embedding vectors, scaled to
// a 0-100 percentage to match the rest of this project's --threshold
// convention. Cosine similarity measures the angle between two vectors,
// not their length -- which is exactly what we want for embeddings: two
// functions that mean the same thing point in roughly the same direction
// in 768-dim space, even if the model's confidence (vector length) differs.
double cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b);

// Given one query embedding and a list of candidate embeddings, returns
// each candidate's index + similarity score, sorted most-to-least similar.
//
// This is a brute-force stand-in for what a library like Faiss would do:
// Faiss builds an index (e.g. clustering vectors, or an approximate graph)
// so it can find nearest neighbors among millions of vectors without
// comparing every pair. For a single codebase's worth of functions
// (dozens to low hundreds), comparing every pair directly is plenty fast
// and avoids depending on a library with no solid Windows/MinGW build.
struct Match {
    size_t index;
    double similarity;
};
std::vector<Match> rankBySimilarity(const std::vector<float>& query,
                                     const std::vector<std::vector<float>>& candidates);
