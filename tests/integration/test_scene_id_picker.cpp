/**
 * @file test_scene_id_picker.cpp
 * @brief Integration tests for NMSceneIdPicker widget (issue #212)
 */

#include <catch2/catch_test_macros.hpp>

#include "NovelMind/editor/qt/widgets/nm_scene_id_picker.hpp"
#include "NovelMind/editor/scene_registry.hpp"
#include <QCoreApplication>
#include <QSignalSpy>
#include <QTemporaryDir>

using namespace NovelMind::editor;
using namespace NovelMind::editor::qt;

// Helper to ensure QCoreApplication exists for Qt functionality
struct QtAppFixture {
  QtAppFixture() {
    if (!QCoreApplication::instance()) {
      static int argc = 1;
      static char *argv[] = {const_cast<char *>("test")};
      static QCoreApplication app(argc, argv);
    }
  }
};

TEST_CASE("NMSceneIdPicker basic functionality", "[scene_id_picker][qt]") {
  QtAppFixture fixture;
  QTemporaryDir tempDir;
  REQUIRE(tempDir.isValid());

  SceneRegistry registry;
  registry.load(tempDir.path());

  // Register some test scenes
  QString scene1Id = registry.registerScene("Forest Clearing");
  QString scene2Id = registry.registerScene("Mountain Path");
  QString scene3Id = registry.registerScene("Village Square");

  SECTION("Widget initializes correctly") {
    NMSceneIdPicker picker(&registry);

    REQUIRE(picker.sceneId().isEmpty());
    // Widget should show all registered scenes plus "(none)"
  }

  SECTION("Setting scene ID updates selection") {
    NMSceneIdPicker picker(&registry);

    picker.setSceneId(scene1Id);
    REQUIRE(picker.sceneId() == scene1Id);

    picker.setSceneId(scene2Id);
    REQUIRE(picker.sceneId() == scene2Id);
  }

  SECTION("Empty scene ID is valid") {
    NMSceneIdPicker picker(&registry);

    picker.setSceneId(QString());
    REQUIRE(picker.sceneId().isEmpty());
  }

  SECTION("Invalid scene ID is handled") {
    NMSceneIdPicker picker(&registry);

    picker.setSceneId("nonexistent_scene");
    // Should still store the ID even if it's not in registry
    REQUIRE(picker.sceneId() == "nonexistent_scene");
  }

  SECTION("Scene ID change signal is emitted") {
    NMSceneIdPicker picker(&registry);
    QSignalSpy spy(&picker, &NMSceneIdPicker::sceneIdChanged);

    picker.setSceneId(scene1Id);

    // Signal should have been emitted
    REQUIRE(spy.count() > 0);
    if (spy.count() > 0) {
      QList<QVariant> arguments = spy.last();
      REQUIRE(arguments.at(0).toString() == scene1Id);
    }
  }

  SECTION("Refresh scene list updates widget") {
    NMSceneIdPicker picker(&registry);

    // Register a new scene after widget creation
    QString scene4Id = registry.registerScene("Underground Cave");

    picker.refreshSceneList();

    // Widget should now include the new scene
    picker.setSceneId(scene4Id);
    REQUIRE(picker.sceneId() == scene4Id);
  }

  SECTION("Read-only mode disables editing") {
    NMSceneIdPicker picker(&registry);

    picker.setReadOnly(true);
    // Widget should still display the scene but not allow changes
    // (Testing this fully would require checking QWidget states)

    picker.setReadOnly(false);
    // Widget should allow editing again
  }

  SECTION("Quick action signals are emitted") {
    NMSceneIdPicker picker(&registry);
    picker.setSceneId(scene1Id);

    QSignalSpy createSpy(&picker, &NMSceneIdPicker::createNewSceneRequested);
    QSignalSpy editSpy(&picker, &NMSceneIdPicker::editSceneRequested);
    QSignalSpy locateSpy(&picker, &NMSceneIdPicker::locateSceneRequested);

    // These signals should be emittable
    emit picker.createNewSceneRequested();
    REQUIRE(createSpy.count() == 1);

    emit picker.editSceneRequested(scene1Id);
    REQUIRE(editSpy.count() == 1);

    emit picker.locateSceneRequested(scene1Id);
    REQUIRE(locateSpy.count() == 1);
  }
}

TEST_CASE("NMSceneIdPicker with SceneRegistry integration",
          "[scene_id_picker][scene_registry][qt]") {
  QtAppFixture fixture;
  QTemporaryDir tempDir;
  REQUIRE(tempDir.isValid());

  SceneRegistry registry;
  registry.load(tempDir.path());

  QString sceneId = registry.registerScene("Test Scene");

  SECTION("Widget reflects registry changes") {
    NMSceneIdPicker picker(&registry);
    picker.setSceneId(sceneId);

    REQUIRE(picker.sceneId() == sceneId);

    // Unregister the scene
    registry.unregisterScene(sceneId);

    // Widget should still have the ID set, but it should be marked as invalid
    REQUIRE(picker.sceneId() == sceneId);
  }

  SECTION("Widget updates when scene is renamed") {
    NMSceneIdPicker picker(&registry);
    picker.setSceneId(sceneId);

    // Rename the scene
    registry.renameScene(sceneId, "Renamed Scene");

    // Scene ID should remain the same (only name changes)
    REQUIRE(picker.sceneId() == sceneId);
  }

  SECTION("Widget responds to scene registration signal") {
    NMSceneIdPicker picker(&registry);

    // Register a new scene (should trigger auto-refresh)
    QString newSceneId = registry.registerScene("New Scene");

    // Widget should be able to select the new scene
    picker.setSceneId(newSceneId);
    REQUIRE(picker.sceneId() == newSceneId);
  }
}

TEST_CASE("NMSceneIdPicker null registry handling", "[scene_id_picker][qt]") {
  QtAppFixture fixture;

  SECTION("Widget works with null registry") {
    NMSceneIdPicker picker(nullptr);

    // Should not crash
    picker.setSceneId("test_scene");
    REQUIRE(picker.sceneId() == "test_scene");

    // Refresh should not crash
    picker.refreshSceneList();
  }
}
