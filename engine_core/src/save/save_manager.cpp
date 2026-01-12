#include "NovelMind/save/save_manager.hpp"
#include "NovelMind/core/logger.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <fstream>

#if defined(NOVELMIND_HAS_ZLIB)
#include <zlib.h>
#endif

#if defined(NOVELMIND_HAS_OPENSSL)
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif

namespace NovelMind::save {

namespace {

constexpr u32 kMagic = 0x564D4E53; // "SNMV"
constexpr u16 kVersionLegacy = 1;
constexpr u16 kVersionCurrent = 2;

enum SaveFlags : u16 { kFlagCompressed = 1 << 0, kFlagEncrypted = 1 << 1 };

constexpr u32 kMaxStringLength = 1024 * 1024;
constexpr u32 kMaxVariableCount = 100000;

struct BufferWriter {
  std::vector<u8> data;

  void writeBytes(const void* src, size_t size) {
    const auto* ptr = reinterpret_cast<const u8*>(src);
    data.insert(data.end(), ptr, ptr + size);
  }

  template <typename T> void writePod(const T& value) { writeBytes(&value, sizeof(T)); }

  void writeString(const std::string& str) {
    u32 len = static_cast<u32>(str.size());
    writePod(len);
    if (len > 0) {
      writeBytes(str.data(), len);
    }
  }
};

struct BufferReader {
  const u8* data = nullptr;
  size_t size = 0;
  size_t offset = 0;

  bool readBytes(void* dst, size_t count) {
    if (offset + count > size) {
      return false;
    }
    std::memcpy(dst, data + offset, count);
    offset += count;
    return true;
  }

  template <typename T> bool readPod(T& value) { return readBytes(&value, sizeof(T)); }

  bool readString(std::string& out, u32 maxLen) {
    u32 len = 0;
    if (!readPod(len) || len > maxLen) {
      return false;
    }
    if (offset + len > size) {
      return false;
    }
    out.assign(reinterpret_cast<const char*>(data + offset), len);
    offset += len;
    return true;
  }
};

u64 nowTimestamp() {
  return static_cast<u64>(std::chrono::system_clock::now().time_since_epoch().count());
}

Result<std::vector<u8>> compressData(const std::vector<u8>& input) {
#if defined(NOVELMIND_HAS_ZLIB)
  if (input.empty()) {
    return Result<std::vector<u8>>::ok({});
  }

  uLongf destLen = compressBound(static_cast<uLong>(input.size()));
  std::vector<u8> out(destLen);
  int res =
      compress2(out.data(), &destLen, input.data(), static_cast<uLong>(input.size()), Z_BEST_SPEED);
  if (res != Z_OK) {
    return Result<std::vector<u8>>::error("Compression failed");
  }
  out.resize(destLen);
  return Result<std::vector<u8>>::ok(std::move(out));
#else
  (void)input;
  return Result<std::vector<u8>>::error("zlib not available");
#endif
}

Result<std::vector<u8>> decompressData(const std::vector<u8>& input, size_t rawSize) {
#if defined(NOVELMIND_HAS_ZLIB)
  std::vector<u8> out(rawSize);
  uLongf destLen = static_cast<uLongf>(rawSize);
  int res = uncompress(out.data(), &destLen, input.data(), static_cast<uLong>(input.size()));
  if (res != Z_OK) {
    return Result<std::vector<u8>>::error("Decompression failed");
  }
  out.resize(destLen);
  return Result<std::vector<u8>>::ok(std::move(out));
#else
  (void)input;
  (void)rawSize;
  return Result<std::vector<u8>>::error("zlib not available");
#endif
}

struct EncryptedPayload {
  std::vector<u8> data;
  std::array<u8, 12> iv{};
  std::array<u8, 16> tag{};
};

Result<EncryptedPayload> encryptData(const std::vector<u8>& input, const std::vector<u8>& key) {
#if defined(NOVELMIND_HAS_OPENSSL)
  if (key.size() != 32) {
    return Result<EncryptedPayload>::error("Invalid encryption key size");
  }

  EncryptedPayload out;
  if (RAND_bytes(out.iv.data(), static_cast<int>(out.iv.size())) != 1) {
    return Result<EncryptedPayload>::error("Failed to generate IV");
  }

  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  if (!ctx) {
    return Result<EncryptedPayload>::error("Failed to create cipher context");
  }

  int len = 0;
  out.data.resize(input.size());

  if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return Result<EncryptedPayload>::error("Encrypt init failed");
  }
  if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(out.iv.size()), nullptr) !=
      1) {
    EVP_CIPHER_CTX_free(ctx);
    return Result<EncryptedPayload>::error("Encrypt IV setup failed");
  }
  if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), out.iv.data()) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return Result<EncryptedPayload>::error("Encrypt key setup failed");
  }

  if (EVP_EncryptUpdate(ctx, out.data.data(), &len, input.data(), static_cast<int>(input.size())) !=
      1) {
    EVP_CIPHER_CTX_free(ctx);
    return Result<EncryptedPayload>::error("Encrypt update failed");
  }
  int total = len;

  if (EVP_EncryptFinal_ex(ctx, out.data.data() + total, &len) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return Result<EncryptedPayload>::error("Encrypt final failed");
  }
  total += len;
  out.data.resize(static_cast<size_t>(total));

  if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, static_cast<int>(out.tag.size()),
                          out.tag.data()) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return Result<EncryptedPayload>::error("Encrypt tag failed");
  }

  EVP_CIPHER_CTX_free(ctx);
  return Result<EncryptedPayload>::ok(std::move(out));
