#include <iostream>
#include <string>
#include <vector>
#include "scanner.h"
#include "reader.h"
#include "extractor.h"
#include "detector.h"
#include "reporter.h"
#include "clang_extractor.h"
#include "winnowing.h"
#include "gitscanner.h"
#include "config.h"
#include "onnxruntime_cxx_api.h"
#include "embedder.h"
#include "tokenizer.h"
#include "similarity.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <set>
#include <cmath>

namespace fs = std::filesystem;

void printUsage() {
    std::cout << "\nUsage:\n";
    std::cout << "  detector.exe --path <directory> [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --path <dir>        Path to scan (required)\n";
    std::cout << "  --threshold <num>   Similarity threshold 0-100 (default: 80)\n";
    std::cout << "  --output <file>     Output report filename (default: report.txt)\n";
    std::cout << "  --html <file>       Save report as HTML\n";
    std::cout << "  --json <file>       Save report as JSON\n";
    std::cout << "  --ignore <dir>      Folder to ignore (can be used multiple times)\n";
    std::cout << "  --git-only          Only scan files tracked by git (skips ignored/build files)\n";
    std::cout << "  --config <file>     Load default settings from a key=value config file\n";
    std::cout << "  --model <onnx>      CodeBERT ONNX model -- enables semantic duplicate detection\n";
    std::cout << "  --tokenizer <json>  tokenizer.json to go with --model\n";
    std::cout << "  --semantic-zscore <num>      Flag pairs more than this many standard deviations\n";
    std::cout << "                               above the codebase's own mean similarity (default: 2.5)\n";
    std::cout << "  --onnx-test         Smoke-test the ONNX Runtime setup (prints version)\n";
    std::cout << "  --embed-test <onnx> Run CodeBERT inference on a fixed test sentence\n";
    std::cout << "  --tokenize-test <tokenizer.json>   Encode a fixed test sentence, print ids\n";
    std::cout << "  --embed <onnx> <tokenizer.json> <text>   Real text -> tokenizer -> embedding\n";
    std::cout << "  --similarity-test <onnx> <tokenizer.json>   Embed 3 sample snippets, print pairwise cosine similarity\n";
    std::cout << "  --help              Show this help message\n\n";
    std::cout << "Examples:\n";
    std::cout << "  detector.exe --path ../src\n";
    std::cout << "  detector.exe --path .. --ignore build --ignore vendor\n";
    std::cout << "  detector.exe --path .. --threshold 70 --output results.txt\n";
    std::cout << "  detector.exe --config myconfig.txt\n";
    std::cout << "  detector.exe --config myconfig.txt --threshold 90   (CLI flag overrides config)\n\n";
}

