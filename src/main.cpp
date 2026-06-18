#include <iostream>
#include <string>
#include <vector>
#include "scanner.h"
#include "reader.h"
#include "extractor.h"
#include "detector.h"
#include "reporter.h"
#include "clang_extractor.h"


void printUsage() {
    std::cout << "\nUsage:\n";
    std::cout << "  detector.exe --path <directory> [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --path <dir>        Path to scan (required)\n";
    std::cout << "  --threshold <num>   Similarity threshold 0-100 (default: 80)\n";
    std::cout << "  --output <file>     Output report filename (default: report.txt)\n";
    std::cout << "  --html <file>       Save report as HTML\n";
    std::cout << "  --ignore <dir>      Folder to ignore (can be used multiple times)\n";
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
    std::vector<std::string> ignorePaths;

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
        else if (arg == "--ignore" && i + 1 < argc) {
            ignorePaths.push_back(argv[i + 1]);
            i++;
        }
        else if (arg == "--clang-test" && i + 1 < argc) {
            extractFunctionsWithClang(argv[i + 1]);
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
    std::cout << "Found " << files.size() << " file(s).\n";

    // Step 2: Read
    std::map<std::string, std::string> contents = readFiles(files);

    // Step 3: Extract
    std::vector<Function> functions = extractFunctions(contents);
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

    return 0;
}