#else
  (void)input;
  (void)key;
  return Result<EncryptedPayload>::error("OpenSSL not available");
#endif
}

Result<std::vector<u8>> decryptData(const std::vector<u8>& input, const std::vector<u8>& key,
                                    const std::array<u8, 12>& iv, const std::array<u8, 16>& tag) {
#if defined(NOVELMIND_HAS_OPENSSL)
  if (key.size() != 32) {
    return Result<std::vector<u8>>::error("Invalid encryption key size");
  }

  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  if (!ctx) {
    return Result<std::vector<u8>>::error("Failed to create cipher context");
  }

  std::vector<u8> out(input.size());
  int len = 0;

  if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return Result<std::vector<u8>>::error("Decrypt init failed");
  }
  if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(iv.size()), nullptr) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return Result<std::vector<u8>>::error("Decrypt IV setup failed");
  }
  if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return Result<std::vector<u8>>::error("Decrypt key setup failed");
  }

  if (EVP_DecryptUpdate(ctx, out.data(), &len, input.data(), static_cast<int>(input.size())) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return Result<std::vector<u8>>::error("Decrypt update failed");
  }
  int total = len;

  if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, static_cast<int>(tag.size()),
                          const_cast<u8*>(tag.data())) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return Result<std::vector<u8>>::error("Decrypt tag setup failed");
  }

  int finalRes = EVP_DecryptFinal_ex(ctx, out.data() + total, &len);
  EVP_CIPHER_CTX_free(ctx);
  if (finalRes != 1) {
    return Result<std::vector<u8>>::error("Decrypt final failed");
  }
  total += len;
  out.resize(static_cast<size_t>(total));
  return Result<std::vector<u8>>::ok(std::move(out));
#else
  (void)input;
  (void)key;
  (void)iv;
  (void)tag;
  return Result<std::vector<u8>>::error("OpenSSL not available");
#endif
}

} // namespace

SaveManager::SaveManager() : m_savePath("./saves/") {}

SaveManager::~SaveManager() = default;

Result<void> SaveManager::save(i32 slot, const SaveData& data) {
  if (slot < 0 || slot >= MAX_SLOTS) {
    return Result<void>::error("Invalid save slot");
  }
  return saveToFile(getSlotFilename(slot), data);
}

