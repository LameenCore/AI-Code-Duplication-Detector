#include <iostream>
#include "scanner.h"

int main() {
    std::string path;
    std::cout << "Enter the path to scan: ";
    std::cin >> path;

    std::vector<std::string> files = scanDirectory(path);

    std::cout << "\nFound " << files.size() << " file(s):\n";
    for (const auto& file : files) {
        std::cout << "  " << file << "\n";
    }

    return 0;
}