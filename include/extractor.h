#pragma once

#include <string>
#include <vector>
#include <map>

// Stores a single extracted function
struct Function {
    std::string name;
    std::string body;
    std::string filename;
};

// Extracts all functions from a map of filename -> file contents
std::vector<Function> extractFunctions(const std::map<std::string, std::string>& fileContents);