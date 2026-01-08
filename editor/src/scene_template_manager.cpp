#include "NovelMind/editor/scene_template_manager.hpp"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFont>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPainter>
#include <QResource>

namespace NovelMind::editor {

// ============================================================================
// SceneTemplateMetadata Implementation
// ============================================================================

QJsonObject SceneTemplateMetadata::toJson() const {
  QJsonObject obj;
  obj.insert("id", id);
  obj.insert("name", name);
  obj.insert("description", description);
  obj.insert("category", category);
  obj.insert("type", type == SceneTemplateType::BuiltIn ? "builtin" : "user");
  obj.insert("previewPath", previewPath);
  obj.insert("author", author);
  obj.insert("version", version);
  obj.insert("created", created.toString(Qt::ISODate));
  obj.insert("modified", modified.toString(Qt::ISODate));

  QJsonArray tagsArray;
  for (const QString &tag : tags) {
    tagsArray.append(tag);
  }
  obj.insert("tags", tagsArray);

  return obj;
}

SceneTemplateMetadata SceneTemplateMetadata::fromJson(const QJsonObject &json) {
  SceneTemplateMetadata meta;
  meta.id = json.value("id").toString();
  meta.name = json.value("name").toString();
  meta.description = json.value("description").toString();
  meta.category = json.value("category").toString("Standard");
  meta.type = json.value("type").toString() == "builtin"
                  ? SceneTemplateType::BuiltIn
                  : SceneTemplateType::User;
  meta.previewPath = json.value("previewPath").toString();
  meta.author = json.value("author").toString();
  meta.version = json.value("version").toString("1.0");
  meta.created =
      QDateTime::fromString(json.value("created").toString(), Qt::ISODate);
  meta.modified =
      QDateTime::fromString(json.value("modified").toString(), Qt::ISODate);

  const QJsonArray tagsArray = json.value("tags").toArray();
  meta.tags.reserve(tagsArray.size());
  for (const auto &value : tagsArray) {
    meta.tags.append(value.toString());
  }

  return meta;
}

// ============================================================================
// SceneTemplate Implementation
// ============================================================================

QJsonObject SceneTemplate::toJson() const {
  QJsonObject obj;
  obj.insert("metadata", metadata.toJson());

  // Convert SceneDocument to JSON
  QJsonObject contentObj;
  contentObj.insert("sceneId", QString::fromStdString(content.sceneId));

  QJsonArray objectsArray;
  for (const auto &sceneObj : content.objects) {
    QJsonObject objEntry;
    objEntry.insert("id", QString::fromStdString(sceneObj.id));
    objEntry.insert("name", QString::fromStdString(sceneObj.name));
    objEntry.insert("type", QString::fromStdString(sceneObj.type));
    objEntry.insert("x", static_cast<double>(sceneObj.x));
    objEntry.insert("y", static_cast<double>(sceneObj.y));
    objEntry.insert("rotation", static_cast<double>(sceneObj.rotation));
    objEntry.insert("scaleX", static_cast<double>(sceneObj.scaleX));
    objEntry.insert("scaleY", static_cast<double>(sceneObj.scaleY));
    objEntry.insert("alpha", static_cast<double>(sceneObj.alpha));
    objEntry.insert("visible", sceneObj.visible);
    objEntry.insert("zOrder", sceneObj.zOrder);

    QJsonObject propsObj;
    for (const auto &[key, value] : sceneObj.properties) {
      propsObj.insert(QString::fromStdString(key),
                      QString::fromStdString(value));
    }
    objEntry.insert("properties", propsObj);

    objectsArray.append(objEntry);
  }
  contentObj.insert("objects", objectsArray);

  obj.insert("content", contentObj);
  return obj;
}

SceneTemplate SceneTemplate::fromJson(const QJsonObject &json) {
  SceneTemplate tmpl;
  tmpl.metadata = SceneTemplateMetadata::fromJson(json.value("metadata").toObject());

  const QJsonObject contentObj = json.value("content").toObject();
  tmpl.content.sceneId = contentObj.value("sceneId").toString().toStdString();

  const QJsonArray objectsArray = contentObj.value("objects").toArray();
  for (const auto &value : objectsArray) {
    const QJsonObject objEntry = value.toObject();
    SceneDocumentObject sceneObj;
    sceneObj.id = objEntry.value("id").toString().toStdString();
    sceneObj.name = objEntry.value("name").toString().toStdString();
    sceneObj.type = objEntry.value("type").toString().toStdString();
    sceneObj.x = static_cast<float>(objEntry.value("x").toDouble());
    sceneObj.y = static_cast<float>(objEntry.value("y").toDouble());
    sceneObj.rotation = static_cast<float>(objEntry.value("rotation").toDouble());
    sceneObj.scaleX = static_cast<float>(objEntry.value("scaleX").toDouble(1.0));
    sceneObj.scaleY = static_cast<float>(objEntry.value("scaleY").toDouble(1.0));
    sceneObj.alpha = static_cast<float>(objEntry.value("alpha").toDouble(1.0));
    sceneObj.visible = objEntry.value("visible").toBool(true);
    sceneObj.zOrder = objEntry.value("zOrder").toInt();

    const QJsonObject propsObj = objEntry.value("properties").toObject();
    for (auto it = propsObj.begin(); it != propsObj.end(); ++it) {
      sceneObj.properties.emplace(it.key().toStdString(),
                                   it.value().toString().toStdString());
    }

    tmpl.content.objects.push_back(std::move(sceneObj));
  }

  return tmpl;
}

// ============================================================================
// SceneTemplateManager Implementation
// ============================================================================

SceneTemplateManager::SceneTemplateManager(QObject *parent) : QObject(parent) {}

SceneTemplateManager::~SceneTemplateManager() = default;

// ============================================================================
// Template Loading
// ============================================================================

int SceneTemplateManager::loadBuiltInTemplates() {
  int loaded = 0;

  // Try to load from Qt resources first
  QDir resourceDir(m_builtInTemplatesPath);
  if (resourceDir.exists()) {
    QStringList filters;
    filters << "*" + QString(TEMPLATE_FILE_EXTENSION);
    QFileInfoList files = resourceDir.entryInfoList(filters, QDir::Files);

    for (const QFileInfo &fileInfo : files) {
      auto tmpl = loadTemplateFromFile(fileInfo.absoluteFilePath(),
                                       SceneTemplateType::BuiltIn);
      if (tmpl.has_value()) {
        m_templates.insert(tmpl->metadata.id, tmpl.value());
        ++loaded;
      }
    }
  }

  // Always create default built-in templates if not loaded from files
  // This ensures templates are always available even without resource files
  createDefaultBuiltInTemplates();

  // Count only newly added templates
  return loaded;
}

int SceneTemplateManager::loadUserTemplates(const QString &projectPath) {
  if (projectPath.isEmpty()) {
    return 0;
  }

  m_currentProjectPath = projectPath;
  int loaded = 0;

  QString userTemplatesPath = QDir(projectPath).filePath(userTemplatesDir());
  QDir userDir(userTemplatesPath);

  if (!userDir.exists()) {
    return 0;
  }

  QStringList filters;
  filters << "*" + QString(TEMPLATE_FILE_EXTENSION);
  QFileInfoList files = userDir.entryInfoList(filters, QDir::Files);

  for (const QFileInfo &fileInfo : files) {
    auto tmpl =
        loadTemplateFromFile(fileInfo.absoluteFilePath(), SceneTemplateType::User);
    if (tmpl.has_value()) {
      // Prefix user template IDs to avoid conflicts with built-in
      QString userTemplateId = "user_" + tmpl->metadata.id;
      tmpl->metadata.id = userTemplateId;
      tmpl->metadata.type = SceneTemplateType::User;
      m_templates.insert(userTemplateId, tmpl.value());
      ++loaded;
    }
  }

  return loaded;
}

void SceneTemplateManager::reloadAllTemplates() {
  clearTemplates();
  loadBuiltInTemplates();
  if (!m_currentProjectPath.isEmpty()) {
    loadUserTemplates(m_currentProjectPath);
  }
  emit templatesReloaded();
}

void SceneTemplateManager::clearTemplates() {
  m_templates.clear();
  m_previewCache.clear();
}

// ============================================================================
// Template Query
// ============================================================================

QStringList SceneTemplateManager::getAvailableTemplateIds() const {
  return m_templates.keys();
}

QList<SceneTemplateMetadata>
SceneTemplateManager::getAvailableTemplates(const QString &category) const {
  QList<SceneTemplateMetadata> result;
  result.reserve(m_templates.size());

  for (auto it = m_templates.constBegin(); it != m_templates.constEnd(); ++it) {
    if (category.isEmpty() || it->metadata.category == category) {
      result.append(it->metadata);
    }
  }

  // Sort by name
  std::sort(result.begin(), result.end(),
            [](const SceneTemplateMetadata &a, const SceneTemplateMetadata &b) {
              return a.name.toLower() < b.name.toLower();
            });

  return result;
}

QStringList SceneTemplateManager::getCategories() const {
  QSet<QString> categories;
  for (auto it = m_templates.constBegin(); it != m_templates.constEnd(); ++it) {
    if (!it->metadata.category.isEmpty()) {
      categories.insert(it->metadata.category);
    }
  }

  QStringList result = categories.values();
  result.sort();
  return result;
}

std::optional<SceneTemplate>
SceneTemplateManager::getTemplate(const QString &templateId) const {
  if (m_templates.contains(templateId)) {
    return m_templates.value(templateId);
  }
  return std::nullopt;
}

SceneTemplateMetadata
SceneTemplateManager::getTemplateMetadata(const QString &templateId) const {
  if (m_templates.contains(templateId)) {
    return m_templates.value(templateId).metadata;
  }
  return SceneTemplateMetadata{};
}

bool SceneTemplateManager::hasTemplate(const QString &templateId) const {
  return m_templates.contains(templateId);
}

QPixmap
SceneTemplateManager::getTemplatePreview(const QString &templateId) const {
  // Check cache
  if (m_previewCache.contains(templateId)) {
    return m_previewCache.value(templateId);
  }

  // Try to load preview from file
  if (m_templates.contains(templateId)) {
    const auto &tmpl = m_templates.value(templateId);
    if (!tmpl.metadata.previewPath.isEmpty()) {
      QPixmap preview(tmpl.metadata.previewPath);
      if (!preview.isNull()) {
        const_cast<SceneTemplateManager *>(this)->m_previewCache.insert(
            templateId, preview);
        return preview;
      }
    }

    // Generate placeholder
    QPixmap placeholder =
        generatePlaceholderPreview(tmpl.metadata.name);
    const_cast<SceneTemplateManager *>(this)->m_previewCache.insert(
        templateId, placeholder);
    return placeholder;
  }

  return QPixmap();
}

// ============================================================================
// Template Instantiation
// ============================================================================

Result<SceneDocument>
SceneTemplateManager::instantiateTemplate(const QString &templateId,
                                          const QString &sceneId) const {
  if (!m_templates.contains(templateId)) {
    return Result<SceneDocument>::error("Template not found: " +
                                        templateId.toStdString());
  }

  const auto &tmpl = m_templates.value(templateId);
  SceneDocument doc = tmpl.content;

  // Update scene ID
  doc.sceneId = sceneId.toStdString();

  // Update object IDs to use the new scene ID as prefix
  for (auto &obj : doc.objects) {
    // Replace template placeholder {{scene_id}} if present
    QString objId = QString::fromStdString(obj.id);
    objId.replace("{{scene_id}}", sceneId);
    obj.id = objId.toStdString();
  }

  return Result<SceneDocument>::ok(doc);
}

Result<void>
SceneTemplateManager::createSceneFromTemplate(const QString &templateId,
                                              const QString &sceneId,
                                              const QString &outputPath) const {
  auto docResult = instantiateTemplate(templateId, sceneId);
  if (docResult.isError()) {
    return Result<void>::error(docResult.error());
  }

  // Ensure directory exists
  QFileInfo fileInfo(outputPath);
  QDir().mkpath(fileInfo.absolutePath());

  // Save the scene document
  auto saveResult = saveSceneDocument(docResult.value(), outputPath.toStdString());
  if (saveResult.isError()) {
    return Result<void>::error(saveResult.error());
  }

  return Result<void>::ok();
}

// ============================================================================
// User Template Management
// ============================================================================

Result<QString>
SceneTemplateManager::saveAsUserTemplate(const SceneDocument &scene,
                                         const QString &name,
                                         const QString &description,
                                         const QString &projectPath) {
  if (projectPath.isEmpty()) {
    return Result<QString>::error("No project path specified");
  }

  // Ensure user templates directory exists
  QString userTemplatesPath = QDir(projectPath).filePath(userTemplatesDir());
  QDir().mkpath(userTemplatesPath);

  // Generate template ID
  QString templateId = generateTemplateId(name);
  QString userTemplateId = "user_" + templateId;

  // Check if template already exists
  if (m_templates.contains(userTemplateId)) {
    // Find a unique ID
    int counter = 1;
    while (m_templates.contains(userTemplateId + "_" + QString::number(counter))) {
      ++counter;
    }
    templateId = templateId + "_" + QString::number(counter);
    userTemplateId = "user_" + templateId;
  }

  // Create template
  SceneTemplate tmpl;
  tmpl.metadata.id = userTemplateId;
  tmpl.metadata.name = name;
  tmpl.metadata.description = description;
  tmpl.metadata.category = "User Templates";
  tmpl.metadata.type = SceneTemplateType::User;
  tmpl.metadata.version = "1.0";
  tmpl.metadata.created = QDateTime::currentDateTime();
  tmpl.metadata.modified = tmpl.metadata.created;
  tmpl.content = scene;

  // Clear the scene ID (will be set on instantiation)
  tmpl.content.sceneId = "{{scene_id}}";

  // Save to file
  QString filePath =
      QDir(userTemplatesPath).filePath(templateId + TEMPLATE_FILE_EXTENSION);
  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return Result<QString>::error("Failed to create template file: " +
                                  filePath.toStdString());
  }

