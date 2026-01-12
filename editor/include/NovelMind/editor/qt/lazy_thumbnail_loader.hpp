#pragma once

/**
 * @file lazy_thumbnail_loader.hpp
 * @brief Lazy thumbnail loading with task cancellation and parallelism limits
 *
 * Provides:
 * - Background thumbnail loading with QThreadPool
 * - Task cancellation via atomic flags
 * - Parallelism limit (configurable max concurrent tasks)
 * - Memory-bounded LRU cache with eviction policy
 * - Safe shutdown (no callbacks after destroy)
 */

#include <QCache>
#include <QDateTime>
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QPixmap>
#include <QPointer>
#include <QQueue>
#include <QRunnable>
#include <QSet>
#include <QThreadPool>
#include <QWaitCondition>

#include <atomic>
#include <memory>

namespace NovelMind::editor::qt {

/**
 * @brief Configuration for the thumbnail loader
 */
struct ThumbnailLoaderConfig {
  int maxConcurrentTasks = 2;     // Max parallel loading tasks
  int maxCacheSizeKB = 50 * 1024; // 50 MB default cache size
  int thumbnailSize = 80;         // Default thumbnail size in pixels
  int queueHighWaterMark = 100;   // Max pending requests before dropping old ones
};

/**
 * @brief Cached thumbnail entry with metadata for invalidation
 */
struct CachedThumbnail {
  QPixmap pixmap;
  QDateTime lastModified;
  qint64 fileSize = 0;
  qint64 accessTime = 0; // For LRU tracking

  int costKB() const {
    if (pixmap.isNull())
      return 0;
    return static_cast<int>(pixmap.width() * pixmap.height() * 4 / 1024);
  }
};

/**
 * @brief Request for loading a thumbnail
 */
struct ThumbnailRequest {
  QString path;
  QSize size;
  int priority = 0; // Higher priority = loaded first
};

/**
 * @brief Task for loading a single thumbnail in background
 */
class ThumbnailLoadTask : public QRunnable {
public:
  ThumbnailLoadTask(const QString& path, const QSize& size,
                    std::shared_ptr<std::atomic<bool>> cancelled, QObject* receiver);

  void run() override;

private:
  QString m_path;
  QSize m_size;
  std::shared_ptr<std::atomic<bool>> m_cancelled;
  QPointer<QObject> m_receiver;
};

/**
 * @brief Lazy thumbnail loader with background loading and caching
 *
 * Thread-safe thumbnail loading with:
 * - Configurable parallelism limit
 * - Task cancellation support
 * - LRU cache with memory limit
 * - Priority queue for visible items
 * - Safe shutdown (waits for tasks or aborts them)
 */
class LazyThumbnailLoader : public QObject {
  Q_OBJECT

public:
  explicit LazyThumbnailLoader(QObject* parent = nullptr);
  explicit LazyThumbnailLoader(const ThumbnailLoaderConfig& config, QObject* parent = nullptr);
  ~LazyThumbnailLoader() override;

  /**
   * @brief Request a thumbnail for a file path
   * @param path File path to load thumbnail for
   * @param size Desired thumbnail size
   * @param priority Loading priority (higher = sooner)
   * @return True if thumbnail is already cached
   */
  bool requestThumbnail(const QString& path, const QSize& size, int priority = 0);

  /**
   * @brief Get a cached thumbnail if available
   * @param path File path
   * @return Cached pixmap or null pixmap if not cached
   */
  QPixmap getCached(const QString& path) const;

  /**
   * @brief Check if a thumbnail is valid (file not modified since caching)
   */
  bool isThumbnailValid(const QString& path) const;

  /**
   * @brief Cancel all pending thumbnail requests
   */
  void cancelPending();

  /**
   * @brief Cancel a specific pending request
   */
  void cancelRequest(const QString& path);

  /**
   * @brief Clear the cache
   */
  void clearCache();

  /**
   * @brief Get cache statistics
   */
  struct CacheStats {
    int cachedCount = 0;
    int pendingCount = 0;
    int activeCount = 0;
    qint64 cacheSizeKB = 0;
    qint64 maxCacheSizeKB = 0;
    int hitCount = 0;
    int missCount = 0;
  };
  CacheStats getStats() const;

  /**
   * @brief Configure the loader
   */
  void setConfig(const ThumbnailLoaderConfig& config);
  ThumbnailLoaderConfig config() const { return m_config; }

  /**
   * @brief Check if shutdown is in progress
   */
  bool isShuttingDown() const { return m_shuttingDown.load(); }

signals:
  /**
   * @brief Emitted when a thumbnail is ready
   * @param path File path
   * @param pixmap Loaded pixmap
   */
  void thumbnailReady(const QString& path, const QPixmap& pixmap);

  /**
   * @brief Emitted when thumbnail loading fails
   * @param path File path
   * @param error Error message
   */
  void thumbnailFailed(const QString& path, const QString& error);

  /**
   * @brief Internal signal for thread-safe thumbnail delivery
   */
  void thumbnailLoadedInternal(const QString& path, const QPixmap& pixmap,
                               const QDateTime& lastModified, qint64 fileSize);

private slots:
  void onThumbnailLoaded(const QString& path, const QPixmap& pixmap, const QDateTime& lastModified,
                         qint64 fileSize);

private:
  void processQueue();
  void trimCache();
  QString cacheKey(const QString& path, const QSize& size) const;

  ThumbnailLoaderConfig m_config;
  QThreadPool* m_threadPool = nullptr;

  mutable QMutex m_mutex;
  QCache<QString, CachedThumbnail> m_cache;
  QQueue<ThumbnailRequest> m_pendingQueue;
  QHash<QString, std::shared_ptr<std::atomic<bool>>> m_activeTasks;
  QSet<QString> m_pendingPaths; // For deduplication

  std::atomic<bool> m_shuttingDown{false};
  mutable std::atomic<int> m_hitCount{0};
  mutable std::atomic<int> m_missCount{0};
  mutable std::atomic<qint64> m_accessCounter{0};
};

} // namespace NovelMind::editor::qt
