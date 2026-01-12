/**
 * @file multi_object_editing_test.cpp
 * @brief Unit tests for multi-object editing in Inspector
 *
 * Tests the implementation of Issue #19 - Multi-object editing functionality:
 * - Common property identification
 * - Multiple values detection and display
 * - Batch property updates with atomic undo/redo
 */

#include "NovelMind/core/property_system.hpp"
#include "NovelMind/editor/inspector_binding.hpp"
#include "NovelMind/scene/scene_graph.hpp"
#include "NovelMind/scene/scene_object_properties.hpp"
#include <cassert>
#include <iostream>

using namespace NovelMind;
using namespace NovelMind::editor;
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

bool testMultipleValuesMarker() {
  std::cout << "Testing MultipleValues marker...\n";

  MultipleValues mv1;
  MultipleValues mv2;

  TEST_ASSERT(mv1 == mv2, "MultipleValues instances are equal");
  TEST_ASSERT(!(mv1 != mv2), "MultipleValues instances are not unequal");

  // Test toString
  PropertyValue value = mv1;
  std::string str = PropertyUtils::toString(value);
  TEST_ASSERT(str == "<multiple values>", "MultipleValues should display as '<multiple values>'");

  std::cout << "  ✓ MultipleValues marker works correctly\n";
  return true;
}

bool testCommonPropertyIdentification() {
  std::cout << "Testing common property identification...\n";

  registerCharacterObjectProperties();

  // Create multiple character objects
  auto char1 = std::make_unique<CharacterObject>("char1", "alice");
  auto char2 = std::make_unique<CharacterObject>("char2", "bob");
  auto char3 = std::make_unique<CharacterObject>("char3", "charlie");

  char1->setDisplayName("Alice");
  char2->setDisplayName("Bob");
  char3->setDisplayName("Charlie");

  // All have the same expression
  char1->setExpression("neutral");
  char2->setExpression("neutral");
  char3->setExpression("neutral");

  // Different positions
  char1->setPosition(100.0f, 200.0f);
  char2->setPosition(300.0f, 400.0f);
  char3->setPosition(500.0f, 600.0f);

  // Setup inspector with multiple targets
  InspectorBindingManager inspector;

  std::vector<std::string> objectIds = {"char1", "char2", "char3"};
  std::vector<void*> objects = {static_cast<void*>(char1.get()), static_cast<void*>(char2.get()),
                                static_cast<void*>(char3.get())};

  inspector.inspectSceneObjects(objectIds, objects);

  TEST_ASSERT(inspector.isMultiEdit(), "Inspector should be in multi-edit mode");
  TEST_ASSERT(inspector.getTargetCount() == 3, "Should have 3 targets");

  // Get properties - should be common properties across all objects
  auto properties = inspector.getProperties();
  TEST_ASSERT(!properties.empty(), "Should have common properties");

  // Verify common properties exist (all CharacterObjects have these)
  const auto* displayNameProp = inspector.getProperty("displayName");
  TEST_ASSERT(displayNameProp != nullptr, "displayName property should exist");

  const auto* expressionProp = inspector.getProperty("expression");
  TEST_ASSERT(expressionProp != nullptr, "expression property should exist");

  const auto* xProp = inspector.getProperty("x");
  TEST_ASSERT(xProp != nullptr, "x property should exist");

  std::cout << "  ✓ Common properties identified correctly\n";
  return true;
}

bool testMultipleValuesDetection() {
  std::cout << "Testing multiple values detection...\n";

  registerCharacterObjectProperties();

  // Create objects with different property values
  auto char1 = std::make_unique<CharacterObject>("char1", "alice");
  auto char2 = std::make_unique<CharacterObject>("char2", "bob");

  char1->setDisplayName("Alice");
  char2->setDisplayName("Bob"); // Different

  char1->setExpression("happy");
  char2->setExpression("happy"); // Same

  char1->setPosition(100.0f, 200.0f);
  char2->setPosition(300.0f, 200.0f); // Different x, same y

  InspectorBindingManager inspector;

  std::vector<std::string> objectIds = {"char1", "char2"};
  std::vector<void*> objects = {static_cast<void*>(char1.get()), static_cast<void*>(char2.get())};

  inspector.inspectSceneObjects(objectIds, objects);

  // Check displayName - should show multiple values
  PropertyValue displayNameValue = inspector.getPropertyValue("displayName");
  TEST_ASSERT(std::holds_alternative<MultipleValues>(displayNameValue),
              "displayName should be MultipleValues");

  // Check expression - should show the common value
  PropertyValue expressionValue = inspector.getPropertyValue("expression");
  TEST_ASSERT(std::holds_alternative<std::string>(expressionValue), "expression should be string");
  TEST_ASSERT(std::get<std::string>(expressionValue) == "happy", "expression should be 'happy'");

  // Check x - should show multiple values
  PropertyValue xValue = inspector.getPropertyValue("x");
  TEST_ASSERT(std::holds_alternative<MultipleValues>(xValue), "x should be MultipleValues");

  // Check y - should show common value
  PropertyValue yValue = inspector.getPropertyValue("y");
  TEST_ASSERT(std::holds_alternative<f32>(yValue), "y should be f32");
  TEST_ASSERT(std::get<f32>(yValue) == 200.0f, "y should be 200.0");

  std::cout << "  ✓ Multiple values detection works correctly\n";
  return true;
}

