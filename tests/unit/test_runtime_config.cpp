/**
 * @file test_runtime_config.cpp
 * @brief Unit tests for RuntimeConfig, ConfigManager, and GameSettings
 */

#include "NovelMind/runtime/config_manager.hpp"
#include "NovelMind/runtime/game_settings.hpp"
#include "NovelMind/runtime/runtime_config.hpp"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using namespace NovelMind::runtime;

// Helper to create a temporary test directory
class TestDirectory {
public:
  TestDirectory() {
    m_path = fs::temp_directory_path() / "novelmind_test_config";
    fs::create_directories(m_path);
    fs::create_directories(m_path / "config");
    fs::create_directories(m_path / "saves");
    fs::create_directories(m_path / "logs");
  }

  ~TestDirectory() {
    try {
      fs::remove_all(m_path);
    } catch (...) {
      // Ignore cleanup errors
    }
  }

  std::string path() const { return m_path.string(); }

  void writeFile(const std::string &relativePath, const std::string &content) {
    std::ofstream file(m_path / relativePath);
    file << content;
  }

private:
  fs::path m_path;
};

// ===========================================================================
// RuntimeConfig tests
// ===========================================================================

TEST_CASE("RuntimeConfig has sensible defaults", "[runtime_config]") {
  RuntimeConfig config;

  SECTION("Game info defaults") {
    REQUIRE(config.game.name == "NovelMind Game");
    REQUIRE(config.game.version == "1.0.0");
    REQUIRE(config.game.buildNumber == 1);
  }

  SECTION("Window defaults") {
    REQUIRE(config.window.width == 1280);
    REQUIRE(config.window.height == 720);
    REQUIRE(config.window.fullscreen == false);
    REQUIRE(config.window.vsync == true);
  }

  SECTION("Audio defaults") {
    REQUIRE(config.audio.master == 1.0f);
    REQUIRE(config.audio.music == 0.8f);
    REQUIRE(config.audio.muted == false);
  }

  SECTION("Text defaults") {
    REQUIRE(config.text.speed == 40);
    REQUIRE(config.text.typewriter == true);
    REQUIRE(config.text.autoAdvanceMs == 1500);
  }

  SECTION("Localization defaults") {
    REQUIRE(config.localization.defaultLocale == "en");
    REQUIRE(config.localization.currentLocale == "en");
    REQUIRE(config.localization.availableLocales.size() >= 1);
  }

  SECTION("Input bindings are initialized") {
    REQUIRE_FALSE(config.input.bindings.empty());
    REQUIRE(config.input.bindings.count(InputAction::Next) > 0);
    REQUIRE(config.input.bindings.count(InputAction::Menu) > 0);
  }
}

TEST_CASE("InputAction string conversion", "[runtime_config]") {
  REQUIRE(inputActionToString(InputAction::Next) == "next");
  REQUIRE(inputActionToString(InputAction::Backlog) == "backlog");
  REQUIRE(inputActionToString(InputAction::Skip) == "skip");
  REQUIRE(inputActionToString(InputAction::Auto) == "auto");
  REQUIRE(inputActionToString(InputAction::QuickSave) == "quick_save");
  REQUIRE(inputActionToString(InputAction::QuickLoad) == "quick_load");
  REQUIRE(inputActionToString(InputAction::Menu) == "menu");
  REQUIRE(inputActionToString(InputAction::FullScreen) == "fullscreen");

  REQUIRE(stringToInputAction("next") == InputAction::Next);
  REQUIRE(stringToInputAction("menu") == InputAction::Menu);
  REQUIRE(stringToInputAction("quick_save") == InputAction::QuickSave);
}

// ===========================================================================
// ConfigManager tests
// ===========================================================================

TEST_CASE("ConfigManager initialization", "[config_manager]") {
  TestDirectory testDir;
  ConfigManager manager;

  auto result = manager.initialize(testDir.path());
  REQUIRE(result.isOk());

  SECTION("Directories are created") {
    REQUIRE(fs::exists(testDir.path() + "/config"));
    REQUIRE(fs::exists(testDir.path() + "/saves"));
    REQUIRE(fs::exists(testDir.path() + "/logs"));
  }
}

TEST_CASE("ConfigManager loads defaults when no config file",
          "[config_manager]") {
  TestDirectory testDir;
  ConfigManager manager;

  REQUIRE(manager.initialize(testDir.path()).isOk());
  REQUIRE(manager.loadConfig().isOk());

  // Should have defaults
  REQUIRE(manager.getConfig().game.name == "NovelMind Game");
}

