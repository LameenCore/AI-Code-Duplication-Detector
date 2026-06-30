#include "clang_extractor.h"
#include <clang-c/Index.h>
#include <iostream>
#include <fstream>
#include <sstream>

struct VisitorData {
    std::vector<Function>* functions;
    std::string filename;
    std::string fileContent;
    int skippedCount = 0;
};

CXChildVisitResult visitor(CXCursor cursor, CXCursor parent, CXClientData clientData) {
    VisitorData* data = static_cast<VisitorData*>(clientData);

    if (clang_getCursorKind(cursor) == CXCursor_FunctionDecl && clang_isCursorDefinition(cursor)) {
        CXString name = clang_getCursorSpelling(cursor);

        CXSourceRange extent = clang_getCursorExtent(cursor);
        CXSourceLocation startLoc = clang_getRangeStart(extent);
        CXSourceLocation endLoc = clang_getRangeEnd(extent);

        unsigned startOffset, endOffset;
        clang_getSpellingLocation(startLoc, nullptr, nullptr, nullptr, &startOffset);
        clang_getSpellingLocation(endLoc, nullptr, nullptr, nullptr, &endOffset);

        // On heavily macro-expanded real-world code (e.g. ImGui's IMGUI_API /
        // IM_ASSERT macros), a function's start or end location can resolve
        // through the macro expansion into a *different* file than the one
        // we read into fileContent -- the offset is valid for that other
        // file, not this one, so it can land past fileContent's actual size.
        // substr() would throw std::out_of_range and crash the whole scan
        // over one function; skip it instead and keep going.
        size_t contentSize = data->fileContent.size();
        if (startOffset > contentSize || endOffset > contentSize || endOffset < startOffset) {
            data->skippedCount++;
            clang_disposeString(name);
            return CXChildVisit_Recurse;
        }

        std::string body = data->fileContent.substr(startOffset, endOffset - startOffset);

        Function func;
        func.name = clang_getCString(name);
        func.body = body;
        func.filename = data->filename;

        data->functions->push_back(func);

        clang_disposeString(name);
    }
    return CXChildVisit_Recurse;
}

std::vector<Function> extractFunctionsWithClang(const std::string& filePath) {
    std::vector<Function> functions;

    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: could not open " << filePath << std::endl;
        return functions;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string fileContent = buffer.str();

    CXIndex index = clang_createIndex(0, 0);
    CXTranslationUnit unit = clang_parseTranslationUnit(
        index, filePath.c_str(), nullptr, 0, nullptr, 0, CXTranslationUnit_None
    );

    if (unit == nullptr) {
        std::cerr << "Error: failed to parse " << filePath << std::endl;
        clang_disposeIndex(index);
        return functions;
    }

    VisitorData data;
    data.functions = &functions;
    data.filename = filePath;
    data.fileContent = fileContent;

    CXCursor rootCursor = clang_getTranslationUnitCursor(unit);
    clang_visitChildren(rootCursor, visitor, &data);

    if (data.skippedCount > 0) {
        std::cerr << "Warning: skipped " << data.skippedCount << " function(s) in "
                  << filePath << " with out-of-range source offsets "
                     "(likely macro-expanded signatures) -- not a crash, "
                     "just incomplete coverage for those functions.\n";
    }

    clang_disposeTranslationUnit(unit);
    clang_disposeIndex(index);

    return functions;
}

std::vector<Function> extractFunctionsWithClangMulti(const std::vector<std::string>& filePaths) {
    std::vector<Function> allFunctions;

    for (const auto& path : filePaths) {
        std::vector<Function> fileFunctions = extractFunctionsWithClang(path);
        allFunctions.insert(allFunctions.end(), fileFunctions.begin(), fileFunctions.end());
    }

    return allFunctions;
}