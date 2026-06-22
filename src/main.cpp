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
#include <fstream>
#include <sstream>
#include <filesystem>
#include <set>

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
    std::cout << "  --help              Show this help message\n\n";
    std::cout << "Examples:\n";
    std::cout << "  detector.exe --path ../src\n";
    std::cout << "  detector.exe --path .. --ignore build --ignore vendor\n";
    std::cout << "  detector.exe --path .. --threshold 70 --output results.txt\n\n";
}

int main(int argc, char* argv[]) {
    std::string path = "";
    double threshold = 80.0;
    std::string outputFile = "report.txt";
    std::string htmlOutputFile = "";
    std::string jsonOutputFile = "";
    std::vector<std::string> ignorePaths;
    bool gitOnly = false;

    if (argc == 1) {
        printUsage();
        return 0;
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
        else if (arg == "--clang-test" && i + 1 < argc) {
            std::vector<Function> funcs = extractFunctionsWithClang(argv[i + 1]);
            for (const auto& f : funcs) {
            std::cout << "Function: " << f.name << " (" << f.body.size() << " chars)\n";
            std::cout << "----\n" << f.body << "\n----\n\n";
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
        }
    }

    std::cout << "Found " << files.size() << " file(s).\n";

    // Step 2 & 3: Extract functions using libclang's real AST (replaces brace-counting)
    std::vector<Function> functions = extractFunctionsWithClangMulti(files);
    std::cout << "Extracted " << functions.size() << " function(s).\n\n";

    // Step 4: Detect
    std::cout << "Analyzing for duplicates...\n\n";
    std::vector<DuplicatePair> duplicates = detectDuplicates(functions, threshold);

    // Step 5: Print to terminal
    writeReport(std::cout, duplicates, functions, path);

    // Step 6: Save to file
    saveReportToFile(outputFile, duplicates, functions, path);

    // Step 7: Save HTML report if requested
    if (!htmlOutputFile.empty()) {
        writeHtmlReport(htmlOutputFile, duplicates, functions, path);
    }

    // Step 8: Save JSON report if requested
    if (!jsonOutputFile.empty()) {
        writeJsonReport(jsonOutputFile, duplicates, functions, path);
    }

    return 0;
}