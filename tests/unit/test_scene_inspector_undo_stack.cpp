/**
 * @file test_scene_inspector_undo_stack.cpp
 * @brief Unit tests for SceneInspectorAPI undo/redo functionality
 *
 * Tests cover:
 * - Undo stack limiting behavior
 * - Order preservation after stack trimming
 * - FIFO removal of oldest entries
 * - Multiple undo/redo operations
 *
 * Related to Issue #563 - Undo stack limiting reverses history order
 */

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "NovelMind/scene/scene_graph.hpp"
#include "NovelMind/scene/scene_inspector.hpp"
#include <memory>

using namespace NovelMind::scene;
using namespace NovelMind;

// Test fixture for SceneInspectorAPI
class SceneInspectorTestFixture {
public:
  SceneInspectorTestFixture() {
    sceneGraph = std::make_unique<SceneGraph>();
    inspector = std::make_unique<SceneInspectorAPI>(sceneGraph.get());
  }

  std::unique_ptr<SceneGraph> sceneGraph;
  std::unique_ptr<SceneInspectorAPI> inspector;
};

TEST_CASE("SceneInspectorAPI - Undo Stack Limiting", "[scene][inspector][undo][bug-563]") {
  SceneInspectorTestFixture fixture;

  SECTION("Stack limiting preserves correct order") {
    // Create a simple command by setting properties
    // We'll use the scene inspector to create objects and modify them

    // First, create enough objects to test with
    std::vector<std::string> objectIds;
    for (int i = 0; i < 10; ++i) {
      auto result = fixture.inspector->createObject(
          LayerType::Character, SceneObjectType::Sprite, "", true);
      REQUIRE(result.isOk());
      objectIds.push_back(result.getValue());
    }

    // Now perform many operations to fill the undo stack beyond the limit
    // Default max history size is 100, so we'll do 105 operations
    for (int i = 0; i < 105; ++i) {
      auto objId = objectIds[i % objectIds.size()];
      fixture.inspector->setProperty(objId, "name",
                                     "Object_" + std::to_string(i), true);
    }

    // The stack should have been limited to 100 entries
    // We can't directly check stack size, but we can verify behavior

    // Undo 3 times - these should undo operations 104, 103, 102
    for (int i = 0; i < 3; ++i) {
      REQUIRE(fixture.inspector->canUndo());
      fixture.inspector->undo();
    }

    // Redo 3 times - these should redo operations 102, 103, 104
    for (int i = 0; i < 3; ++i) {
      REQUIRE(fixture.inspector->canRedo());
      fixture.inspector->redo();
    }

    // Verify the last object has the correct name (from operation 104)
    auto lastObjId = objectIds[104 % objectIds.size()];
    auto nameValue =
        fixture.inspector->getProperty(lastObjId, "name");
    REQUIRE(nameValue.has_value());
    REQUIRE(nameValue.value() == "Object_104");
  }

  SECTION("Undo after stack limiting applies changes in correct order") {
    // Create a test object
    auto result = fixture.inspector->createObject(
        LayerType::Character, SceneObjectType::Sprite, "test_obj", true);
    REQUIRE(result.isOk());
    std::string objId = result.getValue();

    // Set initial value
    fixture.inspector->setProperty(objId, "name", "Initial", true);

    // Perform many operations to exceed stack limit
    for (int i = 0; i < 105; ++i) {
      fixture.inspector->setProperty(objId, "name",
                                     "Value_" + std::to_string(i), true);
    }

    // The stack should now contain operations 6-105 (100 operations)
    // because operations 0-5 were removed (oldest 6 entries)

    // Undo once - should restore to Value_104
    fixture.inspector->undo();
    auto name = fixture.inspector->getProperty(objId, "name");
    REQUIRE(name.has_value());
    REQUIRE(name.value() == "Value_104");

    // Undo again - should restore to Value_103
    fixture.inspector->undo();
    name = fixture.inspector->getProperty(objId, "name");
    REQUIRE(name.has_value());
    REQUIRE(name.value() == "Value_103");

    // Undo 98 more times to get to the oldest entry in stack
    for (int i = 0; i < 98; ++i) {
      fixture.inspector->undo();
    }

    // Should now be at Value_5 (operation 6's undo brings us to Value_5)
    name = fixture.inspector->getProperty(objId, "name");
    REQUIRE(name.has_value());
    REQUIRE(name.value() == "Value_5");

    // Should still be able to undo - this was the create object operation
    REQUIRE(fixture.inspector->canUndo());
  }

  SECTION("FIFO removal - oldest entries are removed first") {
    // Create test objects
    std::vector<std::string> objectIds;
    for (int i = 0; i < 3; ++i) {
      auto result = fixture.inspector->createObject(
          LayerType::Character, SceneObjectType::Sprite,
          "obj_" + std::to_string(i), true);
      REQUIRE(result.isOk());
      objectIds.push_back(result.getValue());
    }

    // Perform 105 property changes on different objects
    for (int i = 0; i < 105; ++i) {
      auto objId = objectIds[i % 3];
      fixture.inspector->setProperty(objId, "name",
                                     "Step_" + std::to_string(i), true);
    }

    // Undo all 100 operations (that fit in the stack)
    int undoCount = 0;
    while (fixture.inspector->canUndo() && undoCount < 100) {
      fixture.inspector->undo();
      undoCount++;
    }

    // Should have been able to undo at least 100 operations
    REQUIRE(undoCount == 100);

    // After undoing all, we should still be able to undo
    // (the object creations are still in the stack)
    REQUIRE(fixture.inspector->canUndo());
  }

  SECTION("Multiple cycles of stack limiting maintain order") {
    // Create a test object
    auto result = fixture.inspector->createObject(
        LayerType::Character, SceneObjectType::Sprite, "test_obj", true);
    REQUIRE(result.isOk());
    std::string objId = result.getValue();

    // First batch: operations 0-104 (105 ops, stack limited to 100)
    for (int i = 0; i < 105; ++i) {
      fixture.inspector->setProperty(objId, "name",
                                     "Batch1_" + std::to_string(i), true);
    }

    // Second batch: 10 more operations (stack re-limited)
    for (int i = 0; i < 10; ++i) {
      fixture.inspector->setProperty(objId, "name",
                                     "Batch2_" + std::to_string(i), true);
    }

    // Current name should be Batch2_9
    auto name = fixture.inspector->getProperty(objId, "name");
    REQUIRE(name.has_value());
    REQUIRE(name.value() == "Batch2_9");

    // Undo once - should restore to Batch2_8
    fixture.inspector->undo();
    name = fixture.inspector->getProperty(objId, "name");
    REQUIRE(name.has_value());
    REQUIRE(name.value() == "Batch2_8");

    // Undo 9 more times to get to last Batch1 operation
    for (int i = 0; i < 9; ++i) {
      fixture.inspector->undo();
    }

    // Should now be at Batch1_104
    name = fixture.inspector->getProperty(objId, "name");
    REQUIRE(name.has_value());
    REQUIRE(name.value() == "Batch1_104");
  }

  SECTION("Undo/Redo order consistency after limiting") {
    // Create a test object
    auto result = fixture.inspector->createObject(
        LayerType::Character, SceneObjectType::Sprite, "test_obj", true);
    REQUIRE(result.isOk());
    std::string objId = result.getValue();

    // Perform operations beyond stack limit
    for (int i = 0; i < 110; ++i) {
      fixture.inspector->setProperty(objId, "name",
                                     "Op_" + std::to_string(i), true);
    }

    // Undo 5 times
    for (int i = 0; i < 5; ++i) {
      fixture.inspector->undo();
    }

    // Current state should be Op_104
    auto name = fixture.inspector->getProperty(objId, "name");
    REQUIRE(name.has_value());
    REQUIRE(name.value() == "Op_104");

    // Redo 5 times
    for (int i = 0; i < 5; ++i) {
      fixture.inspector->redo();
    }

    // Should be back to Op_109
    name = fixture.inspector->getProperty(objId, "name");
    REQUIRE(name.has_value());
    REQUIRE(name.value() == "Op_109");

    // Undo once more
    fixture.inspector->undo();
    name = fixture.inspector->getProperty(objId, "name");
    REQUIRE(name.has_value());
    REQUIRE(name.value() == "Op_108");
  }

  SECTION("Empty undo stack after clearing history") {
    // Create object and perform operations
    auto result = fixture.inspector->createObject(
        LayerType::Character, SceneObjectType::Sprite, "test_obj", true);
    REQUIRE(result.isOk());
    std::string objId = result.getValue();

    fixture.inspector->setProperty(objId, "name", "Test", true);
    REQUIRE(fixture.inspector->canUndo());

    // Clear history
    fixture.inspector->clearHistory();

    // Should not be able to undo
    REQUIRE_FALSE(fixture.inspector->canUndo());
    REQUIRE_FALSE(fixture.inspector->canRedo());
  }
}

