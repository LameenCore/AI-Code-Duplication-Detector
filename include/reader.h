#pragma once

#include <string>
#include <map>
#include <vector>

// Reads all files and returns a map of filename -> file contents
std::map<std::string, std::string> readFiles(const std::vector<std::string>& filePaths);