/**
 * @file config_manager.cpp
 * @brief Configuration Manager implementation
 */

#include "NovelMind/runtime/config_manager.hpp"
#include "NovelMind/core/logger.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace NovelMind::runtime {

// Simple JSON parser helper (minimal implementation)
namespace json {

inline std::string trim(const std::string& s) {
  auto start = s.find_first_not_of(" \t\n\r");
  if (start == std::string::npos)
    return "";
  auto end = s.find_last_not_of(" \t\n\r");
  return s.substr(start, end - start + 1);
}

inline std::string extractString(const std::string& json, const std::string& key) {
  auto keyPos = json.find("\"" + key + "\"");
  if (keyPos == std::string::npos)
    return "";

  auto colonPos = json.find(':', keyPos);
  if (colonPos == std::string::npos)
    return "";

  auto valueStart = json.find('"', colonPos);
  if (valueStart == std::string::npos)
    return "";

  auto valueEnd = json.find('"', valueStart + 1);
  if (valueEnd == std::string::npos)
    return "";

  return json.substr(valueStart + 1, valueEnd - valueStart - 1);
}

inline f32 extractFloat(const std::string& json, const std::string& key, f32 defaultVal) {
  auto keyPos = json.find("\"" + key + "\"");
  if (keyPos == std::string::npos)
    return defaultVal;

  auto colonPos = json.find(':', keyPos);
  if (colonPos == std::string::npos)
    return defaultVal;

  auto valueStart = json.find_first_not_of(" \t\n\r", colonPos + 1);
  if (valueStart == std::string::npos)
    return defaultVal;

  try {
    return std::stof(json.substr(valueStart));
  } catch (...) {
    return defaultVal;
  }
}

inline i32 extractInt(const std::string& json, const std::string& key, i32 defaultVal) {
  auto keyPos = json.find("\"" + key + "\"");
  if (keyPos == std::string::npos)
    return defaultVal;

  auto colonPos = json.find(':', keyPos);
  if (colonPos == std::string::npos)
    return defaultVal;

  auto valueStart = json.find_first_not_of(" \t\n\r", colonPos + 1);
  if (valueStart == std::string::npos)
    return defaultVal;

  try {
    return std::stoi(json.substr(valueStart));
  } catch (...) {
    return defaultVal;
  }
}

inline bool extractBool(const std::string& json, const std::string& key, bool defaultVal) {
  auto keyPos = json.find("\"" + key + "\"");
  if (keyPos == std::string::npos)
    return defaultVal;

  auto colonPos = json.find(':', keyPos);
  if (colonPos == std::string::npos)
    return defaultVal;

  auto valueStart = json.find_first_not_of(" \t\n\r", colonPos + 1);
  if (valueStart == std::string::npos)
    return defaultVal;

  auto remaining = json.substr(valueStart, 5);
  if (remaining.find("true") == 0)
    return true;
  if (remaining.find("false") == 0)
    return false;
  return defaultVal;
}

inline std::string extractObject(const std::string& json, const std::string& key) {
  auto keyPos = json.find("\"" + key + "\"");
  if (keyPos == std::string::npos)
    return "";

  auto braceStart = json.find('{', keyPos);
  if (braceStart == std::string::npos)
    return "";

  int depth = 1;
  size_t pos = braceStart + 1;
  while (pos < json.size() && depth > 0) {
    if (json[pos] == '{')
      depth++;
    else if (json[pos] == '}')
      depth--;
    pos++;
  }

  return json.substr(braceStart, pos - braceStart);
}

inline std::vector<std::string> extractStringArray(const std::string& json,
                                                   const std::string& key) {
  std::vector<std::string> result;

  auto keyPos = json.find("\"" + key + "\"");
  if (keyPos == std::string::npos)
    return result;

  auto bracketStart = json.find('[', keyPos);
  if (bracketStart == std::string::npos)
    return result;

  auto bracketEnd = json.find(']', bracketStart);
  if (bracketEnd == std::string::npos)
    return result;

  std::string arrayContent = json.substr(bracketStart + 1, bracketEnd - bracketStart - 1);

  size_t pos = 0;
  while ((pos = arrayContent.find('"', pos)) != std::string::npos) {
    auto end = arrayContent.find('"', pos + 1);
    if (end != std::string::npos) {
      result.push_back(arrayContent.substr(pos + 1, end - pos - 1));
      pos = end + 1;
    } else {
      break;
    }
  }

  return result;
}

} // namespace json

