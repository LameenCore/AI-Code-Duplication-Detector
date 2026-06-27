#include "tokenizer.h"
#include "json_parser.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cctype>
#include <climits>

void Tokenizer::buildByteEncoder() {
    // RoBERTa/GPT-2's byte-level trick: every one of the 256 possible byte
    // values needs to map to a "visible" character so whitespace, control
    // characters, etc. can sit safely inside a JSON vocab/merge file as
    // ordinary printable text. Printable bytes (33-126, 161-172, 174-255)
    // map to themselves; everything else (space, newline, tab, control
    // chars...) gets shifted up into the 256+ range. This is exactly the
    // formula HuggingFace's bytes_to_unicode() uses.
    auto isPrintable = [](int b) {
        return (b >= 33 && b <= 126) || (b >= 161 && b <= 172) || (b >= 174 && b <= 255);
    };

    int n = 0;
    for (int b = 0; b < 256; b++) {
        unsigned int codepoint;
        if (isPrintable(b)) {
            codepoint = (unsigned int)b;
        } else {
            codepoint = 256 + n;
            n++;
        }

        // UTF-8 encode the codepoint (only ever needs 1 or 2 bytes here,
        // since the highest codepoint we produce is in the 300s).
        std::string out;
        if (codepoint <= 0x7F) {
            out += (char)codepoint;
        } else {
            out += (char)(0xC0 | (codepoint >> 6));
            out += (char)(0x80 | (codepoint & 0x3F));
        }
        byteEncoder[b] = out;
    }
}

std::vector<std::string> Tokenizer::preTokenize(const std::string& text) const {
    // A simplified, ASCII-focused stand-in for the real pre-tokenizer
    // regex (which uses \p{L}/\p{N} Unicode classes we don't have without
    // a regex library that supports them). Since our input is C++ source
    // code -- overwhelmingly ASCII -- isalpha/isdigit covers the cases
    // that matter. Mirrors the real regex's behavior: a run of letters,
    // a run of digits, or a run of "other" punctuation/symbol chars, each
    // optionally preceded by exactly one leading space (extra leading
    // spaces become their own whitespace-only token).
    std::vector<std::string> tokens;
    size_t i = 0, n = text.size();
    static const std::vector<std::string> contractions = {"'s", "'t", "'re", "'ve", "'m", "'ll", "'d"};

    auto isLetter = [](unsigned char c) { return std::isalpha(c) != 0; };
    auto isDigit  = [](unsigned char c) { return std::isdigit(c) != 0; };
    auto isSpace  = [](unsigned char c) { return std::isspace(c) != 0; };

    auto consumeRun = [&](size_t start) -> size_t {
        // start points at the first character to classify (after any
        // optional leading space already accounted for by the caller).
        unsigned char c = (unsigned char)text[start];
        size_t e = start;
        if (isLetter(c)) {
            while (e < n && isLetter((unsigned char)text[e])) e++;
        } else if (isDigit(c)) {
            while (e < n && isDigit((unsigned char)text[e])) e++;
        } else {
            while (e < n && !isSpace((unsigned char)text[e]) &&
                   !isLetter((unsigned char)text[e]) && !isDigit((unsigned char)text[e])) e++;
        }
        return e;
    };

    while (i < n) {
        bool matched = false;
        for (const auto& c : contractions) {
            if (i + c.size() <= n && text.compare(i, c.size(), c) == 0) {
                tokens.push_back(c);
                i += c.size();
                matched = true;
                break;
            }
        }
        if (matched) continue;

        if (isSpace((unsigned char)text[i])) {
            size_t j = i;
            while (j < n && isSpace((unsigned char)text[j])) j++;
            size_t runLen = j - i;

            if (j == n) {
                // Whitespace runs all the way to the end of the text becomes
                // its own token (mirrors \s+(?!\S)).
                tokens.push_back(text.substr(i, runLen));
                i = j;
                continue;
            }
            if (runLen > 1) {
                // All but the last space become their own whitespace token;
                // the last space attaches as the next word's prefix below.
                tokens.push_back(text.substr(i, runLen - 1));
                i = j - 1;
            }
            // text[i] is now a single space immediately followed by content.
            size_t wordStart = i + 1;
            size_t e = consumeRun(wordStart);
            tokens.push_back(text.substr(i, e - i)); // includes the leading space
            i = e;
            continue;
        }

        size_t e = consumeRun(i);
        tokens.push_back(text.substr(i, e - i));
        i = e;
    }

    return tokens;
}

