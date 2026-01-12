/**
 * @file test_settings_registry.cpp
 * @brief Tests for Settings Registry system
 */

#include <catch2/catch_test_macros.hpp>
#include "NovelMind/editor/settings_registry.hpp"
#include <filesystem>
#include <fstream>

using namespace NovelMind::editor;

TEST_CASE("NMSettingsRegistry - Basic construction", "[settings_registry]") {
  NMSettingsRegistry registry;

  // Should construct without errors
  CHECK(registry.getSchemaVersion() == 1);
  CHECK(!registry.isDirty());
}

TEST_CASE("NMSettingsRegistry - Register and retrieve settings", "[settings_registry]") {
  NMSettingsRegistry registry;

  SettingDefinition def;
  def.key = "test.bool_setting";
  def.displayName = "Test Bool";
  def.description = "A test boolean setting";
  def.category = "Test/General";
  def.type = SettingType::Bool;
  def.scope = SettingScope::User;
  def.defaultValue = true;

  registry.registerSetting(def);

  SECTION("Get definition") {
    auto retrieved = registry.getDefinition("test.bool_setting");
    REQUIRE(retrieved.has_value());
    CHECK(retrieved->key == "test.bool_setting");
    CHECK(retrieved->displayName == "Test Bool");
    CHECK(retrieved->type == SettingType::Bool);
  }

  SECTION("Get default value") {
    auto value = registry.getValue("test.bool_setting");
    REQUIRE(value.has_value());
    CHECK(std::get<bool>(*value) == true);
  }

  SECTION("Get via type-safe getter") {
    CHECK(registry.getBool("test.bool_setting", false) == true);
  }
}

TEST_CASE("NMSettingsRegistry - Set and get values", "[settings_registry]") {
  NMSettingsRegistry registry;

  SettingDefinition intDef;
  intDef.key = "test.int_value";
  intDef.displayName = "Test Int";
  intDef.category = "Test";
  intDef.type = SettingType::Int;
  intDef.scope = SettingScope::User;
  intDef.defaultValue = 42;

  registry.registerSetting(intDef);

  SECTION("Set value") {
    std::string error = registry.setValue("test.int_value", 100);
    CHECK(error.empty());
    CHECK(registry.getInt("test.int_value") == 100);
  }

  SECTION("Set value marks dirty") {
    registry.setValue("test.int_value", 100);
    CHECK(registry.isDirty());
  }

  SECTION("Set invalid type") {
    std::string error = registry.setValue("test.int_value", std::string("invalid"));
    CHECK(!error.empty());
  }
}

TEST_CASE("NMSettingsRegistry - Range validation", "[settings_registry]") {
  NMSettingsRegistry registry;

  SettingDefinition rangeDef;
  rangeDef.key = "test.range_value";
  rangeDef.displayName = "Test Range";
  rangeDef.category = "Test";
  rangeDef.type = SettingType::IntRange;
  rangeDef.scope = SettingScope::User;
  rangeDef.defaultValue = 50;
  rangeDef.minValue = 0.0f;
  rangeDef.maxValue = 100.0f;

  registry.registerSetting(rangeDef);

  SECTION("Set value within range") {
    std::string error = registry.setValue("test.range_value", 75);
    CHECK(error.empty());
  }

  SECTION("Set value below range") {
    std::string error = registry.setValue("test.range_value", -10);
    CHECK(!error.empty());
  }

  SECTION("Set value above range") {
    std::string error = registry.setValue("test.range_value", 150);
    CHECK(!error.empty());
  }
}

TEST_CASE("NMSettingsRegistry - Enum validation", "[settings_registry]") {
  NMSettingsRegistry registry;

  SettingDefinition enumDef;
  enumDef.key = "test.enum_value";
  enumDef.displayName = "Test Enum";
  enumDef.category = "Test";
  enumDef.type = SettingType::Enum;
  enumDef.scope = SettingScope::User;
  enumDef.defaultValue = std::string("option1");
  enumDef.enumOptions = {"option1", "option2", "option3"};

  registry.registerSetting(enumDef);

  SECTION("Set valid enum value") {
    std::string error = registry.setValue("test.enum_value", std::string("option2"));
    CHECK(error.empty());
    CHECK(registry.getString("test.enum_value") == "option2");
  }

  SECTION("Set invalid enum value") {
    std::string error = registry.setValue("test.enum_value", std::string("invalid"));
    CHECK(!error.empty());
  }
}

TEST_CASE("NMSettingsRegistry - Reset to defaults", "[settings_registry]") {
  NMSettingsRegistry registry;

  SettingDefinition def;
  def.key = "test.value";
  def.category = "Test";
  def.type = SettingType::Int;
  def.scope = SettingScope::User;
  def.defaultValue = 42;

  registry.registerSetting(def);
  registry.setValue("test.value", 100);

  SECTION("Reset single setting") {
    registry.resetToDefault("test.value");
    CHECK(registry.getInt("test.value") == 42);
  }

  SECTION("Reset all settings") {
    registry.resetAllToDefaults();
    CHECK(registry.getInt("test.value") == 42);
  }
}

