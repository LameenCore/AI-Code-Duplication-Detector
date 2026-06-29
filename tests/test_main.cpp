// Minimal test runner -- no external framework (no gtest install needed).
// Each CHECK() call compares actual vs expected, prints PASS/FAIL, and
// tracks counts. main() returns 1 if anything failed, 0 if everything
// passed -- the same convention command-line tools use, so this could
// later be wired into CI.

#include <iostream>
#include <string>
#include <vector>
#include <cmath>

#include "normalizer.h"
#include "similarity.h"
#include "suggestion.h"
#include "detector.h"
#include "extractor.h"

// computeHealthScore/healthGrade live in reporter.cpp but aren't declared
// in reporter.h (writeReport et al. are the only public surface there).
// They already have external linkage, so forward-declaring them here with
// the exact same signature is enough for the linker to find them.
double computeHealthScore(double duplicationRate, size_t semanticCount, bool semanticEnabled);
std::string healthGrade(double score);

static int passCount = 0;
static int failCount = 0;

void check(bool condition, const std::string& description) {
    if (condition) {
        passCount++;
        std::cout << "  [PASS] " << description << "\n";
    } else {
        failCount++;
        std::cout << "  [FAIL] " << description << "\n";
    }
}

bool nearlyEqual(double a, double b, double tolerance = 0.01) {
    return std::fabs(a - b) < tolerance;
}

void testNormalizeCode() {
    std::cout << "normalizeCode:\n";

    std::string input = "  int add(int a, int b) {\n  // adds two numbers\n  return a + b;\n  }\n";
    std::string result = normalizeCode(input);

    check(result.find("//") == std::string::npos, "strips single-line comments");
    check(result.find("  ") == std::string::npos, "collapses repeated whitespace");
    check(!result.empty() && result.front() != ' ' && result.back() != ' ',
          "trims leading/trailing whitespace");

    std::string multiLine = "int x; /* this is\nmulti-line */ int y;";
    check(normalizeCode(multiLine).find("/*") == std::string::npos,
          "strips multi-line comments");
}

void testNormalizeVariables() {
    std::cout << "normalizeVariables:\n";

    std::string code = "int add(int a, int b) { return a + b; }";
    std::string result = normalizeVariables(code);

    check(result.find("VAR_") != std::string::npos,
          "renames identifiers to VAR_N placeholders");
    check(result.find("int") != std::string::npos,
          "keeps C++ keywords ('int') untouched");
    check(result.find("return") != std::string::npos,
          "keeps C++ keywords ('return') untouched");

    // 'n' is the first identifier encountered here, so it becomes VAR_0 --
    // and every later use of 'n' should reuse VAR_0, not get a new number.
    std::string code2 = "int n = 5; return n + n;";
    std::string result2 = normalizeVariables(code2);
    size_t first = result2.find("VAR_0");
    size_t second = result2.find("VAR_0", first + 1);
    size_t third = result2.find("VAR_0", second + 1);
    check(first != std::string::npos && second != std::string::npos && third != std::string::npos,
          "reuses the same placeholder for a repeated identifier ('n' used three times)");
}

void testCosineSimilarity() {
    std::cout << "cosineSimilarity:\n";

    std::vector<float> a = {1.0f, 0.0f};
    std::vector<float> identical = {1.0f, 0.0f};
    std::vector<float> opposite = {-1.0f, 0.0f};
    std::vector<float> orthogonal = {0.0f, 1.0f};

    check(nearlyEqual(cosineSimilarity(a, identical), 100.0),
          "identical vectors score 100%");
    check(nearlyEqual(cosineSimilarity(a, opposite), 0.0),
          "opposite vectors score 0%");
    check(nearlyEqual(cosineSimilarity(a, orthogonal), 50.0),
          "orthogonal vectors score 50%");

    std::vector<float> empty;
    check(nearlyEqual(cosineSimilarity(empty, empty), 0.0),
          "empty vectors return 0 instead of dividing by zero");

    std::vector<float> mismatched = {1.0f, 0.0f, 0.0f};
    check(nearlyEqual(cosineSimilarity(a, mismatched), 0.0),
          "mismatched-length vectors return 0 instead of crashing");
}

void testBuildSuggestion() {
    std::cout << "buildSuggestion:\n";

    Function f1{"trim", "...", "src/config.cpp"};
    Function f2{"trim", "...", "src/normalizer.cpp"};
    DuplicatePair diffFile{f1, f2, 95.6, 0.0};

    std::string s1 = buildSuggestion(diffFile, false);
    check(s1.find("copy-pasted") != std::string::npos,
          "token-based wording mentions copy-pasted");
    check(s1.find("common header") != std::string::npos,
          "different-file pair suggests a shared header/utility");

    Function f3{"writeHtmlReport", "...", "src/reporter.cpp"};
    Function f4{"writeJsonReport", "...", "src/reporter.cpp"};
    DuplicatePair sameFile{f3, f4, 100.0, 0.0};

    std::string s2 = buildSuggestion(sameFile, false);
    check(s2.find("same-file refactor") != std::string::npos,
          "same-file pair calls it a same-file refactor");

    std::string s3 = buildSuggestion(sameFile, true);
    check(s3.find("CodeBERT") != std::string::npos,
          "semantic wording mentions CodeBERT");
}

void testHealthScore() {
    std::cout << "computeHealthScore / healthGrade:\n";

    check(nearlyEqual(computeHealthScore(0.0, 0, false), 100.0),
          "0% duplication, no semantic = 100");
    check(healthGrade(100.0) == "A", "score 100 grades A");
    check(healthGrade(80.0) == "B", "score 80 grades B");
    check(healthGrade(65.0) == "C", "score 65 grades C");
    check(healthGrade(45.0) == "D", "score 45 grades D");
    check(healthGrade(10.0) == "F", "score 10 grades F");

    // Semantic duplicates subtract 5 points each, but only when enabled.
    check(nearlyEqual(computeHealthScore(0.0, 2, true), 90.0),
          "2 semantic duplicates costs 10 points when semantic is enabled");
    check(nearlyEqual(computeHealthScore(0.0, 2, false), 100.0),
          "semantic duplicates ignored when semantic detection is disabled");

    // Score should clamp to [0, 100], never go negative or over 100.
    check(nearlyEqual(computeHealthScore(150.0, 0, false), 0.0),
          "score clamps at 0, never goes negative");
    check(nearlyEqual(computeHealthScore(0.0, 50, true), 0.0),
          "heavy semantic penalty also clamps at 0");
}

int main() {
    testNormalizeCode();
    testNormalizeVariables();
    testCosineSimilarity();
    testBuildSuggestion();
    testHealthScore();

    std::cout << "\n" << passCount << " passed, " << failCount << " failed.\n";
    return failCount == 0 ? 0 : 1;
}
