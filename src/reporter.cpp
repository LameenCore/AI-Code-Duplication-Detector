#include "reporter.h"
#include "suggestion.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <ctime>

// Escapes characters that would otherwise break JSON string syntax
// (" and \), plus control characters like newlines and tabs.
std::string escapeJson(const std::string& input) {
    std::string output;
    output.reserve(input.size());
    for (char c : input) {
        switch (c) {
            case '"':  output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:   output += c; break;
        }
    }
    return output;
}

std::string getCurrentTime() {
    std::time_t now = std::time(nullptr);
    char buf[80];
    struct tm timeinfo;
    localtime_s(&timeinfo, &now);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return std::string(buf);
}

void writeReport(std::ostream& out,
                 const std::vector<DuplicatePair>& duplicates,
                 const std::vector<Function>& functions,
                 const std::string& scannedPath,
                 const std::vector<DuplicatePair>& semanticDuplicates,
                 bool semanticEnabled) {

    std::string line(60, '=');

    out << line << "\n";
    out << "   AI-GENERATED CODE DUPLICATION DETECTOR\n";
    out << "   Report generated: " << getCurrentTime() << "\n";
    out << "   Scanned path: " << scannedPath << "\n";
    out << line << "\n\n";

    out << "SUMMARY\n";
    out << std::string(60, '-') << "\n";
    out << "  Total functions scanned    : " << functions.size() << "\n";
    out << "  Duplicate pairs found      : " << duplicates.size() << "\n";

    if (!functions.empty()) {
        double duplicationRate = (double)(duplicates.size() * 2) / functions.size() * 100.0;
        out << std::fixed << std::setprecision(1);
        out << "  Duplication rate           : " << duplicationRate << "%\n";
    }
    if (semanticEnabled) {
        out << "  Semantic duplicates found  : " << semanticDuplicates.size() << "\n";
    }

    out << "\n";

    out << "DUPLICATE PAIRS (token-based)\n";
    out << std::string(60, '-') << "\n\n";

    if (duplicates.empty()) {
        out << "No duplicates found.\n\n";
    } else {
        int count = 1;
        for (const auto& pair : duplicates) {
            out << "[#" << count++ << "] ";
            out << std::fixed << std::setprecision(1);
            out << pair.similarity << "% similarity\n";
            out << "  File 1 : " << pair.func1.filename << "\n";
            out << "  Func 1 : " << pair.func1.name << "\n";
            out << "  File 2 : " << pair.func2.filename << "\n";
            out << "  Func 2 : " << pair.func2.name << "\n";
            out << "  Suggestion : " << buildSuggestion(pair, false) << "\n";
            out << std::string(60, '-') << "\n\n";
        }
    }

    // Only printed when --model/--tokenizer were actually given -- an
    // empty semanticDuplicates vector by itself doesn't distinguish
    // "ran semantic detection, found nothing" from "didn't run it at all".
    if (semanticEnabled) {
        out << "SEMANTIC DUPLICATES (CodeBERT, z-score outliers)\n";
        out << std::string(60, '-') << "\n\n";

        if (semanticDuplicates.empty()) {
            out << "No semantic duplicates found.\n\n";
        } else {
            int count = 1;
            for (const auto& pair : semanticDuplicates) {
                out << "[#" << count++ << "] ";
                out << std::fixed << std::setprecision(1);
                out << pair.similarity << "% similarity (z=" << pair.zscore << ")\n";
                out << "  File 1 : " << pair.func1.filename << "\n";
                out << "  Func 1 : " << pair.func1.name << "\n";
                out << "  File 2 : " << pair.func2.filename << "\n";
                out << "  Func 2 : " << pair.func2.name << "\n";
                out << "  Suggestion : " << buildSuggestion(pair, true) << "\n";
                out << std::string(60, '-') << "\n\n";
            }
        }
    }

    out << line << "\n";
    out << "END OF REPORT\n";
    out << line << "\n";
}

