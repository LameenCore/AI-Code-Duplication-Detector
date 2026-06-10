#include "reporter.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <ctime>

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