  QJsonDocument doc(tmpl.toJson());
  file.write(doc.toJson(QJsonDocument::Indented));
  file.close();

  // Add to loaded templates
  m_templates.insert(userTemplateId, tmpl);
  emit userTemplateCreated(userTemplateId);

  return Result<QString>::ok(userTemplateId);
}

Result<void>
SceneTemplateManager::deleteUserTemplate(const QString &templateId,
                                         const QString &projectPath) {
  if (projectPath.isEmpty()) {
    return Result<void>::error("No project path specified");
  }

  if (!m_templates.contains(templateId)) {
    return Result<void>::error("Template not found: " + templateId.toStdString());
  }

  const auto &tmpl = m_templates.value(templateId);
  if (tmpl.metadata.type != SceneTemplateType::User) {
    return Result<void>::error("Cannot delete built-in templates");
  }

  // Extract base ID (remove "user_" prefix)
  QString baseId = templateId.startsWith("user_") ? templateId.mid(5) : templateId;

  // Delete file
  QString userTemplatesPath = QDir(projectPath).filePath(userTemplatesDir());
  QString filePath =
      QDir(userTemplatesPath).filePath(baseId + TEMPLATE_FILE_EXTENSION);

  if (QFile::exists(filePath)) {
    if (!QFile::remove(filePath)) {
      return Result<void>::error("Failed to delete template file: " +
                                 filePath.toStdString());
    }
  }

  // Remove from loaded templates
  m_templates.remove(templateId);
  m_previewCache.remove(templateId);
  emit userTemplateDeleted(templateId);

  return Result<void>::ok();
}

