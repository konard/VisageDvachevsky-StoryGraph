/**
 * @file test_scene_validation.cpp
 * @brief Unit tests for Scene Reference Validation (issue #215)
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "NovelMind/editor/qt/panels/nm_story_graph_panel.hpp"
#include "NovelMind/editor/project_manager.hpp"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

using namespace NovelMind::editor;
using namespace NovelMind::editor::qt;

// Helper to ensure QApplication exists for Qt GUI functionality
struct QtGuiAppFixture {
  QtGuiAppFixture() {
    if (!QApplication::instance()) {
      static int argc = 1;
      static char *argv[] = {const_cast<char *>("test")};
      static QApplication app(argc, argv);
    }
  }
};

TEST_CASE("Scene node validation state", "[scene_validation]") {
  QtGuiAppFixture fixture;

  SECTION("Scene validation error flag") {
    NMGraphNodeItem node("Test Scene", "Scene");

    REQUIRE_FALSE(node.hasSceneValidationError());
    REQUIRE_FALSE(node.hasSceneValidationWarning());

    node.setSceneValidationError(true);
    REQUIRE(node.hasSceneValidationError());

    node.setSceneValidationError(false);
    REQUIRE_FALSE(node.hasSceneValidationError());
  }

  SECTION("Scene validation warning flag") {
    NMGraphNodeItem node("Test Scene", "Scene");

    node.setSceneValidationWarning(true);
    REQUIRE(node.hasSceneValidationWarning());

    node.setSceneValidationWarning(false);
    REQUIRE_FALSE(node.hasSceneValidationWarning());
  }

  SECTION("Scene validation message") {
    NMGraphNodeItem node("Test Scene", "Scene");

    QString testMessage = "Scene file not found";
    node.setSceneValidationMessage(testMessage);
    REQUIRE(node.sceneValidationMessage() == testMessage);
  }

  SECTION("Non-scene nodes don't have validation errors") {
    NMGraphNodeItem dialogueNode("Test Dialogue", "Dialogue");
    NMGraphNodeItem choiceNode("Test Choice", "Choice");

    REQUIRE_FALSE(dialogueNode.isSceneNode());
    REQUIRE_FALSE(choiceNode.isSceneNode());
    REQUIRE_FALSE(dialogueNode.hasSceneValidationError());
    REQUIRE_FALSE(choiceNode.hasSceneValidationError());
  }
}

TEST_CASE("Scene reference validation", "[scene_validation]") {
  QtGuiAppFixture fixture;

  // Create temporary project structure
  QTemporaryDir tempDir;
  REQUIRE(tempDir.isValid());

  QString projectPath = tempDir.path();
  QString scenesPath = projectPath + "/Scenes";
  QDir().mkpath(scenesPath);

  SECTION("validateSceneReferences detects missing scene files") {
    NMStoryGraphScene graphScene;

    // Add scene node with a scene ID
    auto *node = graphScene.addNode("Forest Scene", "Scene", QPointF(0, 0));
    node->setSceneId("forest");

    // Validate - forest.nmscene doesn't exist
    QStringList errors = graphScene.validateSceneReferences(projectPath);
    REQUIRE(errors.size() > 0);
    REQUIRE_THAT(errors[0].toStdString(),
                 Catch::Matchers::ContainsSubstring("Forest Scene"));
    REQUIRE_THAT(errors[0].toStdString(),
                 Catch::Matchers::ContainsSubstring("not found"));
  }

  SECTION("validateSceneReferences passes when scene files exist") {
    // Create a valid .nmscene file
    QFile sceneFile(scenesPath + "/forest.nmscene");
    REQUIRE(sceneFile.open(QIODevice::WriteOnly));
    sceneFile.write("{\"sceneId\": \"forest\", \"objects\": []}");
    sceneFile.close();

    NMStoryGraphScene graphScene;
    auto *node = graphScene.addNode("Forest Scene", "Scene", QPointF(0, 0));
    node->setSceneId("forest");

    // Validate - should pass
    QStringList errors = graphScene.validateSceneReferences(projectPath);
    REQUIRE(errors.size() == 0);
  }

  SECTION("validateSceneReferences detects empty scene ID") {
    NMStoryGraphScene graphScene;

    // Add scene node without scene ID
    auto *node = graphScene.addNode("Unnamed Scene", "Scene", QPointF(0, 0));
    // Don't set scene ID

    QStringList errors = graphScene.validateSceneReferences(projectPath);
    REQUIRE(errors.size() > 0);
    REQUIRE_THAT(errors[0].toStdString(),
                 Catch::Matchers::ContainsSubstring("no scene ID assigned"));
  }

  SECTION("validateSceneReferences ignores non-scene nodes") {
    NMStoryGraphScene graphScene;

    // Add non-scene nodes
    graphScene.addNode("Dialogue", "Dialogue", QPointF(0, 0));
    graphScene.addNode("Choice", "Choice", QPointF(100, 0));

    // Validate - should not report errors for non-scene nodes
    QStringList errors = graphScene.validateSceneReferences(projectPath);
    REQUIRE(errors.size() == 0);
  }

  SECTION("updateSceneValidationState sets error flags correctly") {
    NMStoryGraphScene graphScene;

    // Add scene node with invalid reference
    auto *node = graphScene.addNode("Missing Scene", "Scene", QPointF(0, 0));
    node->setSceneId("missing");

    // Update validation state
    graphScene.updateSceneValidationState(projectPath);

    // Check that error flag is set
    REQUIRE(node->hasSceneValidationError());
    REQUIRE_FALSE(node->hasSceneValidationWarning());
    REQUIRE_FALSE(node->sceneValidationMessage().isEmpty());
  }

  SECTION("updateSceneValidationState clears errors for valid scenes") {
    // Create valid scene file
    QFile sceneFile(scenesPath + "/valid.nmscene");
    REQUIRE(sceneFile.open(QIODevice::WriteOnly));
    sceneFile.write("{\"sceneId\": \"valid\", \"objects\": []}");
    sceneFile.close();

    NMStoryGraphScene graphScene;
    auto *node = graphScene.addNode("Valid Scene", "Scene", QPointF(0, 0));
    node->setSceneId("valid");

    // Set error initially
    node->setSceneValidationError(true);
    REQUIRE(node->hasSceneValidationError());

    // Update validation state - should clear error
    graphScene.updateSceneValidationState(projectPath);

    REQUIRE_FALSE(node->hasSceneValidationError());
    REQUIRE_FALSE(node->hasSceneValidationWarning());
  }

  SECTION("validateGraph includes scene validation errors") {
    NMStoryGraphScene graphScene;

    // Add scene node with invalid reference
    auto *node = graphScene.addNode("Missing Scene", "Scene", QPointF(0, 0));
    node->setSceneId("missing");
    node->setEntry(true); // Set as entry to avoid "no entry" error

    // Validate entire graph
    QStringList errors = graphScene.validateGraph();

    // Should include scene validation errors
    bool hasSceneError = false;
    for (const QString &error : errors) {
      if (error.contains("not found") || error.contains("Missing")) {
        hasSceneError = true;
        break;
      }
    }
    REQUIRE(hasSceneError);
  }
}

TEST_CASE("Scene validation in graph workflow", "[scene_validation]") {
  QtGuiAppFixture fixture;

  // Create temporary project
  QTemporaryDir tempDir;
  REQUIRE(tempDir.isValid());

  QString projectPath = tempDir.path();
  QString scenesPath = projectPath + "/Scenes";
  QDir().mkpath(scenesPath);

  // Create some valid and invalid scene files
  QFile validScene(scenesPath + "/intro.nmscene");
  REQUIRE(validScene.open(QIODevice::WriteOnly));
  validScene.write("{\"sceneId\": \"intro\", \"objects\": []}");
  validScene.close();

  SECTION("Multiple scene nodes with mixed validation states") {
    NMStoryGraphScene graphScene;

    // Valid scene
    auto *validNode = graphScene.addNode("Intro", "Scene", QPointF(0, 0));
    validNode->setSceneId("intro");

    // Invalid scene (missing file)
    auto *invalidNode = graphScene.addNode("Missing", "Scene", QPointF(200, 0));
    invalidNode->setSceneId("missing");

    // Empty scene ID
    auto *emptyNode = graphScene.addNode("Unnamed", "Scene", QPointF(400, 0));

    // Update validation
    graphScene.updateSceneValidationState(projectPath);

    // Check states
    REQUIRE_FALSE(validNode->hasSceneValidationError());
    REQUIRE(invalidNode->hasSceneValidationError());
    REQUIRE(emptyNode->hasSceneValidationError());
  }
}
