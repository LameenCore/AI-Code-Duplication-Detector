#include "scanner.h"
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

std::vector<std::string> scanDirectory(const std::string& path) {
    std::vector<std::string> files;

    // Check if the path actually exists
    if (!fs::exists(path)) {
        std::cerr << "Error: Path does not exist: " << path << std::endl;
        return files;
    }

    // Walk through every file and subfolder recursively
    for (const auto& entry : fs::recursive_directory_iterator(path)) {
        
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            if (ext == ".cpp" || ext == ".h") {
                files.push_back(entry.path().string());
            }
        }
    }

    return files;
}