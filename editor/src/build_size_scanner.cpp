/**
 * @file build_size_scanner.cpp
 * @brief Asset scanning and analysis implementation
 */

#include "NovelMind/editor/build_size_scanner.hpp"
#include "NovelMind/vfs/pack_security.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

#ifdef NOVELMIND_QT6_GUI
#include <QAudioDecoder>
#include <QEventLoop>
#include <QImage>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrl>
#endif

namespace fs = std::filesystem;

namespace NovelMind::editor {

void BuildSizeScanner::scanAssets(const std::string& projectPath,
                                  const BuildSizeAnalysisConfig& config,
                                  std::vector<AssetSizeInfo>& assets) {
  if (projectPath.empty()) {
    return;
  }

  fs::path projectDir(projectPath);
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
        shouldAnalyze = config.analyzeImages;
        break;
      case AssetCategory::Audio:
        shouldAnalyze = config.analyzeAudio;
        break;
      case AssetCategory::Scripts:
        shouldAnalyze = config.analyzeScripts;
        break;
      case AssetCategory::Fonts:
        shouldAnalyze = config.analyzeFonts;
        break;
      case AssetCategory::Video:
        shouldAnalyze = config.analyzeVideo;
        break;
      default:
        shouldAnalyze = config.analyzeOther;
        break;
      }

      if (!shouldAnalyze) {
        continue;
      }

      // Check exclude patterns
      bool excluded = false;
      for (const auto& pattern : config.excludePatterns) {
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

      assets.push_back(info);
    }
  }

  // Scan scripts directory
  fs::path scriptsDir = projectDir / "scripts";
  if (fs::exists(scriptsDir) && config.analyzeScripts) {
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

          assets.push_back(info);
        }
      }
    }
  }
}

void BuildSizeScanner::analyzeAsset(
    AssetSizeInfo& info, const BuildSizeAnalysisConfig& config,
    std::unordered_map<std::string, std::vector<std::string>>& hashToFiles) {
  // Compute file hash for duplicate detection
  std::string hash = computeFileHash(info.path);

  // Track for duplicate detection
  hashToFiles[hash].push_back(info.path);

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
      if (info.imageWidth > config.maxImageDimension ||
          info.imageHeight > config.maxImageDimension) {
        info.isOversized = true;
        info.optimizationSuggestions.push_back("Image dimensions exceed " +
                                               std::to_string(config.maxImageDimension) +
                                               "px. Consider resizing.");
      }
    }
#endif

    // Check for oversized images
    if (info.originalSize > config.largeImageThreshold) {
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

    if (info.originalSize > config.largeAudioThreshold) {
      info.isOversized = true;
      info.optimizationSuggestions.push_back(
          "Consider using OGG Vorbis compression or reducing quality");
    }
  }
}

