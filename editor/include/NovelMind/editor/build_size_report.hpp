#pragma once

/**
 * @file build_size_report.hpp
 * @brief Report generation and export for Build Size Analyzer
 *
 * This module handles:
 * - Export to JSON format
 * - Export to HTML reports
 * - Export to CSV format
 */

#include "NovelMind/core/result.hpp"
#include "NovelMind/editor/build_size_analyzer.hpp"
#include <string>

namespace NovelMind::editor {

/**
 * @brief Handles report generation and export
 *
 * This class provides export functionality for build size analysis results
 * in various formats (JSON, HTML, CSV).
 */
class BuildSizeReport {
public:
  BuildSizeReport() = default;
  ~BuildSizeReport() = default;

  /**
   * @brief Export analysis as JSON
   * @param analysis Analysis results
   * @return JSON string on success, error message on failure
   */
  Result<std::string> exportAsJson(const BuildSizeAnalysis& analysis) const;

  /**
   * @brief Export analysis as HTML report
   * @param analysis Analysis results
   * @param outputPath Output file path
   * @return Success or error result
   */
  Result<void> exportAsHtml(const BuildSizeAnalysis& analysis, const std::string& outputPath) const;

  /**
   * @brief Export analysis as CSV
   * @param analysis Analysis results
   * @param outputPath Output file path
   * @return Success or error result
   */
  Result<void> exportAsCsv(const BuildSizeAnalysis& analysis, const std::string& outputPath) const;
};

} // namespace NovelMind::editor
