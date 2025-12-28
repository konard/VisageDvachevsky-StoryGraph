#pragma once

/**
 * @file game_settings.hpp
 * @brief In-Game Settings Data Layer
 *
 * Provides a settings data management system for visual novels with:
 * - Video settings (resolution, fullscreen, vsync)
 * - Audio settings (volume sliders)
 * - Text settings (speed, auto-advance)
 * - Language selection
 * - Input remapping
 * - Save/Load settings persistence
 *
 * IMPORTANT: This is a DATA-LAYER module, NOT a visual UI component.
 * It provides the backend for settings management that can be integrated
 * with any renderer/UI system (Qt, ImGui, SDL, etc.).
 *
 * For visual rendering, integrate with:
 * - Editor: Uses Qt-based settings panels
 * - Runtime: Uses renderer layer with NovelMind::renderer::* components
 * - Custom: Implement ISettingsView interface for your UI framework
 *
 * The GameSettings class:
 * - Manages setting items (SettingItem) as data structures
 * - Tracks pending changes before Apply
 * - Syncs with ConfigManager for persistence
 * - Provides callbacks for change notification to UI layers
 */

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include "NovelMind/runtime/config_manager.hpp"
#include "NovelMind/runtime/runtime_config.hpp"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace NovelMind::runtime {

/**
 * @brief Settings category
 */
enum class SettingsCategory {
  Video,
  Audio,
  Text,
  Language,
  Input,
  Accessibility
};

/**
 * @brief Setting item type
 */
enum class SettingType {
  Toggle, // On/Off switch
  Slider, // Value slider (0-100)
  Choice, // Dropdown selection
  Key,    // Key binding
  Button, // Action button (Apply, Reset)
  Label   // Information text
};

/**
 * @brief A single setting item
 */
struct SettingItem {
  std::string id;
  std::string label;
  std::string description;
  SettingType type;
  SettingsCategory category;

  // Current value (depending on type)
  bool boolValue = false;
  f32 floatValue = 0.0f;
  i32 intValue = 0;
  std::string stringValue;

  // For sliders
  f32 minValue = 0.0f;
  f32 maxValue = 1.0f;
  f32 step = 0.1f;

  // For choices
  std::vector<std::string> choices;
  i32 selectedChoice = 0;

  // For key bindings
  InputAction action = InputAction::Next;
  InputBinding binding;
};

/**
 * @brief Settings change event
 */
struct SettingsChangeEvent {
  std::string settingId;
  SettingItem oldValue;
  SettingItem newValue;
};

using OnSettingsChanged = std::function<void(const SettingsChangeEvent &)>;
using OnSettingsApplied = std::function<void()>;
using OnSettingsReset = std::function<void()>;

/**
 * @brief Game Settings Manager
 *
 * Manages the in-game settings interface. This class:
 * - Builds the settings UI structure
 * - Tracks pending changes (before Apply)
 * - Applies changes to ConfigManager
 * - Notifies listeners of changes
 *
 * Typical usage:
 * @code
 * GameSettings settings(&configManager);
 * settings.initialize();
 *
 * // User changes a setting in UI
 * settings.setSettingValue("master_volume", 0.8f);
 *
 * // User clicks Apply
 * settings.applyChanges();
 * @endcode
 */
class GameSettings {
public:
  explicit GameSettings(ConfigManager *configManager);
  ~GameSettings();

  // Non-copyable
  GameSettings(const GameSettings &) = delete;
  GameSettings &operator=(const GameSettings &) = delete;

  /**
   * @brief Initialize settings from current config
   */
  Result<void> initialize();

  // =========================================================================
  // Category Access
  // =========================================================================

  /**
   * @brief Get all items in a category
   */
  [[nodiscard]] std::vector<SettingItem>
  getItemsInCategory(SettingsCategory category) const;

  /**
   * @brief Get all categories
   */
  [[nodiscard]] std::vector<SettingsCategory> getCategories() const;

  /**
   * @brief Get category display name
   */
  [[nodiscard]] static std::string getCategoryName(SettingsCategory category);

  // =========================================================================
  // Setting Access
  // =========================================================================

  /**
   * @brief Get a specific setting by ID
   */
  [[nodiscard]] const SettingItem *getSetting(const std::string &id) const;

  /**
   * @brief Get all settings
   */
  [[nodiscard]] const std::vector<SettingItem> &getAllSettings() const;

  // =========================================================================
  // Setting Modification
  // =========================================================================

  /**
   * @brief Set a boolean setting value
   */
  void setBoolValue(const std::string &id, bool value);

  /**
   * @brief Set a float setting value (for sliders)
   */
  void setFloatValue(const std::string &id, f32 value);

  /**
   * @brief Set an integer setting value
   */
  void setIntValue(const std::string &id, i32 value);

  /**
   * @brief Set a string setting value
   */
  void setStringValue(const std::string &id, const std::string &value);

  /**
   * @brief Set choice selection
   */
  void setChoice(const std::string &id, i32 choiceIndex);

  /**
   * @brief Set key binding
   */
  void setKeyBinding(const std::string &id, const InputBinding &binding);

  // =========================================================================
  // Change Management
  // =========================================================================

  /**
   * @brief Check if there are pending changes
   */
  [[nodiscard]] bool hasPendingChanges() const { return m_hasPendingChanges; }

  /**
   * @brief Apply all pending changes to config
   */
  Result<void> applyChanges();

  /**
   * @brief Discard pending changes and revert to saved config
   */
  void discardChanges();

  /**
   * @brief Reset all settings to defaults
   */
  void resetToDefaults();

  /**
   * @brief Reset a specific category to defaults
   */
  void resetCategoryToDefaults(SettingsCategory category);

  // =========================================================================
  // Callbacks
  // =========================================================================

  void setOnSettingsChanged(OnSettingsChanged callback);
  void setOnSettingsApplied(OnSettingsApplied callback);
  void setOnSettingsReset(OnSettingsReset callback);

  // =========================================================================
  // Utility
  // =========================================================================

  /**
   * @brief Get available resolutions
   */
  [[nodiscard]] std::vector<std::pair<i32, i32>>
  getAvailableResolutions() const;

  /**
   * @brief Get available languages
   */
  [[nodiscard]] std::vector<std::string> getAvailableLanguages() const;

  /**
   * @brief Format volume as percentage string
   */
  [[nodiscard]] static std::string formatVolume(f32 volume);

  /**
   * @brief Format resolution as string
   */
  [[nodiscard]] static std::string formatResolution(i32 width, i32 height);

private:
  /**
   * @brief Build the settings items from config
   */
  void buildSettingsItems();

  /**
   * @brief Sync settings from config values
   */
  void syncFromConfig();

  /**
   * @brief Sync settings to config values
   */
  void syncToConfig();

  /**
   * @brief Find setting index by ID
   */
  [[nodiscard]] i32 findSettingIndex(const std::string &id) const;

  /**
   * @brief Notify setting changed
   */
  void notifySettingChanged(const std::string &id, const SettingItem &oldValue);

  ConfigManager *m_configManager;
  std::vector<SettingItem> m_settings;
  bool m_hasPendingChanges = false;

  OnSettingsChanged m_onSettingsChanged;
  OnSettingsApplied m_onSettingsApplied;
  OnSettingsReset m_onSettingsReset;
};

} // namespace NovelMind::runtime
