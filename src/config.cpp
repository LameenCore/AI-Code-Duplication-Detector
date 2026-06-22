#include "config.h"
#include <fstream>
#include <iostream>

// Trims leading/trailing whitespace from a string
static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

void loadConfig(const std::string& filename,
                 std::string& path,
                 double& threshold,
                 std::string& outputFile,
                 std::string& htmlOutputFile,
                 std::string& jsonOutputFile,
                 std::vector<std::string>& ignorePaths,
                 bool& gitOnly) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Warning: could not open config file: " << filename << "\n";
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);

        // Skip blank lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        size_t eq = line.find('=');
        if (eq == std::string::npos) {
            std::cerr << "Warning: skipping malformed config line: " << line << "\n";
            continue;
        }

        std::string key = trim(line.substr(0, eq));
        std::string value = trim(line.substr(eq + 1));

        if (key == "path") {
            path = value;
        } else if (key == "threshold") {
            try {
                threshold = std::stod(value);
            } catch (...) {
                std::cerr << "Warning: invalid threshold in config: " << value << "\n";
            }
        } else if (key == "output") {
            outputFile = value;
        } else if (key == "html") {
            htmlOutputFile = value;
        } else if (key == "json") {
            jsonOutputFile = value;
        } else if (key == "ignore") {
            ignorePaths.push_back(value);
        } else if (key == "git-only") {
            gitOnly = (value == "true" || value == "1");
        } else {
            std::cerr << "Warning: unknown config key: " << key << "\n";
        }
    }

    std::cout << "Loaded config from: " << filename << "\n";
}
