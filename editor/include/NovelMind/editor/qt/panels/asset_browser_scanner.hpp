#pragma once

/**
 * @file asset_browser_scanner.hpp
 * @brief File system scanning and asset import utilities for the Asset Browser
 *
 * Provides:
 * - Asset import destination determination by type
 * - Unique file path generation
 * - Asset metadata extraction
 * - Asset file operations (rename, delete, duplicate)
 */

#include <QCryptographicHash>
#include <QDateTime>
#include <QString>
#include <QStringList>

namespace NovelMind::editor::qt {

/**
 * @brief Asset metadata from file scanning
 */
struct AssetMetadata {
  QString id;          // Stable unique ID
  QString type;        // Asset type (image, audio, font, script, etc.)
  QString path;        // Relative path
  qint64 size = 0;     // File size in bytes
  QString format;      // File format
  QDateTime modified;  // Last modified time
  int width = 0;       // For images: width
  int height = 0;      // For images: height
  double duration = 0; // For audio: duration in seconds
  int sampleRate = 0;  // For audio: sample rate
  int channels = 0;    // For audio: number of channels
  QStringList usages;  // Where this asset is used
};

/**
 * @brief Determine the import destination folder for a given file extension
 * @param extension File extension (without dot)
 * @param rootPath Project assets root path
 * @return Path to the appropriate subfolder (Images, Audio, Fonts, etc.)
 */
QString importDestinationForExtension(const QString &extension,
                                      const QString &rootPath);

/**
 * @brief Generate a unique file path to avoid overwriting existing files
 * @param directory Target directory
 * @param fileName Desired file name
 * @return Unique file path (may have _1, _2, etc. suffix)
 */
QString generateUniquePath(const QString &directory, const QString &fileName);

/**
 * @brief Extract metadata from an asset file
 * @param path Absolute file path
 * @return Asset metadata structure
 */
AssetMetadata getAssetMetadata(const QString &path);

} // namespace NovelMind::editor::qt
