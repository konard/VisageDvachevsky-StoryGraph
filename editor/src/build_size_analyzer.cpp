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

void BuildSizeAnalyzer::setProjectPath(const std::string& projectPath) {
  m_projectPath = projectPath;
}

void BuildSizeAnalyzer::setConfig(const BuildSizeAnalysisConfig& config) {
  m_config = config;
}

Result<BuildSizeAnalysis> BuildSizeAnalyzer::analyze() {
  auto startTime = std::chrono::steady_clock::now();

  // Notify listeners
  for (auto* listener : m_listeners) {
    listener->onAnalysisStarted();
  }

  // Reset analysis
  m_analysis = BuildSizeAnalysis{};
  m_hashToFiles.clear();
  m_referencedAssets.clear();
  m_pathToAssetIndex.clear();

  try {
    BuildSizeScanner scanner;

    // Scan all assets
    reportProgress("Scanning assets...", 0.0f);
    scanner.scanAssets(m_projectPath, m_config, m_analysis.assets);
    m_analysis.totalFileCount = static_cast<i32>(m_analysis.assets.size());

    // Build path-to-asset index for O(1) lookups
    // This eliminates O(n²) complexity in detectDuplicates() and
    // generateSuggestions()
    for (size_t i = 0; i < m_analysis.assets.size(); i++) {
      m_pathToAssetIndex[m_analysis.assets[i].path] = i;
    }

    // Analyze each asset
    reportProgress("Analyzing assets...", 0.2f);
    for (size_t i = 0; i < m_analysis.assets.size(); i++) {
      scanner.analyzeAsset(m_analysis.assets[i], m_config, m_hashToFiles);

      // Update totals
      m_analysis.totalOriginalSize += m_analysis.assets[i].originalSize;
      m_analysis.totalCompressedSize += m_analysis.assets[i].compressedSize;

      f32 progress = 0.2f + 0.3f * static_cast<f32>(i) / static_cast<f32>(m_analysis.assets.size());
      reportProgress("Analyzing: " + m_analysis.assets[i].name, progress);
    }

    // Detect duplicates
    if (m_config.detectDuplicates) {
      reportProgress("Detecting duplicates...", 0.5f);
      m_analysis.totalWastedSpace =
          scanner.detectDuplicates(m_hashToFiles, m_analysis.assets, m_analysis.duplicates);
    }

    // Detect unused assets
    if (m_config.detectUnused) {
      reportProgress("Detecting unused assets...", 0.6f);
      m_analysis.unusedSpace =
          scanner.detectUnused(m_projectPath, m_analysis.assets, m_analysis.unusedAssets);
    }

    // Generate suggestions
    if (m_config.generateSuggestions) {
      reportProgress("Generating suggestions...", 0.8f);
      m_analysis.potentialSavings =
          scanner.generateSuggestions(m_analysis.assets, m_analysis.duplicates,
                                      m_analysis.unusedAssets, m_analysis.suggestions);
    }

    // Calculate summaries
    reportProgress("Calculating summaries...", 0.9f);
    scanner.calculateSummaries(m_analysis.assets, m_analysis.totalOriginalSize,
                               m_analysis.categorySummaries);

    // Calculate overall compression ratio
    if (m_analysis.totalOriginalSize > 0) {
      m_analysis.overallCompressionRatio = static_cast<f32>(m_analysis.totalCompressedSize) /
                                           static_cast<f32>(m_analysis.totalOriginalSize);
    }

    // Finalize
    auto endTime = std::chrono::steady_clock::now();
    m_analysis.analysisTimeMs = std::chrono::duration<f64, std::milli>(endTime - startTime).count();
    m_analysis.analysisTimestamp =
        static_cast<u64>(std::chrono::system_clock::now().time_since_epoch().count());

    reportProgress("Analysis complete", 1.0f);

    // Notify listeners
    for (auto* listener : m_listeners) {
      listener->onAnalysisCompleted(m_analysis);
    }

    return Result<BuildSizeAnalysis>::ok(m_analysis);

  } catch (const std::exception& e) {
    return Result<BuildSizeAnalysis>::error(std::string("Analysis failed: ") + e.what());
  }
}

void BuildSizeAnalyzer::addListener(IBuildSizeListener* listener) {
  if (listener) {
    m_listeners.push_back(listener);
  }
}

