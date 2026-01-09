/**
 * @file settings_registry.cpp
 * @brief Settings Registry implementation
 */

#include "NovelMind/editor/settings_registry.hpp"
#include "NovelMind/core/logger.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

// Simple JSON parsing/generation
// For production, consider using nlohmann/json or similar
namespace {

// Helper to escape JSON strings
std::string escapeJson(const std::string &str) {
  std::string result;
  for (char c : str) {
    switch (c) {
    case '"':
      result += "\\\"";
      break;
    case '\\':
      result += "\\\\";
      break;
    case '\n':
      result += "\\n";
      break;
    case '\r':
      result += "\\r";
      break;
    case '\t':
      result += "\\t";
      break;
    default:
      result += c;
      break;
    }
  }
  return result;
}

// Helper to unescape JSON strings
std::string unescapeJson(const std::string &str) {
  std::string result;
  bool escape = false;
  for (char c : str) {
    if (escape) {
      switch (c) {
      case 'n':
        result += '\n';
        break;
      case 'r':
        result += '\r';
        break;
      case 't':
        result += '\t';
        break;
      default:
        result += c;
        break;
      }
      escape = false;
    } else if (c == '\\') {
      escape = true;
    } else {
      result += c;
    }
  }
  return result;
}

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
  m_definitions[def.key] = def;

  // Set default value if not already set
  if (m_values.find(def.key) == m_values.end()) {
    m_values[def.key] = def.defaultValue;
    m_originalValues[def.key] = def.defaultValue;
  }
}

void NMSettingsRegistry::unregisterSetting(const std::string &key) {
  m_definitions.erase(key);
  m_values.erase(key);
  m_originalValues.erase(key);
  m_changeCallbacks.erase(key);
}

std::optional<SettingDefinition>
NMSettingsRegistry::getDefinition(const std::string &key) const {
  auto it = m_definitions.find(key);
  if (it != m_definitions.end()) {
    return it->second;
  }
  return std::nullopt;
}

const std::unordered_map<std::string, SettingDefinition> &
NMSettingsRegistry::getAllDefinitions() const {
  return m_definitions;
}

std::vector<SettingDefinition>
NMSettingsRegistry::getByCategory(const std::string &category) const {
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
  auto defIt = m_definitions.find(key);
  if (defIt != m_definitions.end()) {
    m_values[key] = defIt->second.defaultValue;
    m_originalValues.erase(key);
    m_isDirty = true;
    notifyChange(key, defIt->second.defaultValue);
  }
}

void NMSettingsRegistry::resetAllToDefaults() {
  for (const auto &[key, def] : m_definitions) {
    m_values[key] = def.defaultValue;
  }
  m_originalValues.clear();
  m_isDirty = true;
}

void NMSettingsRegistry::resetCategoryToDefaults(const std::string &category) {
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
  return m_originalValues.find(key) != m_originalValues.end();
}

std::vector<std::string> NMSettingsRegistry::getModifiedSettings() const {
  std::vector<std::string> result;
  for (const auto &[key, value] : m_originalValues) {
    result.push_back(key);
  }
  return result;
}

void NMSettingsRegistry::revert() {
  for (const auto &[key, originalValue] : m_originalValues) {
    m_values[key] = originalValue;
    notifyChange(key, originalValue);
  }
  m_originalValues.clear();
  m_isDirty = false;
}

void NMSettingsRegistry::apply() {
  m_originalValues.clear();
  m_isDirty = false;
}

void NMSettingsRegistry::registerChangeCallback(
    const std::string &key, SettingChangeCallback callback) {
  m_changeCallbacks[key].push_back(std::move(callback));
}

void NMSettingsRegistry::unregisterChangeCallback(const std::string &key) {
  m_changeCallbacks.erase(key);
}

// ========== Persistence ==========

Result<void> NMSettingsRegistry::loadUserSettings(const std::string &path) {
  m_userSettingsPath = path;
  return loadFromJson(path, SettingScope::User);
}

