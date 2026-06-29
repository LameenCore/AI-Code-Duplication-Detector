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

    // recursive_directory_iterator throws if handed a regular file instead
    // of a directory (e.g. --path foo.cpp by mistake) -- catch that here
    // with a clear message instead of letting it crash the whole program.
    if (!fs::is_directory(path)) {
        std::cerr << "Error: Path is not a directory: " << path << "\n";
        return files;
    }

    try {
        // skip_permission_denied keeps one locked-down subfolder from
        // throwing and aborting the entire scan -- it just gets skipped.
        auto options = fs::directory_options::skip_permission_denied;
        for (const auto& entry : fs::recursive_directory_iterator(path, options)) {
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
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error while scanning " << path << ": " << e.what() << "\n";
    }

    return files;
}