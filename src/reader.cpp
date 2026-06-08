#include "reader.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>

std::map<std::string, std::string> readFiles(const std::vector<std::string>& filePaths) {
    std::map<std::string, std::string> fileContents;

    for (const auto& path : filePaths) {
        std::ifstream file(path);

        if (!file.is_open()) {
            std::cerr << "Could not open file: " << path << "\n";
            continue;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();

        // Remove Windows \r characters
        content.erase(std::remove(content.begin(), content.end(), '\r'), content.end());

        fileContents[path] = content;
        file.close();
    }

    return fileContents;
}