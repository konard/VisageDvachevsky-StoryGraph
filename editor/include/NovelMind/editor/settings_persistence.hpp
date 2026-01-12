#pragma once

/**
 * @file settings_persistence.hpp
 * @brief File I/O operations for settings persistence
 */

#include "NovelMind/core/result.hpp"
#include "NovelMind/editor/settings_registry.hpp"
#include <string>
#include <unordered_map>

namespace NovelMind::editor {

// Forward declaration
class NMSettingsRegistry;

/**
 * @brief Handles loading and saving settings to/from JSON files
 */
class SettingsPersistence {
public:
  /**
   * @brief Load settings from JSON file
   * @param path Path to the JSON file
   * @param scope Scope to filter settings (User or Project)
   * @param definitions Map of setting definitions
   * @param values Map to populate with loaded values
   * @param schemaVersion Output parameter for detected schema version
   * @return Result indicating success or error
   */
  static Result<void> loadFromJson(const std::string &path, SettingScope scope,
                                    const std::unordered_map<std::string, SettingDefinition> &definitions,
                                    std::unordered_map<std::string, SettingValue> &values,
                                    i32 &schemaVersion);

  /**
   * @brief Save settings to JSON file
   * @param path Path to the JSON file
   * @param scope Scope to filter settings (User or Project)
   * @param definitions Map of setting definitions
   * @param values Map of current setting values
   * @param schemaVersion Schema version to write
   * @return Result indicating success or error
   */
  static Result<void> saveToJson(const std::string &path, SettingScope scope,
                                  const std::unordered_map<std::string, SettingDefinition> &definitions,
                                  const std::unordered_map<std::string, SettingValue> &values,
                                  i32 schemaVersion);
};

} // namespace NovelMind::editor
