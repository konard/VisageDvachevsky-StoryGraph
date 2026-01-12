#pragma once

/**
 * @file settings_migration.hpp
 * @brief Version migration support for settings
 */

#include "NovelMind/core/result.hpp"
#include "NovelMind/editor/settings_registry.hpp"
#include <string>
#include <unordered_map>

namespace NovelMind::editor {

/**
 * @brief Handles migration of settings between schema versions
 */
class SettingsMigration {
public:
  /**
   * @brief Migrate settings from one version to another
   * @param values Map of setting values to migrate (modified in place)
   * @param definitions Map of setting definitions
   * @param fromVersion Source schema version
   * @param toVersion Target schema version
   * @return Result indicating success or error with migration details
   */
  static Result<void> migrate(
      std::unordered_map<std::string, SettingValue> &values,
      const std::unordered_map<std::string, SettingDefinition> &definitions,
      i32 fromVersion, i32 toVersion);

  /**
   * @brief Get the current schema version
   * @return Current schema version number
   */
  static constexpr i32 getCurrentVersion() { return 1; }

private:
  /**
   * @brief Migrate from version 1 to version 2
   * @param values Map of setting values (modified in place)
   * @param definitions Map of setting definitions
   * @return Result indicating success or error
   */
  static Result<void> migrateV1ToV2(
      std::unordered_map<std::string, SettingValue> &values,
      const std::unordered_map<std::string, SettingDefinition> &definitions);
};

} // namespace NovelMind::editor
