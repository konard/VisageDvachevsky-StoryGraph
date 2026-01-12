#pragma once

/**
 * @file config_manager.hpp
 * @brief Configuration Manager - Load/Save runtime configuration
 *
 * Handles:
 * - Loading runtime_config.json (base configuration)
 * - Loading/Saving runtime_user.json (user overrides)
 * - Merging configurations with proper precedence
 * - JSON serialization/deserialization
 * - Directory creation for saves/logs
 */

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include "NovelMind/runtime/runtime_config.hpp"
#include <functional>
#include <string>

namespace NovelMind::runtime {

/**
 * @brief Callback for configuration changes
 */
using ConfigChangeCallback = std::function<void(const RuntimeConfig&)>;

/**
 * @brief Configuration Manager
 *
 * Manages loading and saving of runtime configuration files.
 * Implements a layered configuration system:
 * 1. Defaults (built-in)
 * 2. runtime_config.json (game-specific, read-only)
 * 3. runtime_user.json (user overrides, read-write)
 *
 * The manager ensures:
 * - User settings persist across sessions
 * - Original config files are never modified
 * - Required directories are created automatically
 */
class ConfigManager {
public:
  ConfigManager();
  ~ConfigManager();

  // Non-copyable
  ConfigManager(const ConfigManager&) = delete;
  ConfigManager& operator=(const ConfigManager&) = delete;

  /**
   * @brief Initialize with a base directory
   * @param basePath Path to game's root directory (where config/ is located)
   * @return Success or error message
   */
  Result<void> initialize(const std::string& basePath);

  /**
   * @brief Load configuration from files
   *
   * Loads in order:
   * 1. Built-in defaults
   * 2. config/runtime_config.json (if exists)
   * 3. config/runtime_user.json (if exists)
   *
   * @return Success or error message
   */
  Result<void> loadConfig();

  /**
   * @brief Save user settings to runtime_user.json
   * @return Success or error message
   */
  Result<void> saveUserConfig();

  /**
   * @brief Get the current merged configuration
   */
  [[nodiscard]] const RuntimeConfig& getConfig() const { return m_config; }

  /**
   * @brief Get mutable configuration for modifications
   */
  RuntimeConfig& getConfigMutable() { return m_config; }

  /**
   * @brief Reset configuration to defaults
   */
  void resetToDefaults();

  /**
   * @brief Reset only user settings (keeps base config)
   */
  void resetUserSettings();

  /**
   * @brief Set callback for configuration changes
   */
  void setOnConfigChanged(ConfigChangeCallback callback);

  /**
   * @brief Notify listeners that config has changed
   */
  void notifyConfigChanged();

  // =========================================================================
  // Directory Management
  // =========================================================================

  /**
   * @brief Ensure required directories exist
   *
   * Creates:
   * - saves/
   * - logs/
   * - config/
   *
   * @return Success or error message
   */
  Result<void> ensureDirectories();

  /**
   * @brief Get the base path
   */
  [[nodiscard]] const std::string& getBasePath() const { return m_basePath; }

  /**
   * @brief Get the config directory path
   */
  [[nodiscard]] std::string getConfigPath() const;

  /**
   * @brief Get the saves directory path
   */
  [[nodiscard]] std::string getSavesPath() const;

  /**
   * @brief Get the logs directory path
   */
  [[nodiscard]] std::string getLogsPath() const;

  // =========================================================================
  // Individual Setting Accessors (convenience methods)
  // =========================================================================

  // Window
  void setFullscreen(bool fullscreen);
  void setResolution(i32 width, i32 height);
  void setVsync(bool vsync);

  // Audio
  void setMasterVolume(f32 volume);
  void setMusicVolume(f32 volume);
  void setVoiceVolume(f32 volume);
  void setSfxVolume(f32 volume);
  void setMuted(bool muted);

  // Text
  void setTextSpeed(i32 speed);
  void setAutoAdvanceDelay(i32 delayMs);
  void setTypewriterEnabled(bool enabled);
  void setAutoAdvanceEnabled(bool enabled);

  // Localization
  void setLocale(const std::string& locale);

  // Input
  void setInputBinding(InputAction action, const InputBinding& binding);
  [[nodiscard]] const InputBinding& getInputBinding(InputAction action) const;

private:
  /**
   * @brief Load configuration from a JSON file
   */
  Result<void> loadFromFile(const std::string& path, bool isUserConfig);

  /**
   * @brief Parse JSON string into RuntimeConfig
   */
  Result<void> parseJson(const std::string& json, RuntimeConfig& config, bool isUserConfig);

  /**
   * @brief Serialize RuntimeConfig to JSON string
   */
  [[nodiscard]] std::string serializeToJson(const RuntimeConfig& config,
                                            bool userSettingsOnly) const;

  /**
   * @brief Merge user config into base config
   */
  void mergeConfig(const RuntimeConfig& userConfig);

  std::string m_basePath;
  RuntimeConfig m_config;
  RuntimeConfig m_baseConfig; // Original from runtime_config.json
  ConfigChangeCallback m_onConfigChanged;
  bool m_initialized = false;
};

} // namespace NovelMind::runtime
