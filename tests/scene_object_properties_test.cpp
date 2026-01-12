/**
 * @file scene_object_properties_test.cpp
 * @brief Unit tests for scene object property registration
 */

#include "NovelMind/core/property_system.hpp"
#include "NovelMind/scene/scene_graph.hpp"
#include "NovelMind/scene/scene_object_properties.hpp"
#include <cassert>
#include <iostream>

using namespace NovelMind;
using namespace NovelMind::scene;

// Test macro
#define TEST_ASSERT(condition, message)                                                            \
  do {                                                                                             \
    if (!(condition)) {                                                                            \
      std::cerr << "ASSERTION FAILED: " << message << "\n";                                        \
      std::cerr << "  at " << __FILE__ << ":" << __LINE__ << "\n";                                 \
      return false;                                                                                \
    }                                                                                              \
  } while (0)

bool testSceneObjectBaseProperties() {
  std::cout << "Testing SceneObjectBase properties...\n";

  registerSceneObjectBaseProperties();

  const TypeInfo* typeInfo = PropertyRegistry::instance().getTypeInfo<SceneObjectBase>();
  TEST_ASSERT(typeInfo != nullptr, "SceneObjectBase should be registered");

  // Check essential properties exist
  TEST_ASSERT(typeInfo->findProperty("x") != nullptr, "x property exists");
  TEST_ASSERT(typeInfo->findProperty("y") != nullptr, "y property exists");
  TEST_ASSERT(typeInfo->findProperty("scaleX") != nullptr, "scaleX property exists");
  TEST_ASSERT(typeInfo->findProperty("alpha") != nullptr, "alpha property exists");
  TEST_ASSERT(typeInfo->findProperty("visible") != nullptr, "visible property exists");

  std::cout << "  ✓ All base properties registered\n";
  return true;
}

bool testCharacterObjectProperties() {
  std::cout << "Testing CharacterObject properties...\n";

  registerCharacterObjectProperties();

  const TypeInfo* typeInfo = PropertyRegistry::instance().getTypeInfo<CharacterObject>();
  TEST_ASSERT(typeInfo != nullptr, "CharacterObject should be registered");

  // Check character-specific properties
  TEST_ASSERT(typeInfo->findProperty("characterId") != nullptr, "characterId property exists");
  TEST_ASSERT(typeInfo->findProperty("displayName") != nullptr, "displayName property exists");
  TEST_ASSERT(typeInfo->findProperty("expression") != nullptr, "expression property exists");
  TEST_ASSERT(typeInfo->findProperty("highlighted") != nullptr, "highlighted property exists");

  std::cout << "  ✓ All character properties registered\n";
  return true;
}

bool testPropertyGettersSetters() {
  std::cout << "Testing property getters and setters...\n";

  registerSceneObjectBaseProperties();

  // Create a test object
  BackgroundObject bg("test_bg");
  bg.setPosition(100.0f, 200.0f);
  bg.setAlpha(0.5f);

  const TypeInfo* typeInfo = PropertyRegistry::instance().getTypeInfo<SceneObjectBase>();
  TEST_ASSERT(typeInfo != nullptr, "TypeInfo available");

  // Test getter
  const auto* xProp = typeInfo->findProperty("x");
  TEST_ASSERT(xProp != nullptr, "x property found");

  PropertyValue xValue = xProp->getValue(&bg);
  auto* xFloat = std::get_if<f32>(&xValue);
  TEST_ASSERT(xFloat != nullptr && *xFloat == 100.0f, "x getter returns correct value");

  // Test setter
  xProp->setValue(&bg, PropertyValue{150.0f});
  TEST_ASSERT(bg.getX() == 150.0f, "x setter modifies object");

  std::cout << "  ✓ Getters and setters work correctly\n";
  return true;
}

bool testPropertyMetadata() {
  std::cout << "Testing property metadata...\n";

  registerCharacterObjectProperties();

  const TypeInfo* typeInfo = PropertyRegistry::instance().getTypeInfo<CharacterObject>();
  TEST_ASSERT(typeInfo != nullptr, "TypeInfo available");

  const auto* nameProp = typeInfo->findProperty("displayName");
  TEST_ASSERT(nameProp != nullptr, "displayName property found");

  const auto& meta = nameProp->getMeta();
  TEST_ASSERT(meta.name == "displayName", "Property name correct");
  TEST_ASSERT(meta.displayName == "Display Name", "Display name correct");
  TEST_ASSERT(meta.type == PropertyType::String, "Type is String");
  TEST_ASSERT(meta.category == "Character", "Category is Character");

  std::cout << "  ✓ Property metadata is correct\n";
  return true;
}