u64 BuildSizeScanner::detectDuplicates(
    const std::unordered_map<std::string, std::vector<std::string>>& hashToFiles,
    std::vector<AssetSizeInfo>& assets, std::vector<DuplicateGroup>& duplicates) {
  u64 totalWastedSpace = 0;

  for (const auto& [hash, files] : hashToFiles) {
    if (files.size() > 1) {
      // Additional safety: verify all files have the same size
      // This provides defense-in-depth against hash collisions
      u64 firstFileSize = 0;
      bool sizeMismatch = false;

      for (const auto& path : files) {
        for (const auto& asset : assets) {
          if (asset.path == path) {
            if (firstFileSize == 0) {
              firstFileSize = asset.originalSize;
            } else if (asset.originalSize != firstFileSize) {
              // Size mismatch detected - this could be a hash collision
              sizeMismatch = true;
              break;
            }
          }
        }
        if (sizeMismatch)
          break;
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
      totalWastedSpace += group.wastedSpace;

      // Mark duplicates in assets
      bool first = true;
      for (const auto& path : files) {
        if (first) {
          first = false;
          continue;
        }

        for (auto& asset : assets) {
          if (asset.path == path) {
            asset.isDuplicate = true;
            asset.duplicateOf = files[0];
            break;
          }
        }
      }

      duplicates.push_back(group);
    }
  }

  return totalWastedSpace;
}

u64 BuildSizeScanner::detectUnused(const std::string& projectPath,
                                   std::vector<AssetSizeInfo>& assets,
                                   std::vector<std::string>& unusedAssets) {
  // Parse all scripts and scenes to find asset references
  std::unordered_set<std::string> referencedAssets;

  if (projectPath.empty()) {
    return 0;
  }

  fs::path projectDir(projectPath);

  // Parse script files for asset references
  fs::path scriptsDir = projectDir / "scripts";
  if (fs::exists(scriptsDir)) {
    for (const auto& entry : fs::recursive_directory_iterator(scriptsDir)) {
      if (entry.is_regular_file()) {
        std::string ext = entry.path().extension().string();
        if (ext == ".nms" || ext == ".nmscript") {
          parseFileForAssetReferences(entry.path().string(), referencedAssets);
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
          parseFileForAssetReferences(entry.path().string(), referencedAssets);
        }
      }
    }
  }

  // Mark assets as unused if they're not referenced
  u64 unusedSpace = 0;
  for (auto& asset : assets) {
    // Check if the asset path or filename is referenced
    bool isReferenced = false;

    // Check full path
    if (referencedAssets.find(asset.path) != referencedAssets.end()) {
      isReferenced = true;
    }

    // Check filename only
    if (referencedAssets.find(asset.name) != referencedAssets.end()) {
      isReferenced = true;
    }

    // Check relative path from project
    fs::path assetPath(asset.path);
    fs::path relativePath = fs::relative(assetPath, projectDir);
    if (referencedAssets.find(relativePath.string()) != referencedAssets.end()) {
      isReferenced = true;
    }

    if (!isReferenced) {
      asset.isUnused = true;
      unusedAssets.push_back(asset.path);
      unusedSpace += asset.originalSize;
    }
  }

  return unusedSpace;
}

void BuildSizeScanner::parseFileForAssetReferences(
    const std::string& filePath, std::unordered_set<std::string>& referencedAssets) {
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
              referencedAssets.insert(reference);

              // Also add just the filename
              size_t lastSlash = reference.find_last_of("/\\");
              if (lastSlash != std::string::npos) {
                referencedAssets.insert(reference.substr(lastSlash + 1));
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

u64 BuildSizeScanner::generateSuggestions(const std::vector<AssetSizeInfo>& assets,
                                          const std::vector<DuplicateGroup>& duplicates,
                                          const std::vector<std::string>& unusedAssets,
                                          std::vector<OptimizationSuggestion>& suggestions) {
  u64 potentialSavings = 0;

  // Suggest removing duplicates
  for (const auto& dup : duplicates) {
    OptimizationSuggestion suggestion;
    suggestion.priority = OptimizationSuggestion::Priority::High;
    suggestion.type = OptimizationSuggestion::Type::RemoveDuplicate;
    suggestion.assetPath = dup.paths[1]; // First duplicate
    suggestion.description = "Remove duplicate file (same content as " + dup.paths[0] + ")";
    suggestion.estimatedSavings = dup.singleFileSize;
    suggestion.canAutoFix = true;

    suggestions.push_back(suggestion);
    potentialSavings += suggestion.estimatedSavings;
  }

  // Suggest optimizing large images
  for (const auto& asset : assets) {
    if (asset.category == AssetCategory::Images && asset.isOversized) {
      OptimizationSuggestion suggestion;
      suggestion.priority = OptimizationSuggestion::Priority::Medium;
      suggestion.type = OptimizationSuggestion::Type::CompressImage;
      suggestion.assetPath = asset.path;

      // Format bytes helper (inline for this file)
      auto formatBytes = [](u64 bytes) -> std::string {
        const char* units[] = {"B", "KB", "MB", "GB", "TB"};
        i32 unitIndex = 0;
        f64 size = static_cast<f64>(bytes);

        while (size >= 1024.0 && unitIndex < 4) {
          size /= 1024.0;
          unitIndex++;
        }

        std::ostringstream oss;
        if (unitIndex == 0) {
          oss << bytes << " " << units[unitIndex];
        } else {
          oss << std::fixed << std::setprecision(2) << size << " " << units[unitIndex];
        }
        return oss.str();
      };

      suggestion.description = "Large image detected (" + formatBytes(asset.originalSize) +
                               "). Consider resizing or compressing.";
      suggestion.estimatedSavings = asset.originalSize / 2; // Rough estimate
      suggestion.canAutoFix = false;

      suggestions.push_back(suggestion);
      potentialSavings += suggestion.estimatedSavings;
    }

    if (asset.category == AssetCategory::Audio && asset.isOversized) {
      OptimizationSuggestion suggestion;
      suggestion.priority = OptimizationSuggestion::Priority::Medium;
      suggestion.type = OptimizationSuggestion::Type::CompressAudio;
      suggestion.assetPath = asset.path;

      // Format bytes helper (inline for this file)
      auto formatBytes = [](u64 bytes) -> std::string {
        const char* units[] = {"B", "KB", "MB", "GB", "TB"};
        i32 unitIndex = 0;
        f64 size = static_cast<f64>(bytes);

        while (size >= 1024.0 && unitIndex < 4) {
          size /= 1024.0;
          unitIndex++;
        }

        std::ostringstream oss;
        if (unitIndex == 0) {
          oss << bytes << " " << units[unitIndex];
        } else {
          oss << std::fixed << std::setprecision(2) << size << " " << units[unitIndex];
        }
        return oss.str();
      };

      suggestion.description = "Large audio file detected (" + formatBytes(asset.originalSize) +
                               "). Consider using OGG Vorbis.";
      suggestion.estimatedSavings = asset.originalSize / 3; // Rough estimate
      suggestion.canAutoFix = false;

      suggestions.push_back(suggestion);
      potentialSavings += suggestion.estimatedSavings;
    }
  }

  // Suggest removing unused assets
  for (const auto& unusedPath : unusedAssets) {
    for (const auto& asset : assets) {
      if (asset.path == unusedPath) {
        OptimizationSuggestion suggestion;
        suggestion.priority = OptimizationSuggestion::Priority::High;
        suggestion.type = OptimizationSuggestion::Type::RemoveUnused;
        suggestion.assetPath = asset.path;
        suggestion.description = "Asset appears to be unused";
        suggestion.estimatedSavings = asset.originalSize;
        suggestion.canAutoFix = true;

        suggestions.push_back(suggestion);
        potentialSavings += suggestion.estimatedSavings;
        break;
      }
    }
  }

  // Sort suggestions by estimated savings (descending)
  std::sort(suggestions.begin(), suggestions.end(),
            [](const OptimizationSuggestion& a, const OptimizationSuggestion& b) {
              return a.estimatedSavings > b.estimatedSavings;
            });

  return potentialSavings;
}

void BuildSizeScanner::calculateSummaries(const std::vector<AssetSizeInfo>& assets,
                                          u64 totalOriginalSize,
                                          std::vector<CategorySummary>& categorySummaries) {
  // Group by category
  std::unordered_map<AssetCategory, CategorySummary> categoryMap;

  for (const auto& asset : assets) {
    auto& summary = categoryMap[asset.category];
    summary.category = asset.category;
    summary.fileCount++;
    summary.totalOriginalSize += asset.originalSize;
    summary.totalCompressedSize += asset.compressedSize;
  }

  // Calculate percentages and averages
  for (auto& [category, summary] : categoryMap) {
    if (totalOriginalSize > 0) {
      summary.percentageOfTotal = static_cast<f32>(summary.totalOriginalSize) /
                                  static_cast<f32>(totalOriginalSize) * 100.0f;
    }

    if (summary.totalOriginalSize > 0) {
      summary.averageCompressionRatio = static_cast<f32>(summary.totalCompressedSize) /
                                        static_cast<f32>(summary.totalOriginalSize);
    }

    categorySummaries.push_back(summary);
  }

  // Sort by size (descending)
  std::sort(categorySummaries.begin(), categorySummaries.end(),
            [](const CategorySummary& a, const CategorySummary& b) {
              return a.totalOriginalSize > b.totalOriginalSize;
            });
}

std::string BuildSizeScanner::computeFileHash(const std::string& path) {
  // Use SHA-256 for cryptographically secure hash to prevent collisions
  try {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
      return "";
    }

    auto fileSize = file.tellg();
    file.seekg(0);

    // Read entire file into memory for hashing
    // For very large files, this could be optimized with streaming
    std::vector<u8> fileData(static_cast<size_t>(fileSize));
    file.read(reinterpret_cast<char*>(fileData.data()), fileSize);

    if (file.gcount() != fileSize) {
      return "";
    }

    // Calculate SHA-256 hash using existing secure implementation
    auto hash =
        VFS::PackIntegrityChecker::calculateSha256(fileData.data(), static_cast<usize>(fileSize));

    // Convert hash to hex string
    std::ostringstream oss;
    for (const auto& byte : hash) {
      oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    return oss.str();

  } catch (const std::exception&) {
    return "";
  }
}

CompressionType BuildSizeScanner::detectCompression(const std::string& path) {
  std::string ext = fs::path(path).extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

  if (ext == ".png") {
    return CompressionType::Png;
  }
  if (ext == ".jpg" || ext == ".jpeg") {
    return CompressionType::Jpeg;
  }
  if (ext == ".ogg") {
    return CompressionType::Ogg;
  }

  return CompressionType::None;
}

AssetCategory BuildSizeScanner::categorizeAsset(const std::string& path) {
  std::string ext = fs::path(path).extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

  // Images
  if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".gif" ||
      ext == ".webp" || ext == ".tga") {
    return AssetCategory::Images;
  }

  // Audio
  if (ext == ".ogg" || ext == ".wav" || ext == ".mp3" || ext == ".flac" || ext == ".aac" ||
      ext == ".m4a") {
    return AssetCategory::Audio;
  }

  // Scripts
  if (ext == ".nms" || ext == ".nmscript" || ext == ".lua" || ext == ".json") {
    return AssetCategory::Scripts;
  }

  // Fonts
  if (ext == ".ttf" || ext == ".otf" || ext == ".woff" || ext == ".woff2") {
    return AssetCategory::Fonts;
  }

  // Video
  if (ext == ".mp4" || ext == ".webm" || ext == ".avi" || ext == ".mkv") {
    return AssetCategory::Video;
  }

  // Data
  if (ext == ".xml" || ext == ".yaml" || ext == ".yml" || ext == ".csv" || ext == ".dat" ||
      ext == ".bin") {
    return AssetCategory::Data;
  }

  return AssetCategory::Other;
}

} // namespace NovelMind::editor
