#include "NovelMind/vfs/memory_fs.hpp"

namespace NovelMind::vfs {

Result<void> MemoryFileSystem::mount(const std::string& /*packPath*/) {
  return Result<void>::ok();
}

void MemoryFileSystem::unmount(const std::string& /*packPath*/) {
  // No-op for memory file system
}

void MemoryFileSystem::unmountAll() {
  clear();
}

Result<std::vector<u8>> MemoryFileSystem::readFile(const std::string& resourceId) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  auto it = m_resources.find(resourceId);
  if (it == m_resources.end()) {
    return Result<std::vector<u8>>::error("Resource not found: " + resourceId);
  }

  return Result<std::vector<u8>>::ok(it->second.data);
}

bool MemoryFileSystem::exists(const std::string& resourceId) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_resources.find(resourceId) != m_resources.end();
}

std::optional<ResourceInfo> MemoryFileSystem::getInfo(const std::string& resourceId) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  auto it = m_resources.find(resourceId);
  if (it == m_resources.end()) {
    return std::nullopt;
  }

  ResourceInfo info;
  info.id = resourceId;
  info.type = it->second.type;
  info.size = it->second.data.size();
  info.checksum = it->second.checksum;

  return info;
}

std::vector<std::string> MemoryFileSystem::listResources(ResourceType type) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  std::vector<std::string> result;
  result.reserve(m_resources.size());

  for (const auto& [id, entry] : m_resources) {
    if (type == ResourceType::Unknown || entry.type == type) {
      result.push_back(id);
    }
  }

  return result;
}

void MemoryFileSystem::addResource(const std::string& resourceId, std::vector<u8> data,
                                   ResourceType type) {
  std::lock_guard<std::mutex> lock(m_mutex);

  ResourceEntry entry;
  entry.checksum = calculateChecksum(data);
  entry.type = type;
  entry.data = std::move(data);

  m_resources[resourceId] = std::move(entry);
}

void MemoryFileSystem::removeResource(const std::string& resourceId) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_resources.erase(resourceId);
}

void MemoryFileSystem::clear() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_resources.clear();
}

u32 MemoryFileSystem::calculateChecksum(const std::vector<u8>& data) {
  // Simple CRC32 implementation
  u32 crc = 0xFFFFFFFF;
  for (u8 byte : data) {
    crc ^= byte;
    for (int i = 0; i < 8; ++i) {
      crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
    }
  }
  return ~crc;
}

} // namespace NovelMind::vfs