ConfigManager::ConfigManager() = default;
ConfigManager::~ConfigManager() = default;

Result<void> ConfigManager::initialize(const std::string& basePath) {
  m_basePath = basePath;

  // Normalize path
  if (!m_basePath.empty() && m_basePath.back() != '/' && m_basePath.back() != '\\') {
    m_basePath += '/';
  }

  // Ensure directories exist
  auto dirResult = ensureDirectories();
  if (dirResult.isError()) {
    return dirResult;
  }

  m_initialized = true;
  return Result<void>::ok();
}

Result<void> ConfigManager::loadConfig() {
  if (!m_initialized) {
    return Result<void>::error("ConfigManager not initialized");
  }

  // Start with defaults
  m_config = RuntimeConfig();
  m_baseConfig = RuntimeConfig();

  // Try to load base config
  std::string baseConfigPath = getConfigPath() + "runtime_config.json";
  auto baseResult = loadFromFile(baseConfigPath, false);
  if (baseResult.isError()) {
    NOVELMIND_LOG_WARN("Could not load runtime_config.json: " + baseResult.error() +
                       " - using defaults");
  } else {
    m_baseConfig = m_config;
  }

  // Try to load user config
  std::string userConfigPath = getConfigPath() + "runtime_user.json";
  auto userResult = loadFromFile(userConfigPath, true);
  if (userResult.isError()) {
    NOVELMIND_LOG_INFO("No user config found, using base config");
  }

  NOVELMIND_LOG_INFO("Configuration loaded successfully");
  return Result<void>::ok();
}

Result<void> ConfigManager::saveUserConfig() {
  if (!m_initialized) {
    return Result<void>::error("ConfigManager not initialized");
  }

  std::string userConfigPath = getConfigPath() + "runtime_user.json";
  std::string json = serializeToJson(m_config, true);

  try {
    // Create parent directory if needed
    fs::create_directories(getConfigPath());

    // Write atomically (write to temp, then rename)
    std::string tempPath = userConfigPath + ".tmp";
    std::ofstream file(tempPath);
    if (!file.is_open()) {
      return Result<void>::error("Cannot open file for writing: " + userConfigPath);
    }

    file << json;
    file.close();

    // Rename temp to final
    fs::rename(tempPath, userConfigPath);

    NOVELMIND_LOG_INFO("User configuration saved to " + userConfigPath);
    return Result<void>::ok();

  } catch (const std::exception& e) {
    return Result<void>::error(std::string("Failed to save config: ") + e.what());
  }
}

void ConfigManager::resetToDefaults() {
  m_config = RuntimeConfig();
  notifyConfigChanged();
}

void ConfigManager::resetUserSettings() {
  m_config = m_baseConfig;
  notifyConfigChanged();
}

void ConfigManager::setOnConfigChanged(ConfigChangeCallback callback) {
  m_onConfigChanged = std::move(callback);
}

void ConfigManager::notifyConfigChanged() {
  if (m_onConfigChanged) {
    m_onConfigChanged(m_config);
  }
}

Result<void> ConfigManager::ensureDirectories() {
  try {
    fs::create_directories(getConfigPath());
    fs::create_directories(getSavesPath());
    fs::create_directories(getLogsPath());
    return Result<void>::ok();
  } catch (const std::exception& e) {
    return Result<void>::error(std::string("Failed to create directories: ") + e.what());
  }
}

std::string ConfigManager::getConfigPath() const {
  return m_basePath + "config/";
}

std::string ConfigManager::getSavesPath() const {
  return m_basePath + m_config.saves.saveDirectory + "/";
}

std::string ConfigManager::getLogsPath() const {
  return m_basePath + m_config.logging.logDirectory + "/";
}

// Convenience setters
void ConfigManager::setFullscreen(bool fullscreen) {
  m_config.window.fullscreen = fullscreen;
  notifyConfigChanged();
}

