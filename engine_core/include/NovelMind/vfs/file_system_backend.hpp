#pragma once

#include "NovelMind/vfs/file_handle.hpp"
#include "NovelMind/vfs/resource_id.hpp"
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace NovelMind::VFS {

class IFileSystemBackend {
public:
  virtual ~IFileSystemBackend() = default;

  [[nodiscard]] virtual std::string name() const = 0;
  [[nodiscard]] virtual u32 priority() const { return 0; }

  [[nodiscard]] virtual std::unique_ptr<IFileHandle> open(const ResourceId& id) = 0;
  [[nodiscard]] virtual bool exists(const ResourceId& id) const = 0;
  [[nodiscard]] virtual std::optional<ResourceInfo> getInfo(const ResourceId& id) const = 0;
  [[nodiscard]] virtual std::vector<ResourceId>
  list(ResourceType type = ResourceType::Unknown) const = 0;

  virtual Result<void> initialize() { return Result<void>::ok(); }
  virtual void shutdown() {}
};

class MemoryBackend : public IFileSystemBackend {
public:
  MemoryBackend() = default;
  ~MemoryBackend() override = default;

  [[nodiscard]] std::string name() const override { return "memory"; }
  [[nodiscard]] u32 priority() const override { return 100; }

  [[nodiscard]] std::unique_ptr<IFileHandle> open(const ResourceId& id) override;
  [[nodiscard]] bool exists(const ResourceId& id) const override;
  [[nodiscard]] std::optional<ResourceInfo> getInfo(const ResourceId& id) const override;
  [[nodiscard]] std::vector<ResourceId> list(ResourceType type) const override;

  void addResource(const ResourceId& id, std::vector<u8> data);
  void addResource(const std::string& id, std::vector<u8> data,
                   ResourceType type = ResourceType::Data);
  void removeResource(const ResourceId& id);
  void clear();

  [[nodiscard]] usize resourceCount() const { return m_resources.size(); }

private:
  struct ResourceEntry {
    std::vector<u8> data;
    ResourceInfo info;
  };

  [[nodiscard]] static u32 calculateChecksum(const std::vector<u8>& data);

  mutable std::mutex m_mutex;
  std::unordered_map<ResourceId, ResourceEntry> m_resources;
};

} // namespace NovelMind::VFS
