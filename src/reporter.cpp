#include "reporter.h"
#include "suggestion.h"
#include <algorithm>
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

    // ── HELPER LAMBDAS ────────────────────────────────────────────────────
    auto basename = [](const std::string& path) -> std::string {
        size_t pos = path.find_last_of("/\\");
        return pos != std::string::npos ? path.substr(pos + 1) : path;
    };

    auto escapeHtml = [](const std::string& s) -> std::string {
        std::string r; r.reserve(s.size());
        for (char c : s) {
            if      (c == '&')  r += "&amp;";
            else if (c == '<')  r += "&lt;";
            else if (c == '>')  r += "&gt;";
            else if (c == '"')  r += "&quot;";
            else                r += c;
        }
        return r;
    };

    auto shortenPaths = [&scannedPath](const std::string& text) -> std::string {
        std::string result = text;
        for (const std::string& prefix : {scannedPath + "\\", scannedPath + "/"}) {
            size_t pos;
            while ((pos = result.find(prefix)) != std::string::npos)
                result.erase(pos, prefix.size());
        }
        return result;
    };

    // Split a function body into at most maxLines lines for the diff view
    auto splitLines = [](const std::string& body, int maxLines) -> std::vector<std::string> {
        std::vector<std::string> lines;
        std::istringstream ss(body);
        std::string line;
        int count = 0;
        while (std::getline(ss, line) && count < maxLines) {
            lines.push_back(line);
            count++;
        }
        if (lines.empty()) lines.push_back(body.substr(0, std::min((int)body.size(), 80)));
        return lines;
    };

    // ── STATS ─────────────────────────────────────────────────────────────
    std::unordered_set<std::string> uniqueInPairs;
    int exactMatches = 0, nearMatches = 0;
    for (const auto& p : duplicates) {
        uniqueInPairs.insert(p.func1.filename + "|" + p.func1.name);
        uniqueInPairs.insert(p.func2.filename + "|" + p.func2.name);
        if (p.similarity >= 99.9) exactMatches++;
        else nearMatches++;
    }
    double duplicationRate = functions.empty() ? 0.0 :
        (double)uniqueInPairs.size() / functions.size() * 100.0;
    double healthScore = computeHealthScore(duplicationRate, semanticDuplicates.size(), semanticEnabled);

    // Fine-grained grade with +/- modifier
    auto gradePlus = [](double s) -> std::string {
        if (s >= 95) return "A+"; if (s >= 90) return "A";
        if (s >= 82) return "B+"; if (s >= 75) return "B";
        if (s >= 65) return "C+"; if (s >= 60) return "C";
        if (s >= 45) return "D+"; if (s >= 40) return "D";
        return "F";
    };
    std::string grade     = gradePlus(healthScore);
    std::string gradeColor = (healthScore >= 75) ? "#30a14e" :
                             (healthScore >= 50) ? "#d29922" : "#f85149";

    file << std::fixed << std::setprecision(1);

    // ── HTML HEAD + CSS ───────────────────────────────────────────────────
    file << "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n";
    file << "<meta charset=\"UTF-8\">\n";
    file << "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n";
    file << "<title>DeepScan Engine Analysis Report</title>\n";
    file << "<style>\n";
    file << ":root{--bg:#0d1117;--surface:#161b22;--surface2:#1c2128;--border:#30363d;--border2:#21262d;";
    file << "--text:#e6edf3;--muted:#8b949e;--accent:#58a6ff;--green:#30a14e;--yellow:#d29922;";
    file << "--red:#f85149;--purple:#a371f7;--font-mono:'JetBrains Mono','Fira Code','Cascadia Code',monospace}\n";
    file << "*{box-sizing:border-box;margin:0;padding:0}\n";
    file << "body{font-family:system-ui,-apple-system,'Segoe UI',sans-serif;background:var(--bg);color:var(--text);min-height:100vh}\n";
    // Engine banner
    file << ".eng-banner{background:linear-gradient(90deg,#0d1117 0%,#161b22 50%,#0d1117 100%);";
    file << "border-bottom:1px solid var(--border);padding:0 48px}\n";
    file << ".eng-inner{max-width:1200px;margin:0 auto;display:flex;align-items:center;justify-content:space-between;height:60px}\n";
    file << ".eng-logo{display:flex;align-items:center;gap:10px}\n";
    file << ".eng-icon{width:32px;height:32px;background:linear-gradient(135deg,#1f6feb,#58a6ff);border-radius:8px;";
    file << "display:flex;align-items:center;justify-content:center;font-size:.9rem;font-weight:800;color:#fff}\n";
    file << ".eng-name{font-size:.95rem;font-weight:700;color:var(--text);letter-spacing:-.2px}\n";
    file << ".eng-ver{font-size:.7rem;color:var(--muted);margin-top:1px}\n";
    file << ".eng-meta{display:flex;align-items:center;gap:16px}\n";
    file << ".eng-badge{font-size:.7rem;color:var(--muted);background:var(--border2);border:1px solid var(--border);";
    file << "border-radius:4px;padding:3px 8px;font-family:var(--font-mono)}\n";
    // Container
    file << ".container{max-width:1200px;margin:0 auto;padding:32px 48px}\n";
    // Executive summary grid
    file << ".exec-grid{display:grid;grid-template-columns:220px 1fr;gap:20px;margin-bottom:28px}\n";
    file << ".grade-card{background:var(--surface);border:1px solid var(--border);border-radius:12px;";
    file << "display:flex;flex-direction:column;align-items:center;justify-content:center;gap:6px;padding:24px;text-align:center}\n";
    file << ".grade-ring{width:90px;height:90px;border-radius:50%;display:flex;align-items:center;justify-content:center;";
    file << "font-size:2rem;font-weight:800;border:4px solid;font-family:var(--font-mono)}\n";
    file << ".grade-label{font-size:.65rem;text-transform:uppercase;letter-spacing:1px;color:var(--muted);margin-top:4px}\n";
    file << ".grade-score{font-size:1.4rem;font-weight:700;font-family:var(--font-mono)}\n";
    file << ".meta-panel{background:var(--surface);border:1px solid var(--border);border-radius:12px;padding:18px 22px;";
    file << "display:flex;flex-direction:column;justify-content:space-between}\n";
    file << ".meta-path{font-size:.8rem;color:var(--muted);margin-bottom:10px}\n";
    file << ".meta-path strong{color:var(--text);font-family:var(--font-mono)}\n";
    file << ".meta-badges{display:flex;flex-wrap:wrap;gap:8px;margin-bottom:14px}\n";
    file << ".mbadge{display:flex;flex-direction:column;background:var(--bg);border:1px solid var(--border);";
    file << "border-radius:8px;padding:8px 14px;min-width:110px}\n";
    file << ".mbadge-val{font-size:1.5rem;font-weight:700;font-family:var(--font-mono);line-height:1}\n";
    file << ".mbadge-lbl{font-size:.62rem;text-transform:uppercase;letter-spacing:.7px;color:var(--muted);margin-top:3px}\n";
    file << ".health-bar-wrap{display:flex;align-items:center;gap:10px}\n";
    file << ".health-bar{flex:1;height:5px;background:var(--border2);border-radius:3px;overflow:hidden}\n";
    file << ".health-bar-fill{height:100%;border-radius:3px;transition:width .6s ease}\n";
    file << ".health-bar-pct{font-size:.75rem;color:var(--muted);font-family:var(--font-mono);width:42px;text-align:right}\n";
    // Pass detection grid (3 equal cols)
    file << ".pass-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:14px;margin-bottom:28px}\n";
    file << ".pass-card{background:var(--surface);border:1px solid var(--border);border-radius:10px;padding:16px 18px}\n";
    file << ".pass-head{display:flex;align-items:center;gap:8px;margin-bottom:10px}\n";
    file << ".pass-icon{width:26px;height:26px;border-radius:6px;display:flex;align-items:center;justify-content:center;font-size:.75rem;font-weight:700;flex-shrink:0}\n";
    file << ".pass-title{font-size:.82rem;font-weight:600}\n";
    file << ".pass-num{font-size:2rem;font-weight:800;font-family:var(--font-mono);line-height:1}\n";
    file << ".pass-sub{font-size:.72rem;color:var(--muted);margin-top:4px}\n";
    // Anomaly board
    file << ".anomaly-board{background:rgba(248,81,73,.04);border:1px solid rgba(248,81,73,.2);border-radius:10px;padding:18px 20px;margin-bottom:28px}\n";
    file << ".anomaly-head{display:flex;align-items:center;gap:8px;margin-bottom:12px}\n";
    file << ".anomaly-title{font-size:.88rem;font-weight:600;color:#f85149}\n";
    file << ".anomaly-row{display:flex;align-items:center;gap:12px;padding:8px 0;border-bottom:1px solid var(--border2)}\n";
    file << ".anomaly-row:last-child{border-bottom:none}\n";
    file << ".anomaly-rank{font-size:.65rem;color:var(--muted);font-family:var(--font-mono);width:20px;flex-shrink:0}\n";
    file << ".anomaly-sim{font-size:.82rem;font-weight:700;font-family:var(--font-mono);width:52px;flex-shrink:0}\n";
    file << ".anomaly-names{flex:1;font-size:.8rem;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}\n";
    file << ".anomaly-file{font-size:.68rem;color:var(--muted);width:180px;flex-shrink:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;text-align:right}\n";
    // Diff view
    file << ".diff-section{background:var(--surface);border:1px solid var(--border);border-radius:10px;margin-bottom:28px;overflow:hidden}\n";
    file << ".diff-header{background:var(--surface2);border-bottom:1px solid var(--border);padding:10px 18px;display:flex;align-items:center;justify-content:space-between}\n";
    file << ".diff-title{font-size:.82rem;font-weight:600;color:var(--accent)}\n";
    file << ".diff-meta{font-size:.7rem;color:var(--muted)}\n";
    file << ".diff-grid{display:grid;grid-template-columns:1fr 1fr}\n";
    file << ".diff-side{overflow:hidden}\n";
    file << ".diff-side+.diff-side{border-left:1px solid var(--border)}\n";
    file << ".diff-side-head{background:var(--bg);padding:6px 12px;font-size:.68rem;color:var(--muted);font-family:var(--font-mono);border-bottom:1px solid var(--border)}\n";
    file << ".diff-code{font-family:var(--font-mono);font-size:.73rem;line-height:1.7;overflow-x:auto;padding:0}\n";
    file << ".diff-line{display:flex;padding:0 12px;gap:10px;white-space:pre}\n";
    file << ".diff-line:hover{background:rgba(88,166,255,.05)}\n";
    file << ".diff-ln{color:#484f58;user-select:none;min-width:24px;text-align:right;flex-shrink:0}\n";
    file << ".diff-text{flex:1;overflow:hidden;text-overflow:ellipsis;color:var(--text)}\n";
    // Pairs section
    file << ".section-head{display:flex;align-items:center;gap:10px;margin-bottom:14px}\n";
    file << ".section-head h2{font-size:.95rem;font-weight:600}\n";
    file << ".sbadge{background:var(--border2);border:1px solid var(--border);border-radius:20px;padding:2px 10px;font-size:.72rem;color:var(--muted)}\n";
    file << ".search{width:100%;padding:9px 14px;background:var(--surface);border:1px solid var(--border);border-radius:6px;";
    file << "color:var(--text);font-size:.85rem;margin-bottom:12px;outline:none;transition:border .15s;font-family:inherit}\n";
    file << ".search:focus{border-color:var(--accent)}\n";
    file << ".pair{background:var(--surface);border:1px solid var(--border);border-radius:8px;";
    file << "padding:14px 16px;margin-bottom:7px;display:grid;grid-template-columns:62px 1fr;gap:12px;transition:border .15s}\n";
    file << ".pair:hover{border-color:#484f58}\n";
    file << ".sim-badge{text-align:center;padding:6px 0;border-radius:6px;font-weight:700;font-size:.82rem;";
    file << "font-family:var(--font-mono);background:var(--bg);border:1px solid;line-height:1.3}\n";
    file << ".sim-type{font-size:.58rem;font-weight:400;display:block;margin-top:2px;letter-spacing:.3px}\n";
    file << ".pair-body{min-width:0}\n";
    file << ".pair-funcs{font-size:.88rem;font-weight:600;margin-bottom:2px;display:flex;align-items:center;gap:6px;flex-wrap:wrap}\n";
    file << ".fn-name{color:var(--accent)}\n";
    file << ".fn-arrow{color:#484f58;font-size:.75rem}\n";
    file << ".pair-files{font-size:.72rem;color:var(--muted);margin-bottom:8px}\n";
    file << ".dispatch{font-size:.74rem;color:var(--muted);background:var(--bg);border-radius:4px;padding:6px 10px;";
    file << "border-left:2px solid var(--border);line-height:1.5}\n";
    // Pagination
    file << ".pagination{display:flex;align-items:center;gap:5px;margin-top:14px;flex-wrap:wrap}\n";
    file << ".pagination button{background:var(--surface2);border:1px solid var(--border);border-radius:5px;";
    file << "color:var(--muted);cursor:pointer;font-size:.76rem;padding:4px 9px;transition:all .15s}\n";
    file << ".pagination button:hover:not(:disabled){background:var(--border);color:var(--text)}\n";
    file << ".pagination button.active{background:#1f6feb;border-color:#1f6feb;color:#fff}\n";
    file << ".pagination button:disabled{opacity:.4;cursor:default}\n";
    file << ".pg-info{font-size:.74rem;color:var(--muted);margin-left:6px;font-family:var(--font-mono)}\n";
    // Semantic section
    file << ".sem-section{background:rgba(163,113,247,.04);border:1px solid rgba(163,113,247,.2);border-radius:10px;padding:18px 20px;margin-top:24px}\n";
    // Footer
    file << ".footer{text-align:center;color:#484f58;font-size:.72rem;padding:28px;border-top:1px solid var(--border2);margin-top:40px}\n";
    file << ".footer a{color:var(--muted)}\n";
    file << "</style>\n</head>\n<body>\n";

    // ── ENGINE BANNER ─────────────────────────────────────────────────────
    file << "<div class=\"eng-banner\">\n";
    file << "  <div class=\"eng-inner\">\n";
    file << "    <div class=\"eng-logo\">\n";
    file << "      <div class=\"eng-icon\">DS</div>\n";
    file << "      <div><div class=\"eng-name\">DeepScan Engine</div>";
    file << "<div class=\"eng-ver\">Code Intelligence Analysis Report</div></div>\n";
    file << "    </div>\n";
    file << "    <div class=\"eng-meta\">\n";
    file << "      <span class=\"eng-badge\">Generated: " << getCurrentTime() << "</span>\n";
    file << "      <span class=\"eng-badge\" style=\"color:var(--accent)\">" << functions.size() << " functions</span>\n";
    file << "    </div>\n";
    file << "  </div>\n";
    file << "</div>\n";

    file << "<div class=\"container\">\n";

    // ── EXECUTIVE SUMMARY ─────────────────────────────────────────────────
    file << "<div class=\"exec-grid\">\n";

    // Grade card (left)
    file << "  <div class=\"grade-card\">\n";
    file << "    <div class=\"grade-ring\" style=\"color:" << gradeColor << ";border-color:" << gradeColor << "\">"
         << grade << "</div>\n";
    file << "    <div class=\"grade-label\">Codebase Health</div>\n";
    file << "    <div class=\"grade-score\" style=\"color:" << gradeColor << "\">"
         << healthScore << "<span style=\"font-size:.8rem;color:var(--muted);font-weight:400\">/100</span></div>\n";
    file << "  </div>\n";

    // Meta panel (right)
    file << "  <div class=\"meta-panel\">\n";
    file << "    <div class=\"meta-path\">Scanned path: <strong>" << scannedPath << "</strong></div>\n";
    file << "    <div class=\"meta-badges\">\n";

    file << "      <div class=\"mbadge\"><span class=\"mbadge-val\" style=\"color:var(--accent)\">"
         << functions.size() << "</span><span class=\"mbadge-lbl\">Functions</span></div>\n";
    file << "      <div class=\"mbadge\"><span class=\"mbadge-val\" style=\"color:var(--red)\">"
         << duplicates.size() << "</span><span class=\"mbadge-lbl\">Dup Pairs</span></div>\n";
    file << "      <div class=\"mbadge\"><span class=\"mbadge-val\" style=\"color:var(--yellow)\">"
         << duplicationRate << "%</span><span class=\"mbadge-lbl\">Dup Rate</span></div>\n";
    file << "      <div class=\"mbadge\"><span class=\"mbadge-val\" style=\"color:var(--green)\">"
         << exactMatches << "</span><span class=\"mbadge-lbl\">Exact Copies</span></div>\n";
    if (semanticEnabled) {
        file << "      <div class=\"mbadge\"><span class=\"mbadge-val\" style=\"color:var(--purple)\">"
             << semanticDuplicates.size() << "</span><span class=\"mbadge-lbl\">Semantic</span></div>\n";
    }
    file << "    </div>\n";

    // Health bar
    double barPct = std::min(healthScore, 100.0);
    file << "    <div class=\"health-bar-wrap\">\n";
    file << "      <div class=\"health-bar\"><div class=\"health-bar-fill\" style=\"width:" << barPct << "%;background:"
         << gradeColor << "\"></div></div>\n";
    file << "      <span class=\"health-bar-pct\">" << barPct << "%</span>\n";
    file << "    </div>\n";
    file << "  </div>\n";
    file << "</div>\n";

    // ── TRIPLE-PASS DETECTION GRID ────────────────────────────────────────
    file << "<div class=\"pass-grid\">\n";

    // Pass 1: Exact
    file << "  <div class=\"pass-card\">\n";
    file << "    <div class=\"pass-head\">\n";
    file << "      <div class=\"pass-icon\" style=\"background:rgba(248,81,73,.15);color:#f85149\">P1</div>\n";
    file << "      <div class=\"pass-title\">Exact Clone Detection</div>\n";
    file << "    </div>\n";
    file << "    <div class=\"pass-num\" style=\"color:#f85149\">" << exactMatches << "</div>\n";
    file << "    <div class=\"pass-sub\">Normalized hash fingerprinting &mdash; 100% matches</div>\n";
    file << "  </div>\n";

    // Pass 2: Near-clone
    file << "  <div class=\"pass-card\">\n";
    file << "    <div class=\"pass-head\">\n";
    file << "      <div class=\"pass-icon\" style=\"background:rgba(210,153,34,.15);color:#d29922\">P2</div>\n";
    file << "      <div class=\"pass-title\">Near-Clone Analysis</div>\n";
    file << "    </div>\n";
    file << "    <div class=\"pass-num\" style=\"color:#d29922\">" << nearMatches << "</div>\n";
    file << "    <div class=\"pass-sub\">Levenshtein edit-distance + variable normalization</div>\n";
    file << "  </div>\n";

    // Pass 3: Semantic
    file << "  <div class=\"pass-card\">\n";
    file << "    <div class=\"pass-head\">\n";
    file << "      <div class=\"pass-icon\" style=\"background:rgba(163,113,247,.15);color:#a371f7\">P3</div>\n";
    file << "      <div class=\"pass-title\">Semantic Embedding</div>\n";
    file << "    </div>\n";
    if (semanticEnabled) {
        file << "    <div class=\"pass-num\" style=\"color:#a371f7\">" << semanticDuplicates.size() << "</div>\n";
        file << "    <div class=\"pass-sub\">CodeBERT cosine similarity &mdash; z-score outliers</div>\n";
    } else {
        file << "    <div class=\"pass-num\" style=\"color:#484f58\">&#x2014;</div>\n";
        file << "    <div class=\"pass-sub\" style=\"color:#484f58\">Disabled &mdash; pass <code>--semantic</code> to enable</div>\n";
    }
    file << "  </div>\n";
    file << "</div>\n";

    // ── CRITICAL ANOMALIES BOARD (top 5 highest-similarity pairs) ─────────
    if (!duplicates.empty()) {
        int topN = (int)std::min((size_t)5, duplicates.size());
        file << "<div class=\"anomaly-board\">\n";
        file << "  <div class=\"anomaly-head\">\n";
        file << "    <span style=\"color:#f85149;font-size:1rem\">&#9888;</span>\n";
        file << "    <span class=\"anomaly-title\">Critical Anomalies &mdash; Top " << topN << " Highest Similarity</span>\n";
        file << "  </div>\n";
        for (int i = 0; i < topN; i++) {
            const auto& p = duplicates[i];
            std::string sc = p.similarity >= 99.9 ? "#f85149" :
                             p.similarity >= 90.0 ? "#d29922" : "#30a14e";
            file << "  <div class=\"anomaly-row\">\n";
            file << "    <span class=\"anomaly-rank\">#" << (i+1) << "</span>\n";
            file << "    <span class=\"anomaly-sim\" style=\"color:" << sc << "\">" << p.similarity << "%</span>\n";
            file << "    <span class=\"anomaly-names\">"
                 << "<span style=\"color:var(--accent)\">" << p.func1.name << "</span>"
                 << " <span style=\"color:#484f58\">&#8596;</span> "
                 << "<span style=\"color:var(--accent)\">" << p.func2.name << "</span>"
                 << "</span>\n";
            file << "    <span class=\"anomaly-file\" title=\"" << p.func1.filename << " / " << p.func2.filename << "\">"
                 << basename(p.func1.filename) << " / " << basename(p.func2.filename) << "</span>\n";
            file << "  </div>\n";
        }
        file << "</div>\n";
    }

    // ── SIDE-BY-SIDE DIFF VIEW (top pair) ────────────────────────────────
    if (!duplicates.empty()) {
        const auto& top = duplicates[0];
        file << "<div class=\"diff-section\">\n";
        file << "  <div class=\"diff-header\">\n";
        file << "    <span class=\"diff-title\">Side-by-Side Diff &mdash; Highest Similarity Pair</span>\n";
        file << "    <span class=\"diff-meta\">" << top.similarity << "% similarity</span>\n";
        file << "  </div>\n";
        file << "  <div class=\"diff-grid\">\n";

        // Left side
        file << "    <div class=\"diff-side\">\n";
        file << "      <div class=\"diff-side-head\">" << top.func1.name
             << " &mdash; <span style=\"color:#484f58\">" << basename(top.func1.filename) << "</span></div>\n";
        file << "      <div class=\"diff-code\">\n";
        auto lines1 = splitLines(top.func1.body, 25);
        for (int ln = 0; ln < (int)lines1.size(); ln++) {
            file << "        <div class=\"diff-line\"><span class=\"diff-ln\">" << (ln+1) << "</span>"
                 << "<span class=\"diff-text\">" << escapeHtml(lines1[ln]) << "</span></div>\n";
        }
        file << "      </div>\n";
        file << "    </div>\n";

        // Right side
        file << "    <div class=\"diff-side\">\n";
        file << "      <div class=\"diff-side-head\">" << top.func2.name
             << " &mdash; <span style=\"color:#484f58\">" << basename(top.func2.filename) << "</span></div>\n";
        file << "      <div class=\"diff-code\">\n";
        auto lines2 = splitLines(top.func2.body, 25);
        for (int ln = 0; ln < (int)lines2.size(); ln++) {
            file << "        <div class=\"diff-line\"><span class=\"diff-ln\">" << (ln+1) << "</span>"
                 << "<span class=\"diff-text\">" << escapeHtml(lines2[ln]) << "</span></div>\n";
        }
        file << "      </div>\n";
        file << "    </div>\n";

        file << "  </div>\n";
        file << "</div>\n";
    }

    // ── DUPLICATE PAIRS (all, paginated) ─────────────────────────────────
    file << "<div class=\"section-head\">\n";
    file << "  <h2>Token-Based Duplicate Pairs</h2>\n";
    file << "  <span class=\"sbadge\">" << duplicates.size() << " pairs &middot; threshold-filtered</span>\n";
    file << "</div>\n";

    if (duplicates.empty()) {
        file << "<p style=\"color:var(--muted);font-size:.85rem\">No token-based duplicates found at the configured threshold.</p>\n";
    } else {
        file << "<input class=\"search\" type=\"text\" id=\"search\" placeholder=\"&#x1F50D; Filter by function name...\" oninput=\"filterPairs()\">\n";
        file << "<div id=\"pairs-container\">\n";

        for (const auto& p : duplicates) {
            std::string sc  = p.similarity >= 99.9 ? "#f85149" :
                              p.similarity >= 90.0 ? "#d29922" : "#30a14e";
            std::string tag = p.similarity >= 99.9 ? "EXACT" : "NEAR";
            file << "<div class=\"pair\" data-f1=\"" << p.func1.name << "\" data-f2=\"" << p.func2.name << "\">\n";
            file << "  <div class=\"sim-badge\" style=\"color:" << sc << ";border-color:" << sc << "\">"
                 << p.similarity << "%<span class=\"sim-type\">" << tag << "</span></div>\n";
            file << "  <div class=\"pair-body\">\n";
            file << "    <div class=\"pair-funcs\">"
                 << "<span class=\"fn-name\">" << p.func1.name << "</span>"
                 << "<span class=\"fn-arrow\">&#8596;</span>"
                 << "<span class=\"fn-name\">" << p.func2.name << "</span>"
                 << "</div>\n";
            file << "    <div class=\"pair-files\">"
                 << "<span title=\"" << p.func1.filename << "\">" << basename(p.func1.filename) << "</span>"
                 << " <span style=\"color:#484f58\">/</span> "
                 << "<span title=\"" << p.func2.filename << "\">" << basename(p.func2.filename) << "</span>"
                 << "</div>\n";
            file << "    <div class=\"dispatch\">" << shortenPaths(buildSuggestion(p, false)) << "</div>\n";
            file << "  </div>\n";
            file << "</div>\n";
        }

        file << "</div>\n";
        file << "<div class=\"pagination\" id=\"pagination\"></div>\n";
    }

    // ── SEMANTIC SECTION ──────────────────────────────────────────────────
    if (semanticEnabled && !semanticDuplicates.empty()) {
        file << "<div class=\"sem-section\">\n";
        file << "  <div class=\"section-head\" style=\"margin-bottom:12px\">\n";
        file << "    <h2 style=\"color:#a371f7\">Semantic Duplicates</h2>\n";
        file << "    <span class=\"sbadge\" style=\"border-color:rgba(163,113,247,.3);color:#a371f7\">";
        file << "CodeBERT &middot; z-score outliers &middot; " << semanticDuplicates.size() << " pairs</span>\n";
        file << "  </div>\n";
        for (const auto& p : semanticDuplicates) {
            std::string sc = p.similarity >= 95.0 ? "#f85149" :
                             p.similarity >= 85.0 ? "#d29922" : "#a371f7";
            file << "  <div class=\"pair\" style=\"border-color:rgba(163,113,247,.15)\">\n";
            file << "    <div class=\"sim-badge\" style=\"color:" << sc << ";border-color:" << sc << "\">"
                 << p.similarity << "%<span class=\"sim-type\">SEM</span></div>\n";
            file << "    <div class=\"pair-body\">\n";
            file << "      <div class=\"pair-funcs\">"
                 << "<span class=\"fn-name\">" << p.func1.name << "</span>"
                 << "<span class=\"fn-arrow\">&#8596;</span>"
                 << "<span class=\"fn-name\">" << p.func2.name << "</span>"
                 << " <span style=\"font-size:.68rem;color:#a371f7;margin-left:4px\">z=" << p.zscore << "</span>"
                 << "</div>\n";
            file << "      <div class=\"pair-files\">"
                 << "<span title=\"" << p.func1.filename << "\">" << basename(p.func1.filename) << "</span>"
                 << " <span style=\"color:#484f58\">/</span> "
                 << "<span title=\"" << p.func2.filename << "\">" << basename(p.func2.filename) << "</span>"
                 << "</div>\n";
            file << "      <div class=\"dispatch\">" << shortenPaths(buildSuggestion(p, true)) << "</div>\n";
            file << "    </div>\n";
            file << "  </div>\n";
        }
        file << "</div>\n";
    }

    // ── FOOTER ────────────────────────────────────────────────────────────
    file << "<div class=\"footer\">DeepScan Engine &nbsp;&middot;&nbsp; AI-Generated Code Duplication Detector";
    file << " &nbsp;&middot;&nbsp; MIT License &nbsp;&middot;&nbsp; Al-ameen Ajala</div>\n";
    file << "</div>\n";  // end .container

    // ── JAVASCRIPT (no raw strings — avoids )" termination issue) ─────────
    file << "<script>\n";
    file << "var PAGE_SIZE = 25;\n";
    file << "var currentPage = 0;\n";
    file << "var visiblePairs = [];\n";
    file << "var allPairs = [];\n";
    file << "window.onload = function() {\n";
    file << "    allPairs = Array.from(document.querySelectorAll('#pairs-container .pair'));\n";
    file << "    visiblePairs = allPairs;\n";
    file << "    showPage(0);\n";
    file << "};\n";
    file << "function filterPairs() {\n";
    file << "    var q = document.getElementById('search').value.toLowerCase().trim();\n";
    file << "    visiblePairs = q ? allPairs.filter(function(c) {\n";
    file << "        var f1 = (c.getAttribute('data-f1') || '').toLowerCase();\n";
    file << "        var f2 = (c.getAttribute('data-f2') || '').toLowerCase();\n";
    file << "        return f1.indexOf(q) >= 0 || f2.indexOf(q) >= 0;\n";
    file << "    }) : allPairs;\n";
    file << "    showPage(0);\n";
    file << "}\n";
    file << "function showPage(page) {\n";
    file << "    currentPage = page;\n";
    file << "    var start = page * PAGE_SIZE, end = start + PAGE_SIZE;\n";
    file << "    allPairs.forEach(function(c) { c.style.display = 'none'; });\n";
    file << "    visiblePairs.forEach(function(c, i) {\n";
    file << "        if (i >= start && i < end) c.style.display = '';\n";
    file << "    });\n";
    file << "    renderPagination();\n";
    file << "}\n";
    file << "function renderPagination() {\n";
    file << "    var pg = document.getElementById('pagination');\n";
    file << "    if (!pg) return;\n";
    file << "    var total = Math.ceil(visiblePairs.length / PAGE_SIZE);\n";
    file << "    if (total <= 1) { pg.innerHTML = ''; return; }\n";
    file << "    var h = '';\n";
    file << "    var prev = currentPage - 1;\n";
    file << "    var next = currentPage + 1;\n";
    file << "    if (currentPage === 0) {\n";
    file << "        h += '<button disabled>&larr; Prev</button>';\n";
    file << "    } else {\n";
    file << "        h += '<button onclick=\"showPage(' + prev + ')\">&larr; Prev</button>';\n";
    file << "    }\n";
    file << "    for (var i = 0; i < total; i++) {\n";
    file << "        if (i === 0 || i === total-1 || Math.abs(i - currentPage) <= 2) {\n";
    file << "            var cls = (i === currentPage) ? ' class=\"active\"' : '';\n";
    file << "            h += '<button' + cls + ' onclick=\"showPage(' + i + ')\">' + (i+1) + '</button>';\n";
    file << "        } else if (Math.abs(i - currentPage) === 3) {\n";
    file << "            h += '<span style=\"color:#484f58;padding:0 4px\">&hellip;</span>';\n";
    file << "        }\n";
    file << "    }\n";
    file << "    if (currentPage === total - 1) {\n";
    file << "        h += '<button disabled>Next &rarr;</button>';\n";
    file << "    } else {\n";
    file << "        h += '<button onclick=\"showPage(' + next + ')\">Next &rarr;</button>';\n";
    file << "    }\n";
    file << "    var s = currentPage * PAGE_SIZE + 1;\n";
    file << "    var e = Math.min((currentPage + 1) * PAGE_SIZE, visiblePairs.length);\n";
    file << "    h += '<span class=\"pg-info\">Showing ' + s + '&ndash;' + e + ' of ' + visiblePairs.length + '</span>';\n";
    file << "    pg.innerHTML = h;\n";
    file << "}\n";
    file << "</script>\n";
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