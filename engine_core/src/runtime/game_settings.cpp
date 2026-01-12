/**
 * @file game_settings.cpp
 * @brief In-Game Settings implementation
 */

#include "NovelMind/runtime/game_settings.hpp"
#include "NovelMind/core/logger.hpp"
#include <algorithm>
#include <sstream>

namespace NovelMind::runtime {

GameSettings::GameSettings(ConfigManager* configManager) : m_configManager(configManager) {}

GameSettings::~GameSettings() = default;

Result<void> GameSettings::initialize() {
  if (!m_configManager) {
    return Result<void>::error("ConfigManager is null");
  }

  buildSettingsItems();
  syncFromConfig();

  NOVELMIND_LOG_INFO("Game settings initialized with " + std::to_string(m_settings.size()) +
                     " settings");
  return Result<void>::ok();
}

std::vector<SettingItem> GameSettings::getItemsInCategory(SettingsCategory category) const {
  std::vector<SettingItem> result;
  for (const auto& item : m_settings) {
    if (item.category == category) {
      result.push_back(item);
    }
  }
  return result;
}

std::vector<SettingsCategory> GameSettings::getCategories() const {
  return {SettingsCategory::Video,    SettingsCategory::Audio, SettingsCategory::Text,
          SettingsCategory::Language, SettingsCategory::Input, SettingsCategory::Accessibility};
}

std::string GameSettings::getCategoryName(SettingsCategory category) {
  switch (category) {
  case SettingsCategory::Video:
    return "Video";
  case SettingsCategory::Audio:
    return "Audio";
  case SettingsCategory::Text:
    return "Text";
  case SettingsCategory::Language:
    return "Language";
  case SettingsCategory::Input:
    return "Input";
  case SettingsCategory::Accessibility:
    return "Accessibility";
  }
  return "Unknown";
}

const SettingItem* GameSettings::getSetting(const std::string& id) const {
  for (const auto& item : m_settings) {
    if (item.id == id) {
      return &item;
    }
  }
  return nullptr;
}

const std::vector<SettingItem>& GameSettings::getAllSettings() const {
  return m_settings;
}

void GameSettings::setBoolValue(const std::string& id, bool value) {
  i32 idx = findSettingIndex(id);
  if (idx >= 0) {
    SettingItem oldValue = m_settings[static_cast<size_t>(idx)];
    m_settings[static_cast<size_t>(idx)].boolValue = value;
    m_hasPendingChanges = true;
    notifySettingChanged(id, oldValue);
  }
}

void GameSettings::setFloatValue(const std::string& id, f32 value) {
  i32 idx = findSettingIndex(id);
  if (idx >= 0) {
    SettingItem oldValue = m_settings[static_cast<size_t>(idx)];
    auto& setting = m_settings[static_cast<size_t>(idx)];
    setting.floatValue = std::clamp(value, setting.minValue, setting.maxValue);
    m_hasPendingChanges = true;
    notifySettingChanged(id, oldValue);
  }
}

void GameSettings::setIntValue(const std::string& id, i32 value) {
  i32 idx = findSettingIndex(id);
  if (idx >= 0) {
    SettingItem oldValue = m_settings[static_cast<size_t>(idx)];
    m_settings[static_cast<size_t>(idx)].intValue = value;
    m_hasPendingChanges = true;
    notifySettingChanged(id, oldValue);
  }
}

void GameSettings::setStringValue(const std::string& id, const std::string& value) {
  i32 idx = findSettingIndex(id);
  if (idx >= 0) {
    SettingItem oldValue = m_settings[static_cast<size_t>(idx)];
    m_settings[static_cast<size_t>(idx)].stringValue = value;
    m_hasPendingChanges = true;
    notifySettingChanged(id, oldValue);
  }
}

void GameSettings::setChoice(const std::string& id, i32 choiceIndex) {
  i32 idx = findSettingIndex(id);
  if (idx >= 0) {
    auto& setting = m_settings[static_cast<size_t>(idx)];
    if (choiceIndex >= 0 && choiceIndex < static_cast<i32>(setting.choices.size())) {
      SettingItem oldValue = setting;
      setting.selectedChoice = choiceIndex;
      setting.stringValue = setting.choices[static_cast<size_t>(choiceIndex)];
      m_hasPendingChanges = true;
      notifySettingChanged(id, oldValue);
    }
  }
}

void GameSettings::setKeyBinding(const std::string& id, const InputBinding& binding) {
  i32 idx = findSettingIndex(id);
  if (idx >= 0) {
    SettingItem oldValue = m_settings[static_cast<size_t>(idx)];
    m_settings[static_cast<size_t>(idx)].binding = binding;
    m_hasPendingChanges = true;
    notifySettingChanged(id, oldValue);
  }
}

Result<void> GameSettings::applyChanges() {
  if (!m_configManager) {
    return Result<void>::error("ConfigManager is null");
  }

  syncToConfig();

  auto result = m_configManager->saveUserConfig();
  if (result.isError()) {
    return result;
  }

  m_hasPendingChanges = false;

  if (m_onSettingsApplied) {
    m_onSettingsApplied();
  }

  NOVELMIND_LOG_INFO("Settings applied and saved");
  return Result<void>::ok();
}

void GameSettings::discardChanges() {
  syncFromConfig();
  m_hasPendingChanges = false;
}

void GameSettings::resetToDefaults() {
  if (m_configManager) {
    m_configManager->resetToDefaults();
    syncFromConfig();
    m_hasPendingChanges = true;

    if (m_onSettingsReset) {
      m_onSettingsReset();
    }
  }
}

void GameSettings::resetCategoryToDefaults(SettingsCategory category) {
  RuntimeConfig defaults;
  auto& config = m_configManager->getConfigMutable();

  switch (category) {
  case SettingsCategory::Video:
    config.window = defaults.window;
    break;
  case SettingsCategory::Audio:
    config.audio = defaults.audio;
    break;
  case SettingsCategory::Text:
    config.text = defaults.text;
    break;
  case SettingsCategory::Language:
    config.localization.currentLocale = defaults.localization.currentLocale;
    break;
  case SettingsCategory::Input:
    config.input = defaults.input;
    break;
  case SettingsCategory::Accessibility:
    // Reset accessibility-related settings
    break;
  }

  syncFromConfig();
  m_hasPendingChanges = true;
}

void GameSettings::setOnSettingsChanged(OnSettingsChanged callback) {
  m_onSettingsChanged = std::move(callback);
}

void GameSettings::setOnSettingsApplied(OnSettingsApplied callback) {
  m_onSettingsApplied = std::move(callback);
}

void GameSettings::setOnSettingsReset(OnSettingsReset callback) {
  m_onSettingsReset = std::move(callback);
}

std::vector<std::pair<i32, i32>> GameSettings::getAvailableResolutions() const {
  return {{1280, 720},  {1366, 768},  {1600, 900}, {1920, 1080},
          {2560, 1440}, {3840, 2160}, {1024, 768}, {1280, 1024}};
}

std::vector<std::string> GameSettings::getAvailableLanguages() const {
  if (m_configManager) {
    return m_configManager->getConfig().localization.availableLocales;
  }
  return {"en"};
}

std::string GameSettings::formatVolume(f32 volume) {
  return std::to_string(static_cast<i32>(volume * 100.0f)) + "%";
}

std::string GameSettings::formatResolution(i32 width, i32 height) {
  return std::to_string(width) + " x " + std::to_string(height);
}

void GameSettings::buildSettingsItems() {
  m_settings.clear();

  // =========================================================================
  // Video Settings
  // =========================================================================

  {
    SettingItem item;
    item.id = "fullscreen";
    item.label = "Fullscreen";
    item.description = "Enable fullscreen mode";
    item.type = SettingType::Toggle;
    item.category = SettingsCategory::Video;
    m_settings.push_back(item);
  }

  {
    SettingItem item;
    item.id = "resolution";
    item.label = "Resolution";
    item.description = "Screen resolution";
    item.type = SettingType::Choice;
    item.category = SettingsCategory::Video;
    for (auto [w, h] : getAvailableResolutions()) {
      item.choices.push_back(formatResolution(w, h));
    }
    m_settings.push_back(item);
  }

  {
    SettingItem item;
    item.id = "vsync";
    item.label = "V-Sync";
    item.description = "Synchronize frame rate with display";
    item.type = SettingType::Toggle;
    item.category = SettingsCategory::Video;
    m_settings.push_back(item);
  }

  // =========================================================================
  // Audio Settings
  // =========================================================================

  {
    SettingItem item;
    item.id = "master_volume";
    item.label = "Master Volume";
    item.description = "Overall volume level";
    item.type = SettingType::Slider;
    item.category = SettingsCategory::Audio;
    item.minValue = 0.0f;
    item.maxValue = 1.0f;
    item.step = 0.05f;
    m_settings.push_back(item);
  }

  {
    SettingItem item;
    item.id = "music_volume";
    item.label = "Music Volume";
    item.description = "Background music volume";
    item.type = SettingType::Slider;
    item.category = SettingsCategory::Audio;
    item.minValue = 0.0f;
    item.maxValue = 1.0f;
    item.step = 0.05f;
    m_settings.push_back(item);
  }

  {
    SettingItem item;
    item.id = "voice_volume";
    item.label = "Voice Volume";
    item.description = "Character voice volume";
    item.type = SettingType::Slider;
    item.category = SettingsCategory::Audio;
    item.minValue = 0.0f;
    item.maxValue = 1.0f;
    item.step = 0.05f;
    m_settings.push_back(item);
  }

  {
    SettingItem item;
    item.id = "sfx_volume";
    item.label = "Sound Effects";
    item.description = "Sound effects volume";
    item.type = SettingType::Slider;
    item.category = SettingsCategory::Audio;
    item.minValue = 0.0f;
    item.maxValue = 1.0f;
    item.step = 0.05f;
    m_settings.push_back(item);
  }

  {
    SettingItem item;
    item.id = "mute_audio";
    item.label = "Mute All";
    item.description = "Mute all audio";
    item.type = SettingType::Toggle;
    item.category = SettingsCategory::Audio;
    m_settings.push_back(item);
  }

  // =========================================================================
  // Text Settings
  // =========================================================================

  {
    SettingItem item;
    item.id = "text_speed";
    item.label = "Text Speed";
    item.description = "Characters per second";
    item.type = SettingType::Slider;
    item.category = SettingsCategory::Text;
    item.minValue = 10.0f;
    item.maxValue = 200.0f;
    item.step = 10.0f;
    m_settings.push_back(item);
  }

  {
    SettingItem item;
    item.id = "typewriter";
    item.label = "Typewriter Effect";
    item.description = "Display text character by character";
    item.type = SettingType::Toggle;
    item.category = SettingsCategory::Text;
    m_settings.push_back(item);
  }

  {
    SettingItem item;
    item.id = "auto_advance";
    item.label = "Auto Advance";
    item.description = "Automatically advance dialogue";
    item.type = SettingType::Toggle;
    item.category = SettingsCategory::Text;
    m_settings.push_back(item);
  }

  {
    SettingItem item;
    item.id = "auto_delay";
    item.label = "Auto Delay";
    item.description = "Delay before auto-advance (ms)";
    item.type = SettingType::Slider;
    item.category = SettingsCategory::Text;
    item.minValue = 500.0f;
    item.maxValue = 5000.0f;
    item.step = 100.0f;
    m_settings.push_back(item);
  }

  // =========================================================================
  // Language Settings
  // =========================================================================

  {
    SettingItem item;
    item.id = "language";
    item.label = "Language";
    item.description = "Game language";
    item.type = SettingType::Choice;
    item.category = SettingsCategory::Language;
    item.choices = getAvailableLanguages();
    m_settings.push_back(item);
  }

  // =========================================================================
  // Input Settings
  // =========================================================================

  auto addKeyBinding = [this](InputAction action, const std::string& label,
                              const std::string& desc) {
    SettingItem item;
    item.id = "key_" + inputActionToString(action);
    item.label = label;
    item.description = desc;
    item.type = SettingType::Key;
    item.category = SettingsCategory::Input;
    item.action = action;
    m_settings.push_back(item);
  };

  addKeyBinding(InputAction::Next, "Advance", "Advance dialogue");
  addKeyBinding(InputAction::Backlog, "Backlog", "Open text history");
  addKeyBinding(InputAction::Skip, "Skip", "Skip mode");
  addKeyBinding(InputAction::Auto, "Auto", "Auto-advance toggle");
  addKeyBinding(InputAction::QuickSave, "Quick Save", "Quick save game");
  addKeyBinding(InputAction::QuickLoad, "Quick Load", "Quick load game");
  addKeyBinding(InputAction::Menu, "Menu", "Open/close menu");
  addKeyBinding(InputAction::FullScreen, "Fullscreen", "Toggle fullscreen");
  addKeyBinding(InputAction::HideUI, "Hide UI", "Hide/show interface");
}

void GameSettings::syncFromConfig() {
  if (!m_configManager) {
    return;
  }

  const auto& config = m_configManager->getConfig();

  for (auto& item : m_settings) {
    // Video
    if (item.id == "fullscreen") {
      item.boolValue = config.window.fullscreen;
    } else if (item.id == "resolution") {
      std::string res = formatResolution(config.window.width, config.window.height);
      auto it = std::find(item.choices.begin(), item.choices.end(), res);
      item.selectedChoice = (it != item.choices.end())
                                ? static_cast<i32>(std::distance(item.choices.begin(), it))
                                : 0;
      item.stringValue = res;
    } else if (item.id == "vsync") {
      item.boolValue = config.window.vsync;
    }
    // Audio
    else if (item.id == "master_volume") {
      item.floatValue = config.audio.master;
    } else if (item.id == "music_volume") {
      item.floatValue = config.audio.music;
    } else if (item.id == "voice_volume") {
      item.floatValue = config.audio.voice;
    } else if (item.id == "sfx_volume") {
      item.floatValue = config.audio.sfx;
    } else if (item.id == "mute_audio") {
      item.boolValue = config.audio.muted;
    }
    // Text
    else if (item.id == "text_speed") {
      item.floatValue = static_cast<f32>(config.text.speed);
      item.intValue = config.text.speed;
    } else if (item.id == "typewriter") {
      item.boolValue = config.text.typewriter;
    } else if (item.id == "auto_advance") {
      item.boolValue = config.text.autoAdvance;
    } else if (item.id == "auto_delay") {
      item.floatValue = static_cast<f32>(config.text.autoAdvanceMs);
      item.intValue = config.text.autoAdvanceMs;
    }
    // Language
    else if (item.id == "language") {
      auto it =
          std::find(item.choices.begin(), item.choices.end(), config.localization.currentLocale);
      item.selectedChoice = (it != item.choices.end())
                                ? static_cast<i32>(std::distance(item.choices.begin(), it))
                                : 0;
      item.stringValue = config.localization.currentLocale;
    }
    // Input bindings
    else if (item.id.find("key_") == 0) {
      auto bindingIt = config.input.bindings.find(item.action);
      if (bindingIt != config.input.bindings.end()) {
        item.binding = bindingIt->second;
      }
    }
  }
}

void GameSettings::syncToConfig() {
  if (!m_configManager) {
    return;
  }

  auto& config = m_configManager->getConfigMutable();

  for (const auto& item : m_settings) {
    // Video
    if (item.id == "fullscreen") {
      config.window.fullscreen = item.boolValue;
    } else if (item.id == "resolution") {
      auto resolutions = getAvailableResolutions();
      if (item.selectedChoice >= 0 && item.selectedChoice < static_cast<i32>(resolutions.size())) {
        config.window.width = resolutions[static_cast<size_t>(item.selectedChoice)].first;
        config.window.height = resolutions[static_cast<size_t>(item.selectedChoice)].second;
      }
    } else if (item.id == "vsync") {
      config.window.vsync = item.boolValue;
    }
    // Audio
    else if (item.id == "master_volume") {
      config.audio.master = item.floatValue;
    } else if (item.id == "music_volume") {
      config.audio.music = item.floatValue;
    } else if (item.id == "voice_volume") {
      config.audio.voice = item.floatValue;
    } else if (item.id == "sfx_volume") {
      config.audio.sfx = item.floatValue;
    } else if (item.id == "mute_audio") {
      config.audio.muted = item.boolValue;
    }
    // Text
    else if (item.id == "text_speed") {
      config.text.speed = static_cast<i32>(item.floatValue);
    } else if (item.id == "typewriter") {
      config.text.typewriter = item.boolValue;
    } else if (item.id == "auto_advance") {
      config.text.autoAdvance = item.boolValue;
    } else if (item.id == "auto_delay") {
      config.text.autoAdvanceMs = static_cast<i32>(item.floatValue);
    }
    // Language
    else if (item.id == "language") {
      if (item.selectedChoice >= 0 && item.selectedChoice < static_cast<i32>(item.choices.size())) {
        config.localization.currentLocale = item.choices[static_cast<size_t>(item.selectedChoice)];
      }
    }
    // Input bindings
    else if (item.id.find("key_") == 0) {
      config.input.bindings[item.action] = item.binding;
    }
  }

  m_configManager->notifyConfigChanged();
}

i32 GameSettings::findSettingIndex(const std::string& id) const {
  for (size_t i = 0; i < m_settings.size(); ++i) {
    if (m_settings[i].id == id) {
      return static_cast<i32>(i);
    }
  }
  return -1;
}

void GameSettings::notifySettingChanged(const std::string& id, const SettingItem& oldValue) {
  if (m_onSettingsChanged) {
    i32 idx = findSettingIndex(id);
    if (idx >= 0) {
      SettingsChangeEvent event;
      event.settingId = id;
      event.oldValue = oldValue;
      event.newValue = m_settings[static_cast<size_t>(idx)];
      m_onSettingsChanged(event);
    }
  }
}

} // namespace NovelMind::runtime
