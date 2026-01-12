/**
 * @file settings_validation.cpp
 * @brief Value validation for settings
 */

#include "NovelMind/editor/settings_validation.hpp"
#include <algorithm>
#include <string>
#include <variant>

namespace NovelMind::editor {

std::string SettingsValidation::validateValue(const std::string &key,
                                              const SettingValue &value,
                                              const SettingDefinition &definition) {
  // Type-specific validation
  if (definition.type == SettingType::Int ||
      definition.type == SettingType::IntRange) {
    try {
      i32 intVal = std::get<i32>(value);
      if (definition.type == SettingType::IntRange) {
        if (intVal < static_cast<i32>(definition.minValue) ||
            intVal > static_cast<i32>(definition.maxValue)) {
          return "Value out of range [" +
                 std::to_string(static_cast<i32>(definition.minValue)) + ", " +
                 std::to_string(static_cast<i32>(definition.maxValue)) + "]";
        }
      }
    } catch (...) {
      return "Invalid type (expected Int)";
    }
  } else if (definition.type == SettingType::Float ||
             definition.type == SettingType::FloatRange) {
    try {
      f32 floatVal = std::get<f32>(value);
      if (definition.type == SettingType::FloatRange) {
        if (floatVal < definition.minValue || floatVal > definition.maxValue) {
          return "Value out of range [" + std::to_string(definition.minValue) +
                 ", " + std::to_string(definition.maxValue) + "]";
        }
      }
    } catch (...) {
      return "Invalid type (expected Float)";
    }
  } else if (definition.type == SettingType::Bool) {
    try {
      std::get<bool>(value);
    } catch (...) {
      return "Invalid type (expected Bool)";
    }
  } else if (definition.type == SettingType::String ||
             definition.type == SettingType::Path ||
             definition.type == SettingType::Enum) {
    try {
      std::string strVal = std::get<std::string>(value);
      if (definition.type == SettingType::Enum) {
        // Check if value is in enum options
        if (std::find(definition.enumOptions.begin(),
                      definition.enumOptions.end(),
                      strVal) == definition.enumOptions.end()) {
          return "Invalid enum value";
        }
      }
    } catch (...) {
      return "Invalid type (expected String)";
    }
  }

  // Custom validator
  if (definition.validator) {
    return definition.validator(value);
  }

  return ""; // Valid
}

} // namespace NovelMind::editor