TEST_CASE("NMSettingsRegistry - Change tracking", "[settings_registry]") {
  NMSettingsRegistry registry;

  SettingDefinition def;
  def.key = "test.value";
  def.category = "Test";
  def.type = SettingType::Int;
  def.scope = SettingScope::User;
  def.defaultValue = 42;

  registry.registerSetting(def);

  SECTION("Modified tracking") {
    CHECK(!registry.isModified("test.value"));
    registry.setValue("test.value", 100);
    CHECK(registry.isModified("test.value"));
  }

  SECTION("Revert changes") {
    registry.setValue("test.value", 100);
    registry.revert();
    CHECK(registry.getInt("test.value") == 42);
    CHECK(!registry.isDirty());
  }

  SECTION("Apply changes") {
    registry.setValue("test.value", 100);
    registry.apply();
    CHECK(!registry.isDirty());
    CHECK(!registry.isModified("test.value"));
  }
}

TEST_CASE("NMSettingsRegistry - Search", "[settings_registry]") {
  NMSettingsRegistry registry;

  SettingDefinition def1;
  def1.key = "test.audio_volume";
  def1.displayName = "Audio Volume";
  def1.category = "Audio";
  def1.type = SettingType::Float;
  def1.scope = SettingScope::User;
  def1.defaultValue = 1.0f;

  SettingDefinition def2;
  def2.key = "test.video_quality";
  def2.displayName = "Video Quality";
  def2.category = "Video";
  def2.type = SettingType::Int;
  def2.scope = SettingScope::User;
  def2.defaultValue = 3;

  registry.registerSetting(def1);
  registry.registerSetting(def2);

  SECTION("Search by display name") {
    auto results = registry.search("audio");
    CHECK(results.size() == 1);
    CHECK(results[0].key == "test.audio_volume");
  }

  SECTION("Search by category") {
    auto results = registry.search("video");
    CHECK(results.size() == 1);
    CHECK(results[0].key == "test.video_quality");
  }

  SECTION("Empty search returns all") {
    auto results = registry.search("");
    CHECK(results.size() == 2);
  }
}

TEST_CASE("NMSettingsRegistry - Get by category", "[settings_registry]") {
  NMSettingsRegistry registry;

  SettingDefinition def1;
  def1.key = "test.setting1";
  def1.category = "Category1";
  def1.type = SettingType::Bool;
  def1.scope = SettingScope::User;
  def1.defaultValue = true;

  SettingDefinition def2;
  def2.key = "test.setting2";
  def2.category = "Category1";
  def2.type = SettingType::Bool;
  def2.scope = SettingScope::User;
  def2.defaultValue = false;

  SettingDefinition def3;
  def3.key = "test.setting3";
  def3.category = "Category2";
  def3.type = SettingType::Bool;
  def3.scope = SettingScope::User;
  def3.defaultValue = true;

  registry.registerSetting(def1);
  registry.registerSetting(def2);
  registry.registerSetting(def3);

  auto category1 = registry.getByCategory("Category1");
  CHECK(category1.size() == 2);

  auto category2 = registry.getByCategory("Category2");
  CHECK(category2.size() == 1);
}

TEST_CASE("NMSettingsRegistry - Get by scope", "[settings_registry]") {
  NMSettingsRegistry registry;

  SettingDefinition userDef;
  userDef.key = "test.user_setting";
  userDef.category = "Test";
  userDef.type = SettingType::Bool;
  userDef.scope = SettingScope::User;
  userDef.defaultValue = true;

  SettingDefinition projectDef;
  projectDef.key = "test.project_setting";
  projectDef.category = "Test";
  projectDef.type = SettingType::Bool;
  projectDef.scope = SettingScope::Project;
  projectDef.defaultValue = false;

  registry.registerSetting(userDef);
  registry.registerSetting(projectDef);

  auto userSettings = registry.getByScope(SettingScope::User);
  CHECK(userSettings.size() == 1);
  CHECK(userSettings[0].scope == SettingScope::User);

  auto projectSettings = registry.getByScope(SettingScope::Project);
  CHECK(projectSettings.size() == 1);
  CHECK(projectSettings[0].scope == SettingScope::Project);
}