Result<SaveData> SaveManager::load(i32 slot) {
  if (slot < 0 || slot >= MAX_SLOTS) {
    return Result<SaveData>::error("Invalid save slot");
  }
  return loadFromFile(getSlotFilename(slot));
}

Result<void> SaveManager::deleteSave(i32 slot) {
  if (slot < 0 || slot >= MAX_SLOTS) {
    return Result<void>::error("Invalid save slot");
  }

  std::string filename = getSlotFilename(slot);
  if (std::remove(filename.c_str()) != 0) {
    return Result<void>::error("Failed to delete save file");
  }

  NOVELMIND_LOG_INFO("Deleted save slot " + std::to_string(slot));
  return Result<void>::ok();
}

bool SaveManager::slotExists(i32 slot) const {
  if (slot < 0 || slot >= MAX_SLOTS) {
    return false;
  }

  std::ifstream file(getSlotFilename(slot), std::ios::binary);
  return file.good();
}

std::optional<u64> SaveManager::getSlotTimestamp(i32 slot) const {
  auto metadata = getSlotMetadata(slot);
  if (!metadata.has_value()) {
    return std::nullopt;
  }
  return metadata->timestamp;
}

std::optional<SaveMetadata> SaveManager::getSlotMetadata(i32 slot) const {
  if (slot < 0 || slot >= MAX_SLOTS) {
    return std::nullopt;
  }
  return readMetadata(getSlotFilename(slot));
}

i32 SaveManager::getMaxSlots() const {
  return MAX_SLOTS;
}

void SaveManager::setSavePath(const std::string& path) {
  m_savePath = path;
  if (!m_savePath.empty() && m_savePath.back() != '/') {
    m_savePath += '/';
  }
}

const std::string& SaveManager::getSavePath() const {
  return m_savePath;
}

void SaveManager::setConfig(const SaveConfig& config) {
  m_config = config;
}

const SaveConfig& SaveManager::getConfig() const {
  return m_config;
}

Result<void> SaveManager::saveAuto(const SaveData& data) {
  return saveToFile(getAutoSaveFilename(), data);
}

Result<SaveData> SaveManager::loadAuto() {
  return loadFromFile(getAutoSaveFilename());
}

bool SaveManager::autoSaveExists() const {
  std::ifstream file(getAutoSaveFilename(), std::ios::binary);
  return file.good();
}

std::string SaveManager::getSlotFilename(i32 slot) const {
  return m_savePath + "save_" + std::to_string(slot) + ".nmsav";
}

std::string SaveManager::getAutoSaveFilename() const {
  return m_savePath + "autosave.nmsav";
}

u32 SaveManager::calculateChecksum(const SaveData& data) {
  u32 checksum = 0;

  auto hashString = [&checksum](const std::string& str) {
    for (char c : str) {
      checksum = checksum * 31 + static_cast<u32>(c);
    }
  };

  auto hashFloat = [&checksum](f32 value) {
    u32 bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    checksum = checksum * 31 + bits;
  };

  hashString(data.sceneId);
  hashString(data.nodeId);

  for (const auto& [name, value] : data.intVariables) {
    hashString(name);
    checksum = checksum * 31 + static_cast<u32>(value);
  }

  for (const auto& [name, value] : data.floatVariables) {
    hashString(name);
    hashFloat(value);
  }

  for (const auto& [name, value] : data.flags) {
    hashString(name);
    checksum = checksum * 31 + (value ? 1 : 0);
  }

  for (const auto& [name, value] : data.stringVariables) {
    hashString(name);
    hashString(value);
  }

  for (u8 byte : data.thumbnailData) {
    checksum = checksum * 31 + byte;
  }

  checksum = checksum * 31 + static_cast<u32>(data.thumbnailWidth);
  checksum = checksum * 31 + static_cast<u32>(data.thumbnailHeight);

  return checksum;
}

