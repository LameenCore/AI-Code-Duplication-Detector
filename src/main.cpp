#include <iostream>
#include "scanner.h"
#include "reader.h"

int main() {
    std::string path;
    std::cout << "Enter the path to scan: ";
    std::cin >> path;

    // Step 1: Scan for files
    std::vector<std::string> files = scanDirectory(path);
    std::cout << "\nFound " << files.size() << " file(s).\n";

    // Step 2: Read file contents
    std::map<std::string, std::string> contents = readFiles(files);

    // Step 3: Print preview of each file
    for (const auto& entry : contents) {
        std::cout << "\n--- " << entry.first << " ---\n";
        // Print only first 3 lines as preview
        std::string content = entry.second;
        int newlines = 0;
        for (char c : content) {
            std::cout << c;
            if (c == '\n') newlines++;
            if (newlines == 3) break;
        }
    }

    return 0;
}