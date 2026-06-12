#include "normalizer.h"
#include <sstream>
#include <algorithm>

// Remove single line comments (//)
std::string removeSingleLineComments(const std::string& code) {
    std::string result;
    std::istringstream stream(code);
    std::string line;

    while (std::getline(stream, line)) {
        size_t pos = line.find("//");
        if (pos != std::string::npos) {
            line = line.substr(0, pos);
        }
        result += line + "\n";
    }
    return result;
}

// Remove multi-line comments (/* */)
std::string removeMultiLineComments(const std::string& code) {
    std::string result;
    size_t i = 0;

    while (i < code.size()) {
        // Check for start of multi-line comment
        if (i + 1 < code.size() && code[i] == '/' && code[i+1] == '*') {
            // Skip until we find "*/"
            i += 2;
            while (i + 1 < code.size()) {
                if (code[i] == '*' && code[i+1] == '/') {
                    i += 2;
                    break;
                }
                i++;
            }
        } else {
            result += code[i];
            i++;
        }
    }
    return result;
}

// Collapse all whitespace into single spaces
std::string collapseWhitespace(const std::string& code) {
    std::string result;
    bool lastWasSpace = false;

    for (char c : code) {
        if (c == '\n' || c == '\t' || c == '\r') c = ' ';

        if (c == ' ') {
            if (!lastWasSpace) {
                result += c;
                lastWasSpace = true;
            }
        } else {
            result += c;
            lastWasSpace = false;
        }
    }
    return result;
}

// Trim leading and trailing whitespace
std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

std::string normalizeCode(const std::string& code) {
    std::string result = code;

    // Step 1: Remove multi-line comments first
    result = removeMultiLineComments(result);

    // Step 2: Remove single-line comments
    result = removeSingleLineComments(result);

    // Step 3: Collapse all whitespace
    result = collapseWhitespace(result);

    // Step 4: Trim
    result = trim(result);

    return result;
}