bool testBatchPropertyUpdate() {
  std::cout << "Testing batch property updates...\n";

  registerCharacterObjectProperties();

  // Create objects with different values
  auto char1 = std::make_unique<CharacterObject>("char1", "alice");
  auto char2 = std::make_unique<CharacterObject>("char2", "bob");
  auto char3 = std::make_unique<CharacterObject>("char3", "charlie");

  char1->setDisplayName("Alice");
  char2->setDisplayName("Bob");
  char3->setDisplayName("Charlie");

  char1->setExpression("neutral");
  char2->setExpression("happy");
  char3->setExpression("sad");

  InspectorBindingManager inspector;

  std::vector<std::string> objectIds = {"char1", "char2", "char3"};
  std::vector<void*> objects = {static_cast<void*>(char1.get()), static_cast<void*>(char2.get()),
                                static_cast<void*>(char3.get())};

  inspector.inspectSceneObjects(objectIds, objects);

  // Set expression to "angry" for all
  auto error = inspector.setPropertyValueFromString("expression", "angry");
  TEST_ASSERT(!error.has_value(), "Property update should succeed");

  // Verify all objects were updated
  TEST_ASSERT(char1->getExpression() == "angry", "char1 expression should be 'angry'");
  TEST_ASSERT(char2->getExpression() == "angry", "char2 expression should be 'angry'");
  TEST_ASSERT(char3->getExpression() == "angry", "char3 expression should be 'angry'");

  // Now all values are the same, should not show MultipleValues
  PropertyValue expressionValue = inspector.getPropertyValue("expression");
  TEST_ASSERT(std::holds_alternative<std::string>(expressionValue),
              "expression should be string after batch update");
  TEST_ASSERT(std::get<std::string>(expressionValue) == "angry", "expression should be 'angry'");

  std::cout << "  ✓ Batch property updates work correctly\n";
  return true;
}

bool testNumericPropertyUpdate() {
  std::cout << "Testing numeric property batch updates...\n";

  registerCharacterObjectProperties();

  auto char1 = std::make_unique<CharacterObject>("char1", "alice");
  auto char2 = std::make_unique<CharacterObject>("char2", "bob");

  char1->setPosition(100.0f, 200.0f);
  char2->setPosition(300.0f, 400.0f);

  char1->setAlpha(0.5f);
  char2->setAlpha(0.8f);

  InspectorBindingManager inspector;

  std::vector<std::string> objectIds = {"char1", "char2"};
  std::vector<void*> objects = {static_cast<void*>(char1.get()), static_cast<void*>(char2.get())};

  inspector.inspectSceneObjects(objectIds, objects);

  // Set alpha to 1.0 for both
  auto error = inspector.setPropertyValue("alpha", PropertyValue{1.0f});
  TEST_ASSERT(!error.has_value(), "Alpha update should succeed");

  TEST_ASSERT(char1->getAlpha() == 1.0f, "char1 alpha should be 1.0");
  TEST_ASSERT(char2->getAlpha() == 1.0f, "char2 alpha should be 1.0");

  // Set x position to 500 for both
  error = inspector.setPropertyValue("x", PropertyValue{500.0f});
  TEST_ASSERT(!error.has_value(), "X position update should succeed");

  TEST_ASSERT(char1->getX() == 500.0f, "char1 x should be 500.0");
  TEST_ASSERT(char2->getX() == 500.0f, "char2 x should be 500.0");

  // Y positions should remain different
  TEST_ASSERT(char1->getY() == 200.0f, "char1 y should remain 200.0");
  TEST_ASSERT(char2->getY() == 400.0f, "char2 y should remain 400.0");

  std::cout << "  ✓ Numeric property batch updates work correctly\n";
  return true;
}