void BuildSizeAnalyzer::removeListener(IBuildSizeListener* listener) {
  m_listeners.erase(std::remove(m_listeners.begin(), m_listeners.end(), listener),
                    m_listeners.end());
}

Result<void> BuildSizeAnalyzer::applyOptimization(const OptimizationSuggestion& suggestion) {
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
      return Result<void>::error("This optimization type requires manual intervention");

    default:
      return Result<void>::error("Unknown optimization type");
    }
  } catch (const std::exception& e) {
    return Result<void>::error(std::string("Failed to apply optimization: ") + e.what());
  }
}

Result<void> BuildSizeAnalyzer::applyAllAutoOptimizations() {
  i32 applied = 0;
  for (const auto& suggestion : m_analysis.suggestions) {
    if (suggestion.canAutoFix) {
      auto result = applyOptimization(suggestion);
      if (result.isOk()) {
        applied++;
      }
    }
  }

  if (applied > 0) {
    // Re-analyze after optimizations
    return analyze().isOk() ? Result<void>::ok() : Result<void>::error("Re-analysis failed");
  }

  return Result<void>::ok();
}

Result<void> BuildSizeAnalyzer::removeDuplicates() {
  try {
    i32 removedCount = 0;
    std::vector<std::string> errors;

    for (const auto& dupGroup : m_analysis.duplicates) {
      // Keep the first file, remove all duplicates
      for (size_t i = 1; i < dupGroup.paths.size(); i++) {
        const auto& path = dupGroup.paths[i];
        if (fs::exists(path)) {
          try {
            fs::remove(path);
            removedCount++;
          } catch (const std::exception& e) {
            errors.push_back("Failed to remove " + path + ": " + e.what());
          }
        }
      }
    }

    if (!errors.empty()) {
      std::ostringstream oss;
      oss << "Removed " << removedCount << " duplicates, but encountered " << errors.size()
          << " errors:\n";
      for (const auto& error : errors) {
        oss << "  - " << error << "\n";
      }
      return Result<void>::error(oss.str());
    }

    return Result<void>::ok();
  } catch (const std::exception& e) {
    return Result<void>::error(std::string("Failed to remove duplicates: ") + e.what());
  }
}

Result<void> BuildSizeAnalyzer::removeUnusedAssets() {
  try {
    i32 removedCount = 0;
    std::vector<std::string> errors;

    for (const auto& assetPath : m_analysis.unusedAssets) {
      if (fs::exists(assetPath)) {
        try {
          fs::remove(assetPath);
          removedCount++;
        } catch (const std::exception& e) {
          errors.push_back("Failed to remove " + assetPath + ": " + e.what());
        }
      }
    }

    if (!errors.empty()) {
      std::ostringstream oss;
      oss << "Removed " << removedCount << " unused assets, but encountered " << errors.size()
          << " errors:\n";
      for (const auto& error : errors) {
        oss << "  - " << error << "\n";
      }
      return Result<void>::error(oss.str());
    }

    return Result<void>::ok();
  } catch (const std::exception& e) {
    return Result<void>::error(std::string("Failed to remove unused assets: ") + e.what());
  }
}

Result<std::string> BuildSizeAnalyzer::exportAsJson() const {
  BuildSizeReport report;
  return report.exportAsJson(m_analysis);
}

Result<void> BuildSizeAnalyzer::exportAsHtml(const std::string& outputPath) const {
  BuildSizeReport report;
  return report.exportAsHtml(m_analysis, outputPath);
}

Result<void> BuildSizeAnalyzer::exportAsCsv(const std::string& outputPath) const {
  std::ofstream file(outputPath);
  if (!file.is_open()) {
    return Result<void>::error("Cannot create CSV file: " + outputPath);
  }

  // Header
  file << "Path,Name,Category,Original Size,Compressed Size,Compression "
          "Ratio,Is Duplicate,Is Unused\n";

  // Assets
  for (const auto& asset : m_analysis.assets) {
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
         << "\"" << asset.name << "\"," << catName << "," << asset.originalSize << ","
         << asset.compressedSize << "," << asset.compressionRatio << ","
         << (asset.isDuplicate ? "Yes" : "No") << "," << (asset.isUnused ? "Yes" : "No") << "\n";
  }

  file.close();
  return Result<void>::ok();
}

