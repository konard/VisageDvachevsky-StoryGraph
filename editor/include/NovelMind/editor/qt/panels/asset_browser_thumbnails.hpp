#pragma once

/**
 * @file asset_browser_thumbnails.hpp
 * @brief Thumbnail generation and caching for the Asset Browser
 *
 * Provides:
 * - Custom icon provider for file system model
 * - Thumbnail generation for images and audio
 * - Lazy thumbnail loading with priority queue
 * - Thumbnail cache validation
 */

#include <QCache>
#include <QDateTime>
#include <QFileIconProvider>
#include <QPixmap>
#include <QPointer>
#include <QSize>
#include <QString>

namespace NovelMind::editor::qt {

// Forward declarations
class NMAssetBrowserPanel;

/**
 * @brief Cached thumbnail entry with metadata for invalidation
 */
struct ThumbnailCacheEntry {
  QPixmap pixmap;
  QDateTime lastModified;
  qint64 fileSize = 0;
};

/**
 * @brief Custom icon provider for asset thumbnails
 */
class NMAssetIconProvider : public QFileIconProvider {
public:
  explicit NMAssetIconProvider(NMAssetBrowserPanel *panel, QSize iconSize);

  /**
   * @brief Set the icon size for thumbnails
   */
  void setIconSize(const QSize &size);

  /**
   * @brief Get icon for a file (with thumbnail support)
   */
  QIcon icon(const QFileInfo &info) const override;

private:
  QPointer<NMAssetBrowserPanel> m_panel;
  QSize m_iconSize;
};

/**
 * @brief Generate a placeholder audio waveform thumbnail
 * @param path File path (used for deterministic generation)
 * @param size Desired thumbnail size
 * @return Generated waveform pixmap
 */
QPixmap generateAudioWaveform(const QString &path, const QSize &size);

/**
 * @brief Check if a cached thumbnail is still valid
 * @param path File path to check
 * @param entry Cached entry to validate
 * @return true if the cached entry is still valid
 */
bool isThumbnailValid(const QString &path, const ThumbnailCacheEntry &entry);

} // namespace NovelMind::editor::qt
