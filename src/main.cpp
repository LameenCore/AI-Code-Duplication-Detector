#define _GLIBCXX_USE_CXX11_ABI 1
#include <iostream>
#include "scanner.h"
#include "reader.h"
#include "extractor.h"
#include "detector.h"
#include "reporter.h"

int main() {
    std::string path;
    std::cout << "Enter the path to scan: ";
    std::cin >> path;

    // Step 1: Scan
    std::vector<std::string> files = scanDirectory(path);
    std::cout << "\nFound " << files.size() << " file(s).\n";

    // Step 2: Read
    std::map<std::string, std::string> contents = readFiles(files);

    // Step 3: Extract
    std::vector<Function> functions = extractFunctions(contents);
    std::cout << "Extracted " << functions.size() << " function(s).\n";

    // Step 4: Detect
    std::cout << "\nAnalyzing for duplicates...\n";
    std::vector<DuplicatePair> duplicates = detectDuplicates(functions, 70.0);

    // Step 5: Print to terminal
    writeReport(std::cout, duplicates, functions, path);

    // Step 6: Save to file
    saveReportToFile("report.txt", duplicates, functions, path);

    return 0;
}