// ============================================================================
// Private Methods
// ============================================================================

void BuildSizeAnalyzer::scanAssets() {
  if (m_projectPath.empty()) {
    return;
  }

  fs::path projectDir(m_projectPath);
  if (!fs::exists(projectDir)) {
    return;
  }

  // Scan assets directory
  fs::path assetsDir = projectDir / "assets";
  if (fs::exists(assetsDir)) {
    for (const auto& entry : fs::recursive_directory_iterator(assetsDir)) {
      if (!entry.is_regular_file()) {
        continue;
      }

      AssetCategory category = categorizeAsset(entry.path().string());

      // Check if category should be analyzed
      bool shouldAnalyze = false;
      switch (category) {
      case AssetCategory::Images:
        shouldAnalyze = m_config.analyzeImages;
        break;
      case AssetCategory::Audio:
        shouldAnalyze = m_config.analyzeAudio;
        break;
      case AssetCategory::Scripts:
        shouldAnalyze = m_config.analyzeScripts;
        break;
      case AssetCategory::Fonts:
        shouldAnalyze = m_config.analyzeFonts;
        break;
      case AssetCategory::Video:
        shouldAnalyze = m_config.analyzeVideo;
        break;
      default:
        shouldAnalyze = m_config.analyzeOther;
        break;
      }

      if (!shouldAnalyze) {
        continue;
      }

      // Check exclude patterns
      bool excluded = false;
      for (const auto& pattern : m_config.excludePatterns) {
        if (entry.path().string().find(pattern) != std::string::npos) {
          excluded = true;
          break;
        }
      }

      if (excluded) {
        continue;
      }

      AssetSizeInfo info;
      info.path = entry.path().string();
      info.name = entry.path().filename().string();
      info.category = category;
      info.originalSize = static_cast<u64>(entry.file_size());
      info.compressedSize = info.originalSize; // Will be updated during analysis

      m_analysis.assets.push_back(info);
      m_analysis.totalFileCount++;
    }
  }

  // Scan scripts directory
  fs::path scriptsDir = projectDir / "scripts";
  if (fs::exists(scriptsDir) && m_config.analyzeScripts) {
    for (const auto& entry : fs::recursive_directory_iterator(scriptsDir)) {
      if (entry.is_regular_file()) {
        std::string ext = entry.path().extension().string();
        if (ext == ".nms" || ext == ".nmscript") {
          AssetSizeInfo info;
          info.path = entry.path().string();
          info.name = entry.path().filename().string();
          info.category = AssetCategory::Scripts;
          info.originalSize = static_cast<u64>(entry.file_size());
          info.compressedSize = info.originalSize;

          m_analysis.assets.push_back(info);
          m_analysis.totalFileCount++;
        }
      }
    }
  }
}

