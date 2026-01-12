#pragma once

/**
 * @file settings_registry.hpp
 * @brief Centralized Settings Registry System
 *
 * Provides a Unity-style settings system with:
 * - Typed settings with validation
 * - Editor Preferences (user scope) and Project Settings (project scope)
 * - Versioned JSON persistence
 * - Change tracking and dirty state
 * - Search and filtering
 */

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include <any>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace NovelMind::editor {

// ============================================================================
// Setting Types and Values
// ============================================================================

/**
 * @brief Setting value type
 */
enum class SettingType : u8 {
  Bool,
  Int,
  Float,
  String,
  Enum,       // String-backed enum
  Path,       // File/directory path
  Color,      // RGBA color
  Hotkey,     // Keyboard binding
  FloatRange, // Float with min/max slider
  IntRange    // Int with min/max slider
};

/**
 * @brief Setting scope - where it's stored
 */
enum class SettingScope : u8 {
  User,   // Editor Preferences (per-user, not in project)
  Project // Project Settings (stored in project, shared with team)
};

/**
 * @brief Polymorphic setting value
 */
using SettingValue =
    std::variant<bool, i32, f32, std::string,
                 std::vector<std::string> // For enum options or paths
                 >;

/**
 * @brief Validation function for a setting
 * @return empty string if valid, error message if invalid
 */
using SettingValidator = std::function<std::string(const SettingValue &)>;

/**
 * @brief Callback when a setting changes
 */
using SettingChangeCallback =
    std::function<void(const std::string &key, const SettingValue &newValue)>;

// ============================================================================
// Setting Definition
// ============================================================================

/**
 * @brief Complete definition of a setting
 */
struct SettingDefinition {
  std::string key;         // Unique key (e.g. "editor.theme")
  std::string displayName; // User-facing name
  std::string description; // Tooltip/help text
  std::string category;    // Category path (e.g. "Editor/Appearance")
  SettingType type = SettingType::String;
  SettingScope scope = SettingScope::User;

  SettingValue defaultValue; // Default value

  // Type-specific configuration
  std::vector<std::string> enumOptions; // For Enum type
  f32 minValue = 0.0f;                  // For Float/Int range
  f32 maxValue = 1.0f;                  // For Float/Int range

  SettingValidator validator; // Optional validator

  bool requiresRestart = false;  // Does changing this require restart?
  bool isAdvanced = false;       // Hide in simple view?
  std::vector<std::string> tags; // For search/filtering
};

// ============================================================================
// Settings Registry
// ============================================================================

/**
 * @brief Centralized registry of all settings
 *
 * This is the single source of truth for:
 * - Setting definitions (keys, types, defaults, validation)
 * - Current values (user preferences + project settings)
 * - Change tracking and dirty state
 * - Persistence (load/save to JSON)
 */
class NMSettingsRegistry {
public:
  NMSettingsRegistry();
  ~NMSettingsRegistry() = default;

  // Non-copyable (singleton-like usage)
  NMSettingsRegistry(const NMSettingsRegistry &) = delete;
  NMSettingsRegistry &operator=(const NMSettingsRegistry &) = delete;

  /**
   * @brief Register a setting definition
   */
  void registerSetting(const SettingDefinition &def);

  /**
   * @brief Unregister a setting
   */
  void unregisterSetting(const std::string &key);

  /**
   * @brief Get setting definition
   */
  [[nodiscard]] std::optional<SettingDefinition>
  getDefinition(const std::string &key) const;

  /**
   * @brief Get all setting definitions
   */
  [[nodiscard]] const std::unordered_map<std::string, SettingDefinition> &
  getAllDefinitions() const;

  /**
   * @brief Get settings by category
   */
  [[nodiscard]] std::vector<SettingDefinition>
  getByCategory(const std::string &category) const;

  /**
   * @brief Get settings by scope
   */
  [[nodiscard]] std::vector<SettingDefinition>
  getByScope(SettingScope scope) const;

  /**
   * @brief Search settings (by name, description, tags)
   */
  [[nodiscard]] std::vector<SettingDefinition>
  search(const std::string &query) const;

  // ========== Value Management ==========

  /**
   * @brief Get current value of a setting
   */
  [[nodiscard]] std::optional<SettingValue>
  getValue(const std::string &key) const;

  /**
   * @brief Set value of a setting
   * @return error message if validation fails, empty string on success
   */
  std::string setValue(const std::string &key, const SettingValue &value);

  /**
   * @brief Reset setting to default value
   */
  void resetToDefault(const std::string &key);

  /**
   * @brief Reset all settings to defaults
   */
  void resetAllToDefaults();

  /**
   * @brief Reset all settings in a category to defaults
   */
  void resetCategoryToDefaults(const std::string &category);

  // ========== Type-safe Getters ==========

  template <typename T>
  [[nodiscard]] std::optional<T> getValueAs(const std::string &key) const {
    auto value = getValue(key);
    if (!value)
      return std::nullopt;

    try {
      return std::get<T>(*value);
    } catch (...) {
      return std::nullopt;
    }
  }

  [[nodiscard]] bool getBool(const std::string &key,
                             bool defaultVal = false) const;
  [[nodiscard]] i32 getInt(const std::string &key, i32 defaultVal = 0) const;
  [[nodiscard]] f32 getFloat(const std::string &key,
                             f32 defaultVal = 0.0f) const;
  [[nodiscard]] std::string getString(const std::string &key,
                                      const std::string &defaultVal = "") const;

  // ========== Change Tracking ==========

  /**
   * @brief Check if any settings have been modified
   */
  [[nodiscard]] bool isDirty() const { return m_isDirty; }

  /**
   * @brief Check if a specific setting has been modified
   */
  [[nodiscard]] bool isModified(const std::string &key) const;

  /**
   * @brief Get list of all modified settings
   */
  [[nodiscard]] std::vector<std::string> getModifiedSettings() const;

  /**
   * @brief Revert all uncommitted changes
   */
  void revert();

  /**
   * @brief Apply changes (mark as committed)
   */
  void apply();

  /**
   * @brief Register a callback for when a setting changes
   */
  void registerChangeCallback(const std::string &key,
                              SettingChangeCallback callback);

  /**
   * @brief Unregister a change callback
   */
  void unregisterChangeCallback(const std::string &key);

  // ========== Persistence ==========

  /**
   * @brief Load user preferences from JSON file
   */
  Result<void> loadUserSettings(const std::string &path);

  /**
   * @brief Save user preferences to JSON file
   */
  Result<void> saveUserSettings(const std::string &path);

  /**
   * @brief Load project settings from JSON file
   */
  Result<void> loadProjectSettings(const std::string &path);

  /**
   * @brief Save project settings to JSON file
   */
  Result<void> saveProjectSettings(const std::string &path);

  /**
   * @brief Get settings schema version
   */
  [[nodiscard]] i32 getSchemaVersion() const { return m_schemaVersion; }

  /**
   * @brief Set settings schema version
   */
  void setSchemaVersion(i32 version) { m_schemaVersion = version; }

  // ========== Defaults Registration ==========

  /**
   * @brief Register all default editor settings
   */
  void registerEditorDefaults();

  /**
   * @brief Register all default project settings
   */
  void registerProjectDefaults();

private:
  std::string validateValue(const std::string &key,
                            const SettingValue &value) const;

  Result<void> loadFromJson(const std::string &path, SettingScope scope);
  Result<void> saveToJson(const std::string &path, SettingScope scope) const;

  void notifyChange(const std::string &key, const SettingValue &newValue);

  // Setting definitions (key -> definition)
  std::unordered_map<std::string, SettingDefinition> m_definitions;

  // Current values (key -> value)
  std::unordered_map<std::string, SettingValue> m_values;

  // Original values for change tracking (key -> value)
  std::unordered_map<std::string, SettingValue> m_originalValues;

  // Change callbacks (key -> callback)
  std::unordered_map<std::string, std::vector<SettingChangeCallback>>
      m_changeCallbacks;

  bool m_isDirty = false;
  i32 m_schemaVersion = 1;

  std::string m_userSettingsPath;
  std::string m_projectSettingsPath;
};

} // namespace NovelMind::editor