TEST_CASE("SceneInspectorAPI - Basic Undo/Redo Operations",
          "[scene][inspector][undo]") {
  SceneInspectorTestFixture fixture;

  SECTION("Single property change undo/redo") {
    auto result = fixture.inspector->createObject(
        LayerType::Character, SceneObjectType::Sprite, "test_obj", true);
    REQUIRE(result.isOk());
    std::string objId = result.getValue();

    // Set property
    fixture.inspector->setProperty(objId, "name", "NewName", true);

    // Verify property was set
    auto name = fixture.inspector->getProperty(objId, "name");
    REQUIRE(name.has_value());
    REQUIRE(name.value() == "NewName");

    // Undo
    REQUIRE(fixture.inspector->canUndo());
    fixture.inspector->undo();

    // Property should be reverted
    name = fixture.inspector->getProperty(objId, "name");
    REQUIRE(name.has_value());
    REQUIRE(name.value() != "NewName");

    // Redo
    REQUIRE(fixture.inspector->canRedo());
    fixture.inspector->redo();

    // Property should be NewName again
    name = fixture.inspector->getProperty(objId, "name");
    REQUIRE(name.has_value());
    REQUIRE(name.value() == "NewName");
  }

  SECTION("Object creation undo/redo") {
    // Create object
    auto result = fixture.inspector->createObject(
        LayerType::Character, SceneObjectType::Sprite, "test_obj", true);
    REQUIRE(result.isOk());
    std::string objId = result.getValue();

    // Verify object exists
    auto obj = fixture.inspector->getObject(objId);
    REQUIRE(obj.has_value());

    // Undo creation
    REQUIRE(fixture.inspector->canUndo());
    fixture.inspector->undo();

    // Object should not exist
    obj = fixture.inspector->getObject(objId);
    REQUIRE_FALSE(obj.has_value());

    // Redo creation
    REQUIRE(fixture.inspector->canRedo());
    fixture.inspector->redo();

    // Object should exist again
    obj = fixture.inspector->getObject(objId);
    REQUIRE(obj.has_value());
  }

  SECTION("Sequential property changes maintain order") {
    auto result = fixture.inspector->createObject(
        LayerType::Character, SceneObjectType::Sprite, "test_obj", true);
    REQUIRE(result.isOk());
    std::string objId = result.getValue();

    // Set property multiple times
    fixture.inspector->setProperty(objId, "name", "First", true);
    fixture.inspector->setProperty(objId, "name", "Second", true);
    fixture.inspector->setProperty(objId, "name", "Third", true);

    // Verify final state
    auto name = fixture.inspector->getProperty(objId, "name");
    REQUIRE(name.has_value());
    REQUIRE(name.value() == "Third");

    // Undo once - should be "Second"
    fixture.inspector->undo();
    name = fixture.inspector->getProperty(objId, "name");
    REQUIRE(name.has_value());
    REQUIRE(name.value() == "Second");

    // Undo again - should be "First"
    fixture.inspector->undo();
    name = fixture.inspector->getProperty(objId, "name");
    REQUIRE(name.has_value());
    REQUIRE(name.value() == "First");

    // Undo once more - should be back to original/default
    fixture.inspector->undo();
    name = fixture.inspector->getProperty(objId, "name");
    REQUIRE(name.has_value());
    REQUIRE(name.value() != "First");
  }
}
