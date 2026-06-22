#include "gitscanner.h"
#include <cstdio>
#include <array>
#include <iostream>

#ifdef _WIN32
#define POPEN _popen
#define PCLOSE _pclose
#else
#define POPEN popen
#define PCLOSE pclose
#endif

// Runs a shell command as a separate process and returns everything
// it printed to stdout, captured as a single string.
static std::string runCommand(const std::string& cmd) {
    std::array<char, 256> buffer;
    std::string result;

    FILE* pipe = POPEN(cmd.c_str(), "r");
    if (!pipe) {
        return result;
    }

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }

    PCLOSE(pipe);
    return result;
}

bool isGitRepo(const std::string& path) {
    std::string cmd = "git -C \"" + path + "\" rev-parse --is-inside-work-tree 2>&1";
    std::string output = runCommand(cmd);
    return output.find("true") != std::string::npos;
}

std::vector<std::string> getGitTrackedFiles(const std::string& path) {
    std::vector<std::string> files;
    std::string cmd = "git -C \"" + path + "\" ls-files";
    std::string output = runCommand(cmd);

    std::string line;
    for (char c : output) {
        if (c == '\n') {
            if (!line.empty()) {
                files.push_back(line);
            }
            line.clear();
        } else if (c != '\r') {
            line += c;
        }
    }
    if (!line.empty()) {
        files.push_back(line);
    }

    return files;
}