void ConfigManager::setResolution(i32 width, i32 height) {
  m_config.window.width = width;
  m_config.window.height = height;
  notifyConfigChanged();
}

void ConfigManager::setVsync(bool vsync) {
  m_config.window.vsync = vsync;
  notifyConfigChanged();
}

void ConfigManager::setMasterVolume(f32 volume) {
  m_config.audio.master = std::clamp(volume, 0.0f, 1.0f);
  notifyConfigChanged();
}

void ConfigManager::setMusicVolume(f32 volume) {
  m_config.audio.music = std::clamp(volume, 0.0f, 1.0f);
  notifyConfigChanged();
}

void ConfigManager::setVoiceVolume(f32 volume) {
  m_config.audio.voice = std::clamp(volume, 0.0f, 1.0f);
  notifyConfigChanged();
}

void ConfigManager::setSfxVolume(f32 volume) {
  m_config.audio.sfx = std::clamp(volume, 0.0f, 1.0f);
  notifyConfigChanged();
}

void ConfigManager::setMuted(bool muted) {
  m_config.audio.muted = muted;
  notifyConfigChanged();
}

void ConfigManager::setTextSpeed(i32 speed) {
  m_config.text.speed = std::clamp(speed, 1, 200);
  notifyConfigChanged();
}

void ConfigManager::setAutoAdvanceDelay(i32 delayMs) {
  m_config.text.autoAdvanceMs = std::clamp(delayMs, 100, 10000);
  notifyConfigChanged();
}

void ConfigManager::setTypewriterEnabled(bool enabled) {
  m_config.text.typewriter = enabled;
  notifyConfigChanged();
}

void ConfigManager::setAutoAdvanceEnabled(bool enabled) {
  m_config.text.autoAdvance = enabled;
  notifyConfigChanged();
}

void ConfigManager::setLocale(const std::string& locale) {
  m_config.localization.currentLocale = locale;
  notifyConfigChanged();
}

void ConfigManager::setInputBinding(InputAction action, const InputBinding& binding) {
  m_config.input.bindings[action] = binding;
  notifyConfigChanged();
}

const InputBinding& ConfigManager::getInputBinding(InputAction action) const {
  static InputBinding empty;
  auto it = m_config.input.bindings.find(action);
  if (it != m_config.input.bindings.end()) {
    return it->second;
  }
  return empty;
}

