#include "NovelMind/vfs/pack_reader.hpp"
#include "NovelMind/core/logger.hpp"
#include <cstring>

namespace NovelMind::vfs {

PackReader::~PackReader() { unmountAll(); }

Result<void> PackReader::mount(const std::string &packPath) {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_packs.find(packPath) != m_packs.end()) {
    return Result<void>::error("Pack already mounted: " + packPath);
  }

  std::ifstream file(packPath, std::ios::binary);
  if (!file.is_open()) {
    return Result<void>::error("Failed to open pack file: " + packPath);
  }

  MountedPack pack;
  pack.path = packPath;

  auto headerResult = readPackHeader(file, pack.header);
  if (headerResult.isError()) {
    return headerResult;
  }

  auto tableResult = readResourceTable(file, pack);
  if (tableResult.isError()) {
    return tableResult;
  }

  auto stringResult = readStringTable(file, pack);
  if (stringResult.isError()) {
    return stringResult;
  }

  m_packs[packPath] = std::move(pack);
  NOVELMIND_LOG_INFO("Mounted pack: " + packPath);

  return Result<void>::ok();
}

void PackReader::unmount(const std::string &packPath) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_packs.erase(packPath);
  NOVELMIND_LOG_INFO("Unmounted pack: " + packPath);
}

void PackReader::unmountAll() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_packs.clear();
  NOVELMIND_LOG_INFO("Unmounted all packs");
}

Result<std::vector<u8>>
PackReader::readFile(const std::string &resourceId) const {
  // Find entry and copy data needed for reading
  std::string packPath;
  PackResourceEntry entry;
  {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (const auto &[path, pack] : m_packs) {
      auto it = pack.entries.find(resourceId);
      if (it != pack.entries.end()) {
        packPath = path;
        entry = it->second;
        break;
      }
    }
  }

  // Read data outside lock to avoid holding mutex during I/O
  if (!packPath.empty()) {
    return readResourceData(packPath, entry);
  }

  return Result<std::vector<u8>>::error("Resource not found: " + resourceId);
}

bool PackReader::exists(const std::string &resourceId) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  for (const auto &[packPath, pack] : m_packs) {
    if (pack.entries.find(resourceId) != pack.entries.end()) {
      return true;
    }
  }

  return false;
}

std::optional<ResourceInfo>
PackReader::getInfo(const std::string &resourceId) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  for (const auto &[packPath, pack] : m_packs) {
    auto it = pack.entries.find(resourceId);
    if (it != pack.entries.end()) {
      ResourceInfo info;
      info.id = resourceId;
      info.type = static_cast<ResourceType>(it->second.type);
      info.size = static_cast<usize>(it->second.uncompressedSize);
      info.checksum = it->second.checksum;
      return info;
    }
  }

  return std::nullopt;
}

std::vector<std::string> PackReader::listResources(ResourceType type) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  std::vector<std::string> result;

  for (const auto &[packPath, pack] : m_packs) {
    for (const auto &[id, entry] : pack.entries) {
      if (type == ResourceType::Unknown ||
          static_cast<ResourceType>(entry.type) == type) {
        result.push_back(id);
      }
    }
  }

  return result;
}

Result<void> PackReader::readPackHeader(std::ifstream &file,
                                        PackHeader &header) {
  file.read(reinterpret_cast<char *>(&header), sizeof(PackHeader));

  if (!file) {
    return Result<void>::error("Failed to read pack header");
  }

  if (header.magic != PACK_MAGIC) {
    return Result<void>::error("Invalid pack magic number");
  }

  if (header.versionMajor != PACK_VERSION_MAJOR) {
    return Result<void>::error("Incompatible pack version");
  }

  if (header.flags != 0) {
    return Result<void>::error(
        "Secure pack flags set; use SecurePackReader instead of PackReader");
  }

  // Security: Validate resource count to prevent excessive allocations
  constexpr u32 MAX_RESOURCE_COUNT = 1000000; // 1 million resources max
  if (header.resourceCount > MAX_RESOURCE_COUNT) {
    return Result<void>::error("Resource count exceeds maximum allowed");
  }

  return Result<void>::ok();
}

Result<void> PackReader::readResourceTable(std::ifstream &file,
                                           MountedPack &pack) {
  file.seekg(static_cast<std::streamoff>(pack.header.resourceTableOffset));

  if (!file) {
    return Result<void>::error("Failed to seek to resource table");
  }

  for (u32 i = 0; i < pack.header.resourceCount; ++i) {
    PackResourceEntry entry;
    file.read(reinterpret_cast<char *>(&entry), sizeof(PackResourceEntry));

    if (!file) {
      return Result<void>::error("Failed to read resource entry");
    }

    // Entry ID will be resolved after reading string table
    pack.entries[std::to_string(i)] = entry;
  }

  return Result<void>::ok();
}

