#pragma once

#include <string>
#include <vector>

// This function takes a folder path and returns all .cpp and .h files it finds
std::vector<std::string> scanDirectory(const std::string& path);