Result<void> ConfigManager::loadFromFile(const std::string& path, bool isUserConfig) {
  if (!fs::exists(path)) {
    return Result<void>::error("File not found: " + path);
  }

  try {
    std::ifstream file(path);
    if (!file.is_open()) {
      return Result<void>::error("Cannot open file: " + path);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    return parseJson(content, m_config, isUserConfig);

  } catch (const std::exception& e) {
    return Result<void>::error(std::string("Failed to read file: ") + e.what());
  }
}

Result<void> ConfigManager::parseJson(const std::string& jsonStr, RuntimeConfig& config,
                                      [[maybe_unused]] bool isUserConfig) {
  // Parse version
  std::string version = json::extractString(jsonStr, "version");
  if (!version.empty()) {
    config.version = version;
  }

  // Parse game info
  std::string gameObj = json::extractObject(jsonStr, "game");
  if (!gameObj.empty()) {
    std::string name = json::extractString(gameObj, "name");
    if (!name.empty())
      config.game.name = name;

    std::string ver = json::extractString(gameObj, "version");
    if (!ver.empty())
      config.game.version = ver;

    i32 buildNum = json::extractInt(gameObj, "build_number", -1);
    if (buildNum >= 0)
      config.game.buildNumber = static_cast<u32>(buildNum);
  }

  // Parse window settings
  std::string windowObj = json::extractObject(jsonStr, "window");
  if (!windowObj.empty()) {
    i32 width = json::extractInt(windowObj, "width", -1);
    if (width > 0)
      config.window.width = width;

    i32 height = json::extractInt(windowObj, "height", -1);
    if (height > 0)
      config.window.height = height;

    config.window.fullscreen = json::extractBool(windowObj, "fullscreen", config.window.fullscreen);
    config.window.vsync = json::extractBool(windowObj, "vsync", config.window.vsync);
    config.window.resizable = json::extractBool(windowObj, "resizable", config.window.resizable);
    config.window.borderless = json::extractBool(windowObj, "borderless", config.window.borderless);
  }

  // Parse audio settings
  std::string audioObj = json::extractObject(jsonStr, "audio");
  if (!audioObj.empty()) {
    config.audio.master = json::extractFloat(audioObj, "master", config.audio.master);
    config.audio.music = json::extractFloat(audioObj, "music", config.audio.music);
    config.audio.voice = json::extractFloat(audioObj, "voice", config.audio.voice);
    config.audio.sfx = json::extractFloat(audioObj, "sfx", config.audio.sfx);
    config.audio.ambient = json::extractFloat(audioObj, "ambient", config.audio.ambient);
    config.audio.ui = json::extractFloat(audioObj, "ui", config.audio.ui);
    config.audio.muted = json::extractBool(audioObj, "muted", config.audio.muted);
  }

  // Parse text settings
  std::string textObj = json::extractObject(jsonStr, "text");
  if (!textObj.empty()) {
    config.text.speed = json::extractInt(textObj, "speed", config.text.speed);
    config.text.autoAdvanceMs =
        json::extractInt(textObj, "auto_advance_ms", config.text.autoAdvanceMs);
    config.text.typewriter = json::extractBool(textObj, "typewriter", config.text.typewriter);
    config.text.autoAdvance = json::extractBool(textObj, "auto_advance", config.text.autoAdvance);
    config.text.skipUnread = json::extractBool(textObj, "skip_unread", config.text.skipUnread);
  }

  // Parse localization settings
  std::string locObj = json::extractObject(jsonStr, "localization");
  if (!locObj.empty()) {
    std::string defLocale = json::extractString(locObj, "default_locale");
    if (!defLocale.empty())
      config.localization.defaultLocale = defLocale;

    std::string curLocale = json::extractString(locObj, "current_locale");
    if (!curLocale.empty())
      config.localization.currentLocale = curLocale;

    auto locales = json::extractStringArray(locObj, "available_locales");
    if (!locales.empty())
      config.localization.availableLocales = locales;
  }

  // Parse pack settings
  std::string packsObj = json::extractObject(jsonStr, "packs");
  if (!packsObj.empty()) {
    std::string dir = json::extractString(packsObj, "directory");
    if (!dir.empty())
      config.packs.directory = dir;

    std::string indexFile = json::extractString(packsObj, "index_file");
    if (!indexFile.empty())
      config.packs.indexFile = indexFile;

    config.packs.encrypted = json::extractBool(packsObj, "encrypted", config.packs.encrypted);
  }

  // Parse save settings
  std::string savesObj = json::extractObject(jsonStr, "saves");
  if (!savesObj.empty()) {
    std::string saveDir = json::extractString(savesObj, "directory");
    if (!saveDir.empty())
      config.saves.saveDirectory = saveDir;

    config.saves.enableCompression =
        json::extractBool(savesObj, "enable_compression", config.saves.enableCompression);
    config.saves.enableEncryption =
        json::extractBool(savesObj, "enable_encryption", config.saves.enableEncryption);
    config.saves.maxSlots = json::extractInt(savesObj, "max_slots", config.saves.maxSlots);
    config.saves.autoSaveEnabled =
        json::extractBool(savesObj, "auto_save_enabled", config.saves.autoSaveEnabled);
    config.saves.autoSaveIntervalMs =
        json::extractInt(savesObj, "auto_save_interval_ms", config.saves.autoSaveIntervalMs);
  }

  // Parse logging settings (runtime section)
  std::string runtimeObj = json::extractObject(jsonStr, "runtime");
  if (!runtimeObj.empty()) {
    config.logging.enableLogging =
        json::extractBool(runtimeObj, "enable_logging", config.logging.enableLogging);

    std::string logLevel = json::extractString(runtimeObj, "log_level");
    if (!logLevel.empty())
      config.logging.logLevel = logLevel;

    std::string logDir = json::extractString(runtimeObj, "log_directory");
    if (!logDir.empty())
      config.logging.logDirectory = logDir;
  }

  // Parse debug settings
  std::string debugObj = json::extractObject(jsonStr, "debug");
  if (!debugObj.empty()) {
    config.debug.enableDebugConsole =
        json::extractBool(debugObj, "enable_console", config.debug.enableDebugConsole);
    config.debug.showFps = json::extractBool(debugObj, "show_fps", config.debug.showFps);
    config.debug.showDebugOverlay =
        json::extractBool(debugObj, "show_overlay", config.debug.showDebugOverlay);
    config.debug.enableHotReload =
        json::extractBool(debugObj, "hot_reload", config.debug.enableHotReload);
  }

  return Result<void>::ok();
}

std::string ConfigManager::serializeToJson(const RuntimeConfig& config,
                                           bool userSettingsOnly) const {
  std::ostringstream json;
  json << "{\n";
  json << "  \"version\": \"" << config.version << "\",\n";

  // Game info (only if not user settings only)
  if (!userSettingsOnly) {
    json << "  \"game\": {\n";
    json << "    \"name\": \"" << config.game.name << "\",\n";
    json << "    \"version\": \"" << config.game.version << "\",\n";
    json << "    \"build_number\": " << config.game.buildNumber << "\n";
    json << "  },\n";
  }

  // Window settings
  json << "  \"window\": {\n";
  json << "    \"width\": " << config.window.width << ",\n";
  json << "    \"height\": " << config.window.height << ",\n";
  json << "    \"fullscreen\": " << (config.window.fullscreen ? "true" : "false") << ",\n";
  json << "    \"vsync\": " << (config.window.vsync ? "true" : "false") << ",\n";
  json << "    \"resizable\": " << (config.window.resizable ? "true" : "false") << ",\n";
  json << "    \"borderless\": " << (config.window.borderless ? "true" : "false") << "\n";
  json << "  },\n";

  // Audio settings
  json << "  \"audio\": {\n";
  json << "    \"master\": " << config.audio.master << ",\n";
  json << "    \"music\": " << config.audio.music << ",\n";
  json << "    \"voice\": " << config.audio.voice << ",\n";
  json << "    \"sfx\": " << config.audio.sfx << ",\n";
  json << "    \"ambient\": " << config.audio.ambient << ",\n";
  json << "    \"ui\": " << config.audio.ui << ",\n";
  json << "    \"muted\": " << (config.audio.muted ? "true" : "false") << "\n";
  json << "  },\n";

  // Text settings
  json << "  \"text\": {\n";
  json << "    \"speed\": " << config.text.speed << ",\n";
  json << "    \"auto_advance_ms\": " << config.text.autoAdvanceMs << ",\n";
  json << "    \"typewriter\": " << (config.text.typewriter ? "true" : "false") << ",\n";
  json << "    \"auto_advance\": " << (config.text.autoAdvance ? "true" : "false") << ",\n";
  json << "    \"skip_unread\": " << (config.text.skipUnread ? "true" : "false") << "\n";
  json << "  },\n";

  // Localization settings
  json << "  \"localization\": {\n";
  json << "    \"default_locale\": \"" << config.localization.defaultLocale << "\",\n";
  json << "    \"current_locale\": \"" << config.localization.currentLocale << "\",\n";
  json << "    \"available_locales\": [";
  for (size_t i = 0; i < config.localization.availableLocales.size(); ++i) {
    if (i > 0)
      json << ", ";
    json << "\"" << config.localization.availableLocales[i] << "\"";
  }
  json << "]\n";
  json << "  }";

  // Pack settings (only if not user settings only)
  if (!userSettingsOnly) {
    json << ",\n";
    json << "  \"packs\": {\n";
    json << "    \"directory\": \"" << config.packs.directory << "\",\n";
    json << "    \"index_file\": \"" << config.packs.indexFile << "\",\n";
    json << "    \"encrypted\": " << (config.packs.encrypted ? "true" : "false") << "\n";
    json << "  }";
  }

  json << "\n}\n";
  return json.str();
}

void ConfigManager::mergeConfig(const RuntimeConfig& userConfig) {
  // Merge user-modifiable settings
  m_config.window = userConfig.window;
  m_config.audio = userConfig.audio;
  m_config.text = userConfig.text;
  m_config.localization.currentLocale = userConfig.localization.currentLocale;
  m_config.input = userConfig.input;
}

} // namespace NovelMind::runtime
