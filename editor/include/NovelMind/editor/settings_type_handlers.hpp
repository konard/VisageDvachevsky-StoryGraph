#pragma once

/**
 * @file settings_type_handlers.hpp
 * @brief Type serialization and conversion for settings values
 */

#include "NovelMind/editor/settings_registry.hpp"
#include <optional>
#include <string>

namespace NovelMind::editor {

/**
 * @brief Convert SettingValue to string (for display)
 */
std::string settingValueToString(const SettingValue &value);

/**
 * @brief Convert string to SettingValue (for parsing)
 */
std::optional<SettingValue> stringToSettingValue(const std::string &str,
                                                 SettingType type);

/**
 * @brief Get display name for SettingType
 */
const char *settingTypeToString(SettingType type);

/**
 * @brief Get display name for SettingScope
 */
const char *settingScopeToString(SettingScope scope);

// JSON serialization helpers (internal use)
namespace detail {

/**
 * @brief Escape special characters for JSON strings
 */
std::string escapeJson(const std::string &str);

/**
 * @brief Unescape JSON escape sequences
 */
std::string unescapeJson(const std::string &str);

} // namespace detail

} // namespace NovelMind::editor