Result<void>
SceneTemplateManager::updateUserTemplate(const QString &templateId,
                                         const SceneDocument &scene,
                                         const QString &projectPath) {
  if (projectPath.isEmpty()) {
    return Result<void>::error("No project path specified");
  }

  if (!m_templates.contains(templateId)) {
    return Result<void>::error("Template not found: " + templateId.toStdString());
  }

  auto &tmpl = m_templates[templateId];
  if (tmpl.metadata.type != SceneTemplateType::User) {
    return Result<void>::error("Cannot update built-in templates");
  }

  // Update content
  tmpl.content = scene;
  tmpl.content.sceneId = "{{scene_id}}";
  tmpl.metadata.modified = QDateTime::currentDateTime();

  // Extract base ID (remove "user_" prefix)
  QString baseId = templateId.startsWith("user_") ? templateId.mid(5) : templateId;

  // Save to file
  QString userTemplatesPath = QDir(projectPath).filePath(userTemplatesDir());
  QString filePath =
      QDir(userTemplatesPath).filePath(baseId + TEMPLATE_FILE_EXTENSION);

  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return Result<void>::error("Failed to update template file: " +
                               filePath.toStdString());
  }

  QJsonDocument doc(tmpl.toJson());
  file.write(doc.toJson(QJsonDocument::Indented));
  file.close();

  // Clear preview cache to regenerate
  m_previewCache.remove(templateId);
  emit userTemplateUpdated(templateId);

  return Result<void>::ok();
}

