#include "reporter.h"
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
                 const std::string& scannedPath) {

    std::string line(60, '=');

    out << line << "\n";
    out << "   AI-GENERATED CODE DUPLICATION DETECTOR\n";
    out << "   Report generated: " << getCurrentTime() << "\n";
    out << "   Scanned path: " << scannedPath << "\n";
    out << line << "\n\n";

    out << "SUMMARY\n";
    out << std::string(60, '-') << "\n";
    out << "  Total functions scanned : " << functions.size() << "\n";
    out << "  Duplicate pairs found   : " << duplicates.size() << "\n";

    if (!functions.empty()) {
        double duplicationRate = (double)(duplicates.size() * 2) / functions.size() * 100.0;
        out << std::fixed << std::setprecision(1);
        out << "  Duplication rate        : " << duplicationRate << "%\n";
    }

    out << "\n";

    if (duplicates.empty()) {
        out << "No duplicates found. Clean codebase!\n";
        return;
    }

    out << "DUPLICATE PAIRS\n";
    out << std::string(60, '-') << "\n\n";

    int count = 1;
    for (const auto& pair : duplicates) {
        out << "[#" << count++ << "] ";
        out << std::fixed << std::setprecision(1);
        out << pair.similarity << "% similarity\n";
        out << "  File 1 : " << pair.func1.filename << "\n";
        out << "  Func 1 : " << pair.func1.name << "\n";
        out << "  File 2 : " << pair.func2.filename << "\n";
        out << "  Func 2 : " << pair.func2.name << "\n";
        out << std::string(60, '-') << "\n\n";
    }

    out << line << "\n";
    out << "END OF REPORT\n";
    out << line << "\n";
}

void saveReportToFile(const std::string& filename,
                      const std::vector<DuplicatePair>& duplicates,
                      const std::vector<Function>& functions,
                      const std::string& scannedPath) {

    std::ofstream file(filename);

    if (!file.is_open()) {
        std::cerr << "Error: Could not create report file: " << filename << "\n";
        return;
    }

    writeReport(file, duplicates, functions, scannedPath);
    file.close();

    std::cout << "\nReport saved to: " << filename << "\n";
}

void writeHtmlReport(const std::string& filename,
                      const std::vector<DuplicatePair>& duplicates,
                      const std::vector<Function>& functions,
                      const std::string& scannedPath) {
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
    file << "Duplicates found: " << duplicates.size() << "</div>\n";

    file << "<table>\n<tr><th>Function 1</th><th>File 1</th><th>Function 2</th><th>File 2</th><th>Similarity</th></tr>\n";

    for (const auto& pair : duplicates) {
        std::string cssClass = pair.similarity >= 95.0 ? "high" :
                                pair.similarity >= 85.0 ? "medium" : "low";

        file << "<tr>";
        file << "<td>" << pair.func1.name << "</td>";
        file << "<td>" << pair.func1.filename << "</td>";
        file << "<td>" << pair.func2.name << "</td>";
        file << "<td>" << pair.func2.filename << "</td>";
        file << "<td class=\"" << cssClass << "\">" << pair.similarity << "%</td>";
        file << "</tr>\n";
    }

    file << "</table>\n</body>\n</html>\n";
    file.close();

    std::cout << "HTML report saved to: " << filename << std::endl;
}

void writeJsonReport(const std::string& filename,
                      const std::vector<DuplicatePair>& duplicates,
                      const std::vector<Function>& functions,
                      const std::string& scannedPath) {
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
        file << "      \"file2\": \"" << escapeJson(pair.func2.filename) << "\"\n";
        file << "    }" << (i + 1 < duplicates.size() ? "," : "") << "\n";
    }

    file << "  ]\n";
    file << "}\n";

    file.close();

    std::cout << "JSON report saved to: " << filename << std::endl;
}