void BuildSizeAnalyzer::analyzeAsset(AssetSizeInfo& info) {
  // Compute file hash for duplicate detection
  std::string hash = computeFileHash(info.path);

  // Track for duplicate detection
  m_hashToFiles[hash].push_back(info.path);

  // Detect compression type
  info.compression = detectCompression(info.path);

  // Calculate compression ratio (placeholder - would need actual compression)
  info.compressionRatio = 1.0f;

  // Image-specific analysis
  if (info.category == AssetCategory::Images) {
    // Use Qt to get image dimensions
#ifdef NOVELMIND_QT6_GUI
    QImage image(QString::fromStdString(info.path));
    if (!image.isNull()) {
      info.imageWidth = image.width();
      info.imageHeight = image.height();
      info.imageBitDepth = image.depth();

      // Check for oversized dimensions
      if (info.imageWidth > m_config.maxImageDimension ||
          info.imageHeight > m_config.maxImageDimension) {
        info.isOversized = true;
        info.optimizationSuggestions.push_back("Image dimensions exceed " +
                                               std::to_string(m_config.maxImageDimension) +
                                               "px. Consider resizing.");
      }
    }
#endif

    // Check for oversized images
    if (info.originalSize > m_config.largeImageThreshold) {
      info.isOversized = true;
      info.optimizationSuggestions.push_back(
          "Consider reducing image size or using better compression");
    }
  }

  // Audio-specific analysis
  if (info.category == AssetCategory::Audio) {
    // Use Qt Multimedia to analyze audio
#ifdef NOVELMIND_QT6_GUI
    QAudioDecoder decoder;
    decoder.setSource(QUrl::fromLocalFile(QString::fromStdString(info.path)));

    // Event loop to wait for format information
    QEventLoop loop;
    bool formatAvailable = false;

    QObject::connect(&decoder, &QAudioDecoder::bufferReady, [&]() {
      if (!formatAvailable && decoder.bufferAvailable()) {
        formatAvailable = true;
        loop.quit();
      }
    });

    QObject::connect(&decoder, &QAudioDecoder::finished, [&]() { loop.quit(); });

    QObject::connect(
        &decoder, static_cast<void (QAudioDecoder::*)(QAudioDecoder::Error)>(&QAudioDecoder::error),
        [&](QAudioDecoder::Error) { loop.quit(); });

    // Timeout to prevent hanging
    QTimer::singleShot(1000, &loop, &QEventLoop::quit);

    decoder.start();
    loop.exec();

    if (formatAvailable && decoder.bufferAvailable()) {
      auto format = decoder.audioFormat();
      info.audioSampleRate = format.sampleRate();
      info.audioChannels = format.channelCount();

      // Calculate duration from file size and format
      // This is a rough estimate
      if (info.audioSampleRate > 0 && info.audioChannels > 0) {
        i32 bytesPerSample = format.bytesPerSample();
        if (bytesPerSample > 0) {
          f64 totalSamples = static_cast<f64>(info.originalSize) /
                             static_cast<f64>(bytesPerSample) /
                             static_cast<f64>(info.audioChannels);
          info.audioDuration =
              static_cast<f32>(totalSamples) / static_cast<f32>(info.audioSampleRate);
        }
      }
    }

    decoder.stop();
#endif

    if (info.originalSize > m_config.largeAudioThreshold) {
      info.isOversized = true;
      info.optimizationSuggestions.push_back(
          "Consider using OGG Vorbis compression or reducing quality");
    }
  }

  // Update totals
  m_analysis.totalOriginalSize += info.originalSize;
  m_analysis.totalCompressedSize += info.compressedSize;
}

void BuildSizeAnalyzer::detectDuplicates() {
  for (const auto& [hash, files] : m_hashToFiles) {
    if (files.size() > 1) {
      // Additional safety: verify all files have the same size
      // This provides defense-in-depth against hash collisions
      // OPTIMIZED: Use O(1) path-to-asset index lookup instead of O(n) linear
      // search
      u64 firstFileSize = 0;
      bool sizeMismatch = false;

      for (const auto& path : files) {
        auto it = m_pathToAssetIndex.find(path);
        if (it != m_pathToAssetIndex.end()) {
          const auto& asset = m_analysis.assets[it->second];
          if (firstFileSize == 0) {
            firstFileSize = asset.originalSize;
          } else if (asset.originalSize != firstFileSize) {
            // Size mismatch detected - this could be a hash collision
            sizeMismatch = true;
            break;
          }
        }
      }

      // Skip this group if sizes don't match (potential collision)
      if (sizeMismatch) {
        continue;
      }

      DuplicateGroup group;
      group.hash = hash;
      group.paths = files;
      group.singleFileSize = firstFileSize;
      group.wastedSpace = group.singleFileSize * (files.size() - 1);
      m_analysis.totalWastedSpace += group.wastedSpace;

      // Mark duplicates in assets
      // OPTIMIZED: Use O(1) index lookup instead of O(n) linear search
      bool first = true;
      for (const auto& path : files) {
        if (first) {
          first = false;
          continue;
        }

        auto it = m_pathToAssetIndex.find(path);
        if (it != m_pathToAssetIndex.end()) {
          auto& asset = m_analysis.assets[it->second];
          asset.isDuplicate = true;
          asset.duplicateOf = files[0];
        }
      }

      m_analysis.duplicates.push_back(group);
    }
  }
}