Result<void> PackReader::readStringTable(std::ifstream &file,
                                         MountedPack &pack) {
  file.seekg(static_cast<std::streamoff>(pack.header.stringTableOffset));

  if (!file) {
    return Result<void>::error("Failed to seek to string table");
  }

  u32 stringCount = 0;
  file.read(reinterpret_cast<char *>(&stringCount), sizeof(u32));

  if (!file) {
    return Result<void>::error("Failed to read string count");
  }

  // Security: Validate string count to prevent excessive allocations
  constexpr u32 MAX_STRING_COUNT = 10000000; // 10 million strings max
  if (stringCount > MAX_STRING_COUNT) {
    return Result<void>::error("String count exceeds maximum allowed");
  }

  // Read string offsets
  std::vector<u32> offsets(stringCount);
  file.read(reinterpret_cast<char *>(offsets.data()),
            static_cast<std::streamsize>(stringCount * sizeof(u32)));

  if (!file) {
    return Result<void>::error("Failed to read string offsets");
  }

  // Read string data
  auto stringDataStart = file.tellg();
  pack.stringTable.reserve(stringCount);

  // Security: Limit individual string length to prevent excessive allocations
  constexpr usize MAX_STRING_LENGTH = 1024 * 1024; // 1 MB per string max

  for (u32 i = 0; i < stringCount; ++i) {
    file.seekg(stringDataStart + static_cast<std::streamoff>(offsets[i]));
    std::string str;
    std::getline(file, str, '\0');

    if (str.size() > MAX_STRING_LENGTH) {
      return Result<void>::error("String length exceeds maximum allowed");
    }

    pack.stringTable.push_back(str);
  }

  // Re-map entries with actual string IDs
  std::unordered_map<std::string, PackResourceEntry> newEntries;
  u32 index = 0;
  for (auto &[key, entry] : pack.entries) {
    if (entry.idStringOffset < pack.stringTable.size()) {
      const std::string &resourceId = pack.stringTable[entry.idStringOffset];
      newEntries[resourceId] = entry;
    }
    ++index;
  }
  pack.entries = std::move(newEntries);

  return Result<void>::ok();
}

Result<std::vector<u8>>
PackReader::readResourceData(const std::string &packPath,
                             const PackResourceEntry &entry) const {
  // Security: Validate resource size to prevent excessive allocations
  constexpr u64 MAX_RESOURCE_SIZE =
      512ULL * 1024 * 1024; // 512 MB max per resource
  if (entry.compressedSize > MAX_RESOURCE_SIZE) {
    return Result<std::vector<u8>>::error(
        "Resource size exceeds maximum allowed");
  }

  std::ifstream file(packPath, std::ios::binary);
  if (!file.is_open()) {
    return Result<std::vector<u8>>::error("Failed to open pack file");
  }

  auto it = m_packs.find(packPath);
  if (it == m_packs.end()) {
    return Result<std::vector<u8>>::error("Pack not mounted");
  }

  // Security: Validate offset doesn't cause overflow
  u64 absoluteOffset = it->second.header.dataOffset + entry.dataOffset;
  if (absoluteOffset < it->second.header.dataOffset) {
    return Result<std::vector<u8>>::error("Invalid resource offset (overflow)");
  }

  // Get file size to validate offset + size doesn't exceed file bounds
  file.seekg(0, std::ios::end);
  auto fileSize = file.tellg();
  if (fileSize < 0) {
    return Result<std::vector<u8>>::error("Failed to get pack file size");
  }

  if (absoluteOffset + entry.compressedSize > static_cast<u64>(fileSize)) {
    return Result<std::vector<u8>>::error(
        "Resource data extends beyond pack file");
  }

  file.seekg(static_cast<std::streamoff>(absoluteOffset));

  if (!file) {
    return Result<std::vector<u8>>::error("Failed to seek to resource data");
  }

  std::vector<u8> data(static_cast<usize>(entry.compressedSize));
  file.read(reinterpret_cast<char *>(data.data()),
            static_cast<std::streamsize>(entry.compressedSize));

  if (!file) {
    return Result<std::vector<u8>>::error("Failed to read resource data");
  }

  // Decryption and decompression are handled by PackSecurity when enabled.
  // See pack_security.hpp for encryption/compression configuration.

  return Result<std::vector<u8>>::ok(std::move(data));
}

} // namespace NovelMind::vfs
