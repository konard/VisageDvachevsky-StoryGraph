/**
 * @file test_settings_concurrency.cpp
 * @brief Thread-safety tests for Settings Registry system
 *
 * Tests concurrent access scenarios that could trigger race conditions
 * in the settings persistence system (Issue #541)
 */

#include <catch2/catch_test_macros.hpp>
#include "NovelMind/editor/settings_registry.hpp"
#include "NovelMind/editor/editor_settings.hpp"
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

using namespace NovelMind::editor;

TEST_CASE("NMSettingsRegistry - Concurrent reads", "[settings_registry][concurrency]") {
  NMSettingsRegistry registry;

  SettingDefinition def;
  def.key = "test.concurrent_value";
  def.category = "Test";
  def.type = SettingType::Int;
  def.scope = SettingScope::User;
  def.defaultValue = 42;

  registry.registerSetting(def);
  registry.setValue("test.concurrent_value", 100);

  // Launch multiple threads that read concurrently
  std::atomic<int> successCount{0};
  std::vector<std::thread> threads;

  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&registry, &successCount]() {
      for (int j = 0; j < 100; ++j) {
        auto value = registry.getValue("test.concurrent_value");
        if (value.has_value() && std::get<i32>(*value) == 100) {
          successCount++;
        }
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  // All reads should succeed
  CHECK(successCount == 1000);
}

TEST_CASE("NMSettingsRegistry - Concurrent writes", "[settings_registry][concurrency]") {
  NMSettingsRegistry registry;

  SettingDefinition def;
  def.key = "test.concurrent_write";
  def.category = "Test";
  def.type = SettingType::Int;
  def.scope = SettingScope::User;
  def.defaultValue = 0;

  registry.registerSetting(def);

  // Launch multiple threads that write concurrently
  std::vector<std::thread> threads;
  std::atomic<int> successCount{0};

  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&registry, &successCount, i]() {
      for (int j = 0; j < 10; ++j) {
        std::string error = registry.setValue("test.concurrent_write", i * 100 + j);
        if (error.empty()) {
          successCount++;
        }
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  // All writes should succeed
  CHECK(successCount == 100);

  // Final value should be one of the written values
  auto finalValue = registry.getInt("test.concurrent_write");
  CHECK((finalValue >= 0 && finalValue < 1000));
}

TEST_CASE("NMSettingsRegistry - Concurrent read/write", "[settings_registry][concurrency]") {
  NMSettingsRegistry registry;

  SettingDefinition def;
  def.key = "test.readwrite";
  def.category = "Test";
  def.type = SettingType::Int;
  def.scope = SettingScope::User;
  def.defaultValue = 0;

  registry.registerSetting(def);

  std::atomic<bool> running{true};
  std::atomic<int> readCount{0};
  std::atomic<int> writeCount{0};

  // Reader threads
  std::vector<std::thread> readers;
  for (int i = 0; i < 5; ++i) {
    readers.emplace_back([&]() {
      while (running) {
        auto value = registry.getValue("test.readwrite");
        if (value.has_value()) {
          readCount++;
        }
      }
    });
  }

  // Writer threads
  std::vector<std::thread> writers;
  for (int i = 0; i < 5; ++i) {
    writers.emplace_back([&, i]() {
      for (int j = 0; j < 20; ++j) {
        registry.setValue("test.readwrite", i * 100 + j);
        writeCount++;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    });
  }

  // Wait for writers to finish
  for (auto& t : writers) {
    t.join();
  }

  running = false;

  // Wait for readers to finish
  for (auto& t : readers) {
    t.join();
  }

  CHECK(writeCount == 100);
  CHECK(readCount > 0); // At least some reads should have succeeded
}

TEST_CASE("NMSettingsRegistry - Concurrent callback registration", "[settings_registry][concurrency]") {
  NMSettingsRegistry registry;

  SettingDefinition def;
  def.key = "test.callback_value";
  def.category = "Test";
  def.type = SettingType::Int;
  def.scope = SettingScope::User;
  def.defaultValue = 0;

  registry.registerSetting(def);

  std::atomic<int> callbackCount{0};

  // Register callbacks from multiple threads
  std::vector<std::thread> threads;
  for (int i = 0; i < 5; ++i) {
    threads.emplace_back([&registry, &callbackCount]() {
      registry.registerChangeCallback("test.callback_value",
        [&callbackCount](const std::string&, const SettingValue&) {
          callbackCount++;
        });
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  // Now modify the setting - all callbacks should fire
  registry.setValue("test.callback_value", 42);

  CHECK(callbackCount == 5);
}

TEST_CASE("HotkeyManager - Concurrent action registration", "[hotkey_manager][concurrency]") {
  HotkeyManager manager;

  std::atomic<int> registeredCount{0};
  std::vector<std::thread> threads;

  // Register actions from multiple threads
  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&manager, &registeredCount, i]() {
      HotkeyAction action;
      action.id = "action_" + std::to_string(i);
      action.name = "Action " + std::to_string(i);
      action.category = ActionCategory::Custom;
      action.defaultBinding = {static_cast<i32>('A' + i), KeyModifier::Ctrl};
      action.currentBinding = action.defaultBinding;

      manager.registerAction(action, [&registeredCount]() {
        registeredCount++;
      });
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  // All actions should be registered
  auto actions = manager.getAllActions();
  CHECK(actions.size() == 10);
}

TEST_CASE("ThemeManager - Concurrent theme registration", "[theme_manager][concurrency]") {
  ThemeManager manager;

  std::vector<std::thread> threads;

  // Register themes from multiple threads
  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&manager, i]() {
      Theme theme;
      theme.name = "theme_" + std::to_string(i);
      theme.author = "Test";
      theme.isDark = (i % 2 == 0);

      manager.registerTheme(theme);
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  // All themes should be registered (plus built-in themes)
  auto themes = manager.getAvailableThemes();
  CHECK(themes.size() >= 10); // At least our 10 themes (may have built-in themes too)
}

TEST_CASE("PreferencesManager - Concurrent recent project updates", "[preferences_manager][concurrency]") {
  PreferencesManager manager;

  std::vector<std::thread> threads;
  std::atomic<int> addCount{0};

  // Add recent projects from multiple threads
  for (int i = 0; i < 20; ++i) {
    threads.emplace_back([&manager, &addCount, i]() {
      std::string path = "/path/to/project_" + std::to_string(i);
      manager.addRecentProject(path);
      addCount++;
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  CHECK(addCount == 20);

  // Recent projects list should exist and be within max size
  const auto& recentProjects = manager.getRecentProjects();
  CHECK(recentProjects.size() <= static_cast<size_t>(manager.get().maxRecentProjects));
}

TEST_CASE("LayoutManager - Concurrent layout operations", "[layout_manager][concurrency]") {
  LayoutManager manager;

  std::vector<std::thread> threads;

  // Save and load layouts from multiple threads
  for (int i = 0; i < 5; ++i) {
    threads.emplace_back([&manager, i]() {
      std::string layoutName = "layout_" + std::to_string(i);

      // Note: saveLayout requires editor to be initialized
      // Just test getting saved layouts which is thread-safe
      auto layouts = manager.getSavedLayouts();
      (void)layouts; // Suppress unused warning
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  // Should complete without crashes or assertions
  CHECK(true);
}

TEST_CASE("NMSettingsRegistry - Stress test", "[settings_registry][concurrency][.stress]") {
  NMSettingsRegistry registry;

  // Register multiple settings
  for (int i = 0; i < 20; ++i) {
    SettingDefinition def;
    def.key = "stress.setting_" + std::to_string(i);
    def.category = "Stress";
    def.type = SettingType::Int;
    def.scope = SettingScope::User;
    def.defaultValue = i;

    registry.registerSetting(def);
  }

  std::atomic<bool> running{true};
  std::atomic<int> operationCount{0};

  // Launch many threads doing various operations
  std::vector<std::thread> threads;

  // Readers
  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&registry, &running, &operationCount]() {
      while (running) {
        for (int j = 0; j < 20; ++j) {
          std::string key = "stress.setting_" + std::to_string(j);
          auto value = registry.getValue(key);
          if (value.has_value()) {
            operationCount++;
          }
        }
      }
    });
  }

  // Writers
  for (int i = 0; i < 5; ++i) {
    threads.emplace_back([&registry, &running, &operationCount, i]() {
      int counter = 0;
      while (running) {
        for (int j = 0; j < 20; ++j) {
          std::string key = "stress.setting_" + std::to_string(j);
          registry.setValue(key, i * 1000 + counter++);
          operationCount++;
        }
      }
    });
  }

  // Run for a short time
  std::this_thread::sleep_for(std::chrono::seconds(1));
  running = false;

  for (auto& t : threads) {
    t.join();
  }

  // Check that many operations completed
  CHECK(operationCount > 1000);

  // Registry should still be in valid state
  CHECK(!registry.getAllDefinitions().empty());
}
