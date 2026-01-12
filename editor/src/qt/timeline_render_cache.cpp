#include "NovelMind/editor/qt/timeline_render_cache.hpp"

#ifdef NOVELMIND_PROFILING_ENABLED
#include "NovelMind/core/profiler.hpp"
#endif

namespace NovelMind::editor::qt {

TimelineRenderCache::TimelineRenderCache(QObject* parent)
    : TimelineRenderCache(TimelineRenderCacheConfig{}, parent) {}

TimelineRenderCache::TimelineRenderCache(const TimelineRenderCacheConfig& config, QObject* parent)
    : QObject(parent), m_config(config) {}

TimelineRenderCache::~TimelineRenderCache() {
  clear();
}

QPixmap TimelineRenderCache::get(const RenderCacheKey& key, uint64_t currentDataVersion) {
  if (!m_config.enableCache) {
    m_missCount.fetch_add(1);
    return QPixmap();
  }

#ifdef NOVELMIND_PROFILING_ENABLED
  NOVELMIND_PROFILE_SCOPE_CAT("TimelineRenderCache::get", "Timeline");
#endif

  QMutexLocker locker(&m_mutex);

  auto it = m_cache.find(key);
  if (it == m_cache.end()) {
    m_missCount.fetch_add(1);
    return QPixmap();
  }

  auto& [lruIt, entry] = it->second;

  // Check if entry is still valid
  if (entry.dataVersion != currentDataVersion) {
    // Entry is stale, remove it
    m_lruList.erase(lruIt);
    updateMemoryUsage(-entry.memoryBytes());
    m_cache.erase(it);
    m_missCount.fetch_add(1);
    return QPixmap();
  }

  // Move to front of LRU list (most recently used)
  m_lruList.erase(lruIt);
  m_lruList.push_front(key);
  it->second.first = m_lruList.begin();

  // Update access time
  entry.accessTime = m_accessCounter.fetch_add(1);

  m_hitCount.fetch_add(1);
  return entry.pixmap;
}

void TimelineRenderCache::put(const RenderCacheKey& key, const QPixmap& pixmap,
                              uint64_t dataVersion) {
  if (!m_config.enableCache || pixmap.isNull()) {
    return;
  }

#ifdef NOVELMIND_PROFILING_ENABLED
  NOVELMIND_PROFILE_SCOPE_CAT("TimelineRenderCache::put", "Timeline");
#endif

  QMutexLocker locker(&m_mutex);

  qint64 entrySize = pixmap.width() * pixmap.height() * 4;

  // Check if entry is too large for cache
  if (entrySize > m_config.maxMemoryBytes / 2) {
    return; // Don't cache entries that are more than half the cache size
  }

  // Remove existing entry if present
  auto existingIt = m_cache.find(key);
  if (existingIt != m_cache.end()) {
    m_lruList.erase(existingIt->second.first);
    updateMemoryUsage(-existingIt->second.second.memoryBytes());
    m_cache.erase(existingIt);
  }

  // Evict entries if needed to make room
  evictIfNeeded(entrySize);

  // Create new entry
  RenderCacheEntry entry;
  entry.pixmap = pixmap;
  entry.accessTime = m_accessCounter.fetch_add(1);
  entry.creationTime = entry.accessTime;
  entry.dataVersion = dataVersion;

  // Add to LRU list and cache
  m_lruList.push_front(key);
  m_cache[key] = std::make_pair(m_lruList.begin(), entry);

  updateMemoryUsage(entrySize);
}

bool TimelineRenderCache::contains(const RenderCacheKey& key, uint64_t currentDataVersion) const {
  if (!m_config.enableCache) {
    return false;
  }

  QMutexLocker locker(&m_mutex);

  auto it = m_cache.find(key);
  if (it == m_cache.end()) {
    return false;
  }

  return it->second.second.dataVersion == currentDataVersion;
}

void TimelineRenderCache::invalidateTrack(int trackIndex) {
  QMutexLocker locker(&m_mutex);

  // Find and remove all entries for this track
  auto it = m_cache.begin();
  while (it != m_cache.end()) {
    if (it->first.trackIndex == trackIndex) {
      m_lruList.erase(it->second.first);
      updateMemoryUsage(-it->second.second.memoryBytes());
      it = m_cache.erase(it);
    } else {
      ++it;
    }
  }

  locker.unlock();
  emit cacheInvalidated();
}

void TimelineRenderCache::invalidateFrameRange(int startFrame, int endFrame) {
  QMutexLocker locker(&m_mutex);

  // Find and remove all entries that overlap with the frame range
  auto it = m_cache.begin();
  while (it != m_cache.end()) {
    const auto& key = it->first;
    // Check if ranges overlap
    bool overlaps = !(key.endFrame < startFrame || key.startFrame > endFrame);
    if (overlaps) {
      m_lruList.erase(it->second.first);
      updateMemoryUsage(-it->second.second.memoryBytes());
      it = m_cache.erase(it);
    } else {
      ++it;
    }
  }

  locker.unlock();
  emit cacheInvalidated();
}

void TimelineRenderCache::invalidateAll() {
  QMutexLocker locker(&m_mutex);

  m_cache.clear();
  m_lruList.clear();
  m_currentMemoryBytes = 0;

  locker.unlock();
  emit cacheInvalidated();
  emit memoryUsageChanged(0, m_config.maxMemoryBytes);
}

void TimelineRenderCache::clear() {
  invalidateAll();
  m_hitCount.store(0);
  m_missCount.store(0);
  m_evictionCount.store(0);
}

TimelineRenderCache::CacheStats TimelineRenderCache::getStats() const {
  QMutexLocker locker(&m_mutex);

  CacheStats stats;
  stats.entryCount = static_cast<int>(m_cache.size());
  stats.memoryUsedBytes = m_currentMemoryBytes;
  stats.memoryLimitBytes = m_config.maxMemoryBytes;
  stats.hitCount = m_hitCount.load();
  stats.missCount = m_missCount.load();
  stats.evictionCount = m_evictionCount.load();

  return stats;
}

void TimelineRenderCache::setConfig(const TimelineRenderCacheConfig& config) {
  QMutexLocker locker(&m_mutex);

  m_config = config;

  // Evict entries if new memory limit is smaller
  while (m_currentMemoryBytes > m_config.maxMemoryBytes && !m_lruList.empty()) {
    evictLRU();
  }
}

void TimelineRenderCache::setEnabled(bool enabled) {
  if (m_config.enableCache != enabled) {
    m_config.enableCache = enabled;
    if (!enabled) {
      clear();
    }
  }
}

void TimelineRenderCache::evictIfNeeded(qint64 requiredSpace) {
  // Evict entries until we have enough space
  while (m_currentMemoryBytes + requiredSpace > m_config.maxMemoryBytes && !m_lruList.empty()) {
    evictLRU();
  }
}

void TimelineRenderCache::evictLRU() {
  if (m_lruList.empty()) {
    return;
  }

  // Get least recently used key (back of list)
  const RenderCacheKey& lruKey = m_lruList.back();

  auto it = m_cache.find(lruKey);
  if (it != m_cache.end()) {
    updateMemoryUsage(-it->second.second.memoryBytes());
    m_cache.erase(it);
    m_evictionCount.fetch_add(1);
  }

  m_lruList.pop_back();
}

void TimelineRenderCache::updateMemoryUsage(qint64 delta) {
  qint64 oldMemory = m_currentMemoryBytes;
  m_currentMemoryBytes += delta;

  // Emit signal if memory usage changed significantly (>10%)
  qint64 threshold = m_config.maxMemoryBytes / 10;
  if (std::abs(m_currentMemoryBytes - oldMemory) > threshold) {
    // Unlock before emitting signal to prevent deadlock
    // Note: This requires careful handling in the calling code
    emit memoryUsageChanged(m_currentMemoryBytes, m_config.maxMemoryBytes);
  }
}

} // namespace NovelMind::editor::qt
