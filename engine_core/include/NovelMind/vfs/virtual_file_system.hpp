#pragma once

#include "NovelMind/vfs/file_handle.hpp"
#include "NovelMind/vfs/file_system_backend.hpp"
#include "NovelMind/vfs/resource_cache.hpp"
#include <functional>
#include <memory>
#include <vector>

namespace NovelMind::VFS {

struct VFSConfig {
  usize cacheMaxSize = 64 * 1024 * 1024;
  bool enableCaching = true;
  bool enableLogging = false;
};

struct VFSStats {
  usize totalResources = 0;
  usize loadedResources = 0;
  usize backendsCount = 0;
  CacheStats cacheStats;
};

class VirtualFileSystem {
public:
  VirtualFileSystem();
  explicit VirtualFileSystem(const VFSConfig& config);
  ~VirtualFileSystem();

  VirtualFileSystem(const VirtualFileSystem&) = delete;
  VirtualFileSystem& operator=(const VirtualFileSystem&) = delete;

  Result<void> initialize();
  void shutdown();

  void registerBackend(std::unique_ptr<IFileSystemBackend> backend);
  void unregisterBackend(const std::string& name);

  [[nodiscard]] std::unique_ptr<IFileHandle> openStream(const ResourceId& id);
  [[nodiscard]] Result<std::vector<u8>> readAll(const ResourceId& id);
  [[nodiscard]] Result<std::vector<u8>> readAll(const std::string& id);

  [[nodiscard]] bool exists(const ResourceId& id) const;
  [[nodiscard]] bool exists(const std::string& id) const;
  [[nodiscard]] std::optional<ResourceInfo> getInfo(const ResourceId& id) const;
  [[nodiscard]] std::vector<ResourceId>
  listResources(ResourceType type = ResourceType::Unknown) const;

  void clearCache();
  void setCacheMaxSize(usize maxSize);
  [[nodiscard]] VFSStats stats() const;

  using ResourceLoadCallback = std::function<void(const ResourceId&, bool success)>;
  void setLoadCallback(ResourceLoadCallback callback) { m_loadCallback = std::move(callback); }

private:
  [[nodiscard]] IFileSystemBackend* findBackend(const ResourceId& id) const;
  void sortBackendsByPriority();

  VFSConfig m_config;
  std::vector<std::unique_ptr<IFileSystemBackend>> m_backends;
  std::unique_ptr<ResourceCache> m_cache;
  ResourceLoadCallback m_loadCallback;
  bool m_initialized = false;
  mutable std::mutex m_mutex;
};

VirtualFileSystem& getGlobalVFS();
void setGlobalVFS(std::unique_ptr<VirtualFileSystem> vfs);

} // namespace NovelMind::VFS
