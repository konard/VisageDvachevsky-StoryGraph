/**
 * @file build_size_report.cpp
 * @brief Report generation and export implementation
 */

#include "NovelMind/editor/build_size_report.hpp"
#include "NovelMind/editor/build_size_ui.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

namespace NovelMind::editor {

Result<std::string>
BuildSizeReport::exportAsJson(const BuildSizeAnalysis &analysis) const {
  std::ostringstream json;
  json << "{\n";
  json << "  \"totalOriginalSize\": " << analysis.totalOriginalSize << ",\n";
  json << "  \"totalCompressedSize\": " << analysis.totalCompressedSize
       << ",\n";
  json << "  \"totalFileCount\": " << analysis.totalFileCount << ",\n";
  json << "  \"overallCompressionRatio\": " << analysis.overallCompressionRatio
       << ",\n";
  json << "  \"totalWastedSpace\": " << analysis.totalWastedSpace << ",\n";
  json << "  \"unusedSpace\": " << analysis.unusedSpace << ",\n";
  json << "  \"potentialSavings\": " << analysis.potentialSavings << ",\n";
  json << "  \"analysisTimeMs\": " << analysis.analysisTimeMs << ",\n";

  // Categories
  json << "  \"categories\": [\n";
  for (size_t i = 0; i < analysis.categorySummaries.size(); i++) {
    const auto &cat = analysis.categorySummaries[i];
    json << "    {\n";
    json << "      \"category\": " << static_cast<int>(cat.category) << ",\n";
    json << "      \"fileCount\": " << cat.fileCount << ",\n";
    json << "      \"totalOriginalSize\": " << cat.totalOriginalSize << ",\n";
    json << "      \"totalCompressedSize\": " << cat.totalCompressedSize
         << ",\n";
    json << "      \"percentageOfTotal\": " << cat.percentageOfTotal << "\n";
    json << "    }";
    if (i < analysis.categorySummaries.size() - 1)
      json << ",";
    json << "\n";
  }
  json << "  ],\n";

  // Duplicates count
  json << "  \"duplicateGroups\": " << analysis.duplicates.size() << ",\n";

  // Unused assets count
  json << "  \"unusedAssets\": " << analysis.unusedAssets.size() << ",\n";

  // Suggestions count
  json << "  \"suggestions\": " << analysis.suggestions.size() << "\n";

  json << "}\n";

  return Result<std::string>::ok(json.str());
}

Result<void>
BuildSizeReport::exportAsHtml(const BuildSizeAnalysis &analysis,
                              const std::string &outputPath) const {
  std::ofstream file(outputPath);
  if (!file.is_open()) {
    return Result<void>::error("Cannot create HTML file: " + outputPath);
  }

  file << "<!DOCTYPE html>\n";
  file << "<html>\n";
  file << "<head>\n";
  file << "  <title>Build Size Analysis Report</title>\n";
  file << "  <style>\n";
  file << "    body { font-family: Arial, sans-serif; margin: 20px; "
          "background: #1e1e1e; color: #d4d4d4; }\n";
  file << "    h1 { color: #569cd6; }\n";
  file << "    h2 { color: #4ec9b0; }\n";
  file << "    table { border-collapse: collapse; width: 100%; margin-bottom: "
          "20px; }\n";
  file << "    th, td { border: 1px solid #3c3c3c; padding: 8px; text-align: "
          "left; }\n";
  file << "    th { background-color: #252526; }\n";
  file << "    tr:nth-child(even) { background-color: #2d2d30; }\n";
  file << "    .warning { color: #ce9178; }\n";
  file << "    .error { color: #f14c4c; }\n";
  file << "    .size { text-align: right; }\n";
  file << "  </style>\n";
  file << "</head>\n";
  file << "<body>\n";

  file << "  <h1>Build Size Analysis Report</h1>\n";

  // Summary
  file << "  <h2>Summary</h2>\n";
  file << "  <table>\n";
  file << "    <tr><th>Metric</th><th>Value</th></tr>\n";
  file << "    <tr><td>Total Size</td><td class='size'>"
       << SizeVisualization::formatBytes(analysis.totalOriginalSize)
       << "</td></tr>\n";
  file << "    <tr><td>File Count</td><td class='size'>"
       << analysis.totalFileCount << "</td></tr>\n";
  file << "    <tr><td>Compression Ratio</td><td class='size'>" << std::fixed
       << std::setprecision(2) << (analysis.overallCompressionRatio * 100)
       << "%</td></tr>\n";
  file << "    <tr><td>Wasted Space (Duplicates)</td><td class='size'>"
       << SizeVisualization::formatBytes(analysis.totalWastedSpace)
       << "</td></tr>\n";
  file << "    <tr><td>Unused Space</td><td class='size'>"
       << SizeVisualization::formatBytes(analysis.unusedSpace)
       << "</td></tr>\n";
  file << "    <tr><td>Potential Savings</td><td class='size'>"
       << SizeVisualization::formatBytes(analysis.potentialSavings)
       << "</td></tr>\n";
  file << "  </table>\n";

  // Categories
  file << "  <h2>Size by Category</h2>\n";
  file << "  <table>\n";
  file << "    <tr><th>Category</th><th>Files</th><th>Size</th><th>% of "
          "Total</th></tr>\n";
  for (const auto &cat : analysis.categorySummaries) {
    std::string catName;
    switch (cat.category) {
    case AssetCategory::Images:
      catName = "Images";
      break;
    case AssetCategory::Audio:
      catName = "Audio";
      break;
    case AssetCategory::Scripts:
      catName = "Scripts";
      break;
    case AssetCategory::Fonts:
      catName = "Fonts";
      break;
    case AssetCategory::Video:
      catName = "Video";
      break;
    case AssetCategory::Data:
      catName = "Data";
      break;
    default:
      catName = "Other";
      break;
    }
    file << "    <tr><td>" << catName << "</td><td class='size'>"
         << cat.fileCount << "</td><td class='size'>"
         << SizeVisualization::formatBytes(cat.totalOriginalSize)
         << "</td><td class='size'>" << std::fixed << std::setprecision(1)
         << cat.percentageOfTotal << "%</td></tr>\n";
  }
  file << "  </table>\n";

  // Suggestions
  if (!analysis.suggestions.empty()) {
    file << "  <h2>Optimization Suggestions</h2>\n";
    file << "  <table>\n";
    file << "    "
            "<tr><th>Priority</th><th>Type</th><th>Asset</th><th>Description</"
            "th><th>Est. Savings</th></tr>\n";
    for (const auto &sug : analysis.suggestions) {
      std::string priority;
      std::string priorityClass;
      switch (sug.priority) {
      case OptimizationSuggestion::Priority::Critical:
        priority = "Critical";
        priorityClass = "error";
        break;
      case OptimizationSuggestion::Priority::High:
        priority = "High";
        priorityClass = "warning";
        break;
      case OptimizationSuggestion::Priority::Medium:
        priority = "Medium";
        break;
      default:
        priority = "Low";
        break;
      }

      file << "    <tr><td class='" << priorityClass << "'>" << priority
           << "</td><td>" << static_cast<int>(sug.type) << "</td><td>"
           << sug.assetPath << "</td><td>" << sug.description
           << "</td><td class='size'>"
           << SizeVisualization::formatBytes(sug.estimatedSavings)
           << "</td></tr>\n";
    }
    file << "  </table>\n";
  }

  file << "</body>\n";
  file << "</html>\n";

  file.close();
  return Result<void>::ok();
}

Result<void>
BuildSizeReport::exportAsCsv(const BuildSizeAnalysis &analysis,
                             const std::string &outputPath) const {
  std::ofstream file(outputPath);
  if (!file.is_open()) {
    return Result<void>::error("Cannot create CSV file: " + outputPath);
  }

  // Header
  file << "Path,Name,Category,Original Size,Compressed Size,Compression "
          "Ratio,Is Duplicate,Is Unused\n";

  // Assets
  for (const auto &asset : analysis.assets) {
    std::string catName;
    switch (asset.category) {
    case AssetCategory::Images:
      catName = "Images";
      break;
    case AssetCategory::Audio:
      catName = "Audio";
      break;
    case AssetCategory::Scripts:
      catName = "Scripts";
      break;
    case AssetCategory::Fonts:
      catName = "Fonts";
      break;
    case AssetCategory::Video:
      catName = "Video";
      break;
    case AssetCategory::Data:
      catName = "Data";
      break;
    default:
      catName = "Other";
      break;
    }

    file << "\"" << asset.path << "\","
         << "\"" << asset.name << "\"," << catName << "," << asset.originalSize
         << "," << asset.compressedSize << "," << asset.compressionRatio << ","
         << (asset.isDuplicate ? "Yes" : "No") << ","
         << (asset.isUnused ? "Yes" : "No") << "\n";
  }

  file.close();
  return Result<void>::ok();
}

} // namespace NovelMind::editor
