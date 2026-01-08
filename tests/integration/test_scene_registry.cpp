/**
 * @file test_scene_registry.cpp
 * @brief Unit tests for SceneRegistry (issue #206)
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "NovelMind/editor/scene_registry.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>

using namespace NovelMind::editor;

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

TEST_CASE("SceneMetadata JSON serialization", "[scene_registry]") {
  QtAppFixture fixture;

  SceneMetadata meta;
  meta.id = "test_scene";
  meta.name = "Test Scene";
  meta.documentPath = "Scenes/test_scene.nmscene";
  meta.thumbnailPath = "Scenes/.thumbnails/test_scene.png";
  meta.created = QDateTime::fromString("2026-01-08T12:00:00", Qt::ISODate);
  meta.modified = QDateTime::fromString("2026-01-08T13:00:00", Qt::ISODate);
  meta.tags = QStringList{"intro", "tutorial"};
  meta.description = "A test scene for unit tests";

  SECTION("toJson produces valid JSON") {
    QJsonObject json = meta.toJson();

    REQUIRE(json.value("id").toString() == "test_scene");
    REQUIRE(json.value("name").toString() == "Test Scene");
    REQUIRE(json.value("documentPath").toString() == "Scenes/test_scene.nmscene");
    REQUIRE(json.value("thumbnailPath").toString() == "Scenes/.thumbnails/test_scene.png");
    REQUIRE(json.value("tags").toArray().size() == 2);
    REQUIRE(json.value("description").toString() == "A test scene for unit tests");
  }

  SECTION("fromJson correctly parses JSON") {
    QJsonObject json = meta.toJson();
    SceneMetadata parsed = SceneMetadata::fromJson(json);

    REQUIRE(parsed.id == meta.id);
    REQUIRE(parsed.name == meta.name);
    REQUIRE(parsed.documentPath == meta.documentPath);
    REQUIRE(parsed.thumbnailPath == meta.thumbnailPath);
    REQUIRE(parsed.tags == meta.tags);
    REQUIRE(parsed.description == meta.description);
  }

  SECTION("round-trip preserves all fields") {
    QJsonObject json = meta.toJson();
    QJsonDocument doc(json);
    QString jsonStr = doc.toJson();

    QJsonDocument parsedDoc = QJsonDocument::fromJson(jsonStr.toUtf8());
    SceneMetadata roundTripped = SceneMetadata::fromJson(parsedDoc.object());

    REQUIRE(roundTripped.id == meta.id);
    REQUIRE(roundTripped.name == meta.name);
    REQUIRE(roundTripped.documentPath == meta.documentPath);
    REQUIRE(roundTripped.thumbnailPath == meta.thumbnailPath);
    REQUIRE(roundTripped.tags.size() == meta.tags.size());
  }
}

TEST_CASE("SceneRegistry scene management", "[scene_registry]") {
  QtAppFixture fixture;
  SceneRegistry registry;

  SECTION("registerScene creates unique IDs") {
    QString id1 = registry.registerScene("Test Scene");
    QString id2 = registry.registerScene("Test Scene");
    QString id3 = registry.registerScene("Another Scene");

    REQUIRE(!id1.isEmpty());
    REQUIRE(!id2.isEmpty());
    REQUIRE(!id3.isEmpty());
    REQUIRE(id1 != id2);
    REQUIRE(id1 != id3);
    REQUIRE(registry.sceneCount() == 3);
  }

  SECTION("sceneExists returns correct values") {
    QString id = registry.registerScene("Test");

    REQUIRE(registry.sceneExists(id));
    REQUIRE_FALSE(registry.sceneExists("nonexistent"));
  }

  SECTION("getSceneMetadata returns correct data") {
    QString id = registry.registerScene("My Scene");
    SceneMetadata meta = registry.getSceneMetadata(id);

    REQUIRE(meta.id == id);
    REQUIRE(meta.name == "My Scene");
    REQUIRE(!meta.documentPath.isEmpty());
  }

  SECTION("renameScene updates name") {
    QString id = registry.registerScene("Original Name");
    bool result = registry.renameScene(id, "New Name");

    REQUIRE(result);
    REQUIRE(registry.getSceneMetadata(id).name == "New Name");
  }

  SECTION("renameScene fails for nonexistent scene") {
    bool result = registry.renameScene("nonexistent", "New Name");
    REQUIRE_FALSE(result);
  }

  SECTION("unregisterScene removes scene") {
    QString id = registry.registerScene("To Be Removed");
    REQUIRE(registry.sceneCount() == 1);

    bool result = registry.unregisterScene(id);

    REQUIRE(result);
    REQUIRE(registry.sceneCount() == 0);
    REQUIRE_FALSE(registry.sceneExists(id));
  }

  SECTION("unregisterScene fails for nonexistent scene") {
    bool result = registry.unregisterScene("nonexistent");
    REQUIRE_FALSE(result);
  }

  SECTION("getAllSceneIds returns all registered scenes") {
    registry.registerScene("Scene A");
    registry.registerScene("Scene B");
    registry.registerScene("Scene C");

    QStringList ids = registry.getAllSceneIds();
    REQUIRE(ids.size() == 3);
  }

  SECTION("updateSceneMetadata updates metadata") {
    QString id = registry.registerScene("Test");
    SceneMetadata meta = registry.getSceneMetadata(id);
    meta.description = "Updated description";
    meta.tags = QStringList{"tag1", "tag2"};

    bool result = registry.updateSceneMetadata(id, meta);

    REQUIRE(result);
    SceneMetadata updated = registry.getSceneMetadata(id);
    REQUIRE(updated.description == "Updated description");
    REQUIRE(updated.tags.size() == 2);
  }
}

TEST_CASE("SceneRegistry tag filtering", "[scene_registry]") {
  QtAppFixture fixture;
  SceneRegistry registry;

  // Register scenes with tags
  QString id1 = registry.registerScene("Scene 1");
  SceneMetadata meta1 = registry.getSceneMetadata(id1);
  meta1.tags = QStringList{"intro", "outdoor"};
  registry.updateSceneMetadata(id1, meta1);

  QString id2 = registry.registerScene("Scene 2");
  SceneMetadata meta2 = registry.getSceneMetadata(id2);
  meta2.tags = QStringList{"intro", "indoor"};
  registry.updateSceneMetadata(id2, meta2);

  QString id3 = registry.registerScene("Scene 3");
  SceneMetadata meta3 = registry.getSceneMetadata(id3);
  meta3.tags = QStringList{"outdoor"};
  registry.updateSceneMetadata(id3, meta3);

  SECTION("getScenes with empty tags returns all") {
    QList<SceneMetadata> scenes = registry.getScenes();
    REQUIRE(scenes.size() == 3);
  }

  SECTION("getScenes filters by tag") {
    QList<SceneMetadata> introScenes = registry.getScenes(QStringList{"intro"});
    REQUIRE(introScenes.size() == 2);

    QList<SceneMetadata> outdoorScenes = registry.getScenes(QStringList{"outdoor"});
    REQUIRE(outdoorScenes.size() == 2);

    QList<SceneMetadata> indoorScenes = registry.getScenes(QStringList{"indoor"});
    REQUIRE(indoorScenes.size() == 1);
  }

  SECTION("getScenes with multiple tags uses OR logic") {
    QList<SceneMetadata> scenes = registry.getScenes(QStringList{"indoor", "outdoor"});
    REQUIRE(scenes.size() == 3);
  }
}

TEST_CASE("SceneRegistry persistence", "[scene_registry]") {
  QtAppFixture fixture;
  QTemporaryDir tempDir;
  REQUIRE(tempDir.isValid());

  QString projectPath = tempDir.path();

  // Create Scenes directory
  QDir(projectPath).mkdir("Scenes");

  SECTION("save and load roundtrip") {
    // Create and populate registry
    SceneRegistry registry;
    registry.registerScene("Scene 1");
    registry.registerScene("Scene 2");

    // Save
    bool saveResult = registry.save(projectPath);
    REQUIRE(saveResult);
    REQUIRE_FALSE(registry.isModified());

    // Verify file exists
    QString registryPath = QDir(projectPath).filePath("scene_registry.json");
    REQUIRE(QFile::exists(registryPath));

    // Load into new registry
    SceneRegistry loadedRegistry;
    bool loadResult = loadedRegistry.load(projectPath);
    REQUIRE(loadResult);
    REQUIRE(loadedRegistry.sceneCount() == 2);
  }

  SECTION("load creates empty registry for new projects") {
    SceneRegistry registry;
    bool result = registry.load(projectPath);
    REQUIRE(result);
    REQUIRE(registry.sceneCount() == 0);
  }

  SECTION("toJson and fromJson roundtrip") {
    SceneRegistry registry;
    registry.registerScene("Test Scene");

    QJsonObject json = registry.toJson();
    REQUIRE(json.contains("version"));
    REQUIRE(json.contains("scenes"));

    SceneRegistry loadedRegistry;
    bool result = loadedRegistry.fromJson(json);
    REQUIRE(result);
    REQUIRE(loadedRegistry.sceneCount() == 1);
  }
}

TEST_CASE("SceneRegistry validation", "[scene_registry]") {
  QtAppFixture fixture;
  QTemporaryDir tempDir;
  REQUIRE(tempDir.isValid());

  QString projectPath = tempDir.path();
  QDir(projectPath).mkdir("Scenes");

  SceneRegistry registry;
  registry.load(projectPath);

  SECTION("validateScenes reports missing documents") {
    // Register a scene but don't create the file
    registry.registerScene("Missing Scene");

    QStringList errors = registry.validateScenes();
    REQUIRE(!errors.isEmpty());
  }

  SECTION("findBrokenReferences identifies missing files") {
    registry.registerScene("Missing");

    QStringList broken = registry.findBrokenReferences();
    REQUIRE(!broken.isEmpty());
  }

  SECTION("findOrphanedScenes identifies unregistered files") {
    // Create an unregistered scene file
    QString orphanPath = QDir(projectPath).filePath("Scenes/orphan.nmscene");
    QFile orphanFile(orphanPath);
    orphanFile.open(QIODevice::WriteOnly);
    orphanFile.write("{\"sceneId\": \"orphan\", \"objects\": []}");
    orphanFile.close();

    QStringList orphaned = registry.findOrphanedScenes();
    REQUIRE(!orphaned.isEmpty());
    REQUIRE(orphaned.first().contains("orphan.nmscene"));
  }
}

TEST_CASE("SceneRegistry ID generation", "[scene_registry]") {
  QtAppFixture fixture;
  SceneRegistry registry;

  SECTION("sanitizes names with spaces") {
    QString id = registry.registerScene("My Test Scene");
    REQUIRE(!id.contains(' '));
    REQUIRE(id.contains('_'));
  }

  SECTION("sanitizes names with special characters") {
    QString id = registry.registerScene("Scene@#$%");
    REQUIRE(!id.contains('@'));
    REQUIRE(!id.contains('#'));
    REQUIRE(!id.contains('$'));
    REQUIRE(!id.contains('%'));
  }

  SECTION("converts to lowercase") {
    QString id = registry.registerScene("MyScene");
    REQUIRE(id == id.toLower());
  }

  SECTION("handles empty name") {
    QString id = registry.registerScene("");
    REQUIRE(!id.isEmpty());
  }

  SECTION("handles unicode names") {
    QString id = registry.registerScene("日本語シーン");
    REQUIRE(!id.isEmpty());
  }
}

TEST_CASE("SceneRegistry signals", "[scene_registry]") {
  QtAppFixture fixture;
  SceneRegistry registry;

  bool registered = false;
  bool renamed = false;
  bool unregistered = false;
  bool metadataChanged = false;

  QObject::connect(&registry, &SceneRegistry::sceneRegistered,
                   [&](const QString &) { registered = true; });
  QObject::connect(&registry, &SceneRegistry::sceneRenamed,
                   [&](const QString &, const QString &) { renamed = true; });
  QObject::connect(&registry, &SceneRegistry::sceneUnregistered,
                   [&](const QString &) { unregistered = true; });
  QObject::connect(&registry, &SceneRegistry::sceneMetadataChanged,
                   [&](const QString &) { metadataChanged = true; });

  SECTION("sceneRegistered signal emitted") {
    registry.registerScene("Test");
    REQUIRE(registered);
  }

  SECTION("sceneRenamed signal emitted") {
    QString id = registry.registerScene("Test");
    registry.renameScene(id, "New Name");
    REQUIRE(renamed);
  }

  SECTION("sceneUnregistered signal emitted") {
    QString id = registry.registerScene("Test");
    registry.unregisterScene(id);
    REQUIRE(unregistered);
  }

  SECTION("sceneMetadataChanged signal emitted") {
    QString id = registry.registerScene("Test");
    SceneMetadata meta = registry.getSceneMetadata(id);
    meta.description = "Updated";
    registry.updateSceneMetadata(id, meta);
    REQUIRE(metadataChanged);
  }
}

TEST_CASE("SceneRegistry modified flag", "[scene_registry]") {
  QtAppFixture fixture;
  QTemporaryDir tempDir;
  REQUIRE(tempDir.isValid());

  QString projectPath = tempDir.path();
  QDir(projectPath).mkdir("Scenes");

  SceneRegistry registry;
  registry.load(projectPath);

  SECTION("starts unmodified after load") {
    REQUIRE_FALSE(registry.isModified());
  }

  SECTION("registerScene sets modified") {
    registry.registerScene("Test");
    REQUIRE(registry.isModified());
  }

  SECTION("renameScene sets modified") {
    QString id = registry.registerScene("Test");
    registry.save(projectPath);
    registry.renameScene(id, "New Name");
    REQUIRE(registry.isModified());
  }

  SECTION("save clears modified") {
    registry.registerScene("Test");
    REQUIRE(registry.isModified());
    registry.save(projectPath);
    REQUIRE_FALSE(registry.isModified());
  }
}