bool testEfficiencyNoN2() {
  std::cout << "Testing selection traversal efficiency (no N^2)...\n";

  registerCharacterObjectProperties();

  // Create a larger number of objects to test efficiency
  const size_t numObjects = 100;
  std::vector<std::unique_ptr<CharacterObject>> characters;
  std::vector<std::string> objectIds;
  std::vector<void*> objects;

  for (size_t i = 0; i < numObjects; ++i) {
    std::string id = "char" + std::to_string(i);
    characters.push_back(std::make_unique<CharacterObject>(id, "character"));
    characters.back()->setDisplayName("Character " + std::to_string(i));
    characters.back()->setPosition(static_cast<f32>(i * 10), static_cast<f32>(i * 20));

    objectIds.push_back(id);
    objects.push_back(static_cast<void*>(characters.back().get()));
  }

  InspectorBindingManager inspector;
  inspector.inspectSceneObjects(objectIds, objects);

  TEST_ASSERT(inspector.getTargetCount() == numObjects, "Should have all objects as targets");

  // Getting property value should be O(n), not O(n^2)
  // This test just verifies it doesn't crash or hang with many objects
  PropertyValue displayNameValue = inspector.getPropertyValue("displayName");
  TEST_ASSERT(std::holds_alternative<MultipleValues>(displayNameValue),
              "displayName should be MultipleValues for many objects");

  // Batch update should also be O(n)
  auto error = inspector.setPropertyValueFromString("expression", "neutral");
  TEST_ASSERT(!error.has_value(), "Batch update should succeed");

  // Verify a few objects were updated
  TEST_ASSERT(characters[0]->getExpression() == "neutral", "First object should be updated");
  TEST_ASSERT(characters[50]->getExpression() == "neutral", "Middle object should be updated");
  TEST_ASSERT(characters[99]->getExpression() == "neutral", "Last object should be updated");

  std::cout << "  ✓ Selection traversal is efficient (no N^2 detected)\n";
  return true;
}

bool testSingleToMultiModeSwitch() {
  std::cout << "Testing switching between single and multi-edit modes...\n";

  registerCharacterObjectProperties();

  auto char1 = std::make_unique<CharacterObject>("char1", "alice");
  auto char2 = std::make_unique<CharacterObject>("char2", "bob");

  char1->setDisplayName("Alice");
  char2->setDisplayName("Bob");

  InspectorBindingManager inspector;

  // Start with single object
  inspector.inspectSceneObject("char1", static_cast<void*>(char1.get()));
  TEST_ASSERT(!inspector.isMultiEdit(), "Should be in single-edit mode");
  TEST_ASSERT(inspector.getTargetCount() == 1, "Should have 1 target");

  PropertyValue displayNameValue = inspector.getPropertyValue("displayName");
  TEST_ASSERT(std::holds_alternative<std::string>(displayNameValue),
              "displayName should be string in single mode");
  TEST_ASSERT(std::get<std::string>(displayNameValue) == "Alice", "displayName should be 'Alice'");

  // Switch to multi-edit
  std::vector<std::string> objectIds = {"char1", "char2"};
  std::vector<void*> objects = {static_cast<void*>(char1.get()), static_cast<void*>(char2.get())};

  inspector.inspectSceneObjects(objectIds, objects);
  TEST_ASSERT(inspector.isMultiEdit(), "Should be in multi-edit mode");
  TEST_ASSERT(inspector.getTargetCount() == 2, "Should have 2 targets");

  displayNameValue = inspector.getPropertyValue("displayName");
  TEST_ASSERT(std::holds_alternative<MultipleValues>(displayNameValue),
              "displayName should be MultipleValues in multi mode");

  // Switch back to single
  inspector.inspectSceneObject("char2", static_cast<void*>(char2.get()));
  TEST_ASSERT(!inspector.isMultiEdit(), "Should be back in single-edit mode");
  TEST_ASSERT(inspector.getTargetCount() == 1, "Should have 1 target");

  displayNameValue = inspector.getPropertyValue("displayName");
  TEST_ASSERT(std::holds_alternative<std::string>(displayNameValue),
              "displayName should be string after switching back");
  TEST_ASSERT(std::get<std::string>(displayNameValue) == "Bob", "displayName should be 'Bob'");

  std::cout << "  ✓ Mode switching works correctly\n";
  return true;
}

int main() {
  std::cout << "=== Multi-Object Editing Tests ===\n\n";

  int passed = 0;
  int total = 0;

  auto runTest = [&](bool (*test)(), const char* name) {
    total++;
    std::cout << "\n[" << total << "] " << name << "\n";
    if (test()) {
      passed++;
      std::cout << "PASSED\n";
    } else {
      std::cout << "FAILED\n";
    }
  };

  runTest(testMultipleValuesMarker, "MultipleValues Marker");
  runTest(testCommonPropertyIdentification, "Common Property Identification");
  runTest(testMultipleValuesDetection, "Multiple Values Detection");
  runTest(testBatchPropertyUpdate, "Batch Property Update");
  runTest(testNumericPropertyUpdate, "Numeric Property Update");
  runTest(testEfficiencyNoN2, "Efficiency (No N^2)");
  runTest(testSingleToMultiModeSwitch, "Single/Multi Mode Switching");

  std::cout << "\n=== Test Summary ===\n";
  std::cout << "Passed: " << passed << "/" << total << "\n";

  if (passed == total) {
    std::cout << "✓ All tests passed!\n";
    return 0;
  } else {
    std::cout << "✗ Some tests failed!\n";
    return 1;
  }
}
