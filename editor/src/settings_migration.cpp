/**
 * @file settings_migration.cpp
 * @brief Version migration support for settings
 */

#include "NovelMind/editor/settings_migration.hpp"
#include "NovelMind/core/logger.hpp"
#include <string>

namespace NovelMind::editor {

Result<void> SettingsMigration::migrate(
    std::unordered_map<std::string, SettingValue> &values,
    const std::unordered_map<std::string, SettingDefinition> &definitions,
    i32 fromVersion, i32 toVersion) {

  if (fromVersion == toVersion) {
    // No migration needed
    return Result<void>::ok();
  }

  if (fromVersion > toVersion) {
    return Result<void>::error(
        "Cannot migrate backwards from version " +
        std::to_string(fromVersion) + " to " + std::to_string(toVersion));
  }

  NOVELMIND_LOG_INFO("Migrating settings from version " +
                     std::to_string(fromVersion) + " to version " +
                     std::to_string(toVersion));

  // Apply migrations sequentially
  i32 currentVersion = fromVersion;

  while (currentVersion < toVersion) {
    if (currentVersion == 1) {
      auto result = migrateV1ToV2(values, definitions);
      if (!result.isOk()) {
        return result;
      }
      currentVersion = 2;
    } else {
      // No migration path found
      return Result<void>::error(
          "No migration path from version " + std::to_string(currentVersion) +
          " to version " + std::to_string(toVersion));
    }
  }

  NOVELMIND_LOG_INFO("Settings migration completed successfully");
  return Result<void>::ok();
}

Result<void> SettingsMigration::migrateV1ToV2(
    std::unordered_map<std::string, SettingValue> &values,
    const std::unordered_map<std::string, SettingDefinition> &definitions) {

  // Example migration: If we need to migrate from V1 to V2
  // Currently no migration is needed as we're at version 1
  // This is a placeholder for future migrations

  // Example migration operations:
  // 1. Rename settings keys
  // if (values.find("old.key") != values.end()) {
  //   values["new.key"] = values["old.key"];
  //   values.erase("old.key");
  // }

  // 2. Transform values
  // if (auto it = values.find("some.setting"); it != values.end()) {
  //   // Transform value if needed
  // }

  // 3. Set default values for new settings
  // for (const auto &[key, def] : definitions) {
  //   if (values.find(key) == values.end()) {
  //     values[key] = def.defaultValue;
  //   }
  // }

  NOVELMIND_LOG_INFO("Migration from V1 to V2 completed");
  return Result<void>::ok();
}

} // namespace NovelMind::editor
