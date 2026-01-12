#pragma once

#include "NovelMind/vfs/virtual_fs.hpp"
#include <mutex>
#include <unordered_map>

namespace NovelMind::vfs {

class MemoryFileSystem : public IVirtualFileSystem {
public:
  MemoryFileSystem() = default;
  ~MemoryFileSystem() override = default;

  Result<void> mount(const std::string& packPath) override;
  void unmount(const std::string& packPath) override;
  void unmountAll() override;

  [[nodiscard]] Result<std::vector<u8>> readFile(const std::string& resourceId) const override;

  [[nodiscard]] bool exists(const std::string& resourceId) const override;

  [[nodiscard]] std::optional<ResourceInfo> getInfo(const std::string& resourceId) const override;

  [[nodiscard]] std::vector<std::string>
  listResources(ResourceType type = ResourceType::Unknown) const override;

  void addResource(const std::string& resourceId, std::vector<u8> data,
                   ResourceType type = ResourceType::Data);

  void removeResource(const std::string& resourceId);

  void clear();

private:
  struct ResourceEntry {
    std::vector<u8> data;
    ResourceType type;
    u32 checksum;
  };

  [[nodiscard]] static u32 calculateChecksum(const std::vector<u8>& data);

  mutable std::mutex m_mutex;
  std::unordered_map<std::string, ResourceEntry> m_resources;
};

} // namespace NovelMind::vfs
