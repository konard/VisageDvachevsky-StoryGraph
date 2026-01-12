/**
 * @file settings_type_handlers.cpp
 * @brief Type serialization and conversion for settings values
 */

#include "NovelMind/editor/settings_type_handlers.hpp"
#include "NovelMind/core/types.hpp"
#include <string>
#include <variant>
#include <vector>

namespace NovelMind::editor {

// ============================================================================
// JSON Serialization Helpers
// ============================================================================

namespace detail {

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

} // namespace detail

// ============================================================================
// Type Conversion Functions
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