// ============================================================================
// Configuration
// ============================================================================

void SceneTemplateManager::setBuiltInTemplatesPath(const QString &path) {
  m_builtInTemplatesPath = path;
}

// ============================================================================
// Private Methods
// ============================================================================

std::optional<SceneTemplate>
SceneTemplateManager::loadTemplateFromFile(const QString &filePath,
                                           SceneTemplateType type) {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    qWarning() << "Failed to open template file:" << filePath;
    return std::nullopt;
  }

  QByteArray data = file.readAll();
  file.close();

  QJsonParseError error;
  QJsonDocument doc = QJsonDocument::fromJson(data, &error);
  if (error.error != QJsonParseError::NoError) {
    qWarning() << "Failed to parse template JSON:" << error.errorString();
    return std::nullopt;
  }

  SceneTemplate tmpl = SceneTemplate::fromJson(doc.object());
  tmpl.metadata.type = type;

  if (tmpl.metadata.id.isEmpty()) {
    // Generate ID from filename
    QFileInfo fileInfo(filePath);
    tmpl.metadata.id = fileInfo.baseName();
  }

  return tmpl;
}

QString SceneTemplateManager::generateTemplateId(const QString &name) const {
  // Convert to lowercase and replace spaces with underscores
  QString id = name.toLower().trimmed();
  id.replace(' ', '_');

  // Remove characters that aren't alphanumeric or underscore
  QString sanitized;
  for (const QChar &ch : id) {
    if (ch.isLetterOrNumber() || ch == '_') {
      sanitized += ch;
    }
  }

  // Remove consecutive underscores
  while (sanitized.contains("__")) {
    sanitized.replace("__", "_");
  }

  // Remove leading/trailing underscores
  while (sanitized.startsWith('_')) {
    sanitized.remove(0, 1);
  }
  while (sanitized.endsWith('_')) {
    sanitized.chop(1);
  }

  return sanitized.isEmpty() ? "template" : sanitized;
}

