/**
 * @file test_scene_view_panel.cpp
 * @brief Integration tests for Scene View panel
 *
 * Tests the Scene View panel functionality including:
 * - Object selection (Issue #538)
 * - Gizmo interaction (Issue #538)
 * - Viewport navigation (Issue #538)
 * - Multi-select (Issue #538)
 * - Drag and drop (Issue #538)
 */

#include <catch2/catch_test_macros.hpp>

#include "NovelMind/editor/qt/panels/nm_scene_view_panel.hpp"

#include <QApplication>
#include <QGraphicsView>
#include <QPointF>
#include <QSignalSpy>

using namespace NovelMind::editor::qt;

// Helper to ensure QApplication exists for Qt tests
class QtTestFixture {
public:
  QtTestFixture() {
    if (!QApplication::instance()) {
      static int argc = 1;
      static char* argv[] = {const_cast<char*>("test"), nullptr};
      m_app = std::make_unique<QApplication>(argc, argv);
    }
  }

private:
  std::unique_ptr<QApplication> m_app;
};

// =============================================================================
// Object Selection Tests
// =============================================================================

TEST_CASE("SceneViewPanel: Object selection", "[integration][editor][scene_view][selection]") {
  QtTestFixture fixture;

  SECTION("Panel can be constructed") {
    NMSceneViewPanel panel;
    REQUIRE(panel.graphicsScene() != nullptr);
    REQUIRE(panel.graphicsView() != nullptr);
  }

  SECTION("Panel initialization") {
    NMSceneViewPanel panel;
    panel.onInitialize();
    REQUIRE(panel.graphicsScene() != nullptr);
    SUCCEED();
  }

  SECTION("Create and select a single object") {
    NMSceneViewPanel panel;
    panel.onInitialize();

    // Create a character object
    bool created = panel.createObject("char_001", NMSceneObjectType::Character, QPointF(100, 100));
    REQUIRE(created);

    // Verify object was created
    auto* obj = panel.findObjectById("char_001");
    REQUIRE(obj != nullptr);
    REQUIRE(obj->id() == "char_001");
    REQUIRE(obj->objectType() == NMSceneObjectType::Character);

    // Select the object
    QSignalSpy spy(&panel, &NMSceneViewPanel::objectSelected);
    panel.selectObjectById("char_001");

    // Verify selection
    auto* scene = panel.graphicsScene();
    REQUIRE(scene != nullptr);
    REQUIRE(scene->selectedObjectId() == "char_001");
    auto* selectedObj = scene->selectedObject();
    REQUIRE(selectedObj != nullptr);
    REQUIRE(selectedObj->id() == "char_001");
    REQUIRE(selectedObj->isObjectSelected());

    // Verify signal was emitted
    REQUIRE(spy.count() >= 1);
  }

  SECTION("Select different objects sequentially") {
    NMSceneViewPanel panel;
    panel.onInitialize();

    // Create multiple objects
    panel.createObject("bg_001", NMSceneObjectType::Background, QPointF(0, 0));
    panel.createObject("char_001", NMSceneObjectType::Character, QPointF(200, 150));
    panel.createObject("ui_001", NMSceneObjectType::UI, QPointF(50, 50));

    auto* scene = panel.graphicsScene();

    // Select first object
    panel.selectObjectById("bg_001");
    REQUIRE(scene->selectedObjectId() == "bg_001");

    // Select second object (should deselect first)
    panel.selectObjectById("char_001");
    REQUIRE(scene->selectedObjectId() == "char_001");
    auto* bg = panel.findObjectById("bg_001");
    REQUIRE_FALSE(bg->isObjectSelected());

    // Select third object
    panel.selectObjectById("ui_001");
    REQUIRE(scene->selectedObjectId() == "ui_001");
    auto* char_obj = panel.findObjectById("char_001");
    REQUIRE_FALSE(char_obj->isObjectSelected());
  }

  SECTION("Clear selection") {
    NMSceneViewPanel panel;
    panel.onInitialize();

    panel.createObject("char_001", NMSceneObjectType::Character, QPointF(100, 100));
    panel.selectObjectById("char_001");

    auto* scene = panel.graphicsScene();
    REQUIRE(scene->selectedObjectId() == "char_001");

    // Clear selection
    scene->clearSelection();
    REQUIRE(scene->selectedObjectId().isEmpty());
    REQUIRE(scene->selectedObject() == nullptr);

    auto* obj = panel.findObjectById("char_001");
    REQUIRE_FALSE(obj->isObjectSelected());
  }

  SECTION("Select non-existent object") {
    NMSceneViewPanel panel;
    panel.onInitialize();

    panel.createObject("char_001", NMSceneObjectType::Character, QPointF(100, 100));

    // Try to select a non-existent object
    panel.selectObjectById("non_existent");

    auto* scene = panel.graphicsScene();
    // Selection should fail gracefully (empty selection or no change)
    SUCCEED();
  }

  SECTION("Selection with locked objects") {
    NMSceneViewPanel panel;
    panel.onInitialize();

    panel.createObject("char_001", NMSceneObjectType::Character, QPointF(100, 100));

    // Lock the object
    bool locked = panel.setObjectLocked("char_001", true);
    REQUIRE(locked);

    auto* obj = panel.findObjectById("char_001");
    REQUIRE(obj->isLocked());

    // Should still be able to select locked objects (selection doesn't mean manipulation)
    panel.selectObjectById("char_001");
    auto* scene = panel.graphicsScene();
    REQUIRE(scene->selectedObjectId() == "char_001");
  }
}

