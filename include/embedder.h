#pragma once
#include <string>
#include <vector>

// Loads the CodeBERT ONNX model and runs inference on a fixed,
// hardcoded token sequence to prove the load+inference pipeline works
// end-to-end. Real tokenization (text -> input_ids) comes in a later day;
// today's ids were generated once by tools/export_codebert.py.
// Returns the 768-dim pooled embedding vector.
std::vector<float> embedTest(const std::string& modelPath);
