#pragma once

/**
 * @file timeline_render_cache.hpp
 * @brief Render cache for timeline with memory limits and LRU eviction
 *
 * Provides:
 * - Caching of rendered timeline track strips
 * - Memory-bounded cache with configurable limit
 * - LRU eviction policy
 * - Invalidation on data changes
 * - Thread-safe access
 */

#include <QCache>
#include <QColor>
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QPixmap>
#include <QRect>

#include <atomic>
#include <cstdint>
#include <list>
#include <unordered_map>

namespace NovelMind::editor::qt {

/**
 * @brief Configuration for the timeline render cache
 */
struct TimelineRenderCacheConfig {
  qint64 maxMemoryBytes = 32 * 1024 * 1024; // 32 MB default
  int tileWidth = 256;                      // Width of each cached tile
  int tileHeight = 32;                      // Height of each cached tile (track height)
  bool enableCache = true;                  // Master enable/disable
};

/**
 * @brief Key for identifying a cache entry
 */
struct RenderCacheKey {
  int trackIndex = 0;
  int startFrame = 0;
  int endFrame = 0;
  float zoom = 1.0f;
  int pixelsPerFrame = 4;

  bool operator==(const RenderCacheKey& other) const {
    return trackIndex == other.trackIndex && startFrame == other.startFrame &&
           endFrame == other.endFrame && zoom == other.zoom &&
           pixelsPerFrame == other.pixelsPerFrame;
  }
};

} // namespace NovelMind::editor::qt

// Hash function for RenderCacheKey
namespace std {
template <> struct hash<NovelMind::editor::qt::RenderCacheKey> {
  std::size_t operator()(const NovelMind::editor::qt::RenderCacheKey& key) const noexcept {
    std::size_t h = std::hash<int>{}(key.trackIndex);
    h ^= std::hash<int>{}(key.startFrame) << 1;
    h ^= std::hash<int>{}(key.endFrame) << 2;
    h ^= std::hash<float>{}(key.zoom) << 3;
    h ^= std::hash<int>{}(key.pixelsPerFrame) << 4;
    return h;
  }
};
} // namespace std

namespace NovelMind::editor::qt {

/**
 * @brief Cached render entry
 */
struct RenderCacheEntry {
  QPixmap pixmap;
  qint64 accessTime = 0;
  qint64 creationTime = 0;
  uint64_t dataVersion = 0; // For invalidation on data changes

  qint64 memoryBytes() const {
    if (pixmap.isNull())
      return 0;
    return pixmap.width() * pixmap.height() * 4; // 4 bytes per pixel (ARGB)
  }
};

/**
 * @brief LRU cache for timeline rendering with memory limits
 *
 * Thread-safe cache that:
 * - Stores rendered track strips/tiles
 * - Evicts least recently used entries when memory limit is reached
 * - Invalidates entries when data version changes
 * - Provides statistics for monitoring
 */
class TimelineRenderCache : public QObject {
  Q_OBJECT

public:
  explicit TimelineRenderCache(QObject* parent = nullptr);
  explicit TimelineRenderCache(const TimelineRenderCacheConfig& config, QObject* parent = nullptr);
  ~TimelineRenderCache() override;

  /**
   * @brief Get a cached render, or null if not cached
   */
  QPixmap get(const RenderCacheKey& key, uint64_t currentDataVersion);

  /**
   * @brief Store a render in the cache
   */
  void put(const RenderCacheKey& key, const QPixmap& pixmap, uint64_t dataVersion);

  /**
   * @brief Check if a key is cached and valid
   */
  bool contains(const RenderCacheKey& key, uint64_t currentDataVersion) const;

  /**
   * @brief Invalidate all entries for a specific track
   */
  void invalidateTrack(int trackIndex);

  /**
   * @brief Invalidate entries in a frame range
   */
  void invalidateFrameRange(int startFrame, int endFrame);

  /**
   * @brief Invalidate all entries
   */
  void invalidateAll();

  /**
   * @brief Clear the entire cache
   */
  void clear();

  /**
   * @brief Get cache statistics
   */
  struct CacheStats {
    int entryCount = 0;
    qint64 memoryUsedBytes = 0;
    qint64 memoryLimitBytes = 0;
    int hitCount = 0;
    int missCount = 0;
    int evictionCount = 0;
    double hitRate() const {
      int total = hitCount + missCount;
      return total > 0 ? static_cast<double>(hitCount) / total : 0.0;
    }
  };
  CacheStats getStats() const;

  /**
   * @brief Configure the cache
   */
  void setConfig(const TimelineRenderCacheConfig& config);
  TimelineRenderCacheConfig config() const { return m_config; }

  /**
   * @brief Enable/disable caching
   */
  void setEnabled(bool enabled);
  bool isEnabled() const { return m_config.enableCache; }

signals:
  /**
   * @brief Emitted when cache is cleared or significantly changed
   */
  void cacheInvalidated();

  /**
   * @brief Emitted when memory usage changes significantly
   */
  void memoryUsageChanged(qint64 usedBytes, qint64 limitBytes);

private:
  void evictIfNeeded(qint64 requiredSpace);
  void evictLRU();
  void updateMemoryUsage(qint64 delta);

  TimelineRenderCacheConfig m_config;
  mutable QMutex m_mutex;

  // LRU list: front = most recently used, back = least recently used
  std::list<RenderCacheKey> m_lruList;
  std::unordered_map<RenderCacheKey,
                     std::pair<std::list<RenderCacheKey>::iterator, RenderCacheEntry>>
      m_cache;

  qint64 m_currentMemoryBytes = 0;
  std::atomic<qint64> m_accessCounter{0};
  mutable std::atomic<int> m_hitCount{0};
  mutable std::atomic<int> m_missCount{0};
  std::atomic<int> m_evictionCount{0};
};

/**
 * @brief RAII helper for scoped cache operations
 */
class ScopedCacheInvalidation {
public:
  ScopedCacheInvalidation(TimelineRenderCache* cache, int trackIndex)
      : m_cache(cache), m_trackIndex(trackIndex) {}

  ~ScopedCacheInvalidation() {
    if (m_cache) {
      m_cache->invalidateTrack(m_trackIndex);
    }
  }

  ScopedCacheInvalidation(const ScopedCacheInvalidation&) = delete;
  ScopedCacheInvalidation& operator=(const ScopedCacheInvalidation&) = delete;

private:
  TimelineRenderCache* m_cache;
  int m_trackIndex;
};

} // namespace NovelMind::editor::qt
