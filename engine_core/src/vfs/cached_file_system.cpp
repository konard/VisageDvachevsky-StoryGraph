#include "NovelMind/vfs/cached_file_system.hpp"

namespace NovelMind::vfs {

CachedFileSystem::CachedFileSystem(std::unique_ptr<IVirtualFileSystem> inner, usize maxBytes)
    : m_maxBytes(maxBytes), m_inner(std::move(inner)) {}

Result<void> CachedFileSystem::mount(const std::string& packPath) {
  if (m_inner) {
    return m_inner->mount(packPath);
  }
  return Result<void>::error("CachedFileSystem has no inner FS");
}

void CachedFileSystem::unmount(const std::string& packPath) {
  if (m_inner) {
    m_inner->unmount(packPath);
  }
  clearCache();
}

void CachedFileSystem::unmountAll() {
  if (m_inner) {
    m_inner->unmountAll();
  }
  clearCache();
}

Result<std::vector<u8>> CachedFileSystem::readFile(const std::string& resourceId) const {
  auto it = m_cache.find(resourceId);
  if (it != m_cache.end()) {
    touch(resourceId);
    return Result<std::vector<u8>>::ok(it->second.first.data);
  }

  if (!m_inner) {
    return Result<std::vector<u8>>::error("CachedFileSystem has no inner FS");
  }

  auto result = m_inner->readFile(resourceId);
  if (result.isError()) {
    return result;
  }

  CacheEntry entry;
  entry.data = result.value();
  entry.size = entry.data.size();

  m_lru.push_front(resourceId);
  m_cache[resourceId] = {entry, m_lru.begin()};
  m_currentBytes += entry.size;
  evictIfNeeded();

  return Result<std::vector<u8>>::ok(std::move(entry.data));
}

bool CachedFileSystem::exists(const std::string& resourceId) const {
  if (m_cache.find(resourceId) != m_cache.end()) {
    return true;
  }
  return m_inner ? m_inner->exists(resourceId) : false;
}

std::optional<ResourceInfo> CachedFileSystem::getInfo(const std::string& resourceId) const {
  if (m_inner) {
    return m_inner->getInfo(resourceId);
  }
  return std::nullopt;
}

std::vector<std::string> CachedFileSystem::listResources(ResourceType type) const {
  if (m_inner) {
    return m_inner->listResources(type);
  }
  return {};
}

void CachedFileSystem::setMaxBytes(usize maxBytes) {
  m_maxBytes = maxBytes;
  evictIfNeeded();
}

void CachedFileSystem::clearCache() {
  m_cache.clear();
  m_lru.clear();
  m_currentBytes = 0;
}

void CachedFileSystem::touch(const std::string& resourceId) const {
  auto it = m_cache.find(resourceId);
  if (it == m_cache.end()) {
    return;
  }

  m_lru.erase(it->second.second);
  m_lru.push_front(resourceId);
  it->second.second = m_lru.begin();
}

void CachedFileSystem::evictIfNeeded() const {
  if (m_maxBytes == 0) {
    return;
  }
  while (m_currentBytes > m_maxBytes && !m_lru.empty()) {
    const std::string& key = m_lru.back();
    auto it = m_cache.find(key);
    if (it != m_cache.end()) {
      m_currentBytes -= it->second.first.size;
      m_cache.erase(it);
    }
    m_lru.pop_back();
  }
}

} // namespace NovelMind::vfs
