#include "scanner.h"
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

bool shouldIgnore(const std::string& filepath, const std::vector<std::string>& ignorePaths) {
    for (const auto& ignored : ignorePaths) {
        if (filepath.find(ignored) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> scanDirectory(const std::string& path, 
                                        const std::vector<std::string>& ignorePaths) {
    std::vector<std::string> files;

    if (!fs::exists(path)) {
        std::cerr << "Error: Path does not exist: " << path << "\n";
        return files;
    }

    for (const auto& entry : fs::recursive_directory_iterator(path)) {
        if (entry.is_regular_file()) {
            std::string filepath = entry.path().string();
            std::string ext = entry.path().extension().string();

            if (ext == ".cpp" || ext == ".h") {
                if (!shouldIgnore(filepath, ignorePaths)) {
                    files.push_back(filepath);
                }
            }
        }
    }

    return files;
}