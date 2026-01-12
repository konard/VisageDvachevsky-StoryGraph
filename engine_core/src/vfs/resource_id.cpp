#include "NovelMind/vfs/resource_id.hpp"
#include <algorithm>

namespace NovelMind::VFS {

namespace {

u64 fnv1aHash(const std::string& str) {
  constexpr u64 FNV_PRIME = 0x100000001b3ULL;
  constexpr u64 FNV_OFFSET = 0xcbf29ce484222325ULL;

  u64 hash = FNV_OFFSET;
  for (const char c : str) {
    hash ^= static_cast<u64>(static_cast<u8>(c));
    hash *= FNV_PRIME;
  }
  return hash;
}

std::string toLower(const std::string& str) {
  std::string result = str;
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return result;
}

} // anonymous namespace

ResourceId::ResourceId(std::string id) : m_id(std::move(id)) {
  m_type = typeFromExtension(m_id);
  computeHash();
}

ResourceId::ResourceId(std::string id, ResourceType type) : m_id(std::move(id)), m_type(type) {
  computeHash();
}

bool ResourceId::operator==(const ResourceId& other) const {
  return m_hash == other.m_hash && m_id == other.m_id;
}

bool ResourceId::operator!=(const ResourceId& other) const {
  return !(*this == other);
}

bool ResourceId::operator<(const ResourceId& other) const {
  return m_id < other.m_id;
}

void ResourceId::computeHash() {
  m_hash = fnv1aHash(m_id);
}

ResourceType ResourceId::typeFromExtension(const std::string& path) {
  const auto dotPos = path.rfind('.');
  if (dotPos == std::string::npos) {
    return ResourceType::Unknown;
  }

  const std::string ext = toLower(path.substr(dotPos + 1));

  if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" || ext == "tga") {
    return ResourceType::Texture;
  }
  if (ext == "wav" || ext == "ogg" || ext == "mp3" || ext == "flac") {
    return ResourceType::Audio;
  }
  if (ext == "ttf" || ext == "otf") {
    return ResourceType::Font;
  }
  if (ext == "nms" || ext == "nmscript") {
    return ResourceType::Script;
  }
  if (ext == "nmscene" || ext == "scene") {
    return ResourceType::Scene;
  }
  if (ext == "json" || ext == "csv" || ext == "po") {
    return ResourceType::Localization;
  }
  if (ext == "glsl" || ext == "vert" || ext == "frag") {
    return ResourceType::Shader;
  }
  if (ext == "cfg" || ext == "ini" || ext == "xml") {
    return ResourceType::Config;
  }

  return ResourceType::Data;
}

} // namespace NovelMind::VFS
