#ifndef CLANG_EXTRACTOR_H
#define CLANG_EXTRACTOR_H

#include <string>
#include <vector>
#include "extractor.h"

std::vector<Function> extractFunctionsWithClang(const std::string& filePath);
std::vector<Function> extractFunctionsWithClangMulti(const std::vector<std::string>& filePaths);

#endif