void saveReportToFile(const std::string& filename,
                      const std::vector<DuplicatePair>& duplicates,
                      const std::vector<Function>& functions,
                      const std::string& scannedPath,
                      const std::vector<DuplicatePair>& semanticDuplicates,
                      bool semanticEnabled) {

    std::ofstream file(filename);

    if (!file.is_open()) {
        std::cerr << "Error: Could not create report file: " << filename << "\n";
        return;
    }

    writeReport(file, duplicates, functions, scannedPath, semanticDuplicates, semanticEnabled);
    file.close();

    std::cout << "\nReport saved to: " << filename << "\n";
}

void writeHtmlReport(const std::string& filename,
                      const std::vector<DuplicatePair>& duplicates,
                      const std::vector<Function>& functions,
                      const std::string& scannedPath,
                      const std::vector<DuplicatePair>& semanticDuplicates,
                      bool semanticEnabled) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: could not create HTML report file." << std::endl;
        return;
    }

    file << "<!DOCTYPE html>\n<html>\n<head>\n";
    file << "<title>Code Duplication Report</title>\n";
    file << "<style>\n";
    file << "body { font-family: Arial, sans-serif; background: #1e1e1e; color: #eee; margin: 30px; }\n";
    file << "h1 { color: #4dd0e1; }\n";
    file << "h2 { color: #4dd0e1; margin-top: 30px; }\n";
    file << ".meta { color: #aaa; margin-bottom: 20px; }\n";
    file << "table { width: 100%; border-collapse: collapse; margin-top: 15px; }\n";
    file << "th, td { padding: 10px; border: 1px solid #444; text-align: left; }\n";
    file << "th { background: #2a2a2a; color: #4dd0e1; }\n";
    file << "tr:nth-child(even) { background: #252525; }\n";
    file << ".high { color: #ff6b6b; font-weight: bold; }\n";
    file << ".medium { color: #ffd93d; font-weight: bold; }\n";
    file << ".low { color: #6bff84; font-weight: bold; }\n";
    file << "</style>\n</head>\n<body>\n";

    file << "<h1>Code Duplication Report</h1>\n";
    file << "<div class=\"meta\">Scanned path: " << scannedPath << "<br>";
    file << "Generated: " << getCurrentTime() << "<br>";
    file << "Functions scanned: " << functions.size() << "<br>";
    file << "Duplicates found: " << duplicates.size();
    if (semanticEnabled) {
        file << "<br>Semantic duplicates found: " << semanticDuplicates.size();
    }
    file << "</div>\n";

    file << "<h2>Token-Based Duplicates</h2>\n";
    file << "<table>\n<tr><th>Function 1</th><th>File 1</th><th>Function 2</th><th>File 2</th><th>Similarity</th><th>Suggestion</th></tr>\n";

    for (const auto& pair : duplicates) {
        std::string cssClass = pair.similarity >= 95.0 ? "high" :
                                pair.similarity >= 85.0 ? "medium" : "low";

        file << "<tr>";
        file << "<td>" << pair.func1.name << "</td>";
        file << "<td>" << pair.func1.filename << "</td>";
        file << "<td>" << pair.func2.name << "</td>";
        file << "<td>" << pair.func2.filename << "</td>";
        file << "<td class=\"" << cssClass << "\">" << pair.similarity << "%</td>";
        file << "<td>" << buildSuggestion(pair, false) << "</td>";
        file << "</tr>\n";
    }

    file << "</table>\n";

    if (semanticEnabled) {
        file << "<h2>Semantic Duplicates (CodeBERT, z-score outliers)</h2>\n";
        if (semanticDuplicates.empty()) {
            file << "<p>No semantic duplicates found.</p>\n";
        } else {
            file << "<table>\n<tr><th>Function 1</th><th>File 1</th><th>Function 2</th><th>File 2</th><th>Similarity</th><th>z-score</th><th>Suggestion</th></tr>\n";
            for (const auto& pair : semanticDuplicates) {
                std::string cssClass = pair.similarity >= 95.0 ? "high" :
                                        pair.similarity >= 85.0 ? "medium" : "low";
                file << "<tr>";
                file << "<td>" << pair.func1.name << "</td>";
                file << "<td>" << pair.func1.filename << "</td>";
                file << "<td>" << pair.func2.name << "</td>";
                file << "<td>" << pair.func2.filename << "</td>";
                file << "<td class=\"" << cssClass << "\">" << pair.similarity << "%</td>";
                file << "<td>" << pair.zscore << "</td>";
                file << "<td>" << buildSuggestion(pair, true) << "</td>";
                file << "</tr>\n";
            }
            file << "</table>\n";
        }
    }

    file << "</body>\n</html>\n";
    file.close();

    std::cout << "HTML report saved to: " << filename << std::endl;
}

