#pragma once

#include <string>
#include <vector>

// Reads key=value pairs from a config file and overwrites the given
// settings with whatever is found there. Keys that don't appear in
// the file are left untouched, so the caller's existing defaults survive.
void loadConfig(const std::string& filename,
                 std::string& path,
                 double& threshold,
                 std::string& outputFile,
                 std::string& htmlOutputFile,
                 std::string& jsonOutputFile,
                 std::vector<std::string>& ignorePaths,
                 bool& gitOnly);