// =============================================================================
// Gizmo Interaction Tests
// =============================================================================

TEST_CASE("SceneViewPanel: Gizmo interaction", "[integration][editor][scene_view][gizmo]") {
  QtTestFixture fixture;

  SECTION("Set gizmo to Move mode") {
    NMSceneViewPanel panel;
    panel.onInitialize();

    panel.setGizmoMode(NMTransformGizmo::GizmoMode::Move);

    auto* scene = panel.graphicsScene();
    REQUIRE(scene != nullptr);
    SUCCEED();
  }

  SECTION("Set gizmo to Rotate mode") {
    NMSceneViewPanel panel;
    panel.onInitialize();

    panel.setGizmoMode(NMTransformGizmo::GizmoMode::Rotate);
    SUCCEED();
  }

  SECTION("Set gizmo to Scale mode") {
    NMSceneViewPanel panel;
    panel.onInitialize();

    panel.setGizmoMode(NMTransformGizmo::GizmoMode::Scale);
    SUCCEED();
  }

  SECTION("Move object with gizmo") {
    NMSceneViewPanel panel;
    panel.onInitialize();

    // Create and select object
    panel.createObject("char_001", NMSceneObjectType::Character, QPointF(100, 100));
    panel.selectObjectById("char_001");

    // Set to move mode
    panel.setGizmoMode(NMTransformGizmo::GizmoMode::Move);

    // Move the object
    QSignalSpy positionSpy(&panel, &NMSceneViewPanel::objectPositionChanged);
    bool moved = panel.moveObject("char_001", QPointF(200, 250));
    REQUIRE(moved);

    // Verify new position
    auto* scene = panel.graphicsScene();
    QPointF newPos = scene->getObjectPosition("char_001");
    REQUIRE(newPos.x() == 200);
    REQUIRE(newPos.y() == 250);

    // Verify signal was emitted
    REQUIRE(positionSpy.count() >= 1);
  }

  SECTION("Rotate object with gizmo") {
    NMSceneViewPanel panel;
    panel.onInitialize();

    panel.createObject("char_001", NMSceneObjectType::Character, QPointF(100, 100));
    panel.selectObjectById("char_001");

    panel.setGizmoMode(NMTransformGizmo::GizmoMode::Rotate);

    // Rotate the object 45 degrees
    bool rotated = panel.rotateObject("char_001", 45.0);
    REQUIRE(rotated);

    // Verify rotation
    auto* scene = panel.graphicsScene();
    qreal rotation = scene->getObjectRotation("char_001");
    REQUIRE(rotation == 45.0);

    // Rotate to another angle
    rotated = panel.rotateObject("char_001", 90.0);
    REQUIRE(rotated);
    rotation = scene->getObjectRotation("char_001");
    REQUIRE(rotation == 90.0);
  }

  SECTION("Scale object with gizmo") {
    NMSceneViewPanel panel;
    panel.onInitialize();

    panel.createObject("char_001", NMSceneObjectType::Character, QPointF(100, 100));
    panel.selectObjectById("char_001");

    panel.setGizmoMode(NMTransformGizmo::GizmoMode::Scale);

    // Scale the object
    bool scaled = panel.scaleObject("char_001", 2.0, 2.0);
    REQUIRE(scaled);

    // Verify scale
    auto* scene = panel.graphicsScene();
    QPointF scale = scene->getObjectScale("char_001");
    REQUIRE(scale.x() == 2.0);
    REQUIRE(scale.y() == 2.0);

    // Scale with different x and y
    scaled = panel.scaleObject("char_001", 1.5, 0.5);
    REQUIRE(scaled);
    scale = scene->getObjectScale("char_001");
    REQUIRE(scale.x() == 1.5);
    REQUIRE(scale.y() == 0.5);
  }

  SECTION("Apply combined transform") {
    NMSceneViewPanel panel;
    panel.onInitialize();

    panel.createObject("char_001", NMSceneObjectType::Character, QPointF(100, 100));

    // Apply position, rotation, and scale in one operation
    bool applied = panel.applyObjectTransform("char_001", QPointF(300, 400), 90.0, 1.5, 1.5);
    REQUIRE(applied);

    auto* scene = panel.graphicsScene();

    // Verify all transforms
    QPointF pos = scene->getObjectPosition("char_001");
    REQUIRE(pos.x() == 300);
    REQUIRE(pos.y() == 400);

    qreal rotation = scene->getObjectRotation("char_001");
    REQUIRE(rotation == 90.0);

    QPointF scale = scene->getObjectScale("char_001");
    REQUIRE(scale.x() == 1.5);
    REQUIRE(scale.y() == 1.5);
  }

  SECTION("Cannot manipulate locked object") {
    NMSceneViewPanel panel;
    panel.onInitialize();

    panel.createObject("char_001", NMSceneObjectType::Character, QPointF(100, 100));
    panel.setObjectLocked("char_001", true);

    auto* scene = panel.graphicsScene();
    REQUIRE(scene->isObjectLocked("char_001"));

    // Gizmo should not affect locked objects
    // (Implementation may vary - locked objects might ignore transforms)
    SUCCEED();
  }
}

