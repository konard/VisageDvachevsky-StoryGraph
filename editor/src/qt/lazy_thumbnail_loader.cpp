#include "NovelMind/editor/qt/lazy_thumbnail_loader.hpp"

#include <QCoreApplication>
#include <QFileInfo>
#include <QImageReader>
#include <QThread>

#ifdef NOVELMIND_PROFILING_ENABLED
#include "NovelMind/core/profiler.hpp"
#endif

namespace NovelMind::editor::qt {

// =============================================================================
// ThumbnailLoadTask Implementation
// =============================================================================

ThumbnailLoadTask::ThumbnailLoadTask(
    const QString &path, const QSize &size,
    std::shared_ptr<std::atomic<bool>> cancelled, QObject *receiver)
    : m_path(path), m_size(size), m_cancelled(std::move(cancelled)),
      m_receiver(receiver) {
  setAutoDelete(true);
}

void ThumbnailLoadTask::run() {
  // Early exit if cancelled or receiver destroyed
  if (m_cancelled->load() || m_receiver.isNull()) {
    return;
  }

  QFileInfo fileInfo(m_path);
  if (!fileInfo.exists() || !fileInfo.isFile()) {
    return;
  }

  // Check cancellation again before expensive image load
  if (m_cancelled->load()) {
    return;
  }

#ifdef NOVELMIND_PROFILING_ENABLED
  NOVELMIND_PROFILE_SCOPE_CAT("ThumbnailLoadTask::run", "AssetBrowser");
#endif

  QImageReader reader(m_path);
  reader.setAutoTransform(true);

  // Scale during read for efficiency
  QSize originalSize = reader.size();
  if (originalSize.isValid()) {
    QSize scaledSize = originalSize.scaled(m_size, Qt::KeepAspectRatio);
    reader.setScaledSize(scaledSize);
  }

  QImage image = reader.read();

  // Check cancellation after load
  if (m_cancelled->load() || m_receiver.isNull()) {
    return;
  }

  if (image.isNull()) {
    return;
  }

  QPixmap pixmap = QPixmap::fromImage(image);

  // Final cancellation check before delivery
  if (m_cancelled->load()) {
    return;
  }

  // Thread-safe delivery via queued connection
  // Issue #173: Use local copy of QPointer data to prevent TOCTOU race
  // The QPointer check and subsequent use must use the same pointer value
  if (auto *receiver = m_receiver.data()) {
    auto *loader = qobject_cast<LazyThumbnailLoader *>(receiver);
    if (loader && !loader->isShuttingDown()) {
      // Use QMetaObject::invokeMethod for guaranteed thread-safe delivery
      // This ensures the slot is called in the receiver's thread
      QMetaObject::invokeMethod(
          loader,
          [loader, path = m_path, pixmap, lastModified = fileInfo.lastModified(),
           size = fileInfo.size()]() {
            emit loader->thumbnailLoadedInternal(path, pixmap, lastModified,
                                                 size);
          },
          Qt::QueuedConnection);
    }
  }
}

// =============================================================================
// LazyThumbnailLoader Implementation
// =============================================================================

LazyThumbnailLoader::LazyThumbnailLoader(QObject *parent)
    : LazyThumbnailLoader(ThumbnailLoaderConfig{}, parent) {}

LazyThumbnailLoader::LazyThumbnailLoader(const ThumbnailLoaderConfig &config,
                                         QObject *parent)
    : QObject(parent), m_config(config) {
  m_cache.setMaxCost(config.maxCacheSizeKB);

  m_threadPool = new QThreadPool(this);
  m_threadPool->setMaxThreadCount(config.maxConcurrentTasks);

  // Connect internal signal for thread-safe thumbnail delivery
  connect(this, &LazyThumbnailLoader::thumbnailLoadedInternal, this,
          &LazyThumbnailLoader::onThumbnailLoaded, Qt::QueuedConnection);
}

LazyThumbnailLoader::~LazyThumbnailLoader() {
  // Signal shutdown to prevent new work
  m_shuttingDown.store(true);

  // Cancel all pending tasks
  cancelPending();

  // Wait for running tasks to complete (with timeout)
  if (m_threadPool) {
    m_threadPool->waitForDone(2000); // 2 second timeout
  }
}

bool LazyThumbnailLoader::requestThumbnail(const QString &path,
                                           const QSize &size, int priority) {
  if (m_shuttingDown.load()) {
    return false;
  }

  QString key = cacheKey(path, size);

  QMutexLocker locker(&m_mutex);

  // Check cache first
  if (auto *cached = m_cache.object(key)) {
    // Validate cache entry
    QFileInfo info(path);
    if (info.exists() && info.lastModified() == cached->lastModified &&
        info.size() == cached->fileSize) {
      cached->accessTime = m_accessCounter.fetch_add(1);
      m_hitCount.fetch_add(1);
      return true; // Already cached and valid
    }
    // Cache entry is stale, remove it
    m_cache.remove(key);
  }

  m_missCount.fetch_add(1);

  // Check if already pending or active
  if (m_pendingPaths.contains(path) || m_activeTasks.contains(path)) {
    return false;
  }

  // Add to pending queue
  ThumbnailRequest request{path, size, priority};
  m_pendingQueue.enqueue(request);
  m_pendingPaths.insert(path);

  // Trim queue if too large (drop oldest low-priority items)
  while (m_pendingQueue.size() > m_config.queueHighWaterMark) {
    ThumbnailRequest dropped = m_pendingQueue.dequeue();
    m_pendingPaths.remove(dropped.path);
  }

  locker.unlock();
  processQueue();

  return false;
}

QPixmap LazyThumbnailLoader::getCached(const QString &path) const {
  QMutexLocker locker(&m_mutex);

  // Try all common sizes
  for (int size : {64, 80, 96, 128}) {
    QString key = cacheKey(path, QSize(size, size));
    if (auto *cached = m_cache.object(key)) {
      const_cast<CachedThumbnail *>(cached)->accessTime =
          m_accessCounter.fetch_add(1);
      m_hitCount.fetch_add(1);
      return cached->pixmap;
    }
  }

  m_missCount.fetch_add(1);
  return QPixmap();
}

bool LazyThumbnailLoader::isThumbnailValid(const QString &path) const {
  QMutexLocker locker(&m_mutex);

  for (int size : {64, 80, 96, 128}) {
    QString key = cacheKey(path, QSize(size, size));
    if (auto *cached = m_cache.object(key)) {
      QFileInfo info(path);
      if (info.exists() && info.lastModified() == cached->lastModified &&
          info.size() == cached->fileSize) {
        return true;
      }
    }
  }

  return false;
}

void LazyThumbnailLoader::cancelPending() {
  QMutexLocker locker(&m_mutex);

  // Clear pending queue
  m_pendingQueue.clear();
  m_pendingPaths.clear();

  // Signal cancellation to all active tasks
  for (auto &cancelFlag : m_activeTasks) {
    cancelFlag->store(true);
  }
  m_activeTasks.clear();
}

void LazyThumbnailLoader::cancelRequest(const QString &path) {
  QMutexLocker locker(&m_mutex);

  // Remove from pending
  m_pendingPaths.remove(path);

  // Signal cancellation if active
  auto it = m_activeTasks.find(path);
  if (it != m_activeTasks.end()) {
    it.value()->store(true);
    m_activeTasks.erase(it);
  }
}

void LazyThumbnailLoader::clearCache() {
  QMutexLocker locker(&m_mutex);
  m_cache.clear();
  m_hitCount.store(0);
  m_missCount.store(0);
}

LazyThumbnailLoader::CacheStats LazyThumbnailLoader::getStats() const {
  QMutexLocker locker(&m_mutex);

  CacheStats stats;
  stats.cachedCount = static_cast<int>(m_cache.count());
  stats.pendingCount = static_cast<int>(m_pendingQueue.size());
  stats.activeCount = static_cast<int>(m_activeTasks.size());
  stats.cacheSizeKB = m_cache.totalCost();
  stats.maxCacheSizeKB = m_cache.maxCost();
  stats.hitCount = m_hitCount.load();
  stats.missCount = m_missCount.load();

  return stats;
}

void LazyThumbnailLoader::setConfig(const ThumbnailLoaderConfig &config) {
  QMutexLocker locker(&m_mutex);
  m_config = config;
  m_cache.setMaxCost(config.maxCacheSizeKB);
  if (m_threadPool) {
    m_threadPool->setMaxThreadCount(config.maxConcurrentTasks);
  }
}

void LazyThumbnailLoader::onThumbnailLoaded(const QString &path,
                                            const QPixmap &pixmap,
                                            const QDateTime &lastModified,
                                            qint64 fileSize) {
  if (m_shuttingDown.load()) {
    return;
  }

  QMutexLocker locker(&m_mutex);

  // Remove from active tasks
  m_activeTasks.remove(path);

  if (pixmap.isNull()) {
    locker.unlock();
    emit thumbnailFailed(path, tr("Failed to load image"));
    processQueue();
    return;
  }

  // Cache the result
  QString key = cacheKey(path, pixmap.size());
  auto *entry = new CachedThumbnail{pixmap, lastModified, fileSize,
                                    m_accessCounter.fetch_add(1)};

  m_cache.insert(key, entry, entry->costKB());

  locker.unlock();

  // Notify listeners
  emit thumbnailReady(path, pixmap);

  // Process more pending requests
  processQueue();
}

void LazyThumbnailLoader::processQueue() {
  if (m_shuttingDown.load()) {
    return;
  }

  QMutexLocker locker(&m_mutex);

  // Check if we can start more tasks
  while (m_activeTasks.size() < m_config.maxConcurrentTasks &&
         !m_pendingQueue.isEmpty()) {
    ThumbnailRequest request = m_pendingQueue.dequeue();
    m_pendingPaths.remove(request.path);

    // Skip if already active
    if (m_activeTasks.contains(request.path)) {
      continue;
    }

    // Create cancellation flag
    auto cancelFlag = std::make_shared<std::atomic<bool>>(false);
    m_activeTasks[request.path] = cancelFlag;

    // Create and start task
    auto *task =
        new ThumbnailLoadTask(request.path, request.size, cancelFlag, this);
    m_threadPool->start(task);
  }
}

void LazyThumbnailLoader::trimCache() {
  // QCache handles eviction automatically based on cost
  // This method is here for future custom LRU policies if needed
}

QString LazyThumbnailLoader::cacheKey(const QString &path,
                                      const QSize &size) const {
  return QString("%1_%2x%3").arg(path).arg(size.width()).arg(size.height());
}

} // namespace NovelMind::editor::qt
