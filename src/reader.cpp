#include "reader.h"
#include <fstream>
#include <iostream>
#include <sstream>

std::map<std::string, std::string> readFiles(const std::vector<std::string>& filePaths) {
    std::map<std::string, std::string> fileContents;

    for (const auto& path : filePaths) {
        std::ifstream file(path);

        if (!file.is_open()) {
            std::cerr << "Could not open file: " << path << "\n";
            continue;
        }

        // Read entire file contents into a string
        std::stringstream buffer;
        buffer << file.rdbuf();
        fileContents[path] = buffer.str();

        file.close();
    }

    return fileContents;
}