void SceneTemplateManager::createDefaultBuiltInTemplates() {
  // Only create if not already loaded
  if (!m_templates.contains("empty_scene")) {
    m_templates.insert("empty_scene", createEmptySceneTemplate());
  }
  if (!m_templates.contains("dialogue_scene")) {
    m_templates.insert("dialogue_scene", createDialogueSceneTemplate());
  }
  if (!m_templates.contains("choice_scene")) {
    m_templates.insert("choice_scene", createChoiceSceneTemplate());
  }
  if (!m_templates.contains("cutscene")) {
    m_templates.insert("cutscene", createCutsceneTemplate());
  }
  if (!m_templates.contains("title_screen")) {
    m_templates.insert("title_screen", createTitleScreenTemplate());
  }
}

SceneTemplate SceneTemplateManager::createEmptySceneTemplate() const {
  SceneTemplate tmpl;
  tmpl.metadata.id = "empty_scene";
  tmpl.metadata.name = "Empty Scene";
  tmpl.metadata.description =
      "Blank canvas for custom scenes. Start from scratch with no pre-defined "
      "objects.";
  tmpl.metadata.category = "Standard";
  tmpl.metadata.type = SceneTemplateType::BuiltIn;
  tmpl.metadata.author = "NovelMind Team";
  tmpl.metadata.version = "1.0";
  tmpl.metadata.tags = QStringList{"empty", "blank", "custom"};

  tmpl.content.sceneId = "{{scene_id}}";
  // No objects - truly empty

  return tmpl;
}