void writeJsonReport(const std::string& filename,
                      const std::vector<DuplicatePair>& duplicates,
                      const std::vector<Function>& functions,
                      const std::string& scannedPath,
                      const std::vector<DuplicatePair>& semanticDuplicates,
                      bool semanticEnabled) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: could not create JSON report file." << std::endl;
        return;
    }

    double duplicationRate = 0.0;
    if (!functions.empty()) {
        duplicationRate = (double)(duplicates.size() * 2) / functions.size() * 100.0;
    }

    file << std::fixed << std::setprecision(1);

    file << "{\n";
    file << "  \"scannedPath\": \"" << escapeJson(scannedPath) << "\",\n";
    file << "  \"generated\": \"" << escapeJson(getCurrentTime()) << "\",\n";
    file << "  \"totalFunctions\": " << functions.size() << ",\n";
    file << "  \"duplicatesFound\": " << duplicates.size() << ",\n";
    file << "  \"duplicationRate\": " << duplicationRate << ",\n";
    file << "  \"duplicates\": [\n";

    for (size_t i = 0; i < duplicates.size(); i++) {
        const auto& pair = duplicates[i];
        file << "    {\n";
        file << "      \"similarity\": " << pair.similarity << ",\n";
        file << "      \"function1\": \"" << escapeJson(pair.func1.name) << "\",\n";
        file << "      \"file1\": \"" << escapeJson(pair.func1.filename) << "\",\n";
        file << "      \"function2\": \"" << escapeJson(pair.func2.name) << "\",\n";
        file << "      \"file2\": \"" << escapeJson(pair.func2.filename) << "\",\n";
        file << "      \"suggestion\": \"" << escapeJson(buildSuggestion(pair, false)) << "\"\n";
        file << "    }" << (i + 1 < duplicates.size() ? "," : "") << "\n";
    }

    file << "  ]";

    if (semanticEnabled) {
        file << ",\n";
        file << "  \"semanticEnabled\": true,\n";
        file << "  \"semanticDuplicatesFound\": " << semanticDuplicates.size() << ",\n";
        file << "  \"semanticDuplicates\": [\n";
        for (size_t i = 0; i < semanticDuplicates.size(); i++) {
            const auto& pair = semanticDuplicates[i];
            file << "    {\n";
            file << "      \"similarity\": " << pair.similarity << ",\n";
            file << "      \"zscore\": " << pair.zscore << ",\n";
            file << "      \"function1\": \"" << escapeJson(pair.func1.name) << "\",\n";
            file << "      \"file1\": \"" << escapeJson(pair.func1.filename) << "\",\n";
            file << "      \"function2\": \"" << escapeJson(pair.func2.name) << "\",\n";
            file << "      \"file2\": \"" << escapeJson(pair.func2.filename) << "\",\n";
            file << "      \"suggestion\": \"" << escapeJson(buildSuggestion(pair, true)) << "\"\n";
            file << "    }" << (i + 1 < semanticDuplicates.size() ? "," : "") << "\n";
        }
        file << "  ]\n";
    } else {
        file << "\n";
    }

    file << "}\n";

    file.close();

    std::cout << "JSON report saved to: " << filename << std::endl;
}