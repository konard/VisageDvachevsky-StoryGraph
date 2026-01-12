#pragma once

/**
 * @file build_size_scanner.hpp
 * @brief Asset scanning and analysis for Build Size Analyzer
 *
 * This module handles:
 * - File system scanning for assets
 * - Per-asset analysis (size, metadata, etc.)
 * - Duplicate detection via content hashing
 * - Unused asset detection
 * - Optimization suggestion generation
 */

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include "NovelMind/editor/build_size_analyzer.hpp"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace NovelMind::editor {

/**
 * @brief Handles asset scanning and analysis
 *
 * This class provides the core scanning and analysis functionality
 * for the Build Size Analyzer. It scans the project directory,
 * analyzes individual assets, detects duplicates and unused assets,
 * and generates optimization suggestions.
 */
class BuildSizeScanner {
public:
  BuildSizeScanner() = default;
  ~BuildSizeScanner() = default;

  /**
   * @brief Scan project directory for assets
   * @param projectPath Path to project root
   * @param config Analysis configuration
   * @param assets Output vector for discovered assets
   */
  void scanAssets(const std::string& projectPath, const BuildSizeAnalysisConfig& config,
                  std::vector<AssetSizeInfo>& assets);

  /**
   * @brief Analyze a single asset
   * @param info Asset info to analyze (will be updated with analysis results)
   * @param config Analysis configuration
   * @param hashToFiles Map for tracking file hashes (for duplicate detection)
   */
  void analyzeAsset(AssetSizeInfo& info, const BuildSizeAnalysisConfig& config,
                    std::unordered_map<std::string, std::vector<std::string>>& hashToFiles);

  /**
   * @brief Detect duplicate assets
   * @param hashToFiles Map of file hashes to file paths
   * @param assets All assets to check
   * @param duplicates Output vector for duplicate groups
   * @return Total wasted space from duplicates
   */
  u64 detectDuplicates(const std::unordered_map<std::string, std::vector<std::string>>& hashToFiles,
                       std::vector<AssetSizeInfo>& assets, std::vector<DuplicateGroup>& duplicates);

  /**
   * @brief Detect unused assets
   * @param projectPath Path to project root
   * @param assets All assets to check
   * @param unusedAssets Output vector for unused asset paths
   * @return Total space occupied by unused assets
   */
  u64 detectUnused(const std::string& projectPath, std::vector<AssetSizeInfo>& assets,
                   std::vector<std::string>& unusedAssets);

  /**
   * @brief Generate optimization suggestions
   * @param assets All assets to analyze
   * @param duplicates Detected duplicate groups
   * @param unusedAssets List of unused asset paths
   * @param suggestions Output vector for suggestions
   * @return Total potential savings from suggestions
   */
  u64 generateSuggestions(const std::vector<AssetSizeInfo>& assets,
                          const std::vector<DuplicateGroup>& duplicates,
                          const std::vector<std::string>& unusedAssets,
                          std::vector<OptimizationSuggestion>& suggestions);

  /**
   * @brief Calculate category summaries
   * @param assets All assets
   * @param totalOriginalSize Total original size
   * @param categorySummaries Output vector for summaries
   */
  void calculateSummaries(const std::vector<AssetSizeInfo>& assets, u64 totalOriginalSize,
                          std::vector<CategorySummary>& categorySummaries);

private:
  /**
   * @brief Parse a file for asset references
   * @param filePath Path to file to parse
   * @param referencedAssets Set to add found references to
   */
  void parseFileForAssetReferences(const std::string& filePath,
                                   std::unordered_set<std::string>& referencedAssets);

  /**
   * @brief Compute SHA-256 hash of file content
   * @param path File path
   * @return Hex-encoded hash string
   */
  std::string computeFileHash(const std::string& path);

  /**
   * @brief Detect compression type from file extension
   * @param path File path
   * @return Detected compression type
   */
  CompressionType detectCompression(const std::string& path);

  /**
   * @brief Categorize asset by file extension
   * @param path File path
   * @return Asset category
   */
  AssetCategory categorizeAsset(const std::string& path);
};

} // namespace NovelMind::editor