SceneTemplate SceneTemplateManager::createDialogueSceneTemplate() const {
  SceneTemplate tmpl;
  tmpl.metadata.id = "dialogue_scene";
  tmpl.metadata.name = "Dialogue Scene";
  tmpl.metadata.description =
      "Standard visual novel dialogue scene with background, two character "
      "positions (left and right), and dialogue UI placeholder.";
  tmpl.metadata.category = "Visual Novel";
  tmpl.metadata.type = SceneTemplateType::BuiltIn;
  tmpl.metadata.author = "NovelMind Team";
  tmpl.metadata.version = "1.0";
  tmpl.metadata.tags =
      QStringList{"dialogue", "conversation", "vn", "characters"};

  tmpl.content.sceneId = "{{scene_id}}";

  // Background placeholder
  SceneDocumentObject background;
  background.id = "background";
  background.name = "Background";
  background.type = "Background";
  background.x = 0.0f;
  background.y = 0.0f;
  background.zOrder = 0;
  background.properties["placeholder"] = "true";
  background.properties["hint"] = "Drop background image here";
  tmpl.content.objects.push_back(std::move(background));

  // Left character position
  SceneDocumentObject charLeft;
  charLeft.id = "character_left";
  charLeft.name = "Left Character";
  charLeft.type = "Character";
  charLeft.x = 200.0f;
  charLeft.y = 360.0f;
  charLeft.zOrder = 10;
  charLeft.properties["placeholder"] = "true";
  charLeft.properties["hint"] = "Left character position";
  charLeft.properties["position"] = "left";
  tmpl.content.objects.push_back(std::move(charLeft));

  // Right character position
  SceneDocumentObject charRight;
  charRight.id = "character_right";
  charRight.name = "Right Character";
  charRight.type = "Character";
  charRight.x = 1080.0f;
  charRight.y = 360.0f;
  charRight.zOrder = 10;
  charRight.properties["placeholder"] = "true";
  charRight.properties["hint"] = "Right character position";
  charRight.properties["position"] = "right";
  tmpl.content.objects.push_back(std::move(charRight));

  // Dialogue box placeholder
  SceneDocumentObject dialogueBox;
  dialogueBox.id = "dialogue_box";
  dialogueBox.name = "Dialogue Box";
  dialogueBox.type = "UI";
  dialogueBox.x = 0.0f;
  dialogueBox.y = 520.0f;
  dialogueBox.zOrder = 100;
  dialogueBox.properties["placeholder"] = "true";
  dialogueBox.properties["hint"] = "Dialogue UI area";
  dialogueBox.properties["uiType"] = "dialogue";
  tmpl.content.objects.push_back(std::move(dialogueBox));

  return tmpl;
}

