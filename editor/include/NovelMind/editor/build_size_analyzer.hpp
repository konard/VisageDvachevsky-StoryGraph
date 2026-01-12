#pragma once

/**
 * @file build_size_analyzer.hpp
 * @brief Build Size Analyzer for NovelMind
 *
 * Analyzes and visualizes build size:
 * - Asset size breakdown by type
 * - Duplicate detection
 * - Compression analysis
 * - Optimization suggestions
 */

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include "NovelMind/renderer/renderer.hpp"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace NovelMind::editor {

/**
 * @brief Asset category for analysis
 */
enum class AssetCategory : u8 {
  Images,
  Audio,
  Scripts,
  Fonts,
  Video,
  Data,
  Other
};

/**
 * @brief Compression type
 */
enum class CompressionType : u8 { None, Lz4, Zstd, Png, Jpeg, Ogg, Custom };

/**
 * @brief Size information for a single asset
 */
struct AssetSizeInfo {
  std::string path;
  std::string name;
  AssetCategory category;

  u64 originalSize = 0;   // Uncompressed size
  u64 compressedSize = 0; // Compressed size
  CompressionType compression = CompressionType::None;
  f32 compressionRatio = 1.0f;

  // Image-specific
  i32 imageWidth = 0;
  i32 imageHeight = 0;
  i32 imageBitDepth = 0;
  bool hasMipmaps = false;

  // Audio-specific
  f32 audioDuration = 0.0f;
  i32 audioSampleRate = 0;
  i32 audioChannels = 0;

  // Analysis
  bool isUnused = false;
  bool isDuplicate = false;
  std::string duplicateOf; // Path of original if duplicate
  bool isOversized = false;
  std::vector<std::string> optimizationSuggestions;
};

/**
 * @brief Category summary
 */
struct CategorySummary {
  AssetCategory category;
  i32 fileCount = 0;
  u64 totalOriginalSize = 0;
  u64 totalCompressedSize = 0;
  f32 averageCompressionRatio = 1.0f;
  f32 percentageOfTotal = 0.0f;

  std::vector<std::string> topAssets; // Top 5 largest assets
};

/**
 * @brief Duplicate group
 */
struct DuplicateGroup {
  std::string hash; // Content hash
  std::vector<std::string> paths;
  u64 singleFileSize = 0;
  u64 wastedSpace = 0; // Total wasted space
};

/**
 * @brief Optimization suggestion
 */
struct OptimizationSuggestion {
  enum class Priority : u8 { Low, Medium, High, Critical };

  enum class Type : u8 {
    ResizeImage,
    CompressImage,
    CompressAudio,
    RemoveDuplicate,
    RemoveUnused,
    ConvertFormat,
    EnableCompression,
    ReduceQuality,
    SplitAsset,
    MergeAssets
  };

  Priority priority;
  Type type;
  std::string assetPath;
  std::string description;
  u64 estimatedSavings = 0;
  bool canAutoFix = false;
};

/**
 * @brief Complete build size analysis
 */
struct BuildSizeAnalysis {
  // Overall
  u64 totalOriginalSize = 0;
  u64 totalCompressedSize = 0;
  i32 totalFileCount = 0;
  f32 overallCompressionRatio = 1.0f;

  // Per-category
  std::vector<CategorySummary> categorySummaries;

  // All assets
  std::vector<AssetSizeInfo> assets;

  // Duplicates
  std::vector<DuplicateGroup> duplicates;
  u64 totalWastedSpace = 0;

  // Unused assets
  std::vector<std::string> unusedAssets;
  u64 unusedSpace = 0;

  // Optimization suggestions
  std::vector<OptimizationSuggestion> suggestions;
  u64 potentialSavings = 0;

  // Generation info
  u64 analysisTimestamp = 0;
  f64 analysisTimeMs = 0.0;
};

/**
 * @brief Configuration for build size analysis
 */
struct BuildSizeAnalysisConfig {
  bool analyzeImages = true;
  bool analyzeAudio = true;
  bool analyzeScripts = true;
  bool analyzeFonts = true;
  bool analyzeVideo = true;
  bool analyzeOther = true;

  bool detectDuplicates = true;
  bool detectUnused = true;
  bool generateSuggestions = true;

  // Thresholds for suggestions
  u64 largeImageThreshold = 2 * 1024 * 1024;  // 2MB
  u64 largeAudioThreshold = 10 * 1024 * 1024; // 10MB
  i32 maxImageDimension = 4096;
  f32 poorCompressionThreshold = 0.9f; // <10% compression

  // Exclude patterns
  std::vector<std::string> excludePatterns;
};

/**
 * @brief Listener for build size analysis progress
 */
class IBuildSizeListener {
public:
  virtual ~IBuildSizeListener() = default;

  virtual void onAnalysisStarted() = 0;
  virtual void onAnalysisProgress(const std::string &currentTask,
                                  f32 progress) = 0;
  virtual void onAnalysisCompleted(const BuildSizeAnalysis &analysis) = 0;
};

/**
 * @brief Build Size Analyzer
 *
 * Analyzes project build size and provides optimization suggestions:
 * - Per-category breakdown
 * - Duplicate detection
 * - Unused asset detection
 * - Compression analysis
 * - Optimization recommendations
 */
class BuildSizeAnalyzer {
public:
  BuildSizeAnalyzer();
  ~BuildSizeAnalyzer() = default;

  /**
   * @brief Set project path
   */
  void setProjectPath(const std::string &projectPath);

  /**
   * @brief Set configuration
   */
  void setConfig(const BuildSizeAnalysisConfig &config);

  /**
   * @brief Run full analysis
   */
  Result<BuildSizeAnalysis> analyze();

  /**
   * @brief Get last analysis result
   */
  [[nodiscard]] const BuildSizeAnalysis &getAnalysis() const {
    return m_analysis;
  }

  /**
   * @brief Add listener
   */
  void addListener(IBuildSizeListener *listener);

  /**
   * @brief Remove listener
   */
  void removeListener(IBuildSizeListener *listener);

  // Optimization actions

  /**
   * @brief Apply an optimization suggestion
   */
  Result<void> applyOptimization(const OptimizationSuggestion &suggestion);

  /**
   * @brief Apply all auto-fixable optimizations
   */
  Result<void> applyAllAutoOptimizations();

  /**
   * @brief Remove duplicate assets (keep one copy)
   */
  Result<void> removeDuplicates();

  /**
   * @brief Remove unused assets
   */
  Result<void> removeUnusedAssets();

  // Export

  /**
   * @brief Export analysis as JSON
   */
  Result<std::string> exportAsJson() const;

  /**
   * @brief Export analysis as HTML report
   */
  Result<void> exportAsHtml(const std::string &outputPath) const;

  /**
   * @brief Export analysis as CSV
   */
  Result<void> exportAsCsv(const std::string &outputPath) const;

private:
  void reportProgress(const std::string &task, f32 progress);

  std::string m_projectPath;
  BuildSizeAnalysisConfig m_config;
  BuildSizeAnalysis m_analysis;

  std::unordered_map<std::string, std::vector<std::string>>
      m_hashToFiles; // For duplicate detection
  std::unordered_map<std::string, size_t>
      m_pathToAssetIndex; // For O(1) asset lookup by path

  std::vector<IBuildSizeListener *> m_listeners;
};


} // namespace NovelMind::editor
