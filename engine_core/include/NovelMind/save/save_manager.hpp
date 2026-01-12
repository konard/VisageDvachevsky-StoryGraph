#pragma once

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace NovelMind::save {

struct SaveData {
  std::string sceneId;
  std::string nodeId;
  std::map<std::string, i32> intVariables;
  std::map<std::string, f32> floatVariables;
  std::map<std::string, bool> flags;
  std::map<std::string, std::string> stringVariables;
  std::vector<u8> thumbnailData;
  i32 thumbnailWidth = 0;
  i32 thumbnailHeight = 0;
  u64 timestamp;
  u32 checksum;
};

struct SaveMetadata {
  u64 timestamp = 0;
  bool hasThumbnail = false;
  i32 thumbnailWidth = 0;
  i32 thumbnailHeight = 0;
  size_t thumbnailSize = 0;
};

struct SaveConfig {
  bool enableCompression = true;
  bool enableEncryption = false;
  std::vector<u8> encryptionKey;
};

class SaveManager {
public:
  SaveManager();
  ~SaveManager();

  Result<void> save(i32 slot, const SaveData& data);
  Result<SaveData> load(i32 slot);
  Result<void> deleteSave(i32 slot);

  [[nodiscard]] bool slotExists(i32 slot) const;
  [[nodiscard]] std::optional<u64> getSlotTimestamp(i32 slot) const;
  [[nodiscard]] std::optional<SaveMetadata> getSlotMetadata(i32 slot) const;

  [[nodiscard]] i32 getMaxSlots() const;

  void setSavePath(const std::string& path);
  [[nodiscard]] const std::string& getSavePath() const;

  void setConfig(const SaveConfig& config);
  [[nodiscard]] const SaveConfig& getConfig() const;

  Result<void> saveAuto(const SaveData& data);
  Result<SaveData> loadAuto();
  [[nodiscard]] bool autoSaveExists() const;

private:
  [[nodiscard]] std::string getSlotFilename(i32 slot) const;
  [[nodiscard]] std::string getAutoSaveFilename() const;
  [[nodiscard]] static u32 calculateChecksum(const SaveData& data);

  Result<void> saveToFile(const std::string& filename, SaveData data);
  Result<SaveData> loadFromFile(const std::string& filename);
  [[nodiscard]] std::optional<SaveMetadata> readMetadata(const std::string& filename) const;

  std::string m_savePath;
  SaveConfig m_config;
  static constexpr i32 MAX_SLOTS = 100;
};

} // namespace NovelMind::save
