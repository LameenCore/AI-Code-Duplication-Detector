#include "clang_extractor.h"
#include <clang-c/Index.h>
#include <iostream>

CXChildVisitResult visitor(CXCursor cursor, CXCursor parent, CXClientData clientData) {
    if (clang_getCursorKind(cursor) == CXCursor_FunctionDecl) {
        CXString name = clang_getCursorSpelling(cursor);
        CXSourceLocation location = clang_getCursorLocation(cursor);

        CXFile file;
        unsigned line, column;
        clang_getSpellingLocation(location, &file, &line, &column, nullptr);

        std::cout << "Found function: " << clang_getCString(name)
                   << " at line " << line << std::endl;

        clang_disposeString(name);
    }
    return CXChildVisit_Recurse;
}

void extractFunctionsWithClang(const std::string& filePath) {
    CXIndex index = clang_createIndex(0, 0);

    CXTranslationUnit unit = clang_parseTranslationUnit(
        index,
        filePath.c_str(),
        nullptr, 0,
        nullptr, 0,
        CXTranslationUnit_None
    );

    if (unit == nullptr) {
        std::cerr << "Error: failed to parse " << filePath << std::endl;
        return;
    }

    CXCursor rootCursor = clang_getTranslationUnitCursor(unit);
    clang_visitChildren(rootCursor, visitor, nullptr);

    clang_disposeTranslationUnit(unit);
    clang_disposeIndex(index);
}