#include <iostream>
#include <iomanip>
#include "scanner.h"
#include "reader.h"
#include "extractor.h"
#include "detector.h"

int main() {
    std::string path;
    std::cout << "Enter the path to scan: ";
    std::cin >> path;

    // Step 1: Scan
    std::vector<std::string> files = scanDirectory(path);
    std::cout << "\nFound " << files.size() << " file(s).\n";

    // Step 2: Read
    std::map<std::string, std::string> contents = readFiles(files);

    // Step 3: Extract functions
    std::vector<Function> functions = extractFunctions(contents);
    std::cout << "Extracted " << functions.size() << " function(s).\n";

    // Step 4: Detect duplicates
    std::cout << "\nAnalyzing for duplicates...\n";
    std::vector<DuplicatePair> duplicates = detectDuplicates(functions, 70.0);

    if (duplicates.empty()) {
        std::cout << "\nNo duplicates found.\n";
    } else {
        std::cout << "\nFound " << duplicates.size() << " duplicate pair(s):\n";
        std::cout << std::string(60, '-') << "\n";

        for (const auto& pair : duplicates) {
            std::cout << std::fixed << std::setprecision(1);
            std::cout << "[" << pair.similarity << "% match]\n";
            std::cout << "  " << pair.func1.filename << " :: " << pair.func1.name << "\n";
            std::cout << "  " << pair.func2.filename << " :: " << pair.func2.name << "\n";
            std::cout << std::string(60, '-') << "\n";
        }
    }

    return 0;
}