#include <iostream>
#include "scanner.h"
#include "reader.h"
#include "extractor.h"

int main() {
    std::string path;
    std::cout << "Enter the path to scan: ";
    std::cin >> path;

    // Step 1: Scan for files
    std::vector<std::string> files = scanDirectory(path);
    std::cout << "\nFound " << files.size() << " file(s):\n";
    for (const auto& f : files) {
        std::cout << "  " << f << "\n";
    }

    // Step 2: Read file contents
    std::map<std::string, std::string> contents = readFiles(files);
    std::cout << "\nRead " << contents.size() << " file(s).\n";

    // Step 3: Extract functions
    std::vector<Function> functions = extractFunctions(contents);

    std::cout << "\nExtracted " << functions.size() << " function(s):\n";
    for (const auto& func : functions) {
        std::cout << "  [" << func.filename << "] " << func.name << "\n";
    }

    return 0;
}