TEST_CASE("ConfigManager loads config from file", "[config_manager]") {
  TestDirectory testDir;

  // Create a test config file
  std::string configJson = R"({
    "version": "1.0",
    "game": {
      "name": "Test Game",
      "version": "2.0.0",
      "build_number": 42
    },
    "window": {
      "width": 1920,
      "height": 1080,
      "fullscreen": true,
      "vsync": false
    },
    "audio": {
      "master": 0.5,
      "music": 0.7
    },
    "localization": {
      "default_locale": "en",
      "current_locale": "ru",
      "available_locales": ["en", "ru", "ja"]
    }
  })";

  testDir.writeFile("config/runtime_config.json", configJson);

  ConfigManager manager;
  REQUIRE(manager.initialize(testDir.path()).isOk());
  REQUIRE(manager.loadConfig().isOk());

  SECTION("Game info is loaded") {
    REQUIRE(manager.getConfig().game.name == "Test Game");
    REQUIRE(manager.getConfig().game.version == "2.0.0");
    REQUIRE(manager.getConfig().game.buildNumber == 42);
  }

  SECTION("Window settings are loaded") {
    REQUIRE(manager.getConfig().window.width == 1920);
    REQUIRE(manager.getConfig().window.height == 1080);
    REQUIRE(manager.getConfig().window.fullscreen == true);
    REQUIRE(manager.getConfig().window.vsync == false);
  }

  SECTION("Audio settings are loaded") {
    REQUIRE(manager.getConfig().audio.master == 0.5f);
    REQUIRE(manager.getConfig().audio.music == 0.7f);
  }

  SECTION("Localization settings are loaded") {
    REQUIRE(manager.getConfig().localization.currentLocale == "ru");
    REQUIRE(manager.getConfig().localization.availableLocales.size() == 3);
  }
}

TEST_CASE("ConfigManager saves user config", "[config_manager]") {
  TestDirectory testDir;
  ConfigManager manager;

  REQUIRE(manager.initialize(testDir.path()).isOk());
  REQUIRE(manager.loadConfig().isOk());

  // Modify settings
  manager.setMasterVolume(0.75f);
  manager.setFullscreen(true);
  manager.setLocale("ja");

  // Save
  REQUIRE(manager.saveUserConfig().isOk());

  // Verify file was created
  REQUIRE(fs::exists(testDir.path() + "/config/runtime_user.json"));

  // Load in a new manager
  ConfigManager manager2;
  REQUIRE(manager2.initialize(testDir.path()).isOk());
  REQUIRE(manager2.loadConfig().isOk());

  // Settings should be preserved
  REQUIRE(manager2.getConfig().audio.master == 0.75f);
  REQUIRE(manager2.getConfig().window.fullscreen == true);
  REQUIRE(manager2.getConfig().localization.currentLocale == "ja");
}

TEST_CASE("ConfigManager convenience setters", "[config_manager]") {
  TestDirectory testDir;
  ConfigManager manager;

  REQUIRE(manager.initialize(testDir.path()).isOk());
  REQUIRE(manager.loadConfig().isOk());

  SECTION("Volume setters clamp values") {
    manager.setMasterVolume(2.0f); // Over 1.0
    REQUIRE(manager.getConfig().audio.master == 1.0f);

    manager.setMasterVolume(-1.0f); // Under 0.0
    REQUIRE(manager.getConfig().audio.master == 0.0f);

    manager.setMusicVolume(0.5f);
    REQUIRE(manager.getConfig().audio.music == 0.5f);
  }

  SECTION("Text speed setters clamp values") {
    manager.setTextSpeed(500); // Over max
    REQUIRE(manager.getConfig().text.speed == 200);

    manager.setTextSpeed(0); // Under min
    REQUIRE(manager.getConfig().text.speed == 1);
  }

  SECTION("Resolution setter works") {
    manager.setResolution(2560, 1440);
    REQUIRE(manager.getConfig().window.width == 2560);
    REQUIRE(manager.getConfig().window.height == 1440);
  }
}

// ===========================================================================
// GameSettings tests
// ===========================================================================

TEST_CASE("GameSettings initialization", "[game_settings]") {
  TestDirectory testDir;
  ConfigManager configManager;

  REQUIRE(configManager.initialize(testDir.path()).isOk());
  REQUIRE(configManager.loadConfig().isOk());

  GameSettings settings(&configManager);
  REQUIRE(settings.initialize().isOk());

  SECTION("Settings items are created") {
    auto allSettings = settings.getAllSettings();
    REQUIRE_FALSE(allSettings.empty());
  }

  SECTION("Categories are available") {
    auto categories = settings.getCategories();
    REQUIRE(categories.size() >= 5); // Video, Audio, Text, Language, Input
  }

  SECTION("Category names are correct") {
    REQUIRE(GameSettings::getCategoryName(SettingsCategory::Video) == "Video");
    REQUIRE(GameSettings::getCategoryName(SettingsCategory::Audio) == "Audio");
    REQUIRE(GameSettings::getCategoryName(SettingsCategory::Text) == "Text");
  }
}

