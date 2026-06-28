#pragma once

#include <string>
#include <vector>
#include <ostream>
#include "detector.h"

// semanticDuplicates/semanticEnabled default to "off" so every existing
// call site that doesn't know about semantic detection still compiles and
// behaves exactly as before. Pass semanticEnabled=true (even with an empty
// semanticDuplicates vector) to make a report explicitly say "ran semantic
// detection, found nothing" instead of staying silent about it.
void writeReport(std::ostream& out,
                 const std::vector<DuplicatePair>& duplicates,
                 const std::vector<Function>& functions,
                 const std::string& scannedPath,
                 const std::vector<DuplicatePair>& semanticDuplicates = {},
                 bool semanticEnabled = false);

void saveReportToFile(const std::string& filename,
                      const std::vector<DuplicatePair>& duplicates,
                      const std::vector<Function>& functions,
                      const std::string& scannedPath,
                      const std::vector<DuplicatePair>& semanticDuplicates = {},
                      bool semanticEnabled = false);

void writeHtmlReport(const std::string& filename,
                      const std::vector<DuplicatePair>& duplicates,
                      const std::vector<Function>& functions,
                      const std::string& scannedPath,
                      const std::vector<DuplicatePair>& semanticDuplicates = {},
                      bool semanticEnabled = false);

void writeJsonReport(const std::string& filename,
                      const std::vector<DuplicatePair>& duplicates,
                      const std::vector<Function>& functions,
                      const std::string& scannedPath,
                      const std::vector<DuplicatePair>& semanticDuplicates = {},
                      bool semanticEnabled = false);