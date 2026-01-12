#pragma once

#include "NovelMind/core/types.hpp"
#include "NovelMind/vfs/virtual_fs.hpp"
#include <list>
#include <memory>
#include <unordered_map>

namespace NovelMind::vfs {

class CachedFileSystem final : public IVirtualFileSystem {
public:
  explicit CachedFileSystem(std::unique_ptr<IVirtualFileSystem> inner,
                            usize maxBytes = 64 * 1024 * 1024);

  Result<void> mount(const std::string& packPath) override;
  void unmount(const std::string& packPath) override;
  void unmountAll() override;

  [[nodiscard]] Result<std::vector<u8>> readFile(const std::string& resourceId) const override;

  [[nodiscard]] bool exists(const std::string& resourceId) const override;
  [[nodiscard]] std::optional<ResourceInfo> getInfo(const std::string& resourceId) const override;

  [[nodiscard]] std::vector<std::string>
  listResources(ResourceType type = ResourceType::Unknown) const override;

  void setMaxBytes(usize maxBytes);
  void clearCache();

private:
  struct CacheEntry {
    std::vector<u8> data;
    usize size = 0;
  };

  void touch(const std::string& resourceId) const;
  void evictIfNeeded() const;

  mutable std::unordered_map<std::string, std::pair<CacheEntry, std::list<std::string>::iterator>>
      m_cache;
  mutable std::list<std::string> m_lru;
  mutable usize m_currentBytes = 0;
  usize m_maxBytes = 0;

  std::unique_ptr<IVirtualFileSystem> m_inner;
};

} // namespace NovelMind::vfs