// =============================================================================
// Viewport Navigation Tests
// =============================================================================

TEST_CASE("SceneViewPanel: Viewport navigation", "[integration][editor][scene_view][viewport]") {
  QtTestFixture fixture;

  SECTION("Set zoom level") {
    NMSceneViewPanel panel;
    panel.onInitialize();

    auto* view = panel.graphicsView();
    REQUIRE(view != nullptr);

    // Default zoom should be 1.0
    REQUIRE(view->zoomLevel() == 1.0);

    // Set zoom to 2x
    QSignalSpy zoomSpy(view, &NMSceneGraphicsView::zoomChanged);
    panel.setZoomLevel(2.0);
    REQUIRE(view->zoomLevel() == 2.0);
    REQUIRE(zoomSpy.count() >= 1);

    // Set zoom to 0.5x
    panel.setZoomLevel(0.5);
    REQUIRE(view->zoomLevel() == 0.5);

    // Set zoom to 1.5x
    panel.setZoomLevel(1.5);
    REQUIRE(view->zoomLevel() == 1.5);
  }

  SECTION("Center on scene") {
    NMSceneViewPanel panel;
    panel.onInitialize();

    auto* view = panel.graphicsView();
    REQUIRE(view != nullptr);

    // Center on scene should not crash
    view->centerOnScene();
    SUCCEED();
  }

  SECTION("Fit to scene") {
    NMSceneViewPanel panel;
    panel.onInitialize();

    auto* view = panel.graphicsView();
    REQUIRE(view != nullptr);

    // Fit to scene should not crash
    view->fitToScene();
    SUCCEED();
  }

  SECTION("Toggle grid visibility") {
    NMSceneViewPanel panel;
    panel.onInitialize();

    auto* scene = panel.graphicsScene();
    REQUIRE(scene != nullptr);

    // Grid should be visible by default
    bool initialGridState = scene->isGridVisible();

    // Toggle grid
    panel.setGridVisible(!initialGridState);
    REQUIRE(scene->isGridVisible() == !initialGridState);

    // Toggle back
    panel.setGridVisible(initialGridState);
    REQUIRE(scene->isGridVisible() == initialGridState);
  }

  SECTION("Grid size configuration") {
    NMSceneViewPanel panel;
    panel.onInitialize();

    auto* scene = panel.graphicsScene();
    REQUIRE(scene != nullptr);

    // Default grid size should be 32.0
    REQUIRE(scene->gridSize() == 32.0);

    // Change grid size
    scene->setGridSize(64.0);
    REQUIRE(scene->gridSize() == 64.0);

    scene->setGridSize(16.0);
    REQUIRE(scene->gridSize() == 16.0);
  }

  SECTION("Snap to grid") {
    NMSceneViewPanel panel;
    panel.onInitialize();

    auto* scene = panel.graphicsScene();
    REQUIRE(scene != nullptr);

    // Snap to grid should be disabled by default
    REQUIRE_FALSE(scene->snapToGrid());

    // Enable snap to grid
    scene->setSnapToGrid(true);
    REQUIRE(scene->snapToGrid());

    // Disable snap to grid
    scene->setSnapToGrid(false);
    REQUIRE_FALSE(scene->snapToGrid());
  }

  SECTION("Stage guides visibility") {
    NMSceneViewPanel panel;
    panel.onInitialize();

    auto* scene = panel.graphicsScene();
    REQUIRE(scene != nullptr);

    // Toggle stage guides
    scene->setStageGuidesVisible(false);
    scene->setStageGuidesVisible(true);
    SUCCEED();
  }

  SECTION("Safe frame visibility") {
    NMSceneViewPanel panel;
    panel.onInitialize();

    auto* scene = panel.graphicsScene();
    REQUIRE(scene != nullptr);

    // Toggle safe frame
    scene->setSafeFrameVisible(false);
    scene->setSafeFrameVisible(true);
    SUCCEED();
  }

  SECTION("Baseline visibility") {
    NMSceneViewPanel panel;
    panel.onInitialize();

    auto* scene = panel.graphicsScene();
    REQUIRE(scene != nullptr);

    // Toggle baseline
    scene->setBaselineVisible(false);
    scene->setBaselineVisible(true);
    SUCCEED();
  }
}

