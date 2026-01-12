#include "NovelMind/vfs/file_system_backend.hpp"
#include <algorithm>
#include <numeric>

namespace NovelMind::VFS {

namespace {

u32 crc32(const std::vector<u8>& data) {
  constexpr u32 CRC32_POLYNOMIAL = 0xEDB88320;
  u32 crc = 0xFFFFFFFF;

  for (const u8 byte : data) {
    crc ^= byte;
    for (int i = 0; i < 8; ++i) {
      crc = (crc >> 1) ^ ((crc & 1) ? CRC32_POLYNOMIAL : 0);
    }
  }

  return ~crc;
}

} // anonymous namespace

std::unique_ptr<IFileHandle> MemoryBackend::open(const ResourceId& id) {
  std::lock_guard<std::mutex> lock(m_mutex);

  const auto it = m_resources.find(id);
  if (it == m_resources.end()) {
    return nullptr;
  }

  return std::make_unique<MemoryFileHandle>(it->second.data);
}

bool MemoryBackend::exists(const ResourceId& id) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_resources.find(id) != m_resources.end();
}

std::optional<ResourceInfo> MemoryBackend::getInfo(const ResourceId& id) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  const auto it = m_resources.find(id);
  if (it == m_resources.end()) {
    return std::nullopt;
  }

  return it->second.info;
}

std::vector<ResourceId> MemoryBackend::list(ResourceType type) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  std::vector<ResourceId> result;
  result.reserve(m_resources.size());

  for (const auto& [resourceId, entry] : m_resources) {
    if (type == ResourceType::Unknown || entry.info.resourceId.type() == type) {
      result.push_back(resourceId);
    }
  }

  return result;
}

void MemoryBackend::addResource(const ResourceId& id, std::vector<u8> data) {
  std::lock_guard<std::mutex> lock(m_mutex);

  ResourceEntry entry;
  entry.info.resourceId = id;
  entry.info.size = data.size();
  entry.info.compressedSize = data.size();
  entry.info.checksum = calculateChecksum(data);
  entry.info.encrypted = false;
  entry.info.compressed = false;
  entry.data = std::move(data);

  m_resources[id] = std::move(entry);
}

void MemoryBackend::addResource(const std::string& id, std::vector<u8> data, ResourceType type) {
  addResource(ResourceId(id, type), std::move(data));
}

void MemoryBackend::removeResource(const ResourceId& id) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_resources.erase(id);
}

void MemoryBackend::clear() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_resources.clear();
}

u32 MemoryBackend::calculateChecksum(const std::vector<u8>& data) {
  return crc32(data);
}

} // namespace NovelMind::VFS