int main(int argc, char* argv[]) {
    std::string path = "";
    double threshold = 80.0;
    std::string outputFile = "report.txt";
    std::string htmlOutputFile = "";
    std::string jsonOutputFile = "";
    std::vector<std::string> ignorePaths;
    bool gitOnly = false;
    std::string modelPath = "";
    std::string tokenizerPath = "";
    double semanticZScore = 2.5;

    if (argc == 1) {
        printUsage();
        return 0;
    }

    // Pre-pass: look for --config first, before any other flags are
    // processed. Whatever it sets becomes the new "default" — the main
    // parsing loop below still runs afterward, so any matching CLI flag
    // typed alongside --config will overwrite the config file's value.
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            loadConfig(argv[i + 1], path, threshold, outputFile,
                       htmlOutputFile, jsonOutputFile, ignorePaths, gitOnly);
            break;
        }
    }

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--help") {
            printUsage();
            return 0;
        }
        else if (arg == "--path" && i + 1 < argc) {
            path = argv[i + 1];
            i++;
        }
        else if (arg == "--threshold" && i + 1 < argc) {
            try {
                threshold = std::stod(argv[i + 1]);
                if (threshold < 0 || threshold > 100) {
                    std::cerr << "Error: threshold must be between 0 and 100\n";
                    return 1;
                }
            } catch (...) {
                std::cerr << "Error: invalid threshold value\n";
                return 1;
            }
            i++;
        }
        else if (arg == "--output" && i + 1 < argc) {
            outputFile = argv[i + 1];
            i++;
        }
        else if (arg == "--html" && i + 1 < argc) {
            htmlOutputFile = argv[i + 1];
            i++;
        }
        else if (arg == "--json" && i + 1 < argc) {
            jsonOutputFile = argv[i + 1];
            i++;
        }
        else if (arg == "--ignore" && i + 1 < argc) {
            ignorePaths.push_back(argv[i + 1]);
            i++;
        }
        else if (arg == "--git-only") {
            gitOnly = true;
        }
        else if (arg == "--model" && i + 1 < argc) {
            modelPath = argv[i + 1];
            i++;
        }
        else if (arg == "--tokenizer" && i + 1 < argc) {
            tokenizerPath = argv[i + 1];
            i++;
        }
        else if (arg == "--semantic-zscore" && i + 1 < argc) {
            try {
                semanticZScore = std::stod(argv[i + 1]);
            } catch (...) {
                std::cerr << "Error: invalid semantic-zscore value\n";
                return 1;
            }
            i++;
        }
        else if (arg == "--config" && i + 1 < argc) {
            i++; // already handled in the pre-pass above
        }
        else if (arg == "--clang-test" && i + 1 < argc) {
            std::vector<Function> funcs = extractFunctionsWithClang(argv[i + 1]);
            for (const auto& f : funcs) {
            std::cout << "Function: " << f.name << " (" << f.body.size() << " chars)\n";
            std::cout << "----\n" << f.body << "\n----\n\n";
            }
            return 0;
        }
        else if (arg == "--onnx-test") {
            try {
                Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "onnx-test");
                std::cout << "ONNX Runtime initialized successfully.\n";
                std::cout << "ONNX Runtime version: " << Ort::GetVersionString() << "\n";
            } catch (const Ort::Exception& e) {
                std::cerr << "ONNX Runtime error: " << e.what() << "\n";
                return 1;
            }
            return 0;
        }
        else if (arg == "--embed-test" && i + 1 < argc) {
            std::string modelPath = argv[i + 1];
            i++;
            try {
                embedTest(modelPath);
            } catch (const Ort::Exception& e) {
                std::cerr << "ONNX Runtime error: " << e.what() << "\n";
                return 1;
            }
            return 0;
        }
        else if (arg == "--tokenize-test" && i + 1 < argc) {
            std::string tokenizerPath = argv[i + 1];
            i++;
            Tokenizer tok;
            if (!tok.load(tokenizerPath)) {
                return 1;
            }
            std::string sample = "int add(int a, int b) { return a + b; }";
            std::vector<int64_t> ids = tok.encode(sample);
            std::cout << "Encoded \"" << sample << "\" into " << ids.size() << " ids:\n[";
            for (size_t k = 0; k < ids.size(); k++) {
                std::cout << ids[k];
                if (k + 1 < ids.size()) std::cout << ", ";
            }
            std::cout << "]\n";
            std::cout << "Ground truth (from tools/export_codebert.py):\n";
            std::cout << "[0, 2544, 1606, 1640, 2544, 10, 6, 6979, 741, 43, 25522, 671, 10, 2055, 741, 131, 35524, 2]\n";
            return 0;
        }
        else if (arg == "--embed" && i + 3 < argc) {
            std::string modelPath = argv[i + 1];
            std::string tokenizerPath = argv[i + 2];
            std::string text = argv[i + 3];
            i += 3;
            try {
                embedText(modelPath, tokenizerPath, text);
            } catch (const Ort::Exception& e) {
                std::cerr << "ONNX Runtime error: " << e.what() << "\n";
                return 1;
            }
            return 0;
        }
        else if (arg == "--similarity-test" && i + 2 < argc) {
            std::string modelPath = argv[i + 1];
            std::string tokenizerPath = argv[i + 2];
            i += 2;

            // Three snippets: the first two are the *same logic* under
            // different names (the kind of thing an AI assistant might
            // generate twice with slightly different wording); the third
            // is genuinely unrelated. A good semantic similarity check
            // should rank the first pair much higher than either against
            // the third.
            std::vector<std::pair<std::string, std::string>> samples = {
                {"add(a,b)",   "int add(int a, int b) { return a + b; }"},
                {"sum(x,y)",   "int sum(int x, int y) { return x + y; }"},
                {"printHello", "void printHello() { std::cout << \"hello\"; }"}
            };

            // One Embedder, loaded once, reused for all three snippets --
            // unlike embedText() (used by the standalone --embed flag),
            // this doesn't reopen the ~500MB model each time.
            std::vector<std::vector<float>> embeddings;
            try {
                Embedder embedder;
                if (!embedder.load(modelPath, tokenizerPath)) {
                    return 1;
                }
                for (const auto& s : samples) {
                    std::cout << "\nEmbedding \"" << s.first << "\": " << s.second << "\n";
                    embeddings.push_back(embedder.embed(s.second));
                }
            } catch (const Ort::Exception& e) {
                std::cerr << "ONNX Runtime error: " << e.what() << "\n";
                return 1;
            }

            std::cout << "\nPairwise cosine similarity:\n";
            for (size_t a = 0; a < samples.size(); a++) {
                for (size_t b = a + 1; b < samples.size(); b++) {
                    double sim = cosineSimilarity(embeddings[a], embeddings[b]);
                    std::cout << "  " << samples[a].first << " vs " << samples[b].first
                              << ": " << sim << "%\n";
                }
            }
            return 0;
        }
        else if (arg == "--winnow-test" && i + 2 < argc) {
            std::string file1 = argv[i + 1];
            std::string file2 = argv[i + 2];
            i += 2;

            std::ifstream f1(file1, std::ios::binary);
            std::ifstream f2(file2, std::ios::binary);
            std::stringstream ss1, ss2;
            ss1 << f1.rdbuf();
            ss2 << f2.rdbuf();

            std::vector<std::string> tokens1 = tokenize(ss1.str());
            std::vector<std::string> tokens2 = tokenize(ss2.str());

            std::set<size_t> fp1 = getFingerprints(tokens1, 4, 4);
            std::set<size_t> fp2 = getFingerprints(tokens2, 4, 4);

            double similarity = fingerprintSimilarity(fp1, fp2);

            std::cout << "File 1: " << file1 << " (" << tokens1.size() << " tokens, " << fp1.size() << " fingerprints)\n";
            std::cout << "File 2: " << file2 << " (" << tokens2.size() << " tokens, " << fp2.size() << " fingerprints)\n";
            std::cout << "Fingerprint similarity: " << similarity << "%\n";

            return 0;
        }
        else {
            std::cerr << "Unknown argument: " << arg << "\n";
            printUsage();
            return 1;
        }
    }

    if (path.empty()) {
        std::cerr << "Error: --path is required\n";
        printUsage();
        return 1;
    }

    // Print ignored paths if any
    std::cout << "Scanning: " << path << "\n";
    std::cout << "Threshold: " << threshold << "%\n";
    std::cout << "Output: " << outputFile << "\n";
    if (!ignorePaths.empty()) {
        std::cout << "Ignoring: ";
        for (const auto& ig : ignorePaths) std::cout << ig << " ";
        std::cout << "\n";
    }
    std::cout << "\n";

    // Step 1: Scan with ignore list
    std::vector<std::string> files = scanDirectory(path, ignorePaths);

    // Bail out here instead of limping through an empty scan: with 0 files
    // every downstream number (duplicates, health score) would read as a
    // perfect, clean codebase, which is misleading -- it almost always
    // means a bad --path, an --ignore that swallowed everything, or a
    // folder with no .cpp/.h files at all.
    if (files.empty()) {
        std::cerr << "Error: no .cpp/.h files found at \"" << path
                   << "\". Check that the path exists, is a directory, "
                      "and isn't fully excluded by --ignore.\n";
        return 1;
    }

    // Step 1b: If --git-only was requested, narrow the list down to
    // only files git is actually tracking in this repo.
    if (gitOnly) {
        if (!isGitRepo(path)) {
            std::cerr << "Warning: --git-only was given but " << path
                       << " is not inside a git repository. Scanning all files instead.\n";
        } else {
            std::vector<std::string> tracked = getGitTrackedFiles(path);
            std::set<std::string> trackedSet(tracked.begin(), tracked.end());

            std::vector<std::string> filtered;
            for (const auto& f : files) {
                std::string relPath = fs::relative(f, path).generic_string();
                if (trackedSet.count(relPath)) {
                    filtered.push_back(f);
                }
            }
            files = filtered;
            std::cout << "Git-aware mode: " << files.size() << " of the scanned files are tracked by git.\n";

            // Same reasoning as the earlier empty-files check: if --git-only
            // filtered everything out (e.g. scanning outside a repo's
            // tracked set), don't continue into a fake "clean" report.
            if (files.empty()) {
                std::cerr << "Error: --git-only left 0 files to scan. "
                              "Nothing in \"" << path << "\" is tracked by git.\n";
                return 1;
            }
        }
    }

    std::cout << "Found " << files.size() << " file(s).\n";

    // Step 2 & 3: Extract functions using libclang's real AST (replaces brace-counting)
    std::vector<Function> functions = extractFunctionsWithClangMulti(files);
    std::cout << "Extracted " << functions.size() << " function(s).\n\n";

    // Files found but zero functions parsed out of them -- not necessarily
    // wrong (a folder of pure declaration headers could legitimately have
    // none), but the health score and duplicate counts below would read as
    // a perfect score either way, so call out that it's not a sign of a
    // clean codebase, just an empty analysis.
    if (functions.empty()) {
        std::cerr << "Warning: 0 functions extracted from " << files.size()
                   << " file(s). The report below will look \"clean\" but "
                      "that's because there was nothing to analyze -- not "
                      "because the codebase has no duplicates.\n\n";
    }

    // Step 4: Detect (token-based -- catches copy-pasted/lightly-edited code)
    std::cout << "Analyzing for duplicates...\n\n";
    std::vector<DuplicatePair> duplicates = detectDuplicates(functions, threshold);

    // Declared out here (not inside the if-block below) so Steps 5-8 can
    // still see them even when --model/--tokenizer weren't given --
    // semanticEnabled just stays false and semanticDuplicates stays empty.
    std::vector<DuplicatePair> semanticDuplicates;
    bool semanticEnabled = false;

    // Step 4b: Semantic detection (CodeBERT embeddings) -- catches code
    // that was rewritten with different names/structure but means the
    // same thing, which the token-based pass above can't see at all.
    // Opt-in: only runs if both --model and --tokenizer were given, since
    // the model files are large and not part of the repo.
    if (!modelPath.empty() && !tokenizerPath.empty()) {
        std::cout << "Computing semantic embeddings for " << functions.size() << " function(s)...\n";

        Embedder embedder;
        if (!embedder.load(modelPath, tokenizerPath)) {
            std::cerr << "Warning: could not load embedder, skipping semantic detection.\n";
        } else {
            std::vector<std::vector<float>> embeddings;
            embeddings.reserve(functions.size());
            for (size_t fi = 0; fi < functions.size(); fi++) {
                embeddings.push_back(embedder.embed(functions[fi].body));
                if ((fi + 1) % 5 == 0 || fi + 1 == functions.size()) {
                    std::cout << "  embedded " << (fi + 1) << "/" << functions.size() << "\n";
                }
            }

            // Day 24 showed that a fixed cosine-similarity cutoff is
            // useless here: CodeBERT's pooled embeddings are anisotropic,
            // so nearly every pair in a codebase of short, simple
            // functions sits above 99% regardless of actual logic. Fix:
            // compute every pairwise similarity first, then flag only the
            // pairs that are statistical OUTLIERS relative to this
            // codebase's own distribution (z-score), instead of comparing
            // each pair to one global magic number.
            size_t n = functions.size();
            std::vector<double> allSims;
            std::vector<std::pair<size_t, size_t>> allPairs;
            allSims.reserve(n * (n - 1) / 2);
            allPairs.reserve(n * (n - 1) / 2);

            for (size_t a = 0; a < n; a++) {
                for (size_t b = a + 1; b < n; b++) {
                    allSims.push_back(cosineSimilarity(embeddings[a], embeddings[b]));
                    allPairs.push_back({a, b});
                }
            }

            double mean = 0.0;
            for (double s : allSims) mean += s;
            if (!allSims.empty()) mean /= allSims.size();

            double variance = 0.0;
            for (double s : allSims) variance += (s - mean) * (s - mean);
            if (!allSims.empty()) variance /= allSims.size();
            double stddev = std::sqrt(variance);

            std::cout << "\nSemantic similarity stats: mean=" << mean
                      << "%  stddev=" << stddev << "\n";

            for (size_t k = 0; k < allSims.size(); k++) {
                double z = (stddev > 0.0) ? (allSims[k] - mean) / stddev : 0.0;
                if (z >= semanticZScore) {
                    size_t a = allPairs[k].first, b = allPairs[k].second;
                    semanticDuplicates.push_back({functions[a], functions[b], allSims[k], z});
                }
            }
            semanticEnabled = true;

            std::cout << "\n=== Semantic Duplicates (z >= " << semanticZScore << ") ===\n";
            if (semanticDuplicates.empty()) {
                std::cout << "None found.\n";
            } else {
                for (const auto& d : semanticDuplicates) {
                    std::cout << d.func1.name << " (" << d.func1.filename << ") <-> "
                              << d.func2.name << " (" << d.func2.filename << "): "
                              << d.similarity << "% (z=" << d.zscore << ")\n";
                }
            }
            std::cout << "\n";
        }
    }

    // Step 5: Print to terminal
    writeReport(std::cout, duplicates, functions, path, semanticDuplicates, semanticEnabled);

    // Step 6: Save to file
    saveReportToFile(outputFile, duplicates, functions, path, semanticDuplicates, semanticEnabled);

    // Step 7: Save HTML report if requested
    if (!htmlOutputFile.empty()) {
        writeHtmlReport(htmlOutputFile, duplicates, functions, path, semanticDuplicates, semanticEnabled);
    }

    // Step 8: Save JSON report if requested
    if (!jsonOutputFile.empty()) {
        writeJsonReport(jsonOutputFile, duplicates, functions, path, semanticDuplicates, semanticEnabled);
    }

    return 0;
}