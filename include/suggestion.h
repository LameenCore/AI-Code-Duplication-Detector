#pragma once
#include <string>
#include "detector.h"

// Builds a short, human-readable refactoring suggestion for a duplicate
// pair. Wording differs depending on:
//   - isSemantic: token-based matches read as "likely copy-pasted",
//     semantic matches read as "same logic, different code" (CodeBERT
//     caught it despite different names/structure).
//   - whether func1 and func2 live in the same file (a same-file cleanup)
//     or different files (needs a shared header/utility to unify).
std::string buildSuggestion(const DuplicatePair& pair, bool isSemantic);