TEST_CASE("GameSettings tracks pending changes", "[game_settings]") {
  TestDirectory testDir;
  ConfigManager configManager;

  REQUIRE(configManager.initialize(testDir.path()).isOk());
  REQUIRE(configManager.loadConfig().isOk());

  GameSettings settings(&configManager);
  REQUIRE(settings.initialize().isOk());

  REQUIRE_FALSE(settings.hasPendingChanges());

  settings.setFloatValue("master_volume", 0.5f);

  REQUIRE(settings.hasPendingChanges());
}

TEST_CASE("GameSettings apply and discard", "[game_settings]") {
  TestDirectory testDir;
  ConfigManager configManager;

  REQUIRE(configManager.initialize(testDir.path()).isOk());
  REQUIRE(configManager.loadConfig().isOk());

  GameSettings settings(&configManager);
  REQUIRE(settings.initialize().isOk());

  float originalVolume = configManager.getConfig().audio.master;

  settings.setFloatValue("master_volume", 0.3f);
  REQUIRE(settings.hasPendingChanges());

  SECTION("Apply changes persists them") {
    REQUIRE(settings.applyChanges().isOk());
    REQUIRE_FALSE(settings.hasPendingChanges());
    REQUIRE(configManager.getConfig().audio.master == 0.3f);
  }

  SECTION("Discard changes reverts them") {
    settings.discardChanges();
    REQUIRE_FALSE(settings.hasPendingChanges());

    // The setting should be back to original
    auto *setting = settings.getSetting("master_volume");
    REQUIRE(setting != nullptr);
    REQUIRE(setting->floatValue == originalVolume);
  }
}

TEST_CASE("GameSettings get items by category", "[game_settings]") {
  TestDirectory testDir;
  ConfigManager configManager;

  REQUIRE(configManager.initialize(testDir.path()).isOk());
  REQUIRE(configManager.loadConfig().isOk());

  GameSettings settings(&configManager);
  REQUIRE(settings.initialize().isOk());

  SECTION("Video category has expected items") {
    auto videoItems = settings.getItemsInCategory(SettingsCategory::Video);
    REQUIRE_FALSE(videoItems.empty());

    bool hasFullscreen = false;
    bool hasResolution = false;
    for (const auto &item : videoItems) {
      if (item.id == "fullscreen")
        hasFullscreen = true;
      if (item.id == "resolution")
        hasResolution = true;
    }
    REQUIRE(hasFullscreen);
    REQUIRE(hasResolution);
  }

  SECTION("Audio category has expected items") {
    auto audioItems = settings.getItemsInCategory(SettingsCategory::Audio);
    REQUIRE_FALSE(audioItems.empty());

    bool hasMasterVolume = false;
    for (const auto &item : audioItems) {
      if (item.id == "master_volume")
        hasMasterVolume = true;
    }
    REQUIRE(hasMasterVolume);
  }

  SECTION("Input category has key bindings") {
    auto inputItems = settings.getItemsInCategory(SettingsCategory::Input);
    REQUIRE_FALSE(inputItems.empty());

    int keyBindingCount = 0;
    for (const auto &item : inputItems) {
      if (item.type == SettingType::Key)
        keyBindingCount++;
    }
    REQUIRE(keyBindingCount >= 5); // At least some key bindings
  }
}

TEST_CASE("GameSettings available resolutions", "[game_settings]") {
  TestDirectory testDir;
  ConfigManager configManager;

  REQUIRE(configManager.initialize(testDir.path()).isOk());
  REQUIRE(configManager.loadConfig().isOk());

  GameSettings settings(&configManager);
  REQUIRE(settings.initialize().isOk());

  auto resolutions = settings.getAvailableResolutions();
  REQUIRE_FALSE(resolutions.empty());

  // Should include common resolutions
  bool has720p = false;
  bool has1080p = false;
  for (const auto &[w, h] : resolutions) {
    if (w == 1280 && h == 720)
      has720p = true;
    if (w == 1920 && h == 1080)
      has1080p = true;
  }
  REQUIRE(has720p);
  REQUIRE(has1080p);
}

TEST_CASE("GameSettings format helpers", "[game_settings]") {
  REQUIRE(GameSettings::formatVolume(1.0f) == "100%");
  REQUIRE(GameSettings::formatVolume(0.5f) == "50%");
  REQUIRE(GameSettings::formatVolume(0.0f) == "0%");

  REQUIRE(GameSettings::formatResolution(1920, 1080) == "1920 x 1080");
  REQUIRE(GameSettings::formatResolution(1280, 720) == "1280 x 720");
}