bool testPropertyCategories() {
  std::cout << "Testing property categories...\n";

  registerCharacterObjectProperties();

  const TypeInfo* typeInfo = PropertyRegistry::instance().getTypeInfo<CharacterObject>();
  TEST_ASSERT(typeInfo != nullptr, "TypeInfo available");

  auto categories = typeInfo->getPropertiesByCategory();
  TEST_ASSERT(!categories.empty(), "Categories exist");

  // Check that properties are grouped
  bool foundCharacterCategory = false;
  bool foundAppearanceCategory = false;

  for (const auto& [category, props] : categories) {
    if (category == "Character") {
      foundCharacterCategory = true;
      TEST_ASSERT(!props.empty(), "Character category has properties");
    }
    if (category == "Appearance") {
      foundAppearanceCategory = true;
    }
  }

  TEST_ASSERT(foundCharacterCategory, "Character category exists");
  TEST_ASSERT(foundAppearanceCategory, "Appearance category exists");

  std::cout << "  ✓ Properties correctly categorized\n";
  return true;
}

bool testColorConversion() {
  std::cout << "Testing color conversion...\n";

  registerBackgroundObjectProperties();

  BackgroundObject bg("test_bg");
  bg.setTint(renderer::Color(128, 64, 32, 255));

  const TypeInfo* typeInfo = PropertyRegistry::instance().getTypeInfo<BackgroundObject>();
  TEST_ASSERT(typeInfo != nullptr, "TypeInfo available");

  const auto* tintProp = typeInfo->findProperty("tint");
  TEST_ASSERT(tintProp != nullptr, "tint property found");

  // Get color as property value
  PropertyValue tintValue = tintProp->getValue(&bg);
  auto* color = std::get_if<Color>(&tintValue);
  TEST_ASSERT(color != nullptr, "Color value retrieved");

  // Check conversion (128/255 ≈ 0.502)
  TEST_ASSERT(color->r > 0.49f && color->r < 0.51f, "Red component correct");
  TEST_ASSERT(color->g > 0.24f && color->g < 0.26f, "Green component correct");

  // Set color back
  Color newColor(1.0f, 0.5f, 0.25f, 1.0f);
  tintProp->setValue(&bg, PropertyValue{newColor});

  // Verify it was set
  const auto& tint = bg.getTint();
  TEST_ASSERT(tint.r == 255, "Red component set correctly");
  TEST_ASSERT(tint.g >= 127 && tint.g <= 128, "Green component set correctly");

  std::cout << "  ✓ Color conversion works correctly\n";
  return true;
}

bool testEnumProperties() {
  std::cout << "Testing enum properties...\n";

  registerCharacterObjectProperties();

  CharacterObject character("test_char", "test");
  character.setSlotPosition(CharacterObject::Position::Right);

  const TypeInfo* typeInfo = PropertyRegistry::instance().getTypeInfo<CharacterObject>();
  TEST_ASSERT(typeInfo != nullptr, "TypeInfo available");

  const auto* slotProp = typeInfo->findProperty("slotPosition");
  TEST_ASSERT(slotProp != nullptr, "slotPosition property found");

  // Get enum value
  PropertyValue value = slotProp->getValue(&character);
  auto* enumVal = std::get_if<EnumValue>(&value);
  TEST_ASSERT(enumVal != nullptr, "EnumValue retrieved");
  TEST_ASSERT(enumVal->value == 2, "Enum value correct (Right = 2)");

  // Set enum value
  slotProp->setValue(&character, PropertyValue{EnumValue(0, "Left")});
  TEST_ASSERT(character.getSlotPosition() == CharacterObject::Position::Left, "Enum setter works");

  std::cout << "  ✓ Enum properties work correctly\n";
  return true;
}

int main() {
  std::cout << "========================================\n";
  std::cout << "Scene Object Properties Unit Tests\n";
  std::cout << "========================================\n\n";

  int passed = 0;
  int total = 0;

  auto runTest = [&](bool (*test)(), const char* name) {
    total++;
    std::cout << "\n[" << total << "] " << name << "\n";
    if (test()) {
      passed++;
      std::cout << "✓ PASSED\n";
    } else {
      std::cout << "❌ FAILED\n";
    }
  };

  runTest(testSceneObjectBaseProperties, "SceneObjectBase Properties");
  runTest(testCharacterObjectProperties, "CharacterObject Properties");
  runTest(testPropertyGettersSetters, "Property Getters and Setters");
  runTest(testPropertyMetadata, "Property Metadata");
  runTest(testPropertyCategories, "Property Categories");
  runTest(testColorConversion, "Color Conversion");
  runTest(testEnumProperties, "Enum Properties");

  std::cout << "\n========================================\n";
  std::cout << "Results: " << passed << "/" << total << " tests passed\n";
  std::cout << "========================================\n";

  return (passed == total) ? 0 : 1;
}