Result<void> NMSettingsRegistry::saveUserSettings(const std::string &path) {
  m_userSettingsPath = path;
  return saveToJson(path, SettingScope::User);
}

Result<void> NMSettingsRegistry::loadProjectSettings(const std::string &path) {
  m_projectSettingsPath = path;
  return loadFromJson(path, SettingScope::Project);
}

Result<void> NMSettingsRegistry::saveProjectSettings(const std::string &path) {
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

  const auto &def = defIt->second;

  // Type-specific validation
  if (def.type == SettingType::Int || def.type == SettingType::IntRange) {
    try {
      i32 intVal = std::get<i32>(value);
      if (def.type == SettingType::IntRange) {
        if (intVal < static_cast<i32>(def.minValue) ||
            intVal > static_cast<i32>(def.maxValue)) {
          return "Value out of range [" +
                 std::to_string(static_cast<i32>(def.minValue)) + ", " +
                 std::to_string(static_cast<i32>(def.maxValue)) + "]";
        }
      }
    } catch (...) {
      return "Invalid type (expected Int)";
    }
  } else if (def.type == SettingType::Float ||
             def.type == SettingType::FloatRange) {
    try {
      f32 floatVal = std::get<f32>(value);
      if (def.type == SettingType::FloatRange) {
        if (floatVal < def.minValue || floatVal > def.maxValue) {
          return "Value out of range [" + std::to_string(def.minValue) + ", " +
                 std::to_string(def.maxValue) + "]";
        }
      }
    } catch (...) {
      return "Invalid type (expected Float)";
    }
  } else if (def.type == SettingType::Bool) {
    try {
      std::get<bool>(value);
    } catch (...) {
      return "Invalid type (expected Bool)";
    }
  } else if (def.type == SettingType::String || def.type == SettingType::Path ||
             def.type == SettingType::Enum) {
    try {
      std::string strVal = std::get<std::string>(value);
      if (def.type == SettingType::Enum) {
        // Check if value is in enum options
        if (std::find(def.enumOptions.begin(), def.enumOptions.end(), strVal) ==
            def.enumOptions.end()) {
          return "Invalid enum value";
        }
      }
    } catch (...) {
      return "Invalid type (expected String)";
    }
  }

  // Custom validator
  if (def.validator) {
    return def.validator(value);
  }

  return ""; // Valid
}

