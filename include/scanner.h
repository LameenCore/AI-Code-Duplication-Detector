#pragma once

#include <string>
#include <vector>

// Scans directory and returns all .cpp and .h files, skipping ignored paths
std::vector<std::string> scanDirectory(const std::string& path, 
                                        const std::vector<std::string>& ignorePaths = {});