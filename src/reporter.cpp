#include "reporter.h"
#include "suggestion.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <ctime>
#include <sstream>
#include <unordered_set>

namespace {
// ANSI escape codes: "\033[<code>m" tells the terminal to switch color
// until it sees the reset code. Windows 10+ terminals (including modern
// PowerShell) understand these natively -- no library needed.
const char* ANSI_RED    = "\033[91m";
const char* ANSI_YELLOW = "\033[93m";
const char* ANSI_GREEN  = "\033[92m";
const char* ANSI_RESET  = "\033[0m";

// Wraps text in a color code, but only if useColor is true -- this is what
// keeps the exact same escape codes out of report.txt while still using
// them for the std::cout printout.
std::string colorize(const std::string& text, const char* color, bool useColor) {
    if (!useColor) return text;
    return std::string(color) + text + ANSI_RESET;
}

const char* similarityColor(double similarity) {
    if (similarity >= 95.0) return ANSI_RED;
    if (similarity >= 85.0) return ANSI_YELLOW;
    return ANSI_GREEN;
}

const char* gradeColor(const std::string& grade) {
    if (grade == "A" || grade == "B") return ANSI_GREEN;
    if (grade == "C") return ANSI_YELLOW;
    return ANSI_RED;
}
} // namespace

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

// Rolls duplication rate + semantic outlier count into one 0-100 number.
// Starts at a perfect 100 and subtracts a penalty for each signal:
//   - token-based duplication rate is subtracted directly (a codebase that's
//     40% duplicated loses 40 points)
//   - each semantic outlier costs a flat 5 points (semantic hits are rarer
//     and more deliberate than token matches, so they're weighted heavier
//     per-incident even though there are usually fewer of them)
// Clamped to [0, 100] so it always reads as a sane percentage.
double computeHealthScore(double duplicationRate, size_t semanticCount, bool semanticEnabled) {
    double score = 100.0 - duplicationRate;
    if (semanticEnabled) {
        score -= static_cast<double>(semanticCount) * 5.0;
    }
    if (score < 0.0) score = 0.0;
    if (score > 100.0) score = 100.0;
    return score;
}

// Converts the numeric score into a school-style letter grade so the
// headline number is easy to skim without doing mental math.
std::string healthGrade(double score) {
    if (score >= 90.0) return "A";
    if (score >= 75.0) return "B";
    if (score >= 60.0) return "C";
    if (score >= 40.0) return "D";
    return "F";
}

