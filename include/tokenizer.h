#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

// A byte-level BPE tokenizer that reads HuggingFace's tokenizer.json format
// (the format tools/export_codebert.py produced) and turns plain text into
// the input_ids CodeBERT expects. This re-implements the same algorithm
// the Python `tokenizers` library uses for RoBERTa/GPT-2-style models:
//
//   1. Pre-tokenize: split text into word-like chunks (a simplified,
//      ASCII-focused stand-in for the real Unicode-aware regex).
//   2. Byte-level remap: every raw byte of a chunk gets replaced by a
//      "visible" stand-in character (this is why you see "Ġword" in
//      vocab dumps -- Ġ is the stand-in for a literal leading space).
//   3. BPE merge: repeatedly combine the highest-priority adjacent pair
//      of symbols (the merge rules loaded from tokenizer.json) until no
//      more merges apply.
//   4. Vocab lookup: map each final piece to its integer id.
//
// The result is wrapped with <s> (id 0) and </s> (id 2) to match the
// format CodeBERT (a RoBERTa-family model) was trained on.
class Tokenizer {
public:
    // Loads vocab + merge rules from a tokenizer.json file. Returns false
    // (and prints an error) if the file can't be read or parsed.
    bool load(const std::string& tokenizerJsonPath);

    // Turns text into CodeBERT input ids, including the <s>/</s> wrapper.
    std::vector<int64_t> encode(const std::string& text) const;

private:
    std::string byteEncoder[256];
    std::unordered_map<std::string, int64_t> vocab;
    std::unordered_map<std::string, int> mergeRank; // key: first + '\x01' + second
    int64_t unkId = 3;

    void buildByteEncoder();
    std::vector<std::string> preTokenize(const std::string& text) const;
    std::vector<std::string> mapToByteSymbols(const std::string& word) const;
    std::vector<std::string> bpeMerge(std::vector<std::string> symbols) const;
};