void BuildSizeAnalyzer::detectUnused() {
  // Parse all scripts and scenes to find asset references
  m_referencedAssets.clear();

  if (m_projectPath.empty()) {
    return;
  }

  fs::path projectDir(m_projectPath);

  // Parse script files for asset references
  fs::path scriptsDir = projectDir / "scripts";
  if (fs::exists(scriptsDir)) {
    for (const auto& entry : fs::recursive_directory_iterator(scriptsDir)) {
      if (entry.is_regular_file()) {
        std::string ext = entry.path().extension().string();
        if (ext == ".nms" || ext == ".nmscript") {
          parseFileForAssetReferences(entry.path().string());
        }
      }
    }
  }

  // Parse scene files for asset references
  fs::path scenesDir = projectDir / "scenes";
  if (fs::exists(scenesDir)) {
    for (const auto& entry : fs::recursive_directory_iterator(scenesDir)) {
      if (entry.is_regular_file()) {
        std::string ext = entry.path().extension().string();
        if (ext == ".json" || ext == ".scene") {
          parseFileForAssetReferences(entry.path().string());
        }
      }
    }
  }

  // Mark assets as unused if they're not referenced
  for (auto& asset : m_analysis.assets) {
    // Check if the asset path or filename is referenced
    bool isReferenced = false;

    // Check full path
    if (m_referencedAssets.find(asset.path) != m_referencedAssets.end()) {
      isReferenced = true;
    }

    // Check filename only
    if (m_referencedAssets.find(asset.name) != m_referencedAssets.end()) {
      isReferenced = true;
    }

    // Check relative path from project
    fs::path assetPath(asset.path);
    fs::path relativePath = fs::relative(assetPath, projectDir);
    if (m_referencedAssets.find(relativePath.string()) != m_referencedAssets.end()) {
      isReferenced = true;
    }

    if (!isReferenced) {
      asset.isUnused = true;
      m_analysis.unusedAssets.push_back(asset.path);
      m_analysis.unusedSpace += asset.originalSize;
    }
  }
}

void BuildSizeAnalyzer::parseFileForAssetReferences(const std::string& filePath) {
  std::ifstream file(filePath);
  if (!file.is_open()) {
    return;
  }

  std::string line;
  while (std::getline(file, line)) {
    // Look for common asset reference patterns
    // Pattern: "path/to/asset.ext"
    size_t pos = 0;
    while ((pos = line.find('"', pos)) != std::string::npos) {
      size_t end = line.find('"', pos + 1);
      if (end != std::string::npos) {
        std::string reference = line.substr(pos + 1, end - pos - 1);

        // Check if it looks like an asset path
        if (reference.find('.') != std::string::npos) {
          // Extract file extension
          size_t extPos = reference.find_last_of('.');
          if (extPos != std::string::npos) {
            std::string ext = reference.substr(extPos);
            // Check if it's a known asset extension
            if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".ogg" ||
                ext == ".wav" || ext == ".mp3" || ext == ".ttf" || ext == ".otf" || ext == ".mp4" ||
                ext == ".webm") {
              m_referencedAssets.insert(reference);

              // Also add just the filename
              size_t lastSlash = reference.find_last_of("/\\");
              if (lastSlash != std::string::npos) {
                m_referencedAssets.insert(reference.substr(lastSlash + 1));
              }
            }
          }
        }

        pos = end + 1;
      } else {
        break;
      }
    }
  }
}