SceneTemplate SceneTemplateManager::createChoiceSceneTemplate() const {
  SceneTemplate tmpl;
  tmpl.metadata.id = "choice_scene";
  tmpl.metadata.name = "Choice Scene";
  tmpl.metadata.description =
      "Scene for player choices with background, character position, "
      "and choice menu layout.";
  tmpl.metadata.category = "Visual Novel";
  tmpl.metadata.type = SceneTemplateType::BuiltIn;
  tmpl.metadata.author = "NovelMind Team";
  tmpl.metadata.version = "1.0";
  tmpl.metadata.tags = QStringList{"choice", "menu", "decision", "branching"};

  tmpl.content.sceneId = "{{scene_id}}";

  // Background placeholder
  SceneDocumentObject background;
  background.id = "background";
  background.name = "Background";
  background.type = "Background";
  background.x = 0.0f;
  background.y = 0.0f;
  background.zOrder = 0;
  background.properties["placeholder"] = "true";
  background.properties["hint"] = "Drop background image here";
  tmpl.content.objects.push_back(std::move(background));

  // Center character position
  SceneDocumentObject character;
  character.id = "character_center";
  character.name = "Character";
  character.type = "Character";
  character.x = 640.0f;
  character.y = 360.0f;
  character.zOrder = 10;
  character.properties["placeholder"] = "true";
  character.properties["hint"] = "Character speaking";
  character.properties["position"] = "center";
  tmpl.content.objects.push_back(std::move(character));

  // Choice menu placeholder
  SceneDocumentObject choiceMenu;
  choiceMenu.id = "choice_menu";
  choiceMenu.name = "Choice Menu";
  choiceMenu.type = "UI";
  choiceMenu.x = 640.0f;
  choiceMenu.y = 400.0f;
  choiceMenu.zOrder = 100;
  choiceMenu.properties["placeholder"] = "true";
  choiceMenu.properties["hint"] = "Choice menu area";
  choiceMenu.properties["uiType"] = "choice_menu";
  tmpl.content.objects.push_back(std::move(choiceMenu));

  return tmpl;
}

SceneTemplate SceneTemplateManager::createCutsceneTemplate() const {
  SceneTemplate tmpl;
  tmpl.metadata.id = "cutscene";
  tmpl.metadata.name = "Cutscene";
  tmpl.metadata.description =
      "Fullscreen cutscene with no UI elements. Perfect for dramatic "
      "moments, transitions, and cinematic sequences.";
  tmpl.metadata.category = "Cinematic";
  tmpl.metadata.type = SceneTemplateType::BuiltIn;
  tmpl.metadata.author = "NovelMind Team";
  tmpl.metadata.version = "1.0";
  tmpl.metadata.tags =
      QStringList{"cutscene", "cinematic", "fullscreen", "transition"};

  tmpl.content.sceneId = "{{scene_id}}";

  // Fullscreen background
  SceneDocumentObject background;
  background.id = "background";
  background.name = "Cutscene Background";
  background.type = "Background";
  background.x = 0.0f;
  background.y = 0.0f;
  background.zOrder = 0;
  background.properties["placeholder"] = "true";
  background.properties["hint"] = "Fullscreen cutscene image or video";
  background.properties["fullscreen"] = "true";
  tmpl.content.objects.push_back(std::move(background));

  return tmpl;
}

