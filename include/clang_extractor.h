#ifndef CLANG_EXTRACTOR_H
#define CLANG_EXTRACTOR_H

#include <string>
#include <vector>
#include "extractor.h"

std::vector<Function> extractFunctionsWithClang(const std::string& filePath);

#endif