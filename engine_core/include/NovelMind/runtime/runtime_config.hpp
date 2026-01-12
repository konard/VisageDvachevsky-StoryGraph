#pragma once

/**
 * @file runtime_config.hpp
 * @brief Runtime Configuration - Settings for game runtime
 *
 * Provides comprehensive runtime configuration for:
 * - Game metadata (name, version, build number)
 * - Window settings (resolution, fullscreen, vsync)
 * - Audio settings (volume levels for each channel)
 * - Text settings (speed, auto-advance, typewriter)
 * - Localization settings (locale, available languages)
 * - Resource pack settings (paths, encryption)
 * - Logging settings (level, file output)
 * - Input bindings (keyboard/mouse mappings)
 */

#include "NovelMind/core/types.hpp"
#include <map>
#include <string>
#include <vector>

namespace NovelMind::runtime {

/**
 * @brief Game information section
 */
struct GameInfo {
  std::string name = "NovelMind Game";
  std::string version = "1.0.0";
  u32 buildNumber = 1;
};

/**
 * @brief Window configuration section
 */
struct WindowSettings {
  i32 width = 1280;
  i32 height = 720;
  bool fullscreen = false;
  bool vsync = true;
  bool resizable = false;
  bool borderless = false;
};

/**
 * @brief Audio configuration section
 */
struct AudioSettings {
  f32 master = 1.0f;
  f32 music = 0.8f;
  f32 voice = 1.0f;
  f32 sfx = 0.8f;
  f32 ambient = 0.7f;
  f32 ui = 0.6f;
  bool muted = false;
};

/**
 * @brief Text display configuration section
 */
struct TextSettings {
  i32 speed = 40;           // Characters per second
  i32 autoAdvanceMs = 1500; // Delay before auto-advance
  bool typewriter = true;   // Enable typewriter effect
  bool autoAdvance = false; // Enable auto-advance mode
  bool skipUnread = false;  // Allow skipping unread text
};

/**
 * @brief Localization configuration section
 */
struct LocalizationSettings {
  std::string defaultLocale = "en";
  std::vector<std::string> availableLocales = {"en"};
  std::string currentLocale = "en";
};

/**
 * @brief Resource pack configuration section
 */
struct PackSettings {
  std::string directory = "packs";
  std::string indexFile = "packs_index.json";
  bool encrypted = false;
  std::vector<u8> encryptionKey;
};

/**
 * @brief Save system configuration
 */
struct SaveSettings {
  std::string saveDirectory = "saves";
  bool enableCompression = true;
  bool enableEncryption = false;
  i32 maxSlots = 100;
  bool autoSaveEnabled = true;
  i32 autoSaveIntervalMs = 300000; // 5 minutes
};

/**
 * @brief Logging configuration section
 */
struct LoggingSettings {
  bool enableLogging = true;
  std::string logLevel = "info"; // trace, debug, info, warning, error, fatal
  std::string logDirectory = "logs";
  bool logToFile = true;
  bool logToConsole = true;
};

/**
 * @brief Debug/development configuration
 */
struct DebugSettings {
  bool enableDebugConsole = false;
  bool showFps = false;
  bool showDebugOverlay = false;
  bool enableHotReload = false;
};

/**
 * @brief Input action types for control mapping
 */
enum class InputAction {
  Next,       // Advance dialogue
  Backlog,    // Open backlog/history
  Skip,       // Skip mode toggle
  Auto,       // Auto-advance toggle
  QuickSave,  // Quick save
  QuickLoad,  // Quick load
  Menu,       // Open/close menu
  FullScreen, // Toggle fullscreen
  Screenshot, // Take screenshot
  HideUI      // Hide/show UI
};

/**
 * @brief Input binding for an action
 */
struct InputBinding {
  std::vector<std::string> keys;         // Keyboard keys (e.g., "Space", "Enter")
  std::vector<std::string> mouseButtons; // Mouse buttons (e.g., "Left", "Right")
};

/**
 * @brief Input configuration section
 */
struct InputSettings {
  std::map<InputAction, InputBinding> bindings;

  // Initialize default bindings
  void setDefaults() {
    bindings[InputAction::Next] = {{"Space", "Enter"}, {"Left"}};
    bindings[InputAction::Backlog] = {{"PageUp"}, {}};
    bindings[InputAction::Skip] = {{"LCtrl", "RCtrl"}, {}};
    bindings[InputAction::Auto] = {{"A"}, {}};
    bindings[InputAction::QuickSave] = {{"S"}, {}};
    bindings[InputAction::QuickLoad] = {{"L"}, {}};
    bindings[InputAction::Menu] = {{"Escape"}, {}};
    bindings[InputAction::FullScreen] = {{"F11"}, {}};
    bindings[InputAction::Screenshot] = {{"F12"}, {}};
    bindings[InputAction::HideUI] = {{"H"}, {}};
  }
};

/**
 * @brief Complete runtime configuration
 *
 * This structure contains all settings for running a visual novel game.
 * It can be loaded from runtime_config.json and user overrides from
 * runtime_user.json or saves/settings.json.
 */
struct RuntimeConfig {
  std::string version = "1.0";

  GameInfo game;
  WindowSettings window;
  AudioSettings audio;
  TextSettings text;
  LocalizationSettings localization;
  PackSettings packs;
  SaveSettings saves;
  LoggingSettings logging;
  DebugSettings debug;
  InputSettings input;

  // Initialize with defaults
  RuntimeConfig() { input.setDefaults(); }
};

/**
 * @brief Convert InputAction enum to string
 */
inline std::string inputActionToString(InputAction action) {
  switch (action) {
  case InputAction::Next:
    return "next";
  case InputAction::Backlog:
    return "backlog";
  case InputAction::Skip:
    return "skip";
  case InputAction::Auto:
    return "auto";
  case InputAction::QuickSave:
    return "quick_save";
  case InputAction::QuickLoad:
    return "quick_load";
  case InputAction::Menu:
    return "menu";
  case InputAction::FullScreen:
    return "fullscreen";
  case InputAction::Screenshot:
    return "screenshot";
  case InputAction::HideUI:
    return "hide_ui";
  }
  return "unknown";
}

/**
 * @brief Convert string to InputAction enum
 */
inline InputAction stringToInputAction(const std::string& str) {
  if (str == "next")
    return InputAction::Next;
  if (str == "backlog")
    return InputAction::Backlog;
  if (str == "skip")
    return InputAction::Skip;
  if (str == "auto")
    return InputAction::Auto;
  if (str == "quick_save")
    return InputAction::QuickSave;
  if (str == "quick_load")
    return InputAction::QuickLoad;
  if (str == "menu")
    return InputAction::Menu;
  if (str == "fullscreen")
    return InputAction::FullScreen;
  if (str == "screenshot")
    return InputAction::Screenshot;
  if (str == "hide_ui")
    return InputAction::HideUI;
  return InputAction::Next; // Default
}

} // namespace NovelMind::runtime