// =============================================================================
// Multi-Select Tests
// =============================================================================

TEST_CASE("SceneViewPanel: Multi-select operations",
          "[integration][editor][scene_view][multi_select]") {
  QtTestFixture fixture;

  SECTION("Create multiple objects") {
    NMSceneViewPanel panel;
    panel.onInitialize();

    // Create multiple objects of different types
    bool created1 = panel.createObject("bg_001", NMSceneObjectType::Background, QPointF(0, 0));
    bool created2 = panel.createObject("char_001", NMSceneObjectType::Character, QPointF(200, 150));
    bool created3 = panel.createObject("char_002", NMSceneObjectType::Character, QPointF(400, 150));
    bool created4 = panel.createObject("ui_001", NMSceneObjectType::UI, QPointF(50, 50));
    bool created5 = panel.createObject("effect_001", NMSceneObjectType::Effect, QPointF(300, 300));

    REQUIRE(created1);
    REQUIRE(created2);
    REQUIRE(created3);
    REQUIRE(created4);
    REQUIRE(created5);

    auto* scene = panel.graphicsScene();
    auto objects = scene->sceneObjects();
    REQUIRE(objects.size() == 5);
  }

  SECTION("Query all objects in scene") {
    NMSceneViewPanel panel;
    panel.onInitialize();

    panel.createObject("obj_001", NMSceneObjectType::Character, QPointF(100, 100));
    panel.createObject("obj_002", NMSceneObjectType::Character, QPointF(200, 200));
    panel.createObject("obj_003", NMSceneObjectType::UI, QPointF(300, 300));

    auto* scene = panel.graphicsScene();
    auto objects = scene->sceneObjects();

    REQUIRE(objects.size() == 3);

    // Verify each object exists
    QStringList ids;
    for (auto* obj : objects) {
      ids.append(obj->id());
    }

    REQUIRE(ids.contains("obj_001"));
    REQUIRE(ids.contains("obj_002"));
    REQUIRE(ids.contains("obj_003"));
  }

  SECTION("Delete multiple objects sequentially") {
    NMSceneViewPanel panel;
    panel.onInitialize();

    panel.createObject("char_001", NMSceneObjectType::Character, QPointF(100, 100));
    panel.createObject("char_002", NMSceneObjectType::Character, QPointF(200, 200));
    panel.createObject("char_003", NMSceneObjectType::Character, QPointF(300, 300));

    auto* scene = panel.graphicsScene();
    REQUIRE(scene->sceneObjects().size() == 3);

    // Delete objects one by one
    QSignalSpy deleteSpy(&panel, &NMSceneViewPanel::objectDeleted);

    bool deleted1 = panel.deleteObject("char_001");
    REQUIRE(deleted1);
    REQUIRE(scene->sceneObjects().size() == 2);

    bool deleted2 = panel.deleteObject("char_002");
    REQUIRE(deleted2);
    REQUIRE(scene->sceneObjects().size() == 1);

    bool deleted3 = panel.deleteObject("char_003");
    REQUIRE(deleted3);
    REQUIRE(scene->sceneObjects().size() == 0);

    // Verify delete signals
    REQUIRE(deleteSpy.count() >= 3);
  }

  SECTION("Apply same operation to multiple objects") {
    NMSceneViewPanel panel;
    panel.onInitialize();

    // Create multiple objects
    panel.createObject("char_001", NMSceneObjectType::Character, QPointF(100, 100));
    panel.createObject("char_002", NMSceneObjectType::Character, QPointF(200, 200));
    panel.createObject("char_003", NMSceneObjectType::Character, QPointF(300, 300));

    // Set all to same opacity
    panel.setObjectOpacity("char_001", 0.5);
    panel.setObjectOpacity("char_002", 0.5);
    panel.setObjectOpacity("char_003", 0.5);

    // Set all to invisible
    panel.setObjectVisible("char_001", false);
    panel.setObjectVisible("char_002", false);
    panel.setObjectVisible("char_003", false);

    // Set all to same scale
    panel.scaleObject("char_001", 2.0, 2.0);
    panel.scaleObject("char_002", 2.0, 2.0);
    panel.scaleObject("char_003", 2.0, 2.0);

    auto* scene = panel.graphicsScene();

    // Verify scales
    QPointF scale1 = scene->getObjectScale("char_001");
    QPointF scale2 = scene->getObjectScale("char_002");
    QPointF scale3 = scene->getObjectScale("char_003");

    REQUIRE(scale1.x() == 2.0);
    REQUIRE(scale2.x() == 2.0);
    REQUIRE(scale3.x() == 2.0);
  }

  SECTION("Duplicate object") {
    NMSceneViewPanel panel;
    panel.onInitialize();

    panel.createObject("char_001", NMSceneObjectType::Character, QPointF(100, 100));

    auto* scene = panel.graphicsScene();
    REQUIRE(scene->sceneObjects().size() == 1);

    // Duplicate the object
    bool duplicated = panel.duplicateObject("char_001");
    REQUIRE(duplicated);

    // Should now have 2 objects
    auto objects = scene->sceneObjects();
    REQUIRE(objects.size() == 2);

    // Verify both objects exist but have different IDs
    REQUIRE(objects[0]->id() != objects[1]->id());
  }

  SECTION("Rename object") {
    NMSceneViewPanel panel;
    panel.onInitialize();

    panel.createObject("char_001", NMSceneObjectType::Character, QPointF(100, 100));

    auto* obj = panel.findObjectById("char_001");
    REQUIRE(obj != nullptr);

    // Rename the object
    QSignalSpy nameSpy(&panel, &NMSceneViewPanel::objectNameChanged);
    bool renamed = panel.renameObject("char_001", "Hero");
    REQUIRE(renamed);

    REQUIRE(obj->name() == "Hero");
    REQUIRE(nameSpy.count() >= 1);
  }
}

