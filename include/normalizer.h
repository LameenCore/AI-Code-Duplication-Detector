#pragma once
#include <string>

// Removes comments, extra whitespace and normalizes code for comparison
std::string normalizeCode(const std::string& code);

std::string normalizeVariables(const std::string& code);