std::vector<std::string> Tokenizer::mapToByteSymbols(const std::string& word) const {
    std::vector<std::string> symbols;
    symbols.reserve(word.size());
    for (unsigned char b : word) {
        symbols.push_back(byteEncoder[b]);
    }
    return symbols;
}

std::vector<std::string> Tokenizer::bpeMerge(std::vector<std::string> symbols) const {
    if (symbols.size() < 2) return symbols;

    while (true) {
        int bestRank = -1;
        std::string bestFirst, bestSecond;
        for (size_t i = 0; i + 1 < symbols.size(); i++) {
            auto it = mergeRank.find(symbols[i] + "\x01" + symbols[i + 1]);
            if (it != mergeRank.end() && (bestRank == -1 || it->second < bestRank)) {
                bestRank = it->second;
                bestFirst = symbols[i];
                bestSecond = symbols[i + 1];
            }
        }
        if (bestRank == -1) break; // no mergeable pair left

        std::vector<std::string> merged;
        merged.reserve(symbols.size());
        size_t i = 0;
        while (i < symbols.size()) {
            if (i + 1 < symbols.size() && symbols[i] == bestFirst && symbols[i + 1] == bestSecond) {
                merged.push_back(bestFirst + bestSecond);
                i += 2;
            } else {
                merged.push_back(symbols[i]);
                i += 1;
            }
        }
        symbols = std::move(merged);
        if (symbols.size() == 1) break;
    }

    return symbols;
}

bool Tokenizer::load(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        std::cerr << "Tokenizer: could not open " << path << "\n";
        return false;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    std::string content = ss.str();

    JsonValue root;
    try {
        root = parseJson(content);
    } catch (const std::exception& e) {
        std::cerr << "Tokenizer: failed to parse JSON: " << e.what() << "\n";
        return false;
    }

    const JsonValue* model = root.find("model");
    if (!model) {
        std::cerr << "Tokenizer: no 'model' key in tokenizer.json\n";
        return false;
    }

    const JsonValue* vocabObj = model->find("vocab");
    if (!vocabObj || !vocabObj->isObject()) {
        std::cerr << "Tokenizer: no 'model.vocab' object\n";
        return false;
    }
    for (const auto& kv : *vocabObj->objVal) {
        vocab[kv.first] = (int64_t)kv.second.numVal;
    }

    const JsonValue* mergesArr = model->find("merges");
    if (!mergesArr || !mergesArr->isArray()) {
        std::cerr << "Tokenizer: no 'model.merges' array\n";
        return false;
    }
    int rank = 0;
    for (const auto& pairVal : *mergesArr->arrVal) {
        if (pairVal.isArray() && pairVal.arrVal->size() == 2) {
            const std::string& a = (*pairVal.arrVal)[0].strVal;
            const std::string& b = (*pairVal.arrVal)[1].strVal;
            mergeRank[a + "\x01" + b] = rank++;
        } else if (pairVal.isString()) {
            // Older tokenizer.json versions store merges as "a b" strings.
            const std::string& s = pairVal.strVal;
            size_t sp = s.find(' ');
            if (sp != std::string::npos) {
                mergeRank[s.substr(0, sp) + "\x01" + s.substr(sp + 1)] = rank++;
            }
        }
    }

    buildByteEncoder();

    auto unkIt = vocab.find("<unk>");
    unkId = (unkIt != vocab.end()) ? unkIt->second : 3;

    std::cout << "Tokenizer loaded: " << vocab.size() << " vocab entries, "
              << mergeRank.size() << " merge rules.\n";

    return true;
}

std::vector<int64_t> Tokenizer::encode(const std::string& text) const {
    std::vector<int64_t> ids;
    ids.push_back(0); // <s>

    for (const auto& word : preTokenize(text)) {
        std::vector<std::string> symbols = mapToByteSymbols(word);
        std::vector<std::string> pieces = bpeMerge(std::move(symbols));
        for (const auto& piece : pieces) {
            auto it = vocab.find(piece);
            ids.push_back(it != vocab.end() ? it->second : unkId);
        }
    }

    ids.push_back(2); // </s>
    return ids;
}