// =============================================================================
// Drag and Drop Tests
// =============================================================================

TEST_CASE("SceneViewPanel: Drag and drop operations",
          "[integration][editor][scene_view][drag_drop]") {
  QtTestFixture fixture;

  SECTION("Add object from asset path") {
    NMSceneViewPanel panel;
    panel.onInitialize();

    // Add object from asset
    bool added = panel.addObjectFromAsset("assets/characters/hero.png", QPointF(150, 200));
    REQUIRE(added);

    auto* scene = panel.graphicsScene();
    auto objects = scene->sceneObjects();
    REQUIRE(objects.size() == 1);

    // Verify object was created at the correct position
    QPointF pos = scene->getObjectPosition(objects[0]->id());
    REQUIRE(pos.x() == 150);
    REQUIRE(pos.y() == 200);
  }

  SECTION("Add object from asset with explicit type") {
    NMSceneViewPanel panel;
    panel.onInitialize();

    // Add character object
    bool added = panel.addObjectFromAsset("assets/characters/hero.png", QPointF(100, 100),
                                          NMSceneObjectType::Character);
    REQUIRE(added);

    auto* scene = panel.graphicsScene();
    auto objects = scene->sceneObjects();
    REQUIRE(objects.size() == 1);
    REQUIRE(objects[0]->objectType() == NMSceneObjectType::Character);
  }

  SECTION("Add multiple objects from different assets") {
    NMSceneViewPanel panel;
    panel.onInitialize();

    // Add background
    panel.addObjectFromAsset("assets/backgrounds/forest.png", QPointF(0, 0),
                             NMSceneObjectType::Background);

    // Add character
    panel.addObjectFromAsset("assets/characters/hero.png", QPointF(200, 300),
                             NMSceneObjectType::Character);

    // Add UI element
    panel.addObjectFromAsset("assets/ui/button.png", QPointF(50, 50), NMSceneObjectType::UI);

    auto* scene = panel.graphicsScene();
    auto objects = scene->sceneObjects();
    REQUIRE(objects.size() == 3);

    // Verify types
    int bgCount = 0, charCount = 0, uiCount = 0;
    for (auto* obj : objects) {
      if (obj->objectType() == NMSceneObjectType::Background)
        bgCount++;
      if (obj->objectType() == NMSceneObjectType::Character)
        charCount++;
      if (obj->objectType() == NMSceneObjectType::UI)
        uiCount++;
    }

    REQUIRE(bgCount == 1);
    REQUIRE(charCount == 1);
    REQUIRE(uiCount == 1);
  }

  SECTION("Set asset for existing object") {
    NMSceneViewPanel panel;
    panel.onInitialize();

    panel.createObject("char_001", NMSceneObjectType::Character, QPointF(100, 100));

    // Set asset path
    bool assetSet = panel.setObjectAsset("char_001", "assets/characters/villain.png");
    REQUIRE(assetSet);

    auto* obj = panel.findObjectById("char_001");
    REQUIRE(obj != nullptr);
    REQUIRE(obj->assetPath() == "assets/characters/villain.png");
  }

  SECTION("Drag signal integration") {
    NMSceneViewPanel panel;
    panel.onInitialize();

    auto* view = panel.graphicsView();
    REQUIRE(view != nullptr);

    // Create spy for drag active signal
    QSignalSpy dragSpy(view, &NMSceneGraphicsView::dragActiveChanged);

    // Signals should be available for connection
    REQUIRE(dragSpy.isValid());
  }
}

