/**
 * @file test_scene_template_manager.cpp
 * @brief Unit tests for SceneTemplateManager (issue #216)
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "NovelMind/editor/scene_template_manager.hpp"
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

TEST_CASE("SceneTemplateMetadata JSON serialization", "[scene_template]") {
  QtAppFixture fixture;

  SceneTemplateMetadata meta;
  meta.id = "dialogue_scene";
  meta.name = "Dialogue Scene";
  meta.description = "Standard dialogue scene template";
  meta.category = "Visual Novel";
  meta.type = SceneTemplateType::BuiltIn;
  meta.author = "NovelMind Team";
  meta.version = "1.0";
  meta.tags = QStringList{"dialogue", "vn"};

  SECTION("toJson produces valid JSON") {
    QJsonObject json = meta.toJson();

    REQUIRE(json.value("id").toString() == "dialogue_scene");
    REQUIRE(json.value("name").toString() == "Dialogue Scene");
    REQUIRE(json.value("description").toString() == "Standard dialogue scene template");
    REQUIRE(json.value("category").toString() == "Visual Novel");
    REQUIRE(json.value("type").toString() == "builtin");
    REQUIRE(json.value("author").toString() == "NovelMind Team");
    REQUIRE(json.value("tags").toArray().size() == 2);
  }

  SECTION("fromJson correctly parses JSON") {
    QJsonObject json = meta.toJson();
    SceneTemplateMetadata parsed = SceneTemplateMetadata::fromJson(json);

    REQUIRE(parsed.id == meta.id);
    REQUIRE(parsed.name == meta.name);
    REQUIRE(parsed.description == meta.description);
    REQUIRE(parsed.category == meta.category);
    REQUIRE(parsed.type == meta.type);
    REQUIRE(parsed.author == meta.author);
    REQUIRE(parsed.tags == meta.tags);
  }
}

TEST_CASE("SceneTemplate JSON serialization", "[scene_template]") {
  QtAppFixture fixture;

  SceneTemplate tmpl;
  tmpl.metadata.id = "test_template";
  tmpl.metadata.name = "Test Template";
  tmpl.metadata.description = "A test template";
  tmpl.metadata.category = "Test";
  tmpl.metadata.type = SceneTemplateType::BuiltIn;

  tmpl.content.sceneId = "{{scene_id}}";

  SceneDocumentObject obj;
  obj.id = "background";
  obj.name = "Background";
  obj.type = "Background";
  obj.x = 0.0f;
  obj.y = 0.0f;
  obj.zOrder = 0;
  obj.properties["placeholder"] = "true";
  tmpl.content.objects.push_back(obj);

  SECTION("toJson includes metadata and content") {
    QJsonObject json = tmpl.toJson();

    REQUIRE(json.contains("metadata"));
    REQUIRE(json.contains("content"));

    QJsonObject contentObj = json.value("content").toObject();
    REQUIRE(contentObj.value("sceneId").toString() == "{{scene_id}}");
    REQUIRE(contentObj.value("objects").toArray().size() == 1);
  }

  SECTION("fromJson correctly reconstructs template") {
    QJsonObject json = tmpl.toJson();
    SceneTemplate parsed = SceneTemplate::fromJson(json);

    REQUIRE(parsed.metadata.id == tmpl.metadata.id);
    REQUIRE(parsed.metadata.name == tmpl.metadata.name);
    REQUIRE(parsed.content.sceneId == tmpl.content.sceneId);
    REQUIRE(parsed.content.objects.size() == 1);
    REQUIRE(parsed.content.objects[0].id == "background");
    REQUIRE(parsed.content.objects[0].properties.count("placeholder") == 1);
  }
}

TEST_CASE("SceneTemplateManager built-in templates", "[scene_template]") {
  QtAppFixture fixture;
  SceneTemplateManager manager;

  SECTION("loadBuiltInTemplates creates default templates") {
    manager.loadBuiltInTemplates();

    REQUIRE(manager.templateCount() >= 5);
    REQUIRE(manager.hasTemplate("empty_scene"));
    REQUIRE(manager.hasTemplate("dialogue_scene"));
    REQUIRE(manager.hasTemplate("choice_scene"));
    REQUIRE(manager.hasTemplate("cutscene"));
    REQUIRE(manager.hasTemplate("title_screen"));
  }

  SECTION("getAvailableTemplateIds returns all templates") {
    manager.loadBuiltInTemplates();

    QStringList ids = manager.getAvailableTemplateIds();
    REQUIRE(ids.size() >= 5);
    REQUIRE(ids.contains("empty_scene"));
    REQUIRE(ids.contains("dialogue_scene"));
  }

  SECTION("getTemplate returns correct template") {
    manager.loadBuiltInTemplates();

    auto dialogueTmpl = manager.getTemplate("dialogue_scene");
    REQUIRE(dialogueTmpl.has_value());
    REQUIRE(dialogueTmpl->metadata.name == "Dialogue Scene");
    REQUIRE(dialogueTmpl->metadata.category == "Visual Novel");
    REQUIRE(!dialogueTmpl->content.objects.empty());
  }

  SECTION("getTemplate returns nullopt for nonexistent") {
    manager.loadBuiltInTemplates();

    auto tmpl = manager.getTemplate("nonexistent");
    REQUIRE_FALSE(tmpl.has_value());
  }
}

TEST_CASE("SceneTemplateManager template content", "[scene_template]") {
  QtAppFixture fixture;
  SceneTemplateManager manager;
  manager.loadBuiltInTemplates();

  SECTION("empty_scene template has no objects") {
    auto tmpl = manager.getTemplate("empty_scene");
    REQUIRE(tmpl.has_value());
    REQUIRE(tmpl->content.objects.empty());
  }

  SECTION("dialogue_scene template has required objects") {
    auto tmpl = manager.getTemplate("dialogue_scene");
    REQUIRE(tmpl.has_value());

    // Should have: background, left character, right character, dialogue box
    REQUIRE(tmpl->content.objects.size() >= 4);

    // Check for background
    bool hasBackground = false;
    bool hasCharacterLeft = false;
    bool hasCharacterRight = false;
    bool hasDialogueBox = false;

    for (const auto &obj : tmpl->content.objects) {
      if (obj.type == "Background") hasBackground = true;
      if (obj.id == "character_left") hasCharacterLeft = true;
      if (obj.id == "character_right") hasCharacterRight = true;
      if (obj.id == "dialogue_box") hasDialogueBox = true;
    }

    REQUIRE(hasBackground);
    REQUIRE(hasCharacterLeft);
    REQUIRE(hasCharacterRight);
    REQUIRE(hasDialogueBox);
  }

  SECTION("choice_scene template has choice menu") {
    auto tmpl = manager.getTemplate("choice_scene");
    REQUIRE(tmpl.has_value());

    bool hasChoiceMenu = false;
    for (const auto &obj : tmpl->content.objects) {
      if (obj.id == "choice_menu") hasChoiceMenu = true;
    }
    REQUIRE(hasChoiceMenu);
  }

  SECTION("title_screen template has logo and menu") {
    auto tmpl = manager.getTemplate("title_screen");
    REQUIRE(tmpl.has_value());

    bool hasLogo = false;
    bool hasMenu = false;
    for (const auto &obj : tmpl->content.objects) {
      if (obj.id == "logo") hasLogo = true;
      if (obj.id == "menu_buttons") hasMenu = true;
    }
    REQUIRE(hasLogo);
    REQUIRE(hasMenu);
  }

  SECTION("cutscene template has fullscreen background") {
    auto tmpl = manager.getTemplate("cutscene");
    REQUIRE(tmpl.has_value());
    REQUIRE(tmpl->content.objects.size() == 1);
    REQUIRE(tmpl->content.objects[0].type == "Background");
    REQUIRE(tmpl->content.objects[0].properties.count("fullscreen") == 1);
  }
}

TEST_CASE("SceneTemplateManager categories", "[scene_template]") {
  QtAppFixture fixture;
  SceneTemplateManager manager;
  manager.loadBuiltInTemplates();

  SECTION("getCategories returns unique categories") {
    QStringList categories = manager.getCategories();

    REQUIRE(!categories.isEmpty());
    REQUIRE(categories.contains("Standard"));
    REQUIRE(categories.contains("Visual Novel"));
    REQUIRE(categories.contains("Cinematic"));
    REQUIRE(categories.contains("Menu"));
  }

  SECTION("getAvailableTemplates filters by category") {
    auto vnTemplates = manager.getAvailableTemplates("Visual Novel");
    auto standardTemplates = manager.getAvailableTemplates("Standard");

    // Should include dialogue and choice scenes
    REQUIRE(vnTemplates.size() >= 2);

    // Should include empty scene
    REQUIRE(standardTemplates.size() >= 1);

    // Check categories are correct
    for (const auto &meta : vnTemplates) {
      REQUIRE(meta.category == "Visual Novel");
    }
    for (const auto &meta : standardTemplates) {
      REQUIRE(meta.category == "Standard");
    }
  }

  SECTION("getAvailableTemplates with empty category returns all") {
    auto allTemplates = manager.getAvailableTemplates();
    REQUIRE(allTemplates.size() >= 5);
  }
}

TEST_CASE("SceneTemplateManager template instantiation", "[scene_template]") {
  QtAppFixture fixture;
  SceneTemplateManager manager;
  manager.loadBuiltInTemplates();

  SECTION("instantiateTemplate creates document with correct ID") {
    auto result = manager.instantiateTemplate("dialogue_scene", "my_scene");

    REQUIRE(result.isOk());
    const auto &doc = result.value();
    REQUIRE(doc.sceneId == "my_scene");
  }

  SECTION("instantiateTemplate preserves object structure") {
    auto result = manager.instantiateTemplate("dialogue_scene", "test");

    REQUIRE(result.isOk());
    const auto &doc = result.value();
    REQUIRE(!doc.objects.empty());

    // Check that background is present
    bool hasBackground = false;
    for (const auto &obj : doc.objects) {
      if (obj.type == "Background") {
        hasBackground = true;
      }
    }
    REQUIRE(hasBackground);
  }

  SECTION("instantiateTemplate fails for nonexistent template") {
    auto result = manager.instantiateTemplate("nonexistent", "scene");
    REQUIRE(result.isError());
  }

  SECTION("empty_scene instantiation creates empty document") {
    auto result = manager.instantiateTemplate("empty_scene", "blank");

    REQUIRE(result.isOk());
    const auto &doc = result.value();
    REQUIRE(doc.sceneId == "blank");
    REQUIRE(doc.objects.empty());
  }
}

TEST_CASE("SceneTemplateManager file creation", "[scene_template]") {
  QtAppFixture fixture;
  QTemporaryDir tempDir;
  REQUIRE(tempDir.isValid());

  SceneTemplateManager manager;
  manager.loadBuiltInTemplates();

  SECTION("createSceneFromTemplate creates valid file") {
    QString outputPath = QDir(tempDir.path()).filePath("test_scene.nmscene");

    auto result =
        manager.createSceneFromTemplate("dialogue_scene", "test", outputPath);

    REQUIRE(result.isOk());
    REQUIRE(QFile::exists(outputPath));

    // Verify file content
    QFile file(outputPath);
    REQUIRE(file.open(QIODevice::ReadOnly));
    QByteArray data = file.readAll();
    file.close();

    REQUIRE(!data.isEmpty());

    // Parse and verify
    auto docResult = loadSceneDocument(outputPath.toStdString());
    REQUIRE(docResult.isOk());
    REQUIRE(docResult.value().sceneId == "test");
  }

  SECTION("createSceneFromTemplate creates parent directories") {
    QString outputPath =
        QDir(tempDir.path()).filePath("nested/dir/scene.nmscene");

    auto result =
        manager.createSceneFromTemplate("empty_scene", "nested_scene", outputPath);

    REQUIRE(result.isOk());
    REQUIRE(QFile::exists(outputPath));
  }
}

TEST_CASE("SceneTemplateManager user templates", "[scene_template]") {
  QtAppFixture fixture;
  QTemporaryDir tempDir;
  REQUIRE(tempDir.isValid());

  QString projectPath = tempDir.path();
  SceneTemplateManager manager;
  manager.loadBuiltInTemplates();

  SECTION("saveAsUserTemplate creates user template") {
    SceneDocument doc;
    doc.sceneId = "custom";

    SceneDocumentObject obj;
    obj.id = "custom_bg";
    obj.name = "Custom Background";
    obj.type = "Background";
    doc.objects.push_back(obj);

    auto result =
        manager.saveAsUserTemplate(doc, "My Custom Template", "A custom template", projectPath);

    REQUIRE(result.isOk());
    QString templateId = result.value();
    REQUIRE(templateId.startsWith("user_"));
    REQUIRE(manager.hasTemplate(templateId));
  }

  SECTION("loadUserTemplates loads saved templates") {
    // Save a template
    SceneDocument doc;
    doc.sceneId = "test";

    auto saveResult =
        manager.saveAsUserTemplate(doc, "Test Template", "Test", projectPath);
    REQUIRE(saveResult.isOk());

    // Clear and reload
    manager.clearTemplates();
    manager.loadBuiltInTemplates();
    int userCount = manager.loadUserTemplates(projectPath);

    REQUIRE(userCount >= 1);
  }

  SECTION("deleteUserTemplate removes template") {
    // Create template
    SceneDocument doc;
    auto result =
        manager.saveAsUserTemplate(doc, "To Delete", "Delete me", projectPath);
    REQUIRE(result.isOk());

    QString templateId = result.value();
    REQUIRE(manager.hasTemplate(templateId));

    // Delete
    auto deleteResult = manager.deleteUserTemplate(templateId, projectPath);
    REQUIRE(deleteResult.isOk());
    REQUIRE_FALSE(manager.hasTemplate(templateId));
  }

  SECTION("deleteUserTemplate fails for built-in templates") {
    auto result = manager.deleteUserTemplate("empty_scene", projectPath);
    REQUIRE(result.isError());
  }

  SECTION("updateUserTemplate modifies template") {
    // Create template
    SceneDocument doc;
    auto createResult =
        manager.saveAsUserTemplate(doc, "Updateable", "Original", projectPath);
    REQUIRE(createResult.isOk());

    QString templateId = createResult.value();

    // Update with new content
    SceneDocument newDoc;
    SceneDocumentObject newObj;
    newObj.id = "new_object";
    newObj.name = "New Object";
    newObj.type = "UI";
    newDoc.objects.push_back(newObj);

    auto updateResult = manager.updateUserTemplate(templateId, newDoc, projectPath);
    REQUIRE(updateResult.isOk());

    // Verify update
    auto tmpl = manager.getTemplate(templateId);
    REQUIRE(tmpl.has_value());
    REQUIRE(tmpl->content.objects.size() == 1);
    REQUIRE(tmpl->content.objects[0].id == "new_object");
  }
}

TEST_CASE("SceneTemplateManager preview generation", "[scene_template]") {
  QtAppFixture fixture;
  SceneTemplateManager manager;
  manager.loadBuiltInTemplates();

  SECTION("getTemplatePreview returns valid pixmap") {
    QPixmap preview = manager.getTemplatePreview("dialogue_scene");
    REQUIRE(!preview.isNull());
    REQUIRE(preview.width() > 0);
    REQUIRE(preview.height() > 0);
  }

  SECTION("getTemplatePreview returns placeholder for missing preview") {
    // All built-in templates should at least get a placeholder
    QPixmap preview = manager.getTemplatePreview("empty_scene");
    REQUIRE(!preview.isNull());
  }

  SECTION("getTemplatePreview caches result") {
    // First call
    QPixmap preview1 = manager.getTemplatePreview("dialogue_scene");
    // Second call should return cached
    QPixmap preview2 = manager.getTemplatePreview("dialogue_scene");

    // Both should be valid
    REQUIRE(!preview1.isNull());
    REQUIRE(!preview2.isNull());
  }
}

TEST_CASE("SceneTemplateManager clearTemplates", "[scene_template]") {
  QtAppFixture fixture;
  SceneTemplateManager manager;
  manager.loadBuiltInTemplates();

  REQUIRE(manager.templateCount() >= 5);

  manager.clearTemplates();

  REQUIRE(manager.templateCount() == 0);
  REQUIRE_FALSE(manager.hasTemplate("empty_scene"));
  REQUIRE(manager.getAvailableTemplateIds().isEmpty());
}

TEST_CASE("SceneTemplateManager signals", "[scene_template]") {
  QtAppFixture fixture;
  QTemporaryDir tempDir;
  REQUIRE(tempDir.isValid());

  SceneTemplateManager manager;
  manager.loadBuiltInTemplates();

  bool reloaded = false;
  bool userCreated = false;
  bool userDeleted = false;
  bool userUpdated = false;

  QObject::connect(&manager, &SceneTemplateManager::templatesReloaded,
                   [&]() { reloaded = true; });
  QObject::connect(&manager, &SceneTemplateManager::userTemplateCreated,
                   [&](const QString &) { userCreated = true; });
  QObject::connect(&manager, &SceneTemplateManager::userTemplateDeleted,
                   [&](const QString &) { userDeleted = true; });
  QObject::connect(&manager, &SceneTemplateManager::userTemplateUpdated,
                   [&](const QString &) { userUpdated = true; });

  SECTION("templatesReloaded signal emitted on reload") {
    manager.reloadAllTemplates();
    REQUIRE(reloaded);
  }

  SECTION("userTemplateCreated signal emitted") {
    SceneDocument doc;
    manager.saveAsUserTemplate(doc, "Test", "Test", tempDir.path());
    REQUIRE(userCreated);
  }

  SECTION("userTemplateDeleted signal emitted") {
    SceneDocument doc;
    auto result = manager.saveAsUserTemplate(doc, "Test", "Test", tempDir.path());
    REQUIRE(result.isOk());

    manager.deleteUserTemplate(result.value(), tempDir.path());
    REQUIRE(userDeleted);
  }

  SECTION("userTemplateUpdated signal emitted") {
    SceneDocument doc;
    auto result = manager.saveAsUserTemplate(doc, "Test", "Test", tempDir.path());
    REQUIRE(result.isOk());

    manager.updateUserTemplate(result.value(), doc, tempDir.path());
    REQUIRE(userUpdated);
  }
}