void writeReport(std::ostream& out,
                 const std::vector<DuplicatePair>& duplicates,
                 const std::vector<Function>& functions,
                 const std::string& scannedPath,
                 const std::vector<DuplicatePair>& semanticDuplicates,
                 bool semanticEnabled,
                 bool useColor) {

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

    double duplicationRate = 0.0;
    if (!functions.empty()) {
        duplicationRate = (double)(duplicates.size() * 2) / functions.size() * 100.0;
        out << std::fixed << std::setprecision(1);
        out << "  Duplication rate           : " << duplicationRate << "%\n";
    }
    if (semanticEnabled) {
        out << "  Semantic duplicates found  : " << semanticDuplicates.size() << "\n";
    }

    double healthScore = computeHealthScore(duplicationRate, semanticDuplicates.size(), semanticEnabled);
    std::string grade = healthGrade(healthScore);
    std::ostringstream healthLine;
    healthLine << std::fixed << std::setprecision(1) << healthScore << "/100 (" << grade << ")";
    out << std::fixed << std::setprecision(1);
    out << "  Codebase health score      : " << colorize(healthLine.str(), gradeColor(grade), useColor) << "\n";

    out << "\n";

    out << "DUPLICATE PAIRS (token-based)\n";
    out << std::string(60, '-') << "\n\n";

    if (duplicates.empty()) {
        out << "No duplicates found.\n\n";
    } else {
        int count = 1;
        for (const auto& pair : duplicates) {
            std::ostringstream simText;
            simText << std::fixed << std::setprecision(1) << pair.similarity << "% similarity";
            out << "[#" << count++ << "] ";
            out << colorize(simText.str(), similarityColor(pair.similarity), useColor) << "\n";
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
                std::ostringstream simText;
                simText << std::fixed << std::setprecision(1) << pair.similarity
                        << "% similarity (z=" << pair.zscore << ")";
                out << "[#" << count++ << "] ";
                out << colorize(simText.str(), similarityColor(pair.similarity), useColor) << "\n";
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

    // Extract just the filename from a full path for display (full path kept as tooltip)
    auto basename = [](const std::string& path) -> std::string {
        size_t pos = path.find_last_of("/\\");
        return pos != std::string::npos ? path.substr(pos + 1) : path;
    };

    // Strip the scanned path prefix from suggestion text so full Windows paths
    // like "C:\Users\Admin\Desktop\imgui\imgui_draw.cpp" become "imgui_draw.cpp"
    auto shortenPaths = [&scannedPath](const std::string& text) -> std::string {
        std::string result = text;
        for (const std::string& prefix : {scannedPath + "\\", scannedPath + "/"}) {
            size_t pos;
            while ((pos = result.find(prefix)) != std::string::npos)
                result.erase(pos, prefix.size());
        }
        return result;
    };

    // Count unique functions involved in at least one duplicate pair.
    // The old formula (pairs*2 / total) could exceed 100% because one
    // function can appear in many pairs. Unique-function rate is always
    // 0-100% and is a more honest "what fraction of your codebase is
    // duplicated?" metric.
    std::unordered_set<std::string> uniqueInPairs;
    int exactMatches = 0;
    for (const auto& p : duplicates) {
        uniqueInPairs.insert(p.func1.filename + "|" + p.func1.name);
        uniqueInPairs.insert(p.func2.filename + "|" + p.func2.name);
        if (p.similarity >= 99.9) exactMatches++;
    }
    double duplicationRate = functions.empty() ? 0.0 :
        (double)uniqueInPairs.size() / functions.size() * 100.0;

    double healthScore = computeHealthScore(duplicationRate, semanticDuplicates.size(), semanticEnabled);
    std::string grade = healthGrade(healthScore);
    std::string gradeColor = (grade == "A" || grade == "B") ? "#3fb950" :
                              (grade == "C") ? "#d29922" : "#f85149";

    file << std::fixed << std::setprecision(1);

    // ── HEAD ──────────────────────────────────────────────────────────────
    file << R"(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Code Duplication Report</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;background:#0d1117;color:#e6edf3;min-height:100vh}
a{color:#58a6ff;text-decoration:none}

/* header */
.header{background:linear-gradient(135deg,#161b22 0%,#0d1117 100%);border-bottom:1px solid #30363d;padding:28px 48px}
.header h1{font-size:1.4rem;color:#58a6ff;font-weight:700;letter-spacing:-.3px}
.header .sub{color:#8b949e;font-size:.82rem;margin-top:6px}

.container{max-width:1100px;margin:0 auto;padding:32px 48px}

/* stat cards */
.stats{display:grid;grid-template-columns:repeat(4,1fr);gap:14px;margin-bottom:28px}
.stat-card{background:#161b22;border:1px solid #30363d;border-radius:10px;padding:18px 20px}
.stat-label{font-size:.7rem;text-transform:uppercase;letter-spacing:.8px;color:#8b949e;margin-bottom:8px}
.stat-value{font-size:1.9rem;font-weight:700}
.stat-sub{font-size:.75rem;color:#8b949e;margin-top:3px}

/* health card */
.health-card{background:#161b22;border:1px solid #30363d;border-radius:10px;padding:22px 28px;margin-bottom:28px;display:flex;align-items:center;gap:22px}
.health-ring{width:76px;height:76px;border-radius:50%;display:flex;align-items:center;justify-content:center;font-size:1.5rem;font-weight:800;border:4px solid;flex-shrink:0}
.health-info p{color:#8b949e;font-size:.8rem;margin-bottom:4px}
.health-info .score-line{font-size:2rem;font-weight:700;line-height:1}
.health-info .desc{color:#8b949e;font-size:.8rem;margin-top:6px;max-width:500px;line-height:1.5}

/* section */
.section-head{display:flex;align-items:center;gap:10px;margin-bottom:14px}
.section-head h2{font-size:1rem;font-weight:600}
.badge{background:#21262d;border:1px solid #30363d;border-radius:20px;padding:2px 10px;font-size:.75rem;color:#8b949e}

/* search */
.search{width:100%;padding:9px 14px;background:#161b22;border:1px solid #30363d;border-radius:6px;color:#e6edf3;font-size:.88rem;margin-bottom:14px;outline:none;transition:border .15s}
.search:focus{border-color:#58a6ff}

/* pair cards */
.pair{background:#161b22;border:1px solid #30363d;border-radius:8px;padding:14px 18px;margin-bottom:8px;display:flex;gap:14px;align-items:flex-start;transition:border .15s}
.pair:hover{border-color:#484f58}
.sim{flex-shrink:0;width:68px;text-align:center;padding:5px 0;border-radius:6px;font-weight:700;font-size:.85rem;background:#0d1117;border:1px solid}
.pair-body{flex:1;min-width:0}
.funcs{font-size:.92rem;font-weight:600;margin-bottom:3px}
.funcs .arrow{color:#484f58;font-weight:400;margin:0 6px}
.funcs .fn{color:#58a6ff}
.files{font-size:.75rem;color:#8b949e;margin-bottom:8px}
.files .sep{margin:0 6px;color:#484f58}
.suggestion{font-size:.78rem;color:#8b949e;background:#0d1117;border-radius:4px;padding:7px 10px;border-left:3px solid #30363d;line-height:1.5}

.hidden{display:none}
.pagination{display:flex;align-items:center;gap:6px;margin-top:14px;flex-wrap:wrap}
.pagination button{background:#21262d;border:1px solid #30363d;border-radius:6px;color:#8b949e;cursor:pointer;font-size:.8rem;padding:5px 10px;transition:background .15s}
.pagination button:hover:not(:disabled){background:#2d333b;color:#e6edf3}
.pagination button.active{background:#1f6feb;border-color:#1f6feb;color:#fff}
.pagination button:disabled{opacity:.4;cursor:default}
.pagination .pg-info{font-size:.78rem;color:#8b949e;margin-left:6px}

.footer{text-align:center;color:#484f58;font-size:.75rem;padding:28px;border-top:1px solid #21262d;margin-top:40px}
</style>
</head>
<body>
)";

    // ── HEADER ────────────────────────────────────────────────────────────
    file << "<div class=\"header\">\n";
    file << "  <h1>AI-Generated Code Duplication Detector</h1>\n";
    file << "  <div class=\"sub\">Scanned: <strong>" << scannedPath << "</strong>"
         << " &nbsp;·&nbsp; Generated: " << getCurrentTime() << "</div>\n";
    file << "</div>\n";
    file << "<div class=\"container\">\n";

    // ── STAT CARDS ────────────────────────────────────────────────────────
    file << "<div class=\"stats\">\n";

    file << "  <div class=\"stat-card\">\n"
         << "    <div class=\"stat-label\">Functions Scanned</div>\n"
         << "    <div class=\"stat-value\" style=\"color:#58a6ff\">" << functions.size() << "</div>\n"
         << "    <div class=\"stat-sub\">across all source files</div>\n"
         << "  </div>\n";

    file << "  <div class=\"stat-card\">\n"
         << "    <div class=\"stat-label\">Duplicate Pairs</div>\n"
         << "    <div class=\"stat-value\" style=\"color:#f85149\">" << duplicates.size() << "</div>\n"
         << "    <div class=\"stat-sub\">token-based matches</div>\n"
         << "  </div>\n";

    file << "  <div class=\"stat-card\">\n"
         << "    <div class=\"stat-label\">Duplication Rate</div>\n"
         << "    <div class=\"stat-value\" style=\"color:#d29922\">" << duplicationRate << "%</div>\n"
         << "    <div class=\"stat-sub\">functions involved in a pair</div>\n"
         << "  </div>\n";

    if (semanticEnabled) {
        file << "  <div class=\"stat-card\">\n"
             << "    <div class=\"stat-label\">Semantic Duplicates</div>\n"
             << "    <div class=\"stat-value\" style=\"color:#a371f7\">" << semanticDuplicates.size() << "</div>\n"
             << "    <div class=\"stat-sub\">CodeBERT z-score outliers</div>\n"
             << "  </div>\n";
    } else {
        file << "  <div class=\"stat-card\">\n"
             << "    <div class=\"stat-label\">Exact Copies</div>\n"
             << "    <div class=\"stat-value\" style=\"color:#3fb950\">" << exactMatches << "</div>\n"
             << "    <div class=\"stat-sub\">100% similarity matches</div>\n"
             << "  </div>\n";
    }

    file << "</div>\n";

    // ── HEALTH CARD ───────────────────────────────────────────────────────
    file << "<div class=\"health-card\">\n";
    file << "  <div class=\"health-ring\" style=\"color:" << gradeColor << ";border-color:" << gradeColor << "\">"
         << grade << "</div>\n";
    file << "  <div class=\"health-info\">\n";
    file << "    <p>Codebase Health Score</p>\n";
    file << "    <div class=\"score-line\" style=\"color:" << gradeColor << "\">"
         << healthScore << " <span style=\"font-size:1rem;color:#8b949e;font-weight:400\">/ 100</span></div>\n";
    file << "    <div class=\"desc\">Score is calculated from the duplication rate";
    if (semanticEnabled) file << " and semantic duplicate density";
    file << ". Lower duplication = higher score.</div>\n";
    file << "  </div>\n";
    file << "</div>\n";

    // ── DUPLICATE PAIRS ───────────────────────────────────────────────────
    file << "<div class=\"section-head\">\n";
    file << "  <h2>Token-Based Duplicate Pairs</h2>\n";
    file << "  <span class=\"badge\">" << duplicates.size() << " pairs</span>\n";
    file << "</div>\n";

    if (duplicates.empty()) {
        file << "<p style=\"color:#8b949e\">No duplicates found.</p>\n";
    } else {
        file << "<input class=\"search\" type=\"text\" id=\"search\" placeholder=\"Filter by function name...\" "
             << "oninput=\"filterPairs()\">\n";

        file << "<div id=\"pairs-container\">\n";

        int idx = 0;
        for (const auto& pair : duplicates) {
            std::string simColor = pair.similarity >= 95.0 ? "#f85149" :
                                   pair.similarity >= 85.0 ? "#d29922" : "#3fb950";
            file << "<div class=\"pair\" "
                 << "data-f1=\"" << pair.func1.name << "\" data-f2=\"" << pair.func2.name << "\">\n";

            file << "  <div class=\"sim\" style=\"color:" << simColor << ";border-color:" << simColor << "\">"
                 << pair.similarity << "%</div>\n";

            file << "  <div class=\"pair-body\">\n";
            file << "    <div class=\"funcs\">"
                 << "<span class=\"fn\">" << pair.func1.name << "</span>"
                 << "<span class=\"arrow\">&#8596;</span>"
                 << "<span class=\"fn\">" << pair.func2.name << "</span>"
                 << "</div>\n";
            file << "    <div class=\"files\">"
                 << "<span title=\"" << pair.func1.filename << "\">" << basename(pair.func1.filename) << "</span>"
                 << "<span class=\"sep\">/</span>"
                 << "<span title=\"" << pair.func2.filename << "\">" << basename(pair.func2.filename) << "</span>"
                 << "</div>\n";
            file << "    <div class=\"suggestion\">" << shortenPaths(buildSuggestion(pair, false)) << "</div>\n";
            file << "  </div>\n";
            file << "</div>\n";
            idx++;
        }

        file << "</div>\n";

        file << "<div class=\"pagination\" id=\"pagination\"></div>\n";
    }

    // ── SEMANTIC SECTION ──────────────────────────────────────────────────
    if (semanticEnabled && !semanticDuplicates.empty()) {
        file << "<div class=\"section-head\" style=\"margin-top:36px\">\n";
        file << "  <h2>Semantic Duplicates</h2>\n";
        file << "  <span class=\"badge\">CodeBERT &middot; z-score outliers &middot; "
             << semanticDuplicates.size() << " pairs</span>\n";
        file << "</div>\n";

        for (const auto& pair : semanticDuplicates) {
            std::string simColor = pair.similarity >= 95.0 ? "#f85149" :
                                   pair.similarity >= 85.0 ? "#d29922" : "#3fb950";
            file << "<div class=\"pair\">\n";
            file << "  <div class=\"sim\" style=\"color:" << simColor << ";border-color:" << simColor << "\">"
                 << pair.similarity << "%</div>\n";
            file << "  <div class=\"pair-body\">\n";
            file << "    <div class=\"funcs\">"
                 << "<span class=\"fn\">" << pair.func1.name << "</span>"
                 << "<span class=\"arrow\">&#8596;</span>"
                 << "<span class=\"fn\">" << pair.func2.name << "</span>"
                 << " <span style=\"font-size:.72rem;color:#a371f7;margin-left:8px\">z=" << pair.zscore << "</span>"
                 << "</div>\n";
            file << "    <div class=\"files\">"
                 << "<span title=\"" << pair.func1.filename << "\">" << basename(pair.func1.filename) << "</span>"
                 << "<span class=\"sep\">/</span>"
                 << "<span title=\"" << pair.func2.filename << "\">" << basename(pair.func2.filename) << "</span>"
                 << "</div>\n";
            file << "    <div class=\"suggestion\">" << shortenPaths(buildSuggestion(pair, true)) << "</div>\n";
            file << "  </div>\n";
            file << "</div>\n";
        }
    }

    // ── FOOTER + JS ───────────────────────────────────────────────────────
    file << "<div class=\"footer\">AI-Generated Code Duplication Detector &nbsp;&middot;&nbsp; MIT License</div>\n";
    file << "</div>\n";

    file << "<script>\n"
         << "var PAGE_SIZE = 25;\n"
         << "var currentPage = 0;\n"
         << "var visiblePairs = [];\n"
         << "var allPairs = [];\n"
         << "window.onload = function() {\n"
         << "    allPairs = Array.from(document.querySelectorAll('#pairs-container .pair'));\n"
         << "    visiblePairs = allPairs;\n"
         << "    showPage(0);\n"
         << "};\n"
         << "function filterPairs() {\n"
         << "    var q = document.getElementById('search').value.toLowerCase().trim();\n"
         << "    allPairs.forEach(function(c) { c.style.display = 'none'; });\n"
         << "    visiblePairs = q ? allPairs.filter(function(c) {\n"
         << "        var f1 = (c.getAttribute('data-f1') || '').toLowerCase();\n"
         << "        var f2 = (c.getAttribute('data-f2') || '').toLowerCase();\n"
         << "        return f1.includes(q) || f2.includes(q);\n"
         << "    }) : allPairs;\n"
         << "    showPage(0);\n"
         << "}\n"
         << "function showPage(page) {\n"
         << "    currentPage = page;\n"
         << "    var start = page * PAGE_SIZE, end = start + PAGE_SIZE;\n"
         << "    allPairs.forEach(function(c) { c.style.display = 'none'; });\n"
         << "    visiblePairs.forEach(function(c, i) {\n"
         << "        c.style.display = (i >= start && i < end) ? '' : 'none';\n"
         << "    });\n"
         << "    renderPagination();\n"
         << "}\n"
         << "function renderPagination() {\n"
         << "    var pg = document.getElementById('pagination');\n"
         << "    if (!pg) return;\n"
         << "    var total = Math.ceil(visiblePairs.length / PAGE_SIZE);\n"
         << "    if (total <= 1) { pg.innerHTML = ''; return; }\n"
         << "    var h = '';\n"
         << "    h += '<button onclick=\"showPage(' + (currentPage-1) + ')\"' + (currentPage===0?' disabled':'') + '>&larr; Prev</button>';\n"
         << "    for (var i = 0; i < total; i++) {\n"
         << "        if (i===0 || i===total-1 || Math.abs(i-currentPage)<=2) {\n"
         << "            h += '<button onclick=\"showPage(' + i + ')\"' + (i===currentPage?' class=\"active\"':'') + '>' + (i+1) + '</button>';\n"
         << "        } else if (Math.abs(i-currentPage)===3) {\n"
         << "            h += '<span style=\"color:#484f58;padding:0 4px\">&hellip;</span>';\n"
         << "        }\n"
         << "    }\n"
         << "    h += '<button onclick=\"showPage(' + (currentPage+1) + ')\"' + (currentPage===total-1?' disabled':'') + '>Next &rarr;</button>';\n"
         << "    var s = currentPage*PAGE_SIZE+1, e = Math.min((currentPage+1)*PAGE_SIZE, visiblePairs.length);\n"
         << "    h += '<span class=\"pg-info\">Showing ' + s + '&ndash;' + e + ' of ' + visiblePairs.length + '</span>';\n"
         << "    pg.innerHTML = h;\n"
         << "}\n"
         << "</script>\n"
         << "</body>\n</html>\n";

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
    double healthScore = computeHealthScore(duplicationRate, semanticDuplicates.size(), semanticEnabled);

    file << std::fixed << std::setprecision(1);

    file << "{\n";
    file << "  \"scannedPath\": \"" << escapeJson(scannedPath) << "\",\n";
    file << "  \"generated\": \"" << escapeJson(getCurrentTime()) << "\",\n";
    file << "  \"totalFunctions\": " << functions.size() << ",\n";
    file << "  \"duplicatesFound\": " << duplicates.size() << ",\n";
    file << "  \"duplicationRate\": " << duplicationRate << ",\n";
    file << "  \"healthScore\": " << healthScore << ",\n";
    file << "  \"healthGrade\": \"" << healthGrade(healthScore) << "\",\n";
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