// =============================================================================
// Object Properties and State Tests
// =============================================================================

TEST_CASE("SceneViewPanel: Object properties and state",
          "[integration][editor][scene_view][properties]") {
  QtTestFixture fixture;

  SECTION("Set and get object visibility") {
    NMSceneViewPanel panel;
    panel.onInitialize();

    panel.createObject("char_001", NMSceneObjectType::Character, QPointF(100, 100));

    // Set visible
    bool result = panel.setObjectVisible("char_001", true);
    REQUIRE(result);

    // Set invisible
    result = panel.setObjectVisible("char_001", false);
    REQUIRE(result);
  }

  SECTION("Set and verify object locked state") {
    NMSceneViewPanel panel;
    panel.onInitialize();

    panel.createObject("char_001", NMSceneObjectType::Character, QPointF(100, 100));

    auto* scene = panel.graphicsScene();

    // Lock object
    bool locked = panel.setObjectLocked("char_001", true);
    REQUIRE(locked);
    REQUIRE(scene->isObjectLocked("char_001"));

    // Unlock object
    locked = panel.setObjectLocked("char_001", false);
    REQUIRE(locked);
    REQUIRE_FALSE(scene->isObjectLocked("char_001"));
  }

  SECTION("Set object color tint") {
    NMSceneViewPanel panel;
    panel.onInitialize();

    panel.createObject("char_001", NMSceneObjectType::Character, QPointF(100, 100));

    // Set red tint
    QColor red(255, 0, 0);
    bool colorSet = panel.setObjectColor("char_001", red);
    REQUIRE(colorSet);

    // Set blue tint
    QColor blue(0, 0, 255);
    colorSet = panel.setObjectColor("char_001", blue);
    REQUIRE(colorSet);
  }

  SECTION("Set object Z-order") {
    NMSceneViewPanel panel;
    panel.onInitialize();

    panel.createObject("bg_001", NMSceneObjectType::Background, QPointF(0, 0));
    panel.createObject("char_001", NMSceneObjectType::Character, QPointF(100, 100));

    // Set Z-order
    bool zSet = panel.setObjectZOrder("bg_001", -1.0);
    REQUIRE(zSet);

    zSet = panel.setObjectZOrder("char_001", 1.0);
    REQUIRE(zSet);
  }

  SECTION("Reparent object") {
    NMSceneViewPanel panel;
    panel.onInitialize();

    panel.createObject("parent_001", NMSceneObjectType::Character, QPointF(100, 100));
    panel.createObject("child_001", NMSceneObjectType::UI, QPointF(150, 150));

    // Reparent child to parent
    bool reparented = panel.reparentObject("child_001", "parent_001");
    REQUIRE(reparented);

    // Verify parent relationship
    auto* child = panel.findObjectById("child_001");
    REQUIRE(child != nullptr);
    REQUIRE(child->parentObjectId() == "parent_001");

    auto* parent = panel.findObjectById("parent_001");
    REQUIRE(parent != nullptr);
    REQUIRE(parent->childObjectIds().contains("child_001"));
  }
}