void BuildSizeAnalyzer::generateSuggestions() {
  // Suggest removing duplicates
  for (const auto& dup : m_analysis.duplicates) {
    OptimizationSuggestion suggestion;
    suggestion.priority = OptimizationSuggestion::Priority::High;
    suggestion.type = OptimizationSuggestion::Type::RemoveDuplicate;
    suggestion.assetPath = dup.paths[1]; // First duplicate
    suggestion.description = "Remove duplicate file (same content as " + dup.paths[0] + ")";
    suggestion.estimatedSavings = dup.singleFileSize;
    suggestion.canAutoFix = true;

    m_analysis.suggestions.push_back(suggestion);
    m_analysis.potentialSavings += suggestion.estimatedSavings;
  }

  // Suggest optimizing large images
  for (const auto& asset : m_analysis.assets) {
    if (asset.category == AssetCategory::Images && asset.isOversized) {
      OptimizationSuggestion suggestion;
      suggestion.priority = OptimizationSuggestion::Priority::Medium;
      suggestion.type = OptimizationSuggestion::Type::CompressImage;
      suggestion.assetPath = asset.path;
      suggestion.description = "Large image detected (" +
                               SizeVisualization::formatBytes(asset.originalSize) +
                               "). Consider resizing or compressing.";
      suggestion.estimatedSavings = asset.originalSize / 2; // Rough estimate
      suggestion.canAutoFix = false;

      m_analysis.suggestions.push_back(suggestion);
      m_analysis.potentialSavings += suggestion.estimatedSavings;
    }

    if (asset.category == AssetCategory::Audio && asset.isOversized) {
      OptimizationSuggestion suggestion;
      suggestion.priority = OptimizationSuggestion::Priority::Medium;
      suggestion.type = OptimizationSuggestion::Type::CompressAudio;
      suggestion.assetPath = asset.path;
      suggestion.description = "Large audio file detected (" +
                               SizeVisualization::formatBytes(asset.originalSize) +
                               "). Consider using OGG Vorbis.";
      suggestion.estimatedSavings = asset.originalSize / 3; // Rough estimate
      suggestion.canAutoFix = false;

      m_analysis.suggestions.push_back(suggestion);
      m_analysis.potentialSavings += suggestion.estimatedSavings;
    }
  }

  // Suggest removing unused assets
  // OPTIMIZED: Use O(1) path-to-asset index lookup instead of O(n) linear
  // search
  for (const auto& unusedPath : m_analysis.unusedAssets) {
    auto it = m_pathToAssetIndex.find(unusedPath);
    if (it != m_pathToAssetIndex.end()) {
      const auto& asset = m_analysis.assets[it->second];
      OptimizationSuggestion suggestion;
      suggestion.priority = OptimizationSuggestion::Priority::High;
      suggestion.type = OptimizationSuggestion::Type::RemoveUnused;
      suggestion.assetPath = asset.path;
      suggestion.description = "Asset appears to be unused";
      suggestion.estimatedSavings = asset.originalSize;
      suggestion.canAutoFix = true;

      m_analysis.suggestions.push_back(suggestion);
      m_analysis.potentialSavings += suggestion.estimatedSavings;
    }
  }

  // Sort suggestions by estimated savings (descending)
  std::sort(m_analysis.suggestions.begin(), m_analysis.suggestions.end(),
            [](const OptimizationSuggestion& a, const OptimizationSuggestion& b) {
              return a.estimatedSavings > b.estimatedSavings;
            });
}

void BuildSizeAnalyzer::calculateSummaries() {
  // Group by category
  std::unordered_map<AssetCategory, CategorySummary> categoryMap;

  for (const auto& asset : m_analysis.assets) {
    auto& summary = categoryMap[asset.category];
    summary.category = asset.category;
    summary.fileCount++;
    summary.totalOriginalSize += asset.originalSize;
    summary.totalCompressedSize += asset.compressedSize;
  }

  // Calculate percentages and averages
  for (auto& [category, summary] : categoryMap) {
    if (m_analysis.totalOriginalSize > 0) {
      summary.percentageOfTotal = static_cast<f32>(summary.totalOriginalSize) /
                                  static_cast<f32>(m_analysis.totalOriginalSize) * 100.0f;
    }

    if (summary.totalOriginalSize > 0) {
      summary.averageCompressionRatio = static_cast<f32>(summary.totalCompressedSize) /
                                        static_cast<f32>(summary.totalOriginalSize);
    }

    m_analysis.categorySummaries.push_back(summary);
  }

  // Sort by size (descending)
  std::sort(m_analysis.categorySummaries.begin(), m_analysis.categorySummaries.end(),
            [](const CategorySummary& a, const CategorySummary& b) {
              return a.totalOriginalSize > b.totalOriginalSize;
            });

  // Calculate overall compression ratio
  if (m_analysis.totalOriginalSize > 0) {
    m_analysis.overallCompressionRatio = static_cast<f32>(m_analysis.totalCompressedSize) /
                                         static_cast<f32>(m_analysis.totalOriginalSize);
  }
}

void BuildSizeAnalyzer::reportProgress(const std::string& task, f32 progress) {
  for (auto* listener : m_listeners) {
    listener->onAnalysisProgress(task, progress);
  }
}

} // namespace NovelMind::editor