Result<void> NMSettingsRegistry::loadFromJson(const std::string &path,
                                              SettingScope scope) {
  namespace fs = std::filesystem;

  if (!fs::exists(path)) {
    NOVELMIND_LOG_INFO(
        std::string("Settings file not found, using defaults: ") + path);
    return Result<void>::ok();
  }

  try {
    std::ifstream file(path);
    if (!file) {
      return Result<void>::error("Failed to open settings file");
    }

    // Simple JSON parsing
    std::string line;
    std::string currentKey;
    i32 version = 1;

    while (std::getline(file, line)) {
      // Trim whitespace
      line.erase(0, line.find_first_not_of(" \t\r\n"));
      line.erase(line.find_last_not_of(" \t\r\n") + 1);

      if (line.empty() || line[0] == '#')
        continue;

      // Parse version
      if (line.find("\"settings_version\"") != std::string::npos) {
        auto pos = line.find(':');
        if (pos != std::string::npos) {
          std::string versionStr = line.substr(pos + 1);
          versionStr.erase(0, versionStr.find_first_not_of(" \t,"));
          versionStr.erase(versionStr.find_last_not_of(" \t,") + 1);
          version = std::stoi(versionStr);
        }
      }

      // Parse key-value pairs
      if (line.find("\":") != std::string::npos && line[0] == '"') {
        auto colonPos = line.find("\":");
        std::string key =
            line.substr(1, colonPos - 1); // Extract key between quotes
        std::string valueStr = line.substr(colonPos + 2);

        // Remove trailing comma and whitespace
        valueStr.erase(0, valueStr.find_first_not_of(" \t"));
        if (!valueStr.empty() && valueStr.back() == ',') {
          valueStr.pop_back();
        }
        valueStr.erase(valueStr.find_last_not_of(" \t") + 1);

        // Find definition for this key
        auto defIt = m_definitions.find(key);
        if (defIt != m_definitions.end() && defIt->second.scope == scope) {
          const auto &def = defIt->second;

          // Parse value based on type
          if (def.type == SettingType::Bool) {
            bool val = (valueStr == "true");
            m_values[key] = val;
          } else if (def.type == SettingType::Int ||
                     def.type == SettingType::IntRange) {
            i32 val = std::stoi(valueStr);
            m_values[key] = val;
          } else if (def.type == SettingType::Float ||
                     def.type == SettingType::FloatRange) {
            f32 val = std::stof(valueStr);
            m_values[key] = val;
          } else {
            // String type - remove quotes
            if (valueStr.size() >= 2 && valueStr.front() == '"' &&
                valueStr.back() == '"') {
              valueStr = valueStr.substr(1, valueStr.size() - 2);
            }
            m_values[key] = unescapeJson(valueStr);
          }
        }
      }
    }

    m_schemaVersion = version;
    m_originalValues.clear(); // Reset change tracking after load
    m_isDirty = false;

    NOVELMIND_LOG_INFO(std::string("Loaded settings from: ") + path);
    return Result<void>::ok();
  } catch (const std::exception &e) {
    return Result<void>::error(std::string("Failed to load settings: ") +
                               e.what());
  }
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

    std::ofstream file(path);
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

    NOVELMIND_LOG_INFO(std::string("Saved settings to: ") + path);
    return Result<void>::ok();
  } catch (const std::exception &e) {
    return Result<void>::error(std::string("Failed to save settings: ") +
                               e.what());
  }
}

void NMSettingsRegistry::notifyChange(const std::string &key,
                                      const SettingValue &newValue) {
  auto it = m_changeCallbacks.find(key);
  if (it != m_changeCallbacks.end()) {
    for (const auto &callback : it->second) {
      if (callback) {
        callback(key, newValue);
      }
    }
  }
}

// ========== Default Settings Registration ==========