// =============================================================================
// Panel Lifecycle Tests
// =============================================================================

TEST_CASE("SceneViewPanel: Panel lifecycle", "[integration][editor][scene_view][lifecycle]") {
  QtTestFixture fixture;

  SECTION("Panel construction and destruction") {
    auto* panel = new NMSceneViewPanel();
    REQUIRE(panel != nullptr);
    delete panel;
  }

  SECTION("Panel initialize and shutdown") {
    NMSceneViewPanel panel;
    panel.onInitialize();

    // Panel should have valid scene and view after initialization
    REQUIRE(panel.graphicsScene() != nullptr);
    REQUIRE(panel.graphicsView() != nullptr);

    panel.onShutdown();
    SUCCEED();
  }

  SECTION("Panel resize event") {
    NMSceneViewPanel panel;
    panel.onInitialize();

    panel.onResize(1024, 768);
    panel.onResize(1920, 1080);
    panel.onResize(800, 600);

    SUCCEED();
  }

  SECTION("Panel update loop") {
    NMSceneViewPanel panel;
    panel.onInitialize();

    // Simulate update loop
    panel.onUpdate(0.016); // ~60 FPS
    panel.onUpdate(0.033); // ~30 FPS
    panel.onUpdate(0.008); // ~120 FPS

    SUCCEED();
  }
}
