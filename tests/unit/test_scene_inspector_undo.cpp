/**
 * @file test_scene_inspector_undo.cpp
 * @brief Unit tests for SceneInspectorAPI undo/redo functionality
 *
 * Tests cover:
 * - moveObject undo/redo with both X and Y coordinates
 * - scaleObject undo/redo with both scaleX and scaleY
 * - Multiple move operations with undo chain
 * - Integration test: UI move followed by undo
 *
 * Related to Issue #562 - moveObject() only undoes X, not Y
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "NovelMind/scene/scene_inspector.hpp"
#include "NovelMind/scene/scene_graph.hpp"
#include <memory>

using namespace NovelMind::scene;
using namespace NovelMind;

// Test object for scene inspector tests
class TestSceneObject : public SceneObjectBase {
public:
    explicit TestSceneObject(const std::string& id)
        : SceneObjectBase(id, SceneObjectType::Custom) {}

    void render([[maybe_unused]] renderer::IRenderer& renderer) override {}
};

// =============================================================================
// SceneInspectorAPI moveObject Tests
// =============================================================================

TEST_CASE("SceneInspectorAPI moveObject undo restores both X and Y", "[scene_inspector][undo][moveObject]")
{
    SceneGraph graph;
    SceneInspectorAPI inspector(&graph);

    // Create and add a test object
    auto obj = std::make_unique<TestSceneObject>("test_obj");
    graph.addToLayer(LayerType::UI, std::move(obj));

    auto* testObj = graph.findObject("test_obj");
    REQUIRE(testObj != nullptr);

    // Set initial position
    testObj->setPosition(100.0f, 200.0f);
    REQUIRE(testObj->getX() == 100.0f);
    REQUIRE(testObj->getY() == 200.0f);

    // Move object to new position with undo recording
    auto result = inspector.moveObject("test_obj", 300.0f, 400.0f, true);
    REQUIRE(result.isOk());

    // Verify new position
    REQUIRE(testObj->getX() == 300.0f);
    REQUIRE(testObj->getY() == 400.0f);

    // Undo the move
    REQUIRE(inspector.canUndo());
    inspector.undo();

    // CRITICAL: Both X and Y should be restored to original values
    REQUIRE(testObj->getX() == 100.0f);
    REQUIRE(testObj->getY() == 200.0f);
}

TEST_CASE("SceneInspectorAPI moveObject redo works correctly", "[scene_inspector][undo][moveObject][redo]")
{
    SceneGraph graph;
    SceneInspectorAPI inspector(&graph);

    auto obj = std::make_unique<TestSceneObject>("test_obj");
    graph.addToLayer(LayerType::UI, std::move(obj));

    auto* testObj = graph.findObject("test_obj");
    testObj->setPosition(100.0f, 200.0f);

    // Move and undo
    inspector.moveObject("test_obj", 300.0f, 400.0f, true);
    inspector.undo();

    // Verify position after undo
    REQUIRE(testObj->getX() == 100.0f);
    REQUIRE(testObj->getY() == 200.0f);

    // Redo the move
    REQUIRE(inspector.canRedo());
    inspector.redo();

    // Both X and Y should be restored to moved position
    REQUIRE(testObj->getX() == 300.0f);
    REQUIRE(testObj->getY() == 400.0f);
}

TEST_CASE("SceneInspectorAPI multiple moves with undo chain", "[scene_inspector][undo][moveObject][chain]")
{
    SceneGraph graph;
    SceneInspectorAPI inspector(&graph);

    auto obj = std::make_unique<TestSceneObject>("test_obj");
    graph.addToLayer(LayerType::UI, std::move(obj));

    auto* testObj = graph.findObject("test_obj");

    // Perform multiple moves
    testObj->setPosition(0.0f, 0.0f);
    inspector.moveObject("test_obj", 100.0f, 100.0f, true);
    inspector.moveObject("test_obj", 200.0f, 200.0f, true);
    inspector.moveObject("test_obj", 300.0f, 300.0f, true);

    REQUIRE(testObj->getX() == 300.0f);
    REQUIRE(testObj->getY() == 300.0f);

    // Undo once
    inspector.undo();
    REQUIRE(testObj->getX() == 200.0f);
    REQUIRE(testObj->getY() == 200.0f);

    // Undo again
    inspector.undo();
    REQUIRE(testObj->getX() == 100.0f);
    REQUIRE(testObj->getY() == 100.0f);

    // Undo final time
    inspector.undo();
    REQUIRE(testObj->getX() == 0.0f);
    REQUIRE(testObj->getY() == 0.0f);
}

TEST_CASE("SceneInspectorAPI moveObject with recordUndo=false", "[scene_inspector][moveObject]")
{
    SceneGraph graph;
    SceneInspectorAPI inspector(&graph);

    auto obj = std::make_unique<TestSceneObject>("test_obj");
    graph.addToLayer(LayerType::UI, std::move(obj));

    auto* testObj = graph.findObject("test_obj");
    testObj->setPosition(100.0f, 200.0f);

    // Move without recording undo
    inspector.moveObject("test_obj", 300.0f, 400.0f, false);

    REQUIRE(testObj->getX() == 300.0f);
    REQUIRE(testObj->getY() == 400.0f);

    // Should not be able to undo
    REQUIRE_FALSE(inspector.canUndo());
}

// =============================================================================
// SceneInspectorAPI scaleObject Tests
// =============================================================================

TEST_CASE("SceneInspectorAPI scaleObject undo restores both scaleX and scaleY", "[scene_inspector][undo][scaleObject]")
{
    SceneGraph graph;
    SceneInspectorAPI inspector(&graph);

    auto obj = std::make_unique<TestSceneObject>("test_obj");
    graph.addToLayer(LayerType::UI, std::move(obj));

    auto* testObj = graph.findObject("test_obj");
    REQUIRE(testObj != nullptr);

    // Set initial scale
    testObj->setScale(1.0f, 1.0f);
    REQUIRE(testObj->getScaleX() == 1.0f);
    REQUIRE(testObj->getScaleY() == 1.0f);

    // Scale object with undo recording
    auto result = inspector.scaleObject("test_obj", 2.0f, 3.0f, true);
    REQUIRE(result.isOk());

    // Verify new scale
    REQUIRE(testObj->getScaleX() == 2.0f);
    REQUIRE(testObj->getScaleY() == 3.0f);

    // Undo the scale
    REQUIRE(inspector.canUndo());
    inspector.undo();

    // CRITICAL: Both scaleX and scaleY should be restored to original values
    REQUIRE(testObj->getScaleX() == 1.0f);
    REQUIRE(testObj->getScaleY() == 1.0f);
}

TEST_CASE("SceneInspectorAPI scaleObject redo works correctly", "[scene_inspector][undo][scaleObject][redo]")
{
    SceneGraph graph;
    SceneInspectorAPI inspector(&graph);

    auto obj = std::make_unique<TestSceneObject>("test_obj");
    graph.addToLayer(LayerType::UI, std::move(obj));

    auto* testObj = graph.findObject("test_obj");
    testObj->setScale(1.0f, 1.0f);

    // Scale and undo
    inspector.scaleObject("test_obj", 2.5f, 3.5f, true);
    inspector.undo();

    // Verify scale after undo
    REQUIRE(testObj->getScaleX() == 1.0f);
    REQUIRE(testObj->getScaleY() == 1.0f);

    // Redo the scale
    REQUIRE(inspector.canRedo());
    inspector.redo();

    // Both scaleX and scaleY should be restored to scaled values
    REQUIRE(testObj->getScaleX() == 2.5f);
    REQUIRE(testObj->getScaleY() == 3.5f);
}

TEST_CASE("SceneInspectorAPI multiple scales with undo chain", "[scene_inspector][undo][scaleObject][chain]")
{
    SceneGraph graph;
    SceneInspectorAPI inspector(&graph);

    auto obj = std::make_unique<TestSceneObject>("test_obj");
    graph.addToLayer(LayerType::UI, std::move(obj));

    auto* testObj = graph.findObject("test_obj");

    // Perform multiple scales
    testObj->setScale(1.0f, 1.0f);
    inspector.scaleObject("test_obj", 1.5f, 1.5f, true);
    inspector.scaleObject("test_obj", 2.0f, 2.5f, true);
    inspector.scaleObject("test_obj", 3.0f, 4.0f, true);

    REQUIRE(testObj->getScaleX() == 3.0f);
    REQUIRE(testObj->getScaleY() == 4.0f);

    // Undo once
    inspector.undo();
    REQUIRE(testObj->getScaleX() == 2.0f);
    REQUIRE(testObj->getScaleY() == 2.5f);

    // Undo again
    inspector.undo();
    REQUIRE(testObj->getScaleX() == 1.5f);
    REQUIRE(testObj->getScaleY() == 1.5f);

    // Undo final time
    inspector.undo();
    REQUIRE(testObj->getScaleX() == 1.0f);
    REQUIRE(testObj->getScaleY() == 1.0f);
}

// =============================================================================
// CompositeCommand Tests
// =============================================================================

TEST_CASE("CompositeCommand groups multiple commands", "[scene_inspector][composite]")
{
    SceneGraph graph;
    SceneInspectorAPI inspector(&graph);

    auto obj = std::make_unique<TestSceneObject>("test_obj");
    graph.addToLayer(LayerType::UI, std::move(obj));

    auto* testObj = graph.findObject("test_obj");
    testObj->setPosition(0.0f, 0.0f);

    // Create composite command manually
    auto composite = std::make_unique<CompositeCommand>("Test composite");

    auto cmd1 = std::make_unique<SetPropertyCommand>(
        &inspector, "test_obj", "x", "0", "100");
    auto cmd2 = std::make_unique<SetPropertyCommand>(
        &inspector, "test_obj", "y", "0", "200");

    composite->addCommand(std::move(cmd1));
    composite->addCommand(std::move(cmd2));

    // Execute composite
    composite->execute();
    REQUIRE(testObj->getX() == 100.0f);
    REQUIRE(testObj->getY() == 200.0f);

    // Undo composite
    composite->undo();
    REQUIRE(testObj->getX() == 0.0f);
    REQUIRE(testObj->getY() == 0.0f);
}

TEST_CASE("CompositeCommand description", "[scene_inspector][composite]")
{
    auto composite = std::make_unique<CompositeCommand>("Move object test_obj");
    REQUIRE(composite->getDescription() == "Move object test_obj");
}

// =============================================================================
// Error Cases
// =============================================================================

TEST_CASE("SceneInspectorAPI moveObject with non-existent object", "[scene_inspector][error]")
{
    SceneGraph graph;
    SceneInspectorAPI inspector(&graph);

    auto result = inspector.moveObject("nonexistent", 100.0f, 200.0f, true);
    REQUIRE(result.isError());
    REQUIRE(result.getError() == "Object not found: nonexistent");
}

TEST_CASE("SceneInspectorAPI scaleObject with non-existent object", "[scene_inspector][error]")
{
    SceneGraph graph;
    SceneInspectorAPI inspector(&graph);

    auto result = inspector.scaleObject("nonexistent", 2.0f, 3.0f, true);
    REQUIRE(result.isError());
    REQUIRE(result.getError() == "Object not found: nonexistent");
}