SceneTemplate SceneTemplateManager::createTitleScreenTemplate() const {
  SceneTemplate tmpl;
  tmpl.metadata.id = "title_screen";
  tmpl.metadata.name = "Title Screen";
  tmpl.metadata.description =
      "Title/menu screen with logo position and menu button layout. "
      "Ideal for game start screens and main menus.";
  tmpl.metadata.category = "Menu";
  tmpl.metadata.type = SceneTemplateType::BuiltIn;
  tmpl.metadata.author = "NovelMind Team";
  tmpl.metadata.version = "1.0";
  tmpl.metadata.tags = QStringList{"title", "menu", "start", "logo"};

  tmpl.content.sceneId = "{{scene_id}}";

  // Background
  SceneDocumentObject background;
  background.id = "background";
  background.name = "Title Background";
  background.type = "Background";
  background.x = 0.0f;
  background.y = 0.0f;
  background.zOrder = 0;
  background.properties["placeholder"] = "true";
  background.properties["hint"] = "Title screen background";
  tmpl.content.objects.push_back(std::move(background));

  // Logo position
  SceneDocumentObject logo;
  logo.id = "logo";
  logo.name = "Game Logo";
  logo.type = "UI";
  logo.x = 640.0f;
  logo.y = 180.0f;
  logo.zOrder = 50;
  logo.properties["placeholder"] = "true";
  logo.properties["hint"] = "Game logo position";
  logo.properties["uiType"] = "logo";
  tmpl.content.objects.push_back(std::move(logo));

  // Menu buttons area
  SceneDocumentObject menuButtons;
  menuButtons.id = "menu_buttons";
  menuButtons.name = "Menu Buttons";
  menuButtons.type = "UI";
  menuButtons.x = 640.0f;
  menuButtons.y = 450.0f;
  menuButtons.zOrder = 100;
  menuButtons.properties["placeholder"] = "true";
  menuButtons.properties["hint"] = "Menu button layout area";
  menuButtons.properties["uiType"] = "menu";
  tmpl.content.objects.push_back(std::move(menuButtons));

  // Version text placeholder
  SceneDocumentObject versionText;
  versionText.id = "version_text";
  versionText.name = "Version Text";
  versionText.type = "UI";
  versionText.x = 1180.0f;
  versionText.y = 680.0f;
  versionText.zOrder = 100;
  versionText.properties["placeholder"] = "true";
  versionText.properties["hint"] = "Version number position";
  versionText.properties["uiType"] = "text";
  tmpl.content.objects.push_back(std::move(versionText));

  return tmpl;
}

QPixmap SceneTemplateManager::generatePlaceholderPreview(
    const QString &templateName) const {
  QImage image(PREVIEW_WIDTH, PREVIEW_HEIGHT, QImage::Format_ARGB32);
  image.fill(Qt::transparent);

  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setRenderHint(QPainter::TextAntialiasing);

  // Background gradient
  QLinearGradient gradient(0, 0, 0, PREVIEW_HEIGHT);
  gradient.setColorAt(0, QColor(45, 45, 50));
  gradient.setColorAt(1, QColor(35, 35, 40));
  painter.fillRect(image.rect(), gradient);

  // Border
  painter.setPen(QPen(QColor(80, 80, 90), 2));
  painter.drawRect(image.rect().adjusted(1, 1, -1, -1));

  // Template name
  painter.setPen(QColor(200, 200, 200));
  QFont font = painter.font();
  font.setPointSize(12);
  font.setBold(true);
  painter.setFont(font);
  painter.drawText(image.rect(), Qt::AlignCenter | Qt::TextWordWrap, templateName);

  // Template icon indicator
  painter.setPen(QColor(100, 150, 200));
  font.setPointSize(8);
  font.setBold(false);
  painter.setFont(font);
  painter.drawText(image.rect().adjusted(5, 5, -5, -PREVIEW_HEIGHT + 20),
                   Qt::AlignLeft | Qt::AlignTop, "Template");

  painter.end();
  return QPixmap::fromImage(image);
}

} // namespace NovelMind::editor
