/**
 * @file settings_registry.cpp
 * @brief Settings Registry implementation
 */

#include "NovelMind/editor/settings_registry.hpp"
#include "NovelMind/editor/settings_migration.hpp"
#include "NovelMind/editor/settings_persistence.hpp"
#include "NovelMind/editor/settings_type_handlers.hpp"
#include "NovelMind/editor/settings_validation.hpp"
#include "NovelMind/core/logger.hpp"
#include <algorithm>
#include <cctype>

namespace {

// Convert lowercase string for case-insensitive search
std::string toLower(const std::string &str) {
  std::string result = str;
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return result;
}

} // anonymous namespace

namespace NovelMind::editor {

// ============================================================================
// NMSettingsRegistry
// ============================================================================

NMSettingsRegistry::NMSettingsRegistry() {}

void NMSettingsRegistry::registerSetting(const SettingDefinition &def) {
  std::unique_lock lock(m_mutex);
  m_definitions[def.key] = def;

  // Set default value if not already set
  if (m_values.find(def.key) == m_values.end()) {
    m_values[def.key] = def.defaultValue;
    m_originalValues[def.key] = def.defaultValue;
  }
}

void NMSettingsRegistry::unregisterSetting(const std::string &key) {
  std::unique_lock lock(m_mutex);
  m_definitions.erase(key);
  m_values.erase(key);
  m_originalValues.erase(key);
  m_changeCallbacks.erase(key);
}

std::optional<SettingDefinition>
NMSettingsRegistry::getDefinition(const std::string &key) const {
  std::shared_lock lock(m_mutex);
  auto it = m_definitions.find(key);
  if (it != m_definitions.end()) {
    return it->second;
  }
  return std::nullopt;
}

const std::unordered_map<std::string, SettingDefinition> &
NMSettingsRegistry::getAllDefinitions() const {
  std::shared_lock lock(m_mutex);
  return m_definitions;
}

std::vector<SettingDefinition>
NMSettingsRegistry::getByCategory(const std::string &category) const {
  std::shared_lock lock(m_mutex);
  std::vector<SettingDefinition> result;
  for (const auto &[key, def] : m_definitions) {
    if (def.category == category) {
      result.push_back(def);
    }
  }
  return result;
}

std::vector<SettingDefinition>
NMSettingsRegistry::getByScope(SettingScope scope) const {
  std::shared_lock lock(m_mutex);
  std::vector<SettingDefinition> result;
  for (const auto &[key, def] : m_definitions) {
    if (def.scope == scope) {
      result.push_back(def);
    }
  }
  return result;
}

std::vector<SettingDefinition>
NMSettingsRegistry::search(const std::string &query) const {
  std::shared_lock lock(m_mutex);
  if (query.empty()) {
    std::vector<SettingDefinition> all;
    for (const auto &[key, def] : m_definitions) {
      all.push_back(def);
    }
    return all;
  }

  std::string queryLower = toLower(query);
  std::vector<SettingDefinition> result;

  for (const auto &[key, def] : m_definitions) {
    // Search in key, display name, description, and tags
    if (toLower(def.key).find(queryLower) != std::string::npos ||
        toLower(def.displayName).find(queryLower) != std::string::npos ||
        toLower(def.description).find(queryLower) != std::string::npos ||
        toLower(def.category).find(queryLower) != std::string::npos) {
      result.push_back(def);
      continue;
    }

    for (const auto &tag : def.tags) {
      if (toLower(tag).find(queryLower) != std::string::npos) {
        result.push_back(def);
        break;
      }
    }
  }

  return result;
}

// ========== Value Management ==========

std::optional<SettingValue>
NMSettingsRegistry::getValue(const std::string &key) const {
  std::shared_lock lock(m_mutex);
  auto it = m_values.find(key);
  if (it != m_values.end()) {
    return it->second;
  }

  // Return default if not set
  auto defIt = m_definitions.find(key);
  if (defIt != m_definitions.end()) {
    return defIt->second.defaultValue;
  }

  return std::nullopt;
}

std::string NMSettingsRegistry::setValue(const std::string &key,
                                         const SettingValue &value) {
  std::unique_lock lock(m_mutex);
  auto defIt = m_definitions.find(key);
  if (defIt == m_definitions.end()) {
    return "Setting not registered: " + key;
  }

  // Validate
  std::string error = validateValue(key, value);
  if (!error.empty()) {
    return error;
  }

  // Store old value for change tracking
  if (m_originalValues.find(key) == m_originalValues.end()) {
    m_originalValues[key] = m_values[key];
  }

  // Set new value
  m_values[key] = value;
  m_isDirty = true;

  // Notify callbacks
  notifyChange(key, value);

  return ""; // Success
}

void NMSettingsRegistry::resetToDefault(const std::string &key) {
  std::unique_lock lock(m_mutex);
  auto defIt = m_definitions.find(key);
  if (defIt != m_definitions.end()) {
    m_values[key] = defIt->second.defaultValue;
    m_originalValues.erase(key);
    m_isDirty = true;
    notifyChange(key, defIt->second.defaultValue);
  }
}

void NMSettingsRegistry::resetAllToDefaults() {
  std::unique_lock lock(m_mutex);
  for (const auto &[key, def] : m_definitions) {
    m_values[key] = def.defaultValue;
  }
  m_originalValues.clear();
  m_isDirty = true;
}

void NMSettingsRegistry::resetCategoryToDefaults(const std::string &category) {
  std::unique_lock lock(m_mutex);
  for (const auto &[key, def] : m_definitions) {
    if (def.category == category) {
      m_values[key] = def.defaultValue;
      m_originalValues.erase(key);
      notifyChange(key, def.defaultValue);
    }
  }
  m_isDirty = true;
}

// ========== Type-safe Getters ==========

bool NMSettingsRegistry::getBool(const std::string &key,
                                 bool defaultVal) const {
  auto value = getValue(key);
  if (!value)
    return defaultVal;
  try {
    return std::get<bool>(*value);
  } catch (...) {
    return defaultVal;
  }
}

i32 NMSettingsRegistry::getInt(const std::string &key, i32 defaultVal) const {
  auto value = getValue(key);
  if (!value)
    return defaultVal;
  try {
    return std::get<i32>(*value);
  } catch (...) {
    return defaultVal;
  }
}

f32 NMSettingsRegistry::getFloat(const std::string &key, f32 defaultVal) const {
  auto value = getValue(key);
  if (!value)
    return defaultVal;
  try {
    return std::get<f32>(*value);
  } catch (...) {
    return defaultVal;
  }
}

std::string NMSettingsRegistry::getString(const std::string &key,
                                          const std::string &defaultVal) const {
  auto value = getValue(key);
  if (!value)
    return defaultVal;
  try {
    return std::get<std::string>(*value);
  } catch (...) {
    return defaultVal;
  }
}

// ========== Change Tracking ==========

bool NMSettingsRegistry::isModified(const std::string &key) const {
  std::shared_lock lock(m_mutex);
  return m_originalValues.find(key) != m_originalValues.end();
}

std::vector<std::string> NMSettingsRegistry::getModifiedSettings() const {
  std::shared_lock lock(m_mutex);
  std::vector<std::string> result;
  for (const auto &[key, value] : m_originalValues) {
    result.push_back(key);
  }
  return result;
}

void NMSettingsRegistry::revert() {
  std::unique_lock lock(m_mutex);
  for (const auto &[key, originalValue] : m_originalValues) {
    m_values[key] = originalValue;
    notifyChange(key, originalValue);
  }
  m_originalValues.clear();
  m_isDirty = false;
}

void NMSettingsRegistry::apply() {
  std::unique_lock lock(m_mutex);
  m_originalValues.clear();
  m_isDirty = false;
}

void NMSettingsRegistry::registerChangeCallback(
    const std::string &key, SettingChangeCallback callback) {
  std::unique_lock lock(m_mutex);
  m_changeCallbacks[key].push_back(std::move(callback));
}

void NMSettingsRegistry::unregisterChangeCallback(const std::string &key) {
  std::unique_lock lock(m_mutex);
  m_changeCallbacks.erase(key);
}

// ========== Persistence ==========

Result<void> NMSettingsRegistry::loadUserSettings(const std::string &path) {
  std::unique_lock lock(m_mutex);
  m_userSettingsPath = path;
  return loadFromJson(path, SettingScope::User);
}

Result<void> NMSettingsRegistry::saveUserSettings(const std::string &path) {
  std::unique_lock lock(m_mutex);
  m_userSettingsPath = path;
  return saveToJson(path, SettingScope::User);
}

Result<void> NMSettingsRegistry::loadProjectSettings(const std::string &path) {
  std::unique_lock lock(m_mutex);
  m_projectSettingsPath = path;
  return loadFromJson(path, SettingScope::Project);
}

Result<void> NMSettingsRegistry::saveProjectSettings(const std::string &path) {
  std::unique_lock lock(m_mutex);
  m_projectSettingsPath = path;
  return saveToJson(path, SettingScope::Project);
}

// ========== Private Methods ==========

std::string NMSettingsRegistry::validateValue(const std::string &key,
                                              const SettingValue &value) const {
  auto defIt = m_definitions.find(key);
  if (defIt == m_definitions.end()) {
    return "Setting not found";
  }

  return SettingsValidation::validateValue(key, value, defIt->second);
}

Result<void> NMSettingsRegistry::loadFromJson(const std::string &path,
                                              SettingScope scope) {
  auto result = SettingsPersistence::loadFromJson(path, scope, m_definitions,
                                                   m_values, m_schemaVersion);
  if (result.isOk()) {
    m_originalValues.clear(); // Reset change tracking after load
    m_isDirty = false;

    // Apply migrations if needed
    auto migrationResult = SettingsMigration::migrate(
        m_values, m_definitions, m_schemaVersion,
        SettingsMigration::getCurrentVersion());
    if (!migrationResult.isOk()) {
      return migrationResult;
    }
    m_schemaVersion = SettingsMigration::getCurrentVersion();
  }
  return result;
}

Result<void> NMSettingsRegistry::saveToJson(const std::string &path,
                                            SettingScope scope) const {
  namespace fs = std::filesystem;

  try {
    // Ensure directory exists
    fs::path filePath(path);
    if (filePath.has_parent_path()) {
      fs::create_directories(filePath.parent_path());
    }

    // Use atomic write pattern: write to temp file, then rename
    // This prevents data loss if write is interrupted
    fs::path tempPath = filePath;
    tempPath += ".tmp";

    // Write to temporary file
    {
      std::ofstream file(tempPath);
      if (!file) {
        return Result<void>::error("Failed to open settings file for writing");
      }

      // Write JSON
      file << "{\n";
      file << "  \"settings_version\": " << m_schemaVersion << ",\n";
      file << "  \"settings\": {\n";

      bool first = true;
      for (const auto &[key, def] : m_definitions) {
        if (def.scope != scope)
          continue;

        auto valueIt = m_values.find(key);
        if (valueIt == m_values.end())
          continue;

        if (!first)
          file << ",\n";
        first = false;

        file << "    \"" << escapeJson(key) << "\": ";

        const auto &value = valueIt->second;
        if (def.type == SettingType::Bool) {
          file << (std::get<bool>(value) ? "true" : "false");
        } else if (def.type == SettingType::Int ||
                   def.type == SettingType::IntRange) {
          file << std::get<i32>(value);
        } else if (def.type == SettingType::Float ||
                   def.type == SettingType::FloatRange) {
          file << std::get<f32>(value);
        } else {
          file << "\"" << escapeJson(std::get<std::string>(value)) << "\"";
        }
      }

      file << "\n  }\n";
      file << "}\n";

      // Ensure data is flushed to disk
      file.flush();
      if (!file.good()) {
        return Result<void>::error("Failed to write settings file");
      }
    } // Close file before rename

    // Atomic rename: this is atomic on most filesystems
    // If the process crashes during rename, either the old or new file exists
    fs::rename(tempPath, filePath);

    NOVELMIND_LOG_INFO(std::string("Saved settings to: ") + path);
    return Result<void>::ok();
  } catch (const std::exception &e) {
    return Result<void>::error(std::string("Failed to save settings: ") +
                               e.what());
  }
}

void NMSettingsRegistry::notifyChange(const std::string &key,
                                      const SettingValue &newValue) {
  // Copy callbacks to avoid holding lock during callback execution
  // and to prevent iterator invalidation if callbacks modify the registry
  std::vector<SettingChangeCallback> callbacks;
  {
    // We already hold the mutex when this is called, so we don't need to lock again
    // Just copy the callbacks
    auto it = m_changeCallbacks.find(key);
    if (it != m_changeCallbacks.end()) {
      callbacks = it->second;
    }
  }

  // Execute callbacks without holding the lock
  // Note: This is safe because we copied the callbacks
  for (const auto &callback : callbacks) {
    if (callback) {
      callback(key, newValue);
    }
  }
}

// Note: registerEditorDefaults() and registerProjectDefaults() are now in:
// - editor/src/settings_defaults_editor.cpp
// - editor/src/settings_defaults_project.cpp

} // namespace NovelMind::editor
