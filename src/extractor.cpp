#include "extractor.h"
#include <iostream>

std::string extractFunctionName(const std::string& line) {
    size_t parenPos = line.find("(");
    if (parenPos == std::string::npos) return "";

    int end = parenPos - 1;
    while (end >= 0 && line[end] == ' ') end--;

    int start = end;
    while (start >= 0 && (isalnum(line[start]) || line[start] == '_')) start--;

    return line.substr(start + 1, end - start);
}

bool isFunctionDefinition(const std::string& line) {
    if (line.find("(") == std::string::npos) return false;
    if (line.find(")") == std::string::npos) return false;

    std::string trimmed = line;
    size_t start = trimmed.find_first_not_of(" \t");
    if (start != std::string::npos) trimmed = trimmed.substr(start);

    if (trimmed.size() >= 2 && trimmed.substr(0, 2) == "if") return false;
    if (trimmed.size() >= 4 && trimmed.substr(0, 4) == "else") return false;
    if (trimmed.size() >= 3 && trimmed.substr(0, 3) == "for") return false;
    if (trimmed.size() >= 5 && trimmed.substr(0, 5) == "while") return false;
    if (trimmed.size() >= 6 && trimmed.substr(0, 6) == "switch") return false;
    if (trimmed.size() >= 6 && trimmed.substr(0, 6) == "return") return false;
    if (trimmed.size() >= 2 && trimmed.substr(0, 2) == "//") return false;

    size_t parenPos = trimmed.find("(");
    if (parenPos == 0) return false;

    return true;
}

std::vector<Function> extractFunctions(const std::map<std::string, std::string>& fileContents) {
    std::vector<Function> functions;

    for (const auto& entry : fileContents) {
        const std::string& filename = entry.first;
        const std::string& content = entry.second;
        size_t length = content.size();
        size_t i = 0;

        while (i < length) {
            size_t bracePos = content.find("{", i);
            if (bracePos == std::string::npos) break;

            size_t lineStart = content.rfind("\n", bracePos);
            lineStart = (lineStart == std::string::npos) ? 0 : lineStart + 1;

            std::string potentialHeader = content.substr(lineStart, bracePos - lineStart);

            std::string trimmed = potentialHeader;
            size_t ts = trimmed.find_first_not_of(" \t");
            if (ts != std::string::npos) trimmed = trimmed.substr(ts);

            if (trimmed.empty() && lineStart > 1) {
                size_t prevLineEnd = lineStart - 1;
                size_t prevLineStart = content.rfind("\n", prevLineEnd - 1);
                prevLineStart = (prevLineStart == std::string::npos) ? 0 : prevLineStart + 1;
                potentialHeader = content.substr(prevLineStart, prevLineEnd - prevLineStart);
                ts = potentialHeader.find_first_not_of(" \t");
                if (ts != std::string::npos) potentialHeader = potentialHeader.substr(ts);
            }

            if (isFunctionDefinition(potentialHeader)) {
                std::string funcName = extractFunctionName(potentialHeader);

                if (!funcName.empty()) {
                    int braceCount = 0;
                    size_t j = bracePos;
                    bool found = false;

                    while (j < length) {
                        if (content[j] == '{') braceCount++;
                        else if (content[j] == '}') braceCount--;

                        if (braceCount == 0) {
                            Function func;
                            func.name = funcName;
                            func.filename = filename;
                            func.body = content.substr(bracePos, j - bracePos + 1);
                            functions.push_back(func);
                            i = j + 1;
                            found = true;
                            break;
                        }
                        j++;
                    }

                    // If no closing brace found, move past this brace to avoid infinite loop
                    if (!found) i = bracePos + 1;
                    continue;
                }
            }
            i = bracePos + 1;
        }
    }

    return functions;
}