Result<void> SaveManager::saveToFile(const std::string& filename, SaveData data) {
  data.timestamp = nowTimestamp();
  data.checksum = calculateChecksum(data);

  BufferWriter writer;
  writer.writeString(data.sceneId);
  writer.writeString(data.nodeId);

  u32 intCount = static_cast<u32>(data.intVariables.size());
  writer.writePod(intCount);
  for (const auto& [name, value] : data.intVariables) {
    writer.writeString(name);
    writer.writePod(value);
  }

  u32 floatCount = static_cast<u32>(data.floatVariables.size());
  writer.writePod(floatCount);
  for (const auto& [name, value] : data.floatVariables) {
    writer.writeString(name);
    writer.writePod(value);
  }

  u32 flagCount = static_cast<u32>(data.flags.size());
  writer.writePod(flagCount);
  for (const auto& [name, value] : data.flags) {
    writer.writeString(name);
    u8 bval = value ? 1 : 0;
    writer.writePod(bval);
  }

  u32 strCount = static_cast<u32>(data.stringVariables.size());
  writer.writePod(strCount);
  for (const auto& [name, value] : data.stringVariables) {
    writer.writeString(name);
    writer.writeString(value);
  }

  u32 thumbSize = static_cast<u32>(data.thumbnailData.size());
  writer.writePod(thumbSize);
  if (thumbSize > 0) {
    writer.writeBytes(data.thumbnailData.data(), thumbSize);
  }

  std::vector<u8> payload = std::move(writer.data);
  u32 rawSize = static_cast<u32>(payload.size());

  u16 flags = 0;
  if (m_config.enableCompression && !payload.empty()) {
    auto compressed = compressData(payload);
    if (!compressed.isOk()) {
      return Result<void>::error(compressed.error());
    }
    if (compressed.value().size() < payload.size()) {
      payload = std::move(compressed.value());
      flags |= kFlagCompressed;
    }
  }

  std::array<u8, 12> iv{};
  std::array<u8, 16> tag{};
  if (m_config.enableEncryption) {
    auto encrypted = encryptData(payload, m_config.encryptionKey);
    if (!encrypted.isOk()) {
      return Result<void>::error(encrypted.error());
    }
    payload = std::move(encrypted.value().data);
    iv = encrypted.value().iv;
    tag = encrypted.value().tag;
    flags |= kFlagEncrypted;
  }

  std::ofstream file(filename, std::ios::binary);
  if (!file.is_open()) {
    return Result<void>::error("Failed to open save file: " + filename);
  }

  u32 payloadSize = static_cast<u32>(payload.size());
  u32 thumbWidth = static_cast<u32>(std::max(0, data.thumbnailWidth));
  u32 thumbHeight = static_cast<u32>(std::max(0, data.thumbnailHeight));
  u32 thumbStored = static_cast<u32>(data.thumbnailData.size());

  file.write(reinterpret_cast<const char*>(&kMagic), sizeof(kMagic));
  file.write(reinterpret_cast<const char*>(&kVersionCurrent), sizeof(kVersionCurrent));
  file.write(reinterpret_cast<const char*>(&flags), sizeof(flags));
  file.write(reinterpret_cast<const char*>(&payloadSize), sizeof(payloadSize));
  file.write(reinterpret_cast<const char*>(&rawSize), sizeof(rawSize));
  file.write(reinterpret_cast<const char*>(&data.timestamp), sizeof(data.timestamp));
  file.write(reinterpret_cast<const char*>(&data.checksum), sizeof(data.checksum));
  file.write(reinterpret_cast<const char*>(&thumbWidth), sizeof(thumbWidth));
  file.write(reinterpret_cast<const char*>(&thumbHeight), sizeof(thumbHeight));
  file.write(reinterpret_cast<const char*>(&thumbStored), sizeof(thumbStored));
  file.write(reinterpret_cast<const char*>(iv.data()), static_cast<std::streamsize>(iv.size()));
  file.write(reinterpret_cast<const char*>(tag.data()), static_cast<std::streamsize>(tag.size()));

  if (!payload.empty()) {
    file.write(reinterpret_cast<const char*>(payload.data()),
               static_cast<std::streamsize>(payload.size()));
  }

  if (!file) {
    return Result<void>::error("Failed to write save file: " + filename);
  }

  NOVELMIND_LOG_INFO("Saved to file " + filename);
  return Result<void>::ok();
}

