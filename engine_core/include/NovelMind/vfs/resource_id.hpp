#pragma once

#include "NovelMind/core/types.hpp"
#include <functional>
#include <string>

namespace NovelMind::VFS {

enum class ResourceType : u8 {
  Unknown = 0,
  Texture = 1,
  Audio = 2,
  Music = 3,
  Font = 4,
  Script = 5,
  Scene = 6,
  Localization = 7,
  Data = 8,
  Shader = 9,
  Config = 10
};

class ResourceId {
public:
  ResourceId() = default;
  explicit ResourceId(std::string id);
  ResourceId(std::string id, ResourceType type);

  [[nodiscard]] const std::string& id() const { return m_id; }
  [[nodiscard]] ResourceType type() const { return m_type; }
  [[nodiscard]] u64 hash() const { return m_hash; }

  [[nodiscard]] bool isEmpty() const { return m_id.empty(); }
  [[nodiscard]] bool isValid() const { return !m_id.empty(); }

  bool operator==(const ResourceId& other) const;
  bool operator!=(const ResourceId& other) const;
  bool operator<(const ResourceId& other) const;

  static ResourceType typeFromExtension(const std::string& path);

private:
  void computeHash();

  std::string m_id;
  ResourceType m_type = ResourceType::Unknown;
  u64 m_hash = 0;
};

struct ResourceInfo {
  ResourceId resourceId;
  usize size = 0;
  usize compressedSize = 0;
  u32 checksum = 0;
  bool encrypted = false;
  bool compressed = false;
};

} // namespace NovelMind::VFS

namespace std {

template <> struct hash<NovelMind::VFS::ResourceId> {
  size_t operator()(const NovelMind::VFS::ResourceId& id) const noexcept {
    return static_cast<size_t>(id.hash());
  }
};

} // namespace std
