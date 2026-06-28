#include "suggestion.h"

std::string buildSuggestion(const DuplicatePair& pair, bool isSemantic) {
    bool sameFile = (pair.func1.filename == pair.func2.filename);

    std::string suggestion;
    if (isSemantic) {
        suggestion = "Same logic, different code (CodeBERT flagged this, not just matching tokens). ";
        suggestion += "Consider unifying '" + pair.func1.name + "' and '" + pair.func2.name + "' into one function";
    } else {
        suggestion = "Token-level near-duplicate (likely copy-pasted). ";
        suggestion += "Extract a shared helper for '" + pair.func1.name + "' and '" + pair.func2.name + "'";
    }

    if (sameFile) {
        suggestion += " -- both already live in " + pair.func1.filename + ", so this is a same-file refactor.";
    } else {
        suggestion += " -- move the shared version into a common header/utility so " +
                       pair.func1.filename + " and " + pair.func2.filename + " can both call it.";
    }

    return suggestion;
}
