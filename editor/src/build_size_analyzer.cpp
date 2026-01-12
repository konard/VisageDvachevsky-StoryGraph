/**
 * @file build_size_analyzer.cpp
 * @brief Build Size Analyzer implementation for NovelMind
 *
 * Provides comprehensive build size analysis:
 * - Per-category breakdown (images, audio, scripts, etc.)
 * - Duplicate detection via content hashing
 * - Unused asset detection
 * - Optimization suggestions
 * - Export to JSON/HTML/CSV
 */

#include "NovelMind/editor/build_size_analyzer.hpp"
#include "NovelMind/editor/build_size_report.hpp"
#include "NovelMind/editor/build_size_scanner.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;

namespace NovelMind::editor {

// ============================================================================
// BuildSizeAnalyzer Implementation
// ============================================================================

BuildSizeAnalyzer::BuildSizeAnalyzer() = default;

void BuildSizeAnalyzer::setProjectPath(const std::string &projectPath) {
  m_projectPath = projectPath;
}

void BuildSizeAnalyzer::setConfig(const BuildSizeAnalysisConfig &config) {
  m_config = config;
}

Result<BuildSizeAnalysis> BuildSizeAnalyzer::analyze() {
  auto startTime = std::chrono::steady_clock::now();

  // Notify listeners
  for (auto *listener : m_listeners) {
    listener->onAnalysisStarted();
  }

  // Reset analysis
  m_analysis = BuildSizeAnalysis{};
  m_hashToFiles.clear();

  try {
    BuildSizeScanner scanner;

    // Scan all assets
    reportProgress("Scanning assets...", 0.0f);
    scanner.scanAssets(m_projectPath, m_config, m_analysis.assets);
    m_analysis.totalFileCount = static_cast<i32>(m_analysis.assets.size());

    // Analyze each asset
    reportProgress("Analyzing assets...", 0.2f);
    for (size_t i = 0; i < m_analysis.assets.size(); i++) {
      scanner.analyzeAsset(m_analysis.assets[i], m_config, m_hashToFiles);

      // Update totals
      m_analysis.totalOriginalSize += m_analysis.assets[i].originalSize;
      m_analysis.totalCompressedSize += m_analysis.assets[i].compressedSize;

      f32 progress = 0.2f + 0.3f * static_cast<f32>(i) /
                                static_cast<f32>(m_analysis.assets.size());
      reportProgress("Analyzing: " + m_analysis.assets[i].name, progress);
    }

    // Detect duplicates
    if (m_config.detectDuplicates) {
      reportProgress("Detecting duplicates...", 0.5f);
      m_analysis.totalWastedSpace = scanner.detectDuplicates(
          m_hashToFiles, m_analysis.assets, m_analysis.duplicates);
    }

    // Detect unused assets
    if (m_config.detectUnused) {
      reportProgress("Detecting unused assets...", 0.6f);
      m_analysis.unusedSpace = scanner.detectUnused(
          m_projectPath, m_analysis.assets, m_analysis.unusedAssets);
    }

    // Generate suggestions
    if (m_config.generateSuggestions) {
      reportProgress("Generating suggestions...", 0.8f);
      m_analysis.potentialSavings = scanner.generateSuggestions(
          m_analysis.assets, m_analysis.duplicates, m_analysis.unusedAssets,
          m_analysis.suggestions);
    }

    // Calculate summaries
    reportProgress("Calculating summaries...", 0.9f);
    scanner.calculateSummaries(m_analysis.assets, m_analysis.totalOriginalSize,
                               m_analysis.categorySummaries);

    // Calculate overall compression ratio
    if (m_analysis.totalOriginalSize > 0) {
      m_analysis.overallCompressionRatio =
          static_cast<f32>(m_analysis.totalCompressedSize) /
          static_cast<f32>(m_analysis.totalOriginalSize);
    }

    // Finalize
    auto endTime = std::chrono::steady_clock::now();
    m_analysis.analysisTimeMs =
        std::chrono::duration<f64, std::milli>(endTime - startTime).count();
    m_analysis.analysisTimestamp = static_cast<u64>(
        std::chrono::system_clock::now().time_since_epoch().count());

    reportProgress("Analysis complete", 1.0f);

    // Notify listeners
    for (auto *listener : m_listeners) {
      listener->onAnalysisCompleted(m_analysis);
    }

    return Result<BuildSizeAnalysis>::ok(m_analysis);

  } catch (const std::exception &e) {
    return Result<BuildSizeAnalysis>::error(std::string("Analysis failed: ") +
                                            e.what());
  }
}

void BuildSizeAnalyzer::addListener(IBuildSizeListener *listener) {
  if (listener) {
    m_listeners.push_back(listener);
  }
}

void BuildSizeAnalyzer::removeListener(IBuildSizeListener *listener) {
  m_listeners.erase(
      std::remove(m_listeners.begin(), m_listeners.end(), listener),
      m_listeners.end());
}

Result<void>
BuildSizeAnalyzer::applyOptimization(const OptimizationSuggestion &suggestion) {
  try {
    switch (suggestion.type) {
    case OptimizationSuggestion::Type::RemoveDuplicate: {
      // Remove the duplicate file
      if (fs::exists(suggestion.assetPath)) {
        fs::remove(suggestion.assetPath);
        return Result<void>::ok();
      }
      return Result<void>::error("File not found: " + suggestion.assetPath);
    }

    case OptimizationSuggestion::Type::RemoveUnused: {
      // Remove the unused asset
      if (fs::exists(suggestion.assetPath)) {
        fs::remove(suggestion.assetPath);
        return Result<void>::ok();
      }
      return Result<void>::error("File not found: " + suggestion.assetPath);
    }

    case OptimizationSuggestion::Type::ResizeImage:
    case OptimizationSuggestion::Type::CompressImage:
    case OptimizationSuggestion::Type::CompressAudio:
    case OptimizationSuggestion::Type::ConvertFormat:
    case OptimizationSuggestion::Type::EnableCompression:
    case OptimizationSuggestion::Type::ReduceQuality:
    case OptimizationSuggestion::Type::SplitAsset:
    case OptimizationSuggestion::Type::MergeAssets:
      // These require manual intervention or external tools
      return Result<void>::error(
          "This optimization type requires manual intervention");

    default:
      return Result<void>::error("Unknown optimization type");
    }
  } catch (const std::exception &e) {
    return Result<void>::error(std::string("Failed to apply optimization: ") +
                                e.what());
  }
}

Result<void> BuildSizeAnalyzer::applyAllAutoOptimizations() {
  i32 applied = 0;
  for (const auto &suggestion : m_analysis.suggestions) {
    if (suggestion.canAutoFix) {
      auto result = applyOptimization(suggestion);
      if (result.isOk()) {
        applied++;
      }
    }
  }

  if (applied > 0) {
    // Re-analyze after optimizations
    return analyze().isOk() ? Result<void>::ok()
                            : Result<void>::error("Re-analysis failed");
  }

  return Result<void>::ok();
}

Result<void> BuildSizeAnalyzer::removeDuplicates() {
  try {
    i32 removedCount = 0;
    std::vector<std::string> errors;

    for (const auto &dupGroup : m_analysis.duplicates) {
      // Keep the first file, remove all duplicates
      for (size_t i = 1; i < dupGroup.paths.size(); i++) {
        const auto &path = dupGroup.paths[i];
        if (fs::exists(path)) {
          try {
            fs::remove(path);
            removedCount++;
          } catch (const std::exception &e) {
            errors.push_back("Failed to remove " + path + ": " + e.what());
          }
        }
      }
    }

    if (!errors.empty()) {
      std::ostringstream oss;
      oss << "Removed " << removedCount << " duplicates, but encountered "
          << errors.size() << " errors:\n";
      for (const auto &error : errors) {
        oss << "  - " << error << "\n";
      }
      return Result<void>::error(oss.str());
    }

    return Result<void>::ok();
  } catch (const std::exception &e) {
    return Result<void>::error(std::string("Failed to remove duplicates: ") +
                                e.what());
  }
}

Result<void> BuildSizeAnalyzer::removeUnusedAssets() {
  try {
    i32 removedCount = 0;
    std::vector<std::string> errors;

    for (const auto &assetPath : m_analysis.unusedAssets) {
      if (fs::exists(assetPath)) {
        try {
          fs::remove(assetPath);
          removedCount++;
        } catch (const std::exception &e) {
          errors.push_back("Failed to remove " + assetPath + ": " + e.what());
        }
      }
    }

    if (!errors.empty()) {
      std::ostringstream oss;
      oss << "Removed " << removedCount << " unused assets, but encountered "
          << errors.size() << " errors:\n";
      for (const auto &error : errors) {
        oss << "  - " << error << "\n";
      }
      return Result<void>::error(oss.str());
    }

    return Result<void>::ok();
  } catch (const std::exception &e) {
    return Result<void>::error(std::string("Failed to remove unused assets: ") +
                                e.what());
  }
}

Result<std::string> BuildSizeAnalyzer::exportAsJson() const {
  BuildSizeReport report;
  return report.exportAsJson(m_analysis);
}

Result<void>
BuildSizeAnalyzer::exportAsHtml(const std::string &outputPath) const {
  BuildSizeReport report;
  return report.exportAsHtml(m_analysis, outputPath);
}

Result<void>
BuildSizeAnalyzer::exportAsCsv(const std::string &outputPath) const {
  BuildSizeReport report;
  return report.exportAsCsv(m_analysis, outputPath);
}

void BuildSizeAnalyzer::reportProgress(const std::string &task, f32 progress) {
  for (auto *listener : m_listeners) {
    listener->onAnalysisProgress(task, progress);
  }
}

} // namespace NovelMind::editor