Result<SaveData> SaveManager::loadFromFile(const std::string& filename) {
  std::ifstream file(filename, std::ios::binary);
  if (!file.is_open()) {
    return Result<SaveData>::error("Save file not found: " + filename);
  }

  u32 magic = 0;
  u16 version = 0;
  file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
  file.read(reinterpret_cast<char*>(&version), sizeof(version));
  if (!file) {
    return Result<SaveData>::error("Failed to read save file header");
  }

  if (magic != kMagic) {
    return Result<SaveData>::error("Invalid save file format");
  }

  if (version == kVersionLegacy) {
    // Legacy V1 loader
    SaveData data;

    auto readString = [&file](u32 maxLen) -> std::pair<std::string, bool> {
      u32 len = 0;
      file.read(reinterpret_cast<char*>(&len), sizeof(len));
      if (!file || len > maxLen) {
        return {"", false};
      }
      std::string str(len, '\0');
      file.read(str.data(), static_cast<std::streamsize>(len));
      if (!file) {
        return {"", false};
      }
      return {str, true};
    };

    auto [sceneId, sceneOk] = readString(kMaxStringLength);
    if (!sceneOk) {
      return Result<SaveData>::error("Invalid or corrupted scene ID in save file");
    }
    data.sceneId = std::move(sceneId);

    auto [nodeId, nodeOk] = readString(kMaxStringLength);
    if (!nodeOk) {
      return Result<SaveData>::error("Invalid or corrupted node ID in save file");
    }
    data.nodeId = std::move(nodeId);

    u32 intCount = 0;
    file.read(reinterpret_cast<char*>(&intCount), sizeof(intCount));
    if (!file || intCount > kMaxVariableCount) {
      return Result<SaveData>::error("Invalid or corrupted int variable count");
    }
    for (u32 i = 0; i < intCount; ++i) {
      auto [name, nameOk] = readString(kMaxStringLength);
      if (!nameOk) {
        return Result<SaveData>::error("Invalid int variable name at index " + std::to_string(i));
      }
      i32 value = 0;
      file.read(reinterpret_cast<char*>(&value), sizeof(value));
      if (!file) {
        return Result<SaveData>::error("Failed to read int variable value at index " +
                                       std::to_string(i));
      }
      data.intVariables[name] = value;
    }

    u32 flagCount = 0;
    file.read(reinterpret_cast<char*>(&flagCount), sizeof(flagCount));
    if (!file || flagCount > kMaxVariableCount) {
      return Result<SaveData>::error("Invalid or corrupted flag count");
    }
    for (u32 i = 0; i < flagCount; ++i) {
      auto [name, nameOk] = readString(kMaxStringLength);
      if (!nameOk) {
        return Result<SaveData>::error("Invalid flag name at index " + std::to_string(i));
      }
      u8 bval = 0;
      file.read(reinterpret_cast<char*>(&bval), sizeof(bval));
      if (!file) {
        return Result<SaveData>::error("Failed to read flag value at index " + std::to_string(i));
      }
      data.flags[name] = bval != 0;
    }

    u32 strCount = 0;
    file.read(reinterpret_cast<char*>(&strCount), sizeof(strCount));
    if (!file || strCount > kMaxVariableCount) {
      return Result<SaveData>::error("Invalid or corrupted string variable count");
    }
    for (u32 i = 0; i < strCount; ++i) {
      auto [name, nameOk] = readString(kMaxStringLength);
      if (!nameOk) {
        return Result<SaveData>::error("Invalid string variable name at index " +
                                       std::to_string(i));
      }
      auto [value, valueOk] = readString(kMaxStringLength);
      if (!valueOk) {
        return Result<SaveData>::error("Invalid string variable value at index " +
                                       std::to_string(i));
      }
      data.stringVariables[name] = value;
    }

    file.read(reinterpret_cast<char*>(&data.timestamp), sizeof(data.timestamp));
    if (!file) {
      return Result<SaveData>::error("Failed to read timestamp");
    }

    u32 storedChecksum = 0;
    file.read(reinterpret_cast<char*>(&storedChecksum), sizeof(storedChecksum));
    if (!file) {
      return Result<SaveData>::error("Failed to read checksum");
    }
    data.checksum = storedChecksum;

    u32 calculatedChecksum = calculateChecksum(data);
    if (calculatedChecksum != storedChecksum) {
      NOVELMIND_LOG_ERROR("Save file checksum mismatch in file " + filename +
                          " (stored: " + std::to_string(storedChecksum) +
                          ", calculated: " + std::to_string(calculatedChecksum) + ")");
      return Result<SaveData>::error("Save file is corrupted (checksum mismatch)");
    }

    return Result<SaveData>::ok(std::move(data));
  }

  if (version != kVersionCurrent) {
    return Result<SaveData>::error("Unsupported save file version");
  }

  u16 flags = 0;
  u32 payloadSize = 0;
  u32 rawSize = 0;
  u64 timestamp = 0;
  u32 checksum = 0;
  u32 thumbWidth = 0;
  u32 thumbHeight = 0;
  u32 thumbSize = 0;

  file.read(reinterpret_cast<char*>(&flags), sizeof(flags));
  file.read(reinterpret_cast<char*>(&payloadSize), sizeof(payloadSize));
  file.read(reinterpret_cast<char*>(&rawSize), sizeof(rawSize));
  file.read(reinterpret_cast<char*>(&timestamp), sizeof(timestamp));
  file.read(reinterpret_cast<char*>(&checksum), sizeof(checksum));
  file.read(reinterpret_cast<char*>(&thumbWidth), sizeof(thumbWidth));
  file.read(reinterpret_cast<char*>(&thumbHeight), sizeof(thumbHeight));
  file.read(reinterpret_cast<char*>(&thumbSize), sizeof(thumbSize));

  std::array<u8, 12> iv{};
  std::array<u8, 16> tag{};
  file.read(reinterpret_cast<char*>(iv.data()), static_cast<std::streamsize>(iv.size()));
  file.read(reinterpret_cast<char*>(tag.data()), static_cast<std::streamsize>(tag.size()));

  if (!file) {
    return Result<SaveData>::error("Failed to read save file header");
  }

  std::vector<u8> payload(payloadSize);
  if (payloadSize > 0) {
    file.read(reinterpret_cast<char*>(payload.data()),
              static_cast<std::streamsize>(payload.size()));
  }
  if (!file) {
    return Result<SaveData>::error("Failed to read save payload");
  }

  if ((flags & kFlagEncrypted) != 0) {
    auto decrypted = decryptData(payload, m_config.encryptionKey, iv, tag);
    if (!decrypted.isOk()) {
      return Result<SaveData>::error(decrypted.error());
    }
    payload = std::move(decrypted.value());
  }

  if ((flags & kFlagCompressed) != 0) {
    auto decompressed = decompressData(payload, rawSize);
    if (!decompressed.isOk()) {
      return Result<SaveData>::error(decompressed.error());
    }
    payload = std::move(decompressed.value());
  }

  BufferReader reader{payload.data(), payload.size(), 0};
  SaveData data;
  data.timestamp = timestamp;
  data.checksum = checksum;

  if (!reader.readString(data.sceneId, kMaxStringLength)) {
    return Result<SaveData>::error("Invalid scene ID in save file");
  }
  if (!reader.readString(data.nodeId, kMaxStringLength)) {
    return Result<SaveData>::error("Invalid node ID in save file");
  }

  u32 intCount = 0;
  if (!reader.readPod(intCount) || intCount > kMaxVariableCount) {
    return Result<SaveData>::error("Invalid int variable count");
  }
  for (u32 i = 0; i < intCount; ++i) {
    std::string name;
    if (!reader.readString(name, kMaxStringLength)) {
      return Result<SaveData>::error("Invalid int variable name at index " + std::to_string(i));
    }
    i32 value = 0;
    if (!reader.readPod(value)) {
      return Result<SaveData>::error("Failed to read int variable value at index " +
                                     std::to_string(i));
    }
    data.intVariables[name] = value;
  }

  u32 floatCount = 0;
  if (!reader.readPod(floatCount) || floatCount > kMaxVariableCount) {
    return Result<SaveData>::error("Invalid float variable count");
  }
  for (u32 i = 0; i < floatCount; ++i) {
    std::string name;
    if (!reader.readString(name, kMaxStringLength)) {
      return Result<SaveData>::error("Invalid float variable name at index " + std::to_string(i));
    }
    f32 value = 0.0f;
    if (!reader.readPod(value)) {
      return Result<SaveData>::error("Failed to read float variable value at index " +
                                     std::to_string(i));
    }
    data.floatVariables[name] = value;
  }

  u32 flagCount = 0;
  if (!reader.readPod(flagCount) || flagCount > kMaxVariableCount) {
    return Result<SaveData>::error("Invalid flag count");
  }
  for (u32 i = 0; i < flagCount; ++i) {
    std::string name;
    if (!reader.readString(name, kMaxStringLength)) {
      return Result<SaveData>::error("Invalid flag name at index " + std::to_string(i));
    }
    u8 bval = 0;
    if (!reader.readPod(bval)) {
      return Result<SaveData>::error("Failed to read flag value at index " + std::to_string(i));
    }
    data.flags[name] = bval != 0;
  }

  u32 strCount = 0;
  if (!reader.readPod(strCount) || strCount > kMaxVariableCount) {
    return Result<SaveData>::error("Invalid string variable count");
  }
  for (u32 i = 0; i < strCount; ++i) {
    std::string name;
    if (!reader.readString(name, kMaxStringLength)) {
      return Result<SaveData>::error("Invalid string variable name at index " + std::to_string(i));
    }
    std::string value;
    if (!reader.readString(value, kMaxStringLength)) {
      return Result<SaveData>::error("Invalid string variable value at index " + std::to_string(i));
    }
    data.stringVariables[name] = value;
  }

  u32 payloadThumbSize = 0;
  if (!reader.readPod(payloadThumbSize)) {
    return Result<SaveData>::error("Failed to read thumbnail size");
  }
  if (payloadThumbSize > 0) {
    if (reader.offset + payloadThumbSize > reader.size) {
      return Result<SaveData>::error("Invalid thumbnail data");
    }
    data.thumbnailData.assign(reader.data + reader.offset,
                              reader.data + reader.offset + payloadThumbSize);
    reader.offset += payloadThumbSize;
  }
  data.thumbnailWidth = static_cast<i32>(thumbWidth);
  data.thumbnailHeight = static_cast<i32>(thumbHeight);

  if (payloadThumbSize != thumbSize) {
    return Result<SaveData>::error("Thumbnail size mismatch");
  }

  u32 calculatedChecksum = calculateChecksum(data);
  if (calculatedChecksum != checksum) {
    NOVELMIND_LOG_ERROR("Save file checksum mismatch in file " + filename +
                        " (stored: " + std::to_string(checksum) +
                        ", calculated: " + std::to_string(calculatedChecksum) + ")");
    return Result<SaveData>::error("Save file is corrupted (checksum mismatch)");
  }

  return Result<SaveData>::ok(std::move(data));
}

