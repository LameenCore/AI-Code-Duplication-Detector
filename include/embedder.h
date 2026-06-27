#pragma once
#include <string>
#include <vector>

// Loads the CodeBERT ONNX model and runs inference on a fixed, hardcoded
// token sequence (kept as a permanent regression check). Returns the
// 768-dim pooled embedding vector.
std::vector<float> embedTest(const std::string& modelPath);

// Full pipeline: real text -> real tokenizer -> CodeBERT inference.
// Returns the 768-dim pooled embedding vector for the given text.
std::vector<float> embedText(const std::string& modelPath,
                              const std::string& tokenizerPath,
                              const std::string& text);