TEST_CASE("NMSettingsRegistry - JSON persistence", "[settings_registry]") {
  namespace fs = std::filesystem;

  const std::string testFile = "/tmp/test_settings.json";

  // Clean up before test
  if (fs::exists(testFile)) {
    fs::remove(testFile);
  }

  NMSettingsRegistry registry;

  SettingDefinition def1;
  def1.key = "test.bool_value";
  def1.category = "Test";
  def1.type = SettingType::Bool;
  def1.scope = SettingScope::User;
  def1.defaultValue = false;

  SettingDefinition def2;
  def2.key = "test.int_value";
  def2.category = "Test";
  def2.type = SettingType::Int;
  def2.scope = SettingScope::User;
  def2.defaultValue = 42;

  SettingDefinition def3;
  def3.key = "test.float_value";
  def3.category = "Test";
  def3.type = SettingType::Float;
  def3.scope = SettingScope::User;
  def3.defaultValue = 3.14f;

  SettingDefinition def4;
  def4.key = "test.string_value";
  def4.category = "Test";
  def4.type = SettingType::String;
  def4.scope = SettingScope::User;
  def4.defaultValue = std::string("hello");

  registry.registerSetting(def1);
  registry.registerSetting(def2);
  registry.registerSetting(def3);
  registry.registerSetting(def4);

  // Set some values
  registry.setValue("test.bool_value", true);
  registry.setValue("test.int_value", 100);
  registry.setValue("test.float_value", 2.718f);
  registry.setValue("test.string_value", std::string("world"));

  SECTION("Save to JSON") {
    auto result = registry.saveUserSettings(testFile);
    REQUIRE(result);
    CHECK(fs::exists(testFile));
  }

  SECTION("Load from JSON") {
    // First save
    registry.saveUserSettings(testFile);

    // Create new registry and load
    NMSettingsRegistry newRegistry;
    newRegistry.registerSetting(def1);
    newRegistry.registerSetting(def2);
    newRegistry.registerSetting(def3);
    newRegistry.registerSetting(def4);

    auto result = newRegistry.loadUserSettings(testFile);
    REQUIRE(result);

    CHECK(newRegistry.getBool("test.bool_value") == true);
    CHECK(newRegistry.getInt("test.int_value") == 100);
    CHECK(newRegistry.getFloat("test.float_value") == Catch::Approx(2.718f));
    CHECK(newRegistry.getString("test.string_value") == "world");
  }

  // Clean up
  if (fs::exists(testFile)) {
    fs::remove(testFile);
  }
}

TEST_CASE("NMSettingsRegistry - Default editor settings", "[settings_registry]") {
  NMSettingsRegistry registry;
  registry.registerEditorDefaults();

  SECTION("Has general settings") {
    auto value = registry.getValue("editor.general.autosave");
    REQUIRE(value.has_value());
    CHECK(std::get<bool>(*value) == true);
  }

  SECTION("Has appearance settings") {
    auto value = registry.getValue("editor.appearance.theme");
    REQUIRE(value.has_value());
    CHECK(std::get<std::string>(*value) == "dark");
  }

  SECTION("Has workspace settings") {
    auto value = registry.getValue("editor.workspace.default_layout");
    REQUIRE(value.has_value());
  }

  SECTION("All editor settings are User scope") {
    auto editorSettings = registry.getByScope(SettingScope::User);
    for (const auto& def : editorSettings) {
      CHECK(def.key.find("editor.") == 0);
    }
  }
}

TEST_CASE("NMSettingsRegistry - Default project settings", "[settings_registry]") {
  NMSettingsRegistry registry;
  registry.registerProjectDefaults();

  SECTION("Has project settings") {
    auto value = registry.getValue("project.name");
    REQUIRE(value.has_value());
  }

  SECTION("Has localization settings") {
    auto value = registry.getValue("project.localization.default_locale");
    REQUIRE(value.has_value());
    CHECK(std::get<std::string>(*value) == "en");
  }

  SECTION("All project settings are Project scope") {
    auto projectSettings = registry.getByScope(SettingScope::Project);
    for (const auto& def : projectSettings) {
      CHECK(def.key.find("project.") == 0);
      CHECK(def.scope == SettingScope::Project);
    }
  }
}

TEST_CASE("NMSettingsRegistry - Helper functions", "[settings_registry]") {
  SECTION("settingValueToString") {
    CHECK(settingValueToString(true) == "true");
    CHECK(settingValueToString(false) == "false");
    CHECK(settingValueToString(42) == "42");
    CHECK(settingValueToString(3.14f) != "");
    CHECK(settingValueToString(std::string("hello")) == "hello");
  }

  SECTION("stringToSettingValue") {
    auto boolVal = stringToSettingValue("true", SettingType::Bool);
    REQUIRE(boolVal.has_value());
    CHECK(std::get<bool>(*boolVal) == true);

    auto intVal = stringToSettingValue("42", SettingType::Int);
    REQUIRE(intVal.has_value());
    CHECK(std::get<i32>(*intVal) == 42);

    auto floatVal = stringToSettingValue("3.14", SettingType::Float);
    REQUIRE(floatVal.has_value());
    CHECK(std::get<f32>(*floatVal) == Catch::Approx(3.14f));

    auto strVal = stringToSettingValue("hello", SettingType::String);
    REQUIRE(strVal.has_value());
    CHECK(std::get<std::string>(*strVal) == "hello");
  }

  SECTION("settingTypeToString") {
    CHECK(std::string(settingTypeToString(SettingType::Bool)) == "Bool");
    CHECK(std::string(settingTypeToString(SettingType::Int)) == "Int");
    CHECK(std::string(settingTypeToString(SettingType::Float)) == "Float");
    CHECK(std::string(settingTypeToString(SettingType::String)) == "String");
    CHECK(std::string(settingTypeToString(SettingType::Enum)) == "Enum");
  }

  SECTION("settingScopeToString") {
    CHECK(std::string(settingScopeToString(SettingScope::User)).find("User") != std::string::npos);
    CHECK(std::string(settingScopeToString(SettingScope::Project)).find("Project") !=
          std::string::npos);
  }
}
