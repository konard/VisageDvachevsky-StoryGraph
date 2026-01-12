#pragma once

/**
 * @file settings_validation.hpp
 * @brief Value validation for settings
 */

#include "NovelMind/editor/settings_registry.hpp"
#include <string>

namespace NovelMind::editor {

/**
 * @brief Handles validation of setting values
 */
class SettingsValidation {
public:
  /**
   * @brief Validate a setting value against its definition
   * @param key Setting key
   * @param value Value to validate
   * @param definition Setting definition with validation rules
   * @return Empty string if valid, error message if invalid
   */
  static std::string validateValue(const std::string &key,
                                   const SettingValue &value,
                                   const SettingDefinition &definition);
};

} // namespace NovelMind::editor