std::optional<SaveMetadata> SaveManager::readMetadata(const std::string& filename) const {
  std::ifstream file(filename, std::ios::binary);
  if (!file.is_open()) {
    return std::nullopt;
  }

  u32 magic = 0;
  u16 version = 0;
  file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
  file.read(reinterpret_cast<char*>(&version), sizeof(version));
  if (!file || magic != kMagic) {
    return std::nullopt;
  }

  SaveMetadata metadata;
  if (version == kVersionLegacy) {
    auto readString = [&file](u32 maxLen) -> bool {
      u32 len = 0;
      file.read(reinterpret_cast<char*>(&len), sizeof(len));
      if (!file || len > maxLen) {
        return false;
      }
      file.seekg(static_cast<std::streamoff>(len), std::ios::cur);
      return static_cast<bool>(file);
    };

    if (!readString(kMaxStringLength)) {
      return std::nullopt;
    }
    if (!readString(kMaxStringLength)) {
      return std::nullopt;
    }

    u32 intCount = 0;
    file.read(reinterpret_cast<char*>(&intCount), sizeof(intCount));
    if (!file || intCount > kMaxVariableCount) {
      return std::nullopt;
    }
    for (u32 i = 0; i < intCount; ++i) {
      if (!readString(kMaxStringLength)) {
        return std::nullopt;
      }
      file.seekg(static_cast<std::streamoff>(sizeof(i32)), std::ios::cur);
    }

    u32 flagCount = 0;
    file.read(reinterpret_cast<char*>(&flagCount), sizeof(flagCount));
    if (!file || flagCount > kMaxVariableCount) {
      return std::nullopt;
    }
    for (u32 i = 0; i < flagCount; ++i) {
      if (!readString(kMaxStringLength)) {
        return std::nullopt;
      }
      file.seekg(static_cast<std::streamoff>(sizeof(u8)), std::ios::cur);
    }

    u32 strCount = 0;
    file.read(reinterpret_cast<char*>(&strCount), sizeof(strCount));
    if (!file || strCount > kMaxVariableCount) {
      return std::nullopt;
    }
    for (u32 i = 0; i < strCount; ++i) {
      if (!readString(kMaxStringLength)) {
        return std::nullopt;
      }
      if (!readString(kMaxStringLength)) {
        return std::nullopt;
      }
    }

    u64 timestamp = 0;
    file.read(reinterpret_cast<char*>(&timestamp), sizeof(timestamp));
    if (!file) {
      return std::nullopt;
    }
    metadata.timestamp = timestamp;
    return metadata;
  }

  if (version != kVersionCurrent) {
    return std::nullopt;
  }

  u16 flags = 0;
  u32 payloadSize = 0;
  u32 rawSize = 0;
  u64 timestamp = 0;
  u32 checksum = 0;
  u32 thumbWidth = 0;
  u32 thumbHeight = 0;
  u32 thumbSize = 0;

  file.read(reinterpret_cast<char*>(&flags), sizeof(flags));
  file.read(reinterpret_cast<char*>(&payloadSize), sizeof(payloadSize));
  file.read(reinterpret_cast<char*>(&rawSize), sizeof(rawSize));
  file.read(reinterpret_cast<char*>(&timestamp), sizeof(timestamp));
  file.read(reinterpret_cast<char*>(&checksum), sizeof(checksum));
  file.read(reinterpret_cast<char*>(&thumbWidth), sizeof(thumbWidth));
  file.read(reinterpret_cast<char*>(&thumbHeight), sizeof(thumbHeight));
  file.read(reinterpret_cast<char*>(&thumbSize), sizeof(thumbSize));

  if (!file) {
    return std::nullopt;
  }

  (void)flags;
  (void)payloadSize;
  (void)rawSize;
  (void)checksum;

  metadata.timestamp = timestamp;
  metadata.hasThumbnail = thumbSize > 0;
  metadata.thumbnailWidth = static_cast<i32>(thumbWidth);
  metadata.thumbnailHeight = static_cast<i32>(thumbHeight);
  metadata.thumbnailSize = thumbSize;

  return metadata;
}

} // namespace NovelMind::save
