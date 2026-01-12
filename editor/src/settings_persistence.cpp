/**
 * @file settings_persistence.cpp
 * @brief File I/O operations for settings persistence
 */

#include "NovelMind/editor/settings_persistence.hpp"
#include "NovelMind/core/logger.hpp"
#include "NovelMind/editor/settings_type_handlers.hpp"
#include <filesystem>
#include <fstream>
#include <string>

namespace NovelMind::editor {

Result<void> SettingsPersistence::loadFromJson(
    const std::string &path, SettingScope scope,
    const std::unordered_map<std::string, SettingDefinition> &definitions,
    std::unordered_map<std::string, SettingValue> &values,
    i32 &schemaVersion) {
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
        auto defIt = definitions.find(key);
        if (defIt != definitions.end() && defIt->second.scope == scope) {
          const auto &def = defIt->second;

          // Parse value based on type
          if (def.type == SettingType::Bool) {
            bool val = (valueStr == "true");
            values[key] = val;
          } else if (def.type == SettingType::Int ||
                     def.type == SettingType::IntRange) {
            i32 val = std::stoi(valueStr);
            values[key] = val;
          } else if (def.type == SettingType::Float ||
                     def.type == SettingType::FloatRange) {
            f32 val = std::stof(valueStr);
            values[key] = val;
          } else {
            // String type - remove quotes
            if (valueStr.size() >= 2 && valueStr.front() == '"' &&
                valueStr.back() == '"') {
              valueStr = valueStr.substr(1, valueStr.size() - 2);
            }
            values[key] = detail::unescapeJson(valueStr);
          }
        }
      }
    }

    schemaVersion = version;

    NOVELMIND_LOG_INFO(std::string("Loaded settings from: ") + path);
    return Result<void>::ok();
  } catch (const std::exception &e) {
    return Result<void>::error(std::string("Failed to load settings: ") +
                               e.what());
  }
}

Result<void> SettingsPersistence::saveToJson(
    const std::string &path, SettingScope scope,
    const std::unordered_map<std::string, SettingDefinition> &definitions,
    const std::unordered_map<std::string, SettingValue> &values,
    i32 schemaVersion) {
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
    file << "  \"settings_version\": " << schemaVersion << ",\n";
    file << "  \"settings\": {\n";

    bool first = true;
    for (const auto &[key, def] : definitions) {
      if (def.scope != scope)
        continue;

      auto valueIt = values.find(key);
      if (valueIt == values.end())
        continue;

      if (!first)
        file << ",\n";
      first = false;

      file << "    \"" << detail::escapeJson(key) << "\": ";

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
        file << "\"" << detail::escapeJson(std::get<std::string>(value)) << "\"";
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

} // namespace NovelMind::editor
