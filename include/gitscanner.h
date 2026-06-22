#pragma once

#include <string>
#include <vector>

// Returns true if the given path is inside a git repository
bool isGitRepo(const std::string& path);

// Returns every file git tracks under the given path, as paths
// relative to that path, in forward-slash form (matching git's own output)
std::vector<std::string> getGitTrackedFiles(const std::string& path);
