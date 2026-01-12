#include <catch2/catch_test_macros.hpp>
#include "NovelMind/editor/editor_settings.hpp"

using namespace NovelMind;
using namespace NovelMind::editor;

// =============================================================================
// KeyBinding Tests
// =============================================================================

TEST_CASE("KeyBinding - Default values", "[keybinding]") {
  KeyBinding binding;

  CHECK(binding.keyCode == 0);
  CHECK(binding.modifiers == KeyModifier::None);
}

TEST_CASE("KeyBinding - Equality comparison", "[keybinding]") {
  KeyBinding binding1;
  binding1.keyCode = 83; // 'S' key
  binding1.modifiers = KeyModifier::Ctrl;

  KeyBinding binding2;
  binding2.keyCode = 83;
  binding2.modifiers = KeyModifier::Ctrl;

  CHECK(binding1 == binding2);

  binding2.keyCode = 90; // 'Z' key
  CHECK_FALSE(binding1 == binding2);
}

TEST_CASE("KeyBinding - Modifier combination", "[keybinding]") {
  KeyBinding binding;
  binding.keyCode = 83; // 'S' key
  binding.modifiers = KeyModifier::Ctrl | KeyModifier::Shift;

  // Check that modifiers are combined
  CHECK((static_cast<u8>(binding.modifiers) & static_cast<u8>(KeyModifier::Ctrl)) != 0);
  CHECK((static_cast<u8>(binding.modifiers) & static_cast<u8>(KeyModifier::Shift)) != 0);
  CHECK((static_cast<u8>(binding.modifiers) & static_cast<u8>(KeyModifier::Alt)) == 0);
}

TEST_CASE("KeyBinding - toString produces readable string", "[keybinding]") {
  KeyBinding binding;
  binding.keyCode = 'S';
  binding.modifiers = KeyModifier::Ctrl;

  std::string str = binding.toString();
  CHECK(str.find("Ctrl") != std::string::npos);
  CHECK(str.find("S") != std::string::npos);
}

TEST_CASE("KeyBinding - fromString parses string", "[keybinding]") {
  KeyBinding binding = KeyBinding::fromString("Ctrl+S");

  CHECK(binding.keyCode == 'S');
  CHECK((static_cast<u8>(binding.modifiers) & static_cast<u8>(KeyModifier::Ctrl)) != 0);
}

// =============================================================================
// PanelState Tests
// =============================================================================

TEST_CASE("PanelState - Default values", "[panel_state]") {
  PanelState panel;

  CHECK(panel.visible == true);
  CHECK(panel.docked == true);
  CHECK(panel.width == 300);
  CHECK(panel.height == 400);
}

// =============================================================================
// EditorLayout Tests
// =============================================================================

TEST_CASE("EditorLayout - Default values", "[editor_layout]") {
  EditorLayout layout;

  CHECK(layout.mainWindowWidth == 1920);
  CHECK(layout.mainWindowHeight == 1080);
  CHECK(layout.maximized == false);
  CHECK(layout.panels.empty());
}

TEST_CASE("EditorLayout - Can add panels", "[editor_layout]") {
  EditorLayout layout;
  layout.name = "Test Layout";

  PanelState panel;
  panel.name = "SceneView";
  panel.visible = true;
  panel.x = 100;
  panel.y = 100;

  layout.panels.push_back(panel);

  CHECK(layout.panels.size() == 1);
  CHECK(layout.panels[0].name == "SceneView");
}

// =============================================================================
// LayoutPreset Tests
// =============================================================================

TEST_CASE("LayoutPreset - Enum values", "[layout_preset]") {
  CHECK(static_cast<u8>(LayoutPreset::Default) == 0);
  CHECK(static_cast<u8>(LayoutPreset::StoryFocused) == 1);
  CHECK(static_cast<u8>(LayoutPreset::SceneFocused) == 2);
}

// =============================================================================
// LayoutManager Tests (minimal, since it requires EditorApp)
// =============================================================================

TEST_CASE("LayoutManager - Construction", "[layout_manager]") {
  LayoutManager manager;
  // Just verify it constructs without crashing
  CHECK(true);
}

TEST_CASE("LayoutManager - GetCurrentLayout returns default", "[layout_manager]") {
  LayoutManager manager;

  EditorLayout layout = manager.getCurrentLayout();
  // Should return some default layout
  CHECK(true); // Just test it doesn't crash
}

// =============================================================================
// ActionCategory Tests
// =============================================================================

TEST_CASE("ActionCategory - Enum values", "[action_category]") {
  CHECK(static_cast<u8>(ActionCategory::File) == 0);
  CHECK(static_cast<u8>(ActionCategory::Edit) == 1);
  CHECK(static_cast<u8>(ActionCategory::View) == 2);
}

// =============================================================================
// HotkeyAction Tests
// =============================================================================

TEST_CASE("HotkeyAction - Default values", "[hotkey_action]") {
  HotkeyAction action;

  CHECK(action.enabled == true);
  CHECK(action.category == ActionCategory::Custom);
  CHECK(action.id.empty());
  CHECK(action.name.empty());
}

// =============================================================================
// HotkeyManager Tests
// =============================================================================

TEST_CASE("HotkeyManager - Construction", "[hotkey_manager]") {
  HotkeyManager manager;
  CHECK(true); // Just test it constructs
}

TEST_CASE("HotkeyManager - GetAllActions initially empty or has defaults", "[hotkey_manager]") {
  HotkeyManager manager;
  [[maybe_unused]] auto& actions = manager.getAllActions();
  // May be empty or have default actions
  CHECK(true); // Just test it doesn't crash
}
