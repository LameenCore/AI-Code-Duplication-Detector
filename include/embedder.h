#pragma once
#include <string>
#include <vector>
#include <memory>
#include "tokenizer.h"
#include "onnxruntime_cxx_api.h"

// Holds one loaded CodeBERT ONNX session + one loaded tokenizer, so
// embedding many functions in a row (the real scan pipeline) doesn't
// reload the ~500MB model or re-parse the ~3.5MB tokenizer.json file on
// every single call. load() once, then embed() as many times as you want.
class Embedder {
public:
    bool load(const std::string& modelPath, const std::string& tokenizerPath);
    std::vector<float> embed(const std::string& text) const;

    // Exposed for the debug/test flags that print the token ids as well
    // as the embedding -- the actual scan pipeline doesn't need this.
    std::vector<int64_t> tokenize(const std::string& text) const { return tokenizer.encode(text); }

private:
    Tokenizer tokenizer;
    Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "embedder"};
    std::unique_ptr<Ort::Session> session;
};

// Hardcoded-sentence regression check (unchanged from Day 21).
std::vector<float> embedTest(const std::string& modelPath);

// One-shot text -> tokenizer -> embedding, for the standalone test flags
// (--embed, --similarity-test). Builds a temporary Embedder internally --
// fine for a single call, but the real scan pipeline uses its own
// long-lived Embedder instance instead of this function.
std::vector<float> embedText(const std::string& modelPath,
                              const std::string& tokenizerPath,
                              const std::string& text);
