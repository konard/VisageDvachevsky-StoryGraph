#pragma once

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include <optional>
#include <string>
#include <vector>

namespace NovelMind::vfs {

enum class ResourceType : u8 {
  Unknown = 0,
  Texture = 1,
  Audio = 2,
  Music = 3,
  Font = 4,
  Script = 5,
  Scene = 6,
  Localization = 7,
  Data = 8
};

struct ResourceInfo {
  std::string id;
  ResourceType type;
  usize size;
  u32 checksum;
};

class IVirtualFileSystem {
public:
  virtual ~IVirtualFileSystem() = default;

  virtual Result<void> mount(const std::string& packPath) = 0;
  virtual void unmount(const std::string& packPath) = 0;
  virtual void unmountAll() = 0;

  [[nodiscard]] virtual Result<std::vector<u8>> readFile(const std::string& resourceId) const = 0;

  [[nodiscard]] virtual bool exists(const std::string& resourceId) const = 0;

  [[nodiscard]] virtual std::optional<ResourceInfo>
  getInfo(const std::string& resourceId) const = 0;

  [[nodiscard]] virtual std::vector<std::string>
  listResources(ResourceType type = ResourceType::Unknown) const = 0;
};

} // namespace NovelMind::vfs