void NMSettingsRegistry::registerEditorDefaults() {
  // ========== General ==========
  registerSetting({
      "editor.general.autosave",
      "Auto Save",
      "Automatically save project at intervals",
      "Editor/General",
      SettingType::Bool,
      SettingScope::User,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  registerSetting({
      "editor.general.autosave_interval",
      "Auto Save Interval (seconds)",
      "Time between automatic saves",
      "Editor/General",
      SettingType::IntRange,
      SettingScope::User,
      300,     // default
      {},      // enum options
      60.0f,   // min
      3600.0f, // max
      {},      // validator
      false,   // requiresRestart
      false,   // isAdvanced
      {}       // tags
  });

  registerSetting({
      "editor.general.confirm_on_close",
      "Confirm on Close",
      "Ask for confirmation when closing with unsaved changes",
      "Editor/General",
      SettingType::Bool,
      SettingScope::User,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  registerSetting({
      "editor.general.reopen_last_project",
      "Reopen Last Project",
      "Automatically reopen the last project on startup",
      "Editor/General",
      SettingType::Bool,
      SettingScope::User,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  // ========== Appearance ==========
  registerSetting({
      "editor.appearance.theme",
      "Theme",
      "Editor color theme",
      "Editor/Appearance",
      SettingType::Enum,
      SettingScope::User,
      std::string("dark"),
      {"dark", "light"},
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  registerSetting({
      "editor.appearance.ui_scale",
      "UI Scale",
      "Interface scaling factor",
      "Editor/Appearance",
      SettingType::FloatRange,
      SettingScope::User,
      1.0f,
      {},
      0.5f,  // min
      2.0f,  // max
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  registerSetting({
      "editor.appearance.show_tooltips",
      "Show Tooltips",
      "Display helpful tooltips on hover",
      "Editor/Appearance",
      SettingType::Bool,
      SettingScope::User,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  // ========== Workspace ==========
  registerSetting({
      "editor.workspace.default_layout",
      "Default Layout",
      "Workspace layout to use for new projects",
      "Editor/Workspace",
      SettingType::Enum,
      SettingScope::User,
      std::string("default"),
      {"default", "story_focused", "scene_focused", "script_focused",
       "minimal"},
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  registerSetting({
      "editor.workspace.remember_layout",
      "Remember Layout",
      "Restore window layout from last session",
      "Editor/Workspace",
      SettingType::Bool,
      SettingScope::User,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  // ========== Script Editor ==========
  registerSetting({
      "editor.script.show_line_numbers",
      "Show Line Numbers",
      "Display line numbers in script editor",
      "Editor/Script Editor",
      SettingType::Bool,
      SettingScope::User,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  registerSetting({
      "editor.script.word_wrap",
      "Word Wrap",
      "Wrap long lines in script editor",
      "Editor/Script Editor",
      SettingType::Bool,
      SettingScope::User,
      false,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  registerSetting({
      "editor.script.tab_size",
      "Tab Size",
      "Number of spaces per tab",
      "Editor/Script Editor",
      SettingType::IntRange,
      SettingScope::User,
      4,
      {},
      2.0f,  // min
      8.0f,  // max
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  registerSetting({
      "editor.script.insert_spaces",
      "Insert Spaces",
      "Use spaces instead of tabs",
      "Editor/Script Editor",
      SettingType::Bool,
      SettingScope::User,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  registerSetting({
      "editor.script.auto_complete",
      "Auto Complete",
      "Enable code completion suggestions",
      "Editor/Script Editor",
      SettingType::Bool,
      SettingScope::User,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  registerSetting({
      "editor.script.highlight_current_line",
      "Highlight Current Line",
      "Highlight the line with the cursor",
      "Editor/Script Editor",
      SettingType::Bool,
      SettingScope::User,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  // ========== Script Editor - Font Settings ==========
  registerSetting({
      "editor.script.font_family",
      "Font Family",
      "Font family for script editor (e.g. 'Fira Code', 'Consolas', 'Courier New')",
      "Editor/Script Editor/Font",
      SettingType::String,
      SettingScope::User,
      std::string("monospace"),
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {"font", "editor"}     // tags
  });

  registerSetting({
      "editor.script.font_size",
      "Font Size",
      "Font size in pixels",
      "Editor/Script Editor/Font",
      SettingType::IntRange,
      SettingScope::User,
      14,
      {},
      8.0f,   // min
      32.0f,  // max
      {},     // validator
      false,  // requiresRestart
      false,  // isAdvanced
      {"font", "editor"}     // tags
  });

  registerSetting({
      "editor.script.font_ligatures",
      "Enable Font Ligatures",
      "Enable font ligatures for programming fonts (e.g. -> => !=)",
      "Editor/Script Editor/Font",
      SettingType::Bool,
      SettingScope::User,
      false,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {"font", "editor"}     // tags
  });

  // ========== Script Editor - Display Settings ==========
  registerSetting({
      "editor.script.show_minimap",
      "Show Minimap",
      "Display code minimap on the right side",
      "Editor/Script Editor/Display",
      SettingType::Bool,
      SettingScope::User,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {"display", "editor"}     // tags
  });

  registerSetting({
      "editor.script.show_breadcrumbs",
      "Show Breadcrumbs",
      "Display breadcrumb navigation bar",
      "Editor/Script Editor/Display",
      SettingType::Bool,
      SettingScope::User,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {"display", "editor"}     // tags
  });

  registerSetting({
      "editor.script.show_whitespace",
      "Show Whitespace Characters",
      "Display whitespace characters (spaces, tabs)",
      "Editor/Script Editor/Display",
      SettingType::Bool,
      SettingScope::User,
      false,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {"display", "editor"}     // tags
  });

  // ========== Script Editor - Indentation Settings ==========
  registerSetting({
      "editor.script.auto_indent",
      "Auto Indent New Lines",
      "Automatically indent new lines based on context",
      "Editor/Script Editor/Indentation",
      SettingType::Bool,
      SettingScope::User,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {"indentation", "editor"}     // tags
  });

  // ========== Script Editor - Diagnostics Settings ==========
  registerSetting({
      "editor.script.diagnostic_delay",
      "Diagnostic Delay (ms)",
      "Delay before running validation after typing",
      "Editor/Script Editor/Diagnostics",
      SettingType::IntRange,
      SettingScope::User,
      600,
      {},
      100.0f,  // min
      2000.0f, // max
      {},      // validator
      false,   // requiresRestart
      false,   // isAdvanced
      {"diagnostics", "editor"}     // tags
  });

  registerSetting({
      "editor.script.show_inline_errors",
      "Show Inline Errors",
      "Display error messages inline in the editor",
      "Editor/Script Editor/Diagnostics",
      SettingType::Bool,
      SettingScope::User,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {"diagnostics", "editor"}     // tags
  });

  registerSetting({
      "editor.script.show_error_underlines",
      "Show Error Underlines",
      "Underline errors and warnings in the text",
      "Editor/Script Editor/Diagnostics",
      SettingType::Bool,
      SettingScope::User,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {"diagnostics", "editor"}     // tags
  });

  registerSetting({
      "editor.script.validate_on_save",
      "Validate on Save",
      "Run validation when saving files",
      "Editor/Script Editor/Diagnostics",
      SettingType::Bool,
      SettingScope::User,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {"diagnostics", "editor"}     // tags
  });

  registerSetting({
      "editor.script.validate_on_focus_change",
      "Validate on Focus Change",
      "Run validation when switching between tabs",
      "Editor/Script Editor/Diagnostics",
      SettingType::Bool,
      SettingScope::User,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {"diagnostics", "editor"}     // tags
  });

  registerSetting({
      "editor.script.max_errors_displayed",
      "Max Errors Displayed",
      "Maximum number of errors to show in the error list",
      "Editor/Script Editor/Diagnostics",
      SettingType::IntRange,
      SettingScope::User,
      100,
      {},
      10.0f,   // min
      1000.0f, // max
      {},      // validator
      false,   // requiresRestart
      true,    // isAdvanced
      {"diagnostics", "editor"}     // tags
  });

  // ========== Script Editor - Advanced Settings ==========
  registerSetting({
      "editor.script.auto_save_on_focus_loss",
      "Auto-Save on Focus Loss",
      "Automatically save when switching to another tab or window",
      "Editor/Script Editor/Advanced",
      SettingType::Bool,
      SettingScope::User,
      false,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      true,  // isAdvanced
      {"auto-save", "editor"}     // tags
  });

  registerSetting({
      "editor.script.auto_save_delay",
      "Auto-Save Delay (seconds)",
      "Time to wait before auto-saving after changes",
      "Editor/Script Editor/Advanced",
      SettingType::IntRange,
      SettingScope::User,
      30,
      {},
      5.0f,    // min
      300.0f,  // max
      {},      // validator
      false,   // requiresRestart
      true,    // isAdvanced
      {"auto-save", "editor"}     // tags
  });

  registerSetting({
      "editor.script.restore_open_files",
      "Restore Open Files on Startup",
      "Reopen files that were open in the previous session",
      "Editor/Script Editor/Advanced",
      SettingType::Bool,
      SettingScope::User,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      true,  // isAdvanced
      {"session", "editor"}     // tags
  });

  registerSetting({
      "editor.script.restore_cursor_position",
      "Restore Cursor Position",
      "Restore cursor position when reopening files",
      "Editor/Script Editor/Advanced",
      SettingType::Bool,
      SettingScope::User,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      true,  // isAdvanced
      {"session", "editor"}     // tags
  });

  registerSetting({
      "editor.script.file_watcher_enabled",
      "File Watcher",
      "Watch for external file changes",
      "Editor/Script Editor/Advanced",
      SettingType::Enum,
      SettingScope::User,
      std::string("enabled"),
      {"enabled", "disabled", "prompt"},
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      true,  // isAdvanced
      {"file-watcher", "editor"}     // tags
  });

  registerSetting({
      "editor.script.virtual_rendering_threshold",
      "Virtual Rendering Threshold (lines)",
      "Use virtual rendering for files larger than this many lines",
      "Editor/Script Editor/Advanced",
      SettingType::IntRange,
      SettingScope::User,
      1000,
      {},
      100.0f,   // min
      10000.0f, // max
      {},       // validator
      false,    // requiresRestart
      true,     // isAdvanced
      {"performance", "editor"}     // tags
  });

  registerSetting({
      "editor.script.minimap_update_delay",
      "Minimap Update Delay (ms)",
      "Delay before updating minimap after changes",
      "Editor/Script Editor/Advanced",
      SettingType::IntRange,
      SettingScope::User,
      100,
      {},
      0.0f,    // min
      500.0f,  // max
      {},      // validator
      false,   // requiresRestart
      true,    // isAdvanced
      {"performance", "minimap", "editor"}     // tags
  });

  // ========== Audio ==========
  registerSetting({
      "editor.audio.input_device",
      "Input Device",
      "Microphone for voice recording",
      "Editor/Audio",
      SettingType::String,
      SettingScope::User,
      std::string("default"),
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  registerSetting({
      "editor.audio.output_device",
      "Output Device",
      "Audio playback device",
      "Editor/Audio",
      SettingType::String,
      SettingScope::User,
      std::string("default"),
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  registerSetting({
      "editor.audio.buffer_size",
      "Buffer Size",
      "Audio buffer size (lower = less latency, more CPU)",
      "Editor/Audio",
      SettingType::Enum,
      SettingScope::User,
      std::string("512"),
      {"128", "256", "512", "1024", "2048"},
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  // ========== Preview ==========
  registerSetting({
      "editor.preview.scale",
      "Preview Scale",
      "Scale factor for preview window",
      "Editor/Preview",
      SettingType::FloatRange,
      SettingScope::User,
      1.0f,
      {},
      0.25f, // min
      2.0f,  // max
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  registerSetting({
      "editor.preview.show_fps",
      "Show FPS",
      "Display FPS counter in preview",
      "Editor/Preview",
      SettingType::Bool,
      SettingScope::User,
      false,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  registerSetting({
      "editor.preview.vsync",
      "V-Sync",
      "Enable vertical synchronization",
      "Editor/Preview",
      SettingType::Bool,
      SettingScope::User,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  // ========== Diagnostics ==========
  registerSetting({
      "editor.diagnostics.log_level",
      "Log Level",
      "Minimum severity level for logging",
      "Editor/Diagnostics",
      SettingType::Enum,
      SettingScope::User,
      std::string("info"),
      {"trace", "debug", "info", "warning", "error"},
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  registerSetting({
      "editor.diagnostics.show_performance_overlay",
      "Show Performance Overlay",
      "Display performance metrics overlay",
      "Editor/Diagnostics",
      SettingType::Bool,
      SettingScope::User,
      false,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });
}

void NMSettingsRegistry::registerProjectDefaults() {
  // ========== Project Info ==========
  registerSetting({
      "project.name",
      "Project Name",
      "Name of the visual novel project",
      "Project/General",
      SettingType::String,
      SettingScope::Project,
      std::string("My Visual Novel"),
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  registerSetting({
      "project.author",
      "Author",
      "Project author/creator",
      "Project/General",
      SettingType::String,
      SettingScope::Project,
      std::string(""),
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  // ========== Localization ==========
  registerSetting({
      "project.localization.default_locale",
      "Default Locale",
      "Default language for the project",
      "Project/Localization",
      SettingType::Enum,
      SettingScope::Project,
      std::string("en"),
      {"en", "ja", "zh", "ko", "ru", "fr", "de", "es"},
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  registerSetting({
      "project.localization.fallback_locale",
      "Fallback Locale",
      "Language to use when translation is missing",
      "Project/Localization",
      SettingType::Enum,
      SettingScope::Project,
      std::string("en"),
      {"en", "ja", "zh", "ko", "ru", "fr", "de", "es"},
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  // ========== Audio ==========
  registerSetting({
      "project.audio.sample_rate",
      "Sample Rate (Hz)",
      "Audio sample rate for the project",
      "Project/Audio",
      SettingType::Enum,
      SettingScope::Project,
      std::string("48000"),
      {"44100", "48000", "96000"},
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  registerSetting({
      "project.audio.channels",
      "Channels",
      "Audio channel configuration",
      "Project/Audio",
      SettingType::Enum,
      SettingScope::Project,
      std::string("stereo"),
      {"mono", "stereo"},
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  // ========== Timeline ==========
  registerSetting({
      "project.timeline.default_fps",
      "Default FPS",
      "Frames per second for timeline",
      "Project/Timeline",
      SettingType::IntRange,
      SettingScope::Project,
      60,
      {},
      24.0f,  // min
      120.0f, // max
      {},     // validator
      false,  // requiresRestart
      false,  // isAdvanced
      {}      // tags
  });

  registerSetting({
      "project.timeline.snap_enabled",
      "Enable Snapping",
      "Snap to grid in timeline",
      "Project/Timeline",
      SettingType::Bool,
      SettingScope::Project,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  registerSetting({
      "project.timeline.snap_interval",
      "Snap Interval (frames)",
      "Grid interval for timeline snapping",
      "Project/Timeline",
      SettingType::IntRange,
      SettingScope::Project,
      5,
      {},
      1.0f,  // min
      60.0f, // max
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  // ========== Build ==========
  registerSetting({
      "project.build.output_format",
      "Output Format",
      "Build output format",
      "Project/Build",
      SettingType::Enum,
      SettingScope::Project,
      std::string("standalone"),
      {"standalone", "web", "mobile"},
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  registerSetting({
      "project.build.compression",
      "Compression",
      "Enable asset compression in builds",
      "Project/Build",
      SettingType::Bool,
      SettingScope::Project,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  // ========== Script VM Security ==========
  registerSetting({
      "project.vm.security.max_stack_size",
      "Max Stack Size",
      "Maximum VM stack size (entries)",
      "Project/Script Security",
      SettingType::IntRange,
      SettingScope::Project,
      1024,
      {},
      256.0f,  // min
      8192.0f, // max
      {},      // validator
      false,   // requiresRestart
      false,   // isAdvanced
      {}       // tags
  });

  registerSetting({
      "project.vm.security.max_call_depth",
      "Max Call Depth",
      "Maximum function call depth",
      "Project/Script Security",
      SettingType::IntRange,
      SettingScope::Project,
      64,
      {},
      16.0f,  // min
      256.0f, // max
      {},     // validator
      false,  // requiresRestart
      false,  // isAdvanced
      {}      // tags
  });

  registerSetting({
      "project.vm.security.max_instructions_per_step",
      "Max Instructions Per Step",
      "Maximum instructions executed per update step",
      "Project/Script Security",
      SettingType::IntRange,
      SettingScope::Project,
      10000,
      {},
      1000.0f,   // min
      100000.0f, // max
      {},        // validator
      false,     // requiresRestart
      false,     // isAdvanced
      {}         // tags
  });

  registerSetting({
      "project.vm.security.max_string_length",
      "Max String Length",
      "Maximum string length (characters)",
      "Project/Script Security",
      SettingType::IntRange,
      SettingScope::Project,
      65536,
      {},
      1024.0f,    // min
      1048576.0f, // max
      {},         // validator
      false,      // requiresRestart
      false,      // isAdvanced
      {}          // tags
  });

  registerSetting({
      "project.vm.security.max_variables",
      "Max Variables",
      "Maximum number of variables",
      "Project/Script Security",
      SettingType::IntRange,
      SettingScope::Project,
      1024,
      {},
      128.0f,  // min
      8192.0f, // max
      {},      // validator
      false,   // requiresRestart
      false,   // isAdvanced
      {}       // tags
  });

  registerSetting({
      "project.vm.security.max_loop_iterations",
      "Max Loop Iterations",
      "Maximum loop iterations (infinite loop detection)",
      "Project/Script Security",
      SettingType::IntRange,
      SettingScope::Project,
      100000,
      {},
      10000.0f,    // min
      10000000.0f, // max
      {},          // validator
      false,       // requiresRestart
      false,       // isAdvanced
      {}           // tags
  });

  registerSetting({
      "project.vm.security.allow_native_calls",
      "Allow Native Calls",
      "Allow calling native C++ functions from scripts",
      "Project/Script Security",
      SettingType::Bool,
      SettingScope::Project,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  registerSetting({
      "project.vm.security.allow_file_access",
      "Allow File Access",
      "Allow scripts to read/write files",
      "Project/Script Security",
      SettingType::Bool,
      SettingScope::Project,
      false,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  registerSetting({
      "project.vm.security.allow_network_access",
      "Allow Network Access",
      "Allow scripts to make network requests",
      "Project/Script Security",
      SettingType::Bool,
      SettingScope::Project,
      false,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });
}

// ============================================================================
// Helper Functions
// ============================================================================

std::string settingValueToString(const SettingValue &value) {
  return std::visit(
      [](auto &&arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, bool>) {
          return arg ? "true" : "false";
        } else if constexpr (std::is_same_v<T, i32>) {
          return std::to_string(arg);
        } else if constexpr (std::is_same_v<T, f32>) {
          return std::to_string(arg);
        } else if constexpr (std::is_same_v<T, std::string>) {
          return arg;
        } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
          std::string result = "[";
          for (size_t i = 0; i < arg.size(); ++i) {
            if (i > 0)
              result += ", ";
            result += arg[i];
          }
          result += "]";
          return result;
        }
        return "";
      },
      value);
}

std::optional<SettingValue> stringToSettingValue(const std::string &str,
                                                 SettingType type) {
  try {
    switch (type) {
    case SettingType::Bool:
      return (str == "true" || str == "1");
    case SettingType::Int:
    case SettingType::IntRange:
      return std::stoi(str);
    case SettingType::Float:
    case SettingType::FloatRange:
      return std::stof(str);
    case SettingType::String:
    case SettingType::Enum:
    case SettingType::Path:
    case SettingType::Color:
    case SettingType::Hotkey:
      return str;
    }
  } catch (...) {
    return std::nullopt;
  }
  return std::nullopt;
}

const char *settingTypeToString(SettingType type) {
  switch (type) {
  case SettingType::Bool:
    return "Bool";
  case SettingType::Int:
    return "Int";
  case SettingType::Float:
    return "Float";
  case SettingType::String:
    return "String";
  case SettingType::Enum:
    return "Enum";
  case SettingType::Path:
    return "Path";
  case SettingType::Color:
    return "Color";
  case SettingType::Hotkey:
    return "Hotkey";
  case SettingType::FloatRange:
    return "Float Range";
  case SettingType::IntRange:
    return "Int Range";
  }
  return "Unknown";
}

const char *settingScopeToString(SettingScope scope) {
  switch (scope) {
  case SettingScope::User:
    return "User (Editor Preferences)";
  case SettingScope::Project:
    return "Project Settings";
  }
  return "Unknown";
}

} // namespace NovelMind::editor
