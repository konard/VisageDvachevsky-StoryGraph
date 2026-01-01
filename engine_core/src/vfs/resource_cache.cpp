#include "NovelMind/vfs/resource_cache.hpp"

namespace NovelMind::VFS {

ResourceCache::ResourceCache(usize maxSize) : m_maxSize(maxSize) {}

void ResourceCache::setMaxSize(usize maxSize) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_maxSize = maxSize;
  evictIfNeeded(0);
}

std::optional<std::vector<u8>> ResourceCache::get(const ResourceId &id) {
  std::lock_guard<std::mutex> lock(m_mutex);

  const auto it = m_cache.find(id);
  if (it == m_cache.end()) {
    ++m_stats.missCount;
    return std::nullopt;
  }

  ++m_stats.hitCount;
  it->second.lastAccess = std::chrono::steady_clock::now();
  ++it->second.accessCount;
  updateAccessOrder(id);

  return it->second.data;
}

void ResourceCache::put(const ResourceId &id, std::vector<u8> data) {
  std::lock_guard<std::mutex> lock(m_mutex);

  const usize dataSize = data.size();

  if (dataSize > m_maxSize) {
    return;
  }

  const auto existingIt = m_cache.find(id);
  if (existingIt != m_cache.end()) {
    m_currentSize -= existingIt->second.data.size();
    m_cache.erase(existingIt);
    // Decrement entry count when removing existing entry
    if (m_stats.entryCount > 0) {
      --m_stats.entryCount;
    }

    const auto orderIt = m_orderIterators.find(id);
    if (orderIt != m_orderIterators.end()) {
      m_accessOrder.erase(orderIt->second);
      m_orderIterators.erase(orderIt);
    }
  }

  evictIfNeeded(dataSize);

  CacheEntry entry;
  entry.data = std::move(data);
  entry.lastAccess = std::chrono::steady_clock::now();
  entry.accessCount = 1;

  m_cache[id] = std::move(entry);
  m_currentSize += dataSize;

  m_accessOrder.push_front(id);
  m_orderIterators[id] = m_accessOrder.begin();

  ++m_stats.entryCount;
}

void ResourceCache::remove(const ResourceId &id) {
  std::lock_guard<std::mutex> lock(m_mutex);

  const auto it = m_cache.find(id);
  if (it != m_cache.end()) {
    m_currentSize -= it->second.data.size();
    m_cache.erase(it);

    const auto orderIt = m_orderIterators.find(id);
    if (orderIt != m_orderIterators.end()) {
      m_accessOrder.erase(orderIt->second);
      m_orderIterators.erase(orderIt);
    }

    --m_stats.entryCount;
  }
}

void ResourceCache::clear() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_cache.clear();
  m_accessOrder.clear();
  m_orderIterators.clear();
  m_currentSize = 0;
  m_stats.entryCount = 0;
}

bool ResourceCache::contains(const ResourceId &id) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_cache.find(id) != m_cache.end();
}

CacheStats ResourceCache::stats() const {
  std::lock_guard<std::mutex> lock(m_mutex);

  CacheStats result = m_stats;
  result.totalSize = m_currentSize;
  result.entryCount = m_cache.size();
  return result;
}

void ResourceCache::resetStats() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_stats.hitCount = 0;
  m_stats.missCount = 0;
  m_stats.evictionCount = 0;
}

void ResourceCache::evictIfNeeded(usize requiredSpace) {
  while (m_currentSize + requiredSpace > m_maxSize && !m_accessOrder.empty()) {
    const ResourceId &lruId = m_accessOrder.back();
    const auto it = m_cache.find(lruId);

    if (it != m_cache.end()) {
      m_currentSize -= it->second.data.size();
      m_cache.erase(it);
      ++m_stats.evictionCount;
      // Decrement entry count when evicting
      if (m_stats.entryCount > 0) {
        --m_stats.entryCount;
      }
    }

    m_orderIterators.erase(lruId);
    m_accessOrder.pop_back();
  }
}

void ResourceCache::updateAccessOrder(const ResourceId &id) {
  const auto orderIt = m_orderIterators.find(id);
  if (orderIt != m_orderIterators.end()) {
    // Erase from list using the stored iterator (invalidates orderIt->second)
    m_accessOrder.erase(orderIt->second);
    // Add to front of list
    m_accessOrder.push_front(id);
    // Update map with new iterator (orderIt is still valid map iterator)
    orderIt->second = m_accessOrder.begin();
  }
}

} // namespace NovelMind::VFS
