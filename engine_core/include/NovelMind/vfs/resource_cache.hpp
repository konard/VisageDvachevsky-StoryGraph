#pragma once

#include "NovelMind/core/types.hpp"
#include "NovelMind/vfs/resource_id.hpp"
#include <chrono>
#include <list>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace NovelMind::VFS {

struct CacheEntry {
  std::vector<u8> data;
  std::chrono::steady_clock::time_point lastAccess;
  usize accessCount = 0;
};

struct CacheStats {
  usize totalSize = 0;
  usize entryCount = 0;
  usize hitCount = 0;
  usize missCount = 0;
  usize evictionCount = 0;

  [[nodiscard]] f64 hitRate() const {
    const auto total = hitCount + missCount;
    return total > 0 ? static_cast<f64>(hitCount) / static_cast<f64>(total) : 0.0;
  }
};

class ResourceCache {
public:
  explicit ResourceCache(usize maxSize = 64 * 1024 * 1024);
  ~ResourceCache() = default;

  ResourceCache(const ResourceCache&) = delete;
  ResourceCache& operator=(const ResourceCache&) = delete;

  void setMaxSize(usize maxSize);
  [[nodiscard]] usize maxSize() const { return m_maxSize; }

  [[nodiscard]] std::optional<std::vector<u8>> get(const ResourceId& id);
  void put(const ResourceId& id, std::vector<u8> data);
  void remove(const ResourceId& id);
  void clear();

  [[nodiscard]] bool contains(const ResourceId& id) const;
  [[nodiscard]] usize currentSize() const { return m_currentSize; }
  [[nodiscard]] usize entryCount() const { return m_cache.size(); }

  [[nodiscard]] CacheStats stats() const;
  void resetStats();

private:
  void evictIfNeeded(usize requiredSpace);
  void updateAccessOrder(const ResourceId& id);

  mutable std::mutex m_mutex;
  usize m_maxSize;
  usize m_currentSize = 0;

  std::unordered_map<ResourceId, CacheEntry> m_cache;
  std::list<ResourceId> m_accessOrder;
  std::unordered_map<ResourceId, std::list<ResourceId>::iterator> m_orderIterators;

  mutable CacheStats m_stats;
};

} // namespace NovelMind::VFS
