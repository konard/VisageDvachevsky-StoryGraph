#include "NovelMind/editor/scene_registry.hpp"
#include "NovelMind/editor/scene_document.hpp"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPainter>
#include <QRegularExpression>

namespace NovelMind::editor {

// ============================================================================
// SceneMetadata Implementation
// ============================================================================

QJsonObject SceneMetadata::toJson() const {
  QJsonObject obj;
  obj.insert("id", id);
  obj.insert("name", name);
  obj.insert("documentPath", documentPath);
  obj.insert("thumbnailPath", thumbnailPath);
  obj.insert("created", created.toString(Qt::ISODate));
  obj.insert("modified", modified.toString(Qt::ISODate));

  QJsonArray tagsArray;
  for (const QString& tag : tags) {
    tagsArray.append(tag);
  }
  obj.insert("tags", tagsArray);

  if (!description.isEmpty()) {
    obj.insert("description", description);
  }

  // Serialize referencing nodes (issue #211)
  if (!referencingNodes.isEmpty()) {
    QJsonArray refsArray;
    for (const QString& ref : referencingNodes) {
      refsArray.append(ref);
    }
    obj.insert("referencingNodes", refsArray);
  }

  return obj;
}

SceneMetadata SceneMetadata::fromJson(const QJsonObject& json) {
  SceneMetadata meta;
  meta.id = json.value("id").toString();
  meta.name = json.value("name").toString();
  meta.documentPath = json.value("documentPath").toString();
  meta.thumbnailPath = json.value("thumbnailPath").toString();
  meta.created = QDateTime::fromString(json.value("created").toString(), Qt::ISODate);
  meta.modified = QDateTime::fromString(json.value("modified").toString(), Qt::ISODate);
  meta.description = json.value("description").toString();

  const QJsonArray tagsArray = json.value("tags").toArray();
  meta.tags.reserve(tagsArray.size());
  for (const auto& value : tagsArray) {
    meta.tags.append(value.toString());
  }

  // Deserialize referencing nodes (issue #211)
  const QJsonArray refsArray = json.value("referencingNodes").toArray();
  meta.referencingNodes.reserve(refsArray.size());
  for (const auto& value : refsArray) {
    meta.referencingNodes.append(value.toString());
  }

  return meta;
}

// ============================================================================
// SceneRegistry Implementation
// ============================================================================

SceneRegistry::SceneRegistry(QObject* parent) : QObject(parent) {}

SceneRegistry::~SceneRegistry() = default;

// ============================================================================
// Scene Management
// ============================================================================

QString SceneRegistry::registerScene(const QString& name, const QString& basePath) {
  QString sceneId = generateUniqueSceneId(name);

  SceneMetadata meta;
  meta.id = sceneId;
  meta.name = name;
  meta.created = QDateTime::currentDateTime();
  meta.modified = meta.created;

  // Construct document path
  QString docPath = basePath.isEmpty() ? QString("Scenes/%1.nmscene").arg(sceneId)
                                       : QString("Scenes/%1/%2.nmscene").arg(basePath, sceneId);
  meta.documentPath = docPath;

  // Thumbnail path will be set when generated
  meta.thumbnailPath = QString("%1/%2.png").arg(THUMBNAILS_DIR, sceneId);

  m_scenes.insert(sceneId, meta);
  m_modified = true;

  emit sceneRegistered(sceneId);
  return sceneId;
}

bool SceneRegistry::sceneExists(const QString& sceneId) const {
  return m_scenes.contains(sceneId);
}

SceneMetadata SceneRegistry::getSceneMetadata(const QString& sceneId) const {
  return m_scenes.value(sceneId);
}

QString SceneRegistry::getSceneDocumentPath(const QString& sceneId) const {
  if (m_scenes.contains(sceneId)) {
    return m_scenes.value(sceneId).documentPath;
  }
  return QString();
}

QString SceneRegistry::getSceneThumbnailPath(const QString& sceneId) const {
  if (m_scenes.contains(sceneId)) {
    return m_scenes.value(sceneId).thumbnailPath;
  }
  return QString();
}

bool SceneRegistry::renameScene(const QString& sceneId, const QString& newName) {
  if (!m_scenes.contains(sceneId)) {
    return false;
  }

  // Capture old name before updating
  const QString oldName = m_scenes[sceneId].name;

  m_scenes[sceneId].name = newName;
  updateModifiedTime(sceneId);
  m_modified = true;

  emit sceneRenamed(sceneId, oldName, newName);
  return true;
}

bool SceneRegistry::unregisterScene(const QString& sceneId) {
  if (!m_scenes.contains(sceneId)) {
    return false;
  }

  m_scenes.remove(sceneId);
  m_modified = true;

  emit sceneUnregistered(sceneId);
  return true;
}

bool SceneRegistry::deleteScene(const QString& sceneId) {
  // Alias for unregisterScene for API compatibility (issue #211)
  return unregisterScene(sceneId);
}

QString SceneRegistry::getScenePath(const QString& sceneId) const {
  if (m_projectPath.isEmpty() || !m_scenes.contains(sceneId)) {
    return QString();
  }

  const QString docPath = m_scenes.value(sceneId).documentPath;
  if (docPath.isEmpty()) {
    return QString();
  }

  return QDir(m_projectPath).filePath(docPath);
}

QStringList SceneRegistry::getAllSceneIds() const {
  return m_scenes.keys();
}

QList<SceneMetadata> SceneRegistry::getScenes(const QStringList& tags) const {
  QList<SceneMetadata> result;
  result.reserve(m_scenes.size());

  for (const auto& meta : m_scenes) {
    if (tags.isEmpty()) {
      result.append(meta);
    } else {
      // Check if scene has any of the requested tags
      bool hasTag = false;
      for (const QString& tag : tags) {
        if (meta.tags.contains(tag, Qt::CaseInsensitive)) {
          hasTag = true;
          break;
        }
      }
      if (hasTag) {
        result.append(meta);
      }
    }
  }

  return result;
}

bool SceneRegistry::updateSceneMetadata(const QString& sceneId, const SceneMetadata& metadata) {
  if (!m_scenes.contains(sceneId)) {
    return false;
  }

  // Preserve the ID
  SceneMetadata updated = metadata;
  updated.id = sceneId;
  updated.modified = QDateTime::currentDateTime();

  m_scenes[sceneId] = updated;
  m_modified = true;

  emit sceneMetadataChanged(sceneId);
  return true;
}

// ============================================================================
// Cross-Reference Tracking (Issue #211)
// ============================================================================

bool SceneRegistry::addSceneReference(const QString& sceneId, const QString& nodeIdString) {
  if (!m_scenes.contains(sceneId)) {
    return false;
  }

  if (m_scenes[sceneId].referencingNodes.contains(nodeIdString)) {
    return false; // Already exists
  }

  m_scenes[sceneId].referencingNodes.append(nodeIdString);
  updateModifiedTime(sceneId);
  m_modified = true;

  emit sceneReferenceAdded(sceneId, nodeIdString);
  return true;
}

bool SceneRegistry::removeSceneReference(const QString& sceneId, const QString& nodeIdString) {
  if (!m_scenes.contains(sceneId)) {
    return false;
  }

  int index = static_cast<int>(m_scenes[sceneId].referencingNodes.indexOf(nodeIdString));
  if (index == -1) {
    return false; // Not found
  }

  m_scenes[sceneId].referencingNodes.removeAt(index);
  updateModifiedTime(sceneId);
  m_modified = true;

  emit sceneReferenceRemoved(sceneId, nodeIdString);
  return true;
}

QStringList SceneRegistry::getSceneReferences(const QString& sceneId) const {
  if (!m_scenes.contains(sceneId)) {
    return QStringList();
  }

  return m_scenes.value(sceneId).referencingNodes;
}

void SceneRegistry::updateSceneReferences(const QString& oldSceneId, const QString& newSceneId) {
  // This method is called when a scene ID is changed
  // The references are already moved to the new ID by renameSceneId()
  // This is just a hook for any additional reference update logic if needed
  Q_UNUSED(oldSceneId);
  Q_UNUSED(newSceneId);
}

bool SceneRegistry::renameSceneId(const QString& oldId, const QString& newId) {
  if (!m_scenes.contains(oldId)) {
    return false;
  }

  if (m_scenes.contains(newId)) {
    return false; // New ID already exists
  }

  // Validate new ID format
  QString sanitizedNewId = sanitizeForId(newId);
  if (sanitizedNewId != newId) {
    // If the new ID would be sanitized differently, reject it
    // Caller should provide a properly formatted ID
    return false;
  }

  // Get the scene metadata
  SceneMetadata meta = m_scenes.take(oldId);

  // Update the ID
  meta.id = newId;

  // Update the document path
  // Replace the old ID in the path with the new ID
  QString oldDocPath = meta.documentPath;
  QString basePath = QFileInfo(oldDocPath).path();
  meta.documentPath = QString("%1/%2.nmscene").arg(basePath, newId);

  // Update thumbnail path
  meta.thumbnailPath = QString("%1/%2.png").arg(THUMBNAILS_DIR, newId);

  // Update modified time
  meta.modified = QDateTime::currentDateTime();

  // Insert with new ID
  m_scenes.insert(newId, meta);
  m_modified = true;

  // Update references tracking
  updateSceneReferences(oldId, newId);

  // Emit signal so Story Graph can update its nodes
  emit sceneIdChanged(oldId, newId);

  return true;
}

// ============================================================================
// Thumbnail Management
// ============================================================================

bool SceneRegistry::generateThumbnail(const QString& sceneId, const QSize& size) {
  if (!m_scenes.contains(sceneId)) {
    return false;
  }

  if (m_projectPath.isEmpty()) {
    return false;
  }

  const SceneMetadata& meta = m_scenes[sceneId];

  // Load scene document
  QString documentPath = QDir(m_projectPath).filePath(meta.documentPath);
  auto docResult = loadSceneDocument(documentPath.toStdString());
  if (docResult.isError()) {
    qWarning() << "Failed to load scene document for thumbnail:" << documentPath;
    return false;
  }

  const SceneDocument& doc = docResult.value();

  // Create thumbnail image
  QImage image(size, QImage::Format_ARGB32);
  image.fill(Qt::transparent);

  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setRenderHint(QPainter::SmoothPixmapTransform);

  // Fill background
  painter.fillRect(image.rect(), QColor(40, 40, 45));

  // Draw simple representation of scene objects
  // This is a basic implementation - can be enhanced later
  const int margin = 10;
  const QRect contentRect = image.rect().adjusted(margin, margin, -margin, -margin);

  // Draw scene name as placeholder
  painter.setPen(QColor(180, 180, 180));
  QFont font = painter.font();
  font.setPointSize(12);
  painter.setFont(font);
  painter.drawText(contentRect, Qt::AlignCenter | Qt::TextWordWrap, meta.name);

  // Draw object count indicator
  if (!doc.objects.empty()) {
    QString objectInfo = QString("%1 objects").arg(doc.objects.size());
    painter.setPen(QColor(100, 100, 100));
    font.setPointSize(8);
    painter.setFont(font);
    painter.drawText(contentRect, Qt::AlignBottom | Qt::AlignRight, objectInfo);
  }

  painter.end();

  // Ensure thumbnail directory exists
  QString thumbnailDir = QDir(m_projectPath).filePath(THUMBNAILS_DIR);
  QDir().mkpath(thumbnailDir);

  // Save thumbnail
  QString thumbnailPath = QDir(m_projectPath).filePath(meta.thumbnailPath);
  if (!image.save(thumbnailPath)) {
    qWarning() << "Failed to save thumbnail:" << thumbnailPath;
    return false;
  }

  // Update metadata
  m_scenes[sceneId].modified = QDateTime::currentDateTime();
  m_modified = true;

  emit sceneThumbnailUpdated(sceneId);
  return true;
}

void SceneRegistry::clearThumbnailCache() {
  if (m_projectPath.isEmpty()) {
    return;
  }

  QDir thumbnailDir(QDir(m_projectPath).filePath(THUMBNAILS_DIR));
  if (thumbnailDir.exists()) {
    // Remove all .png files
    QStringList filters;
    filters << "*.png";
    QFileInfoList files = thumbnailDir.entryInfoList(filters, QDir::Files);
    for (const QFileInfo& fileInfo : files) {
      QFile::remove(fileInfo.absoluteFilePath());
    }
  }
}

QString SceneRegistry::getAbsoluteThumbnailPath(const QString& sceneId) const {
  if (m_projectPath.isEmpty() || !m_scenes.contains(sceneId)) {
    return QString();
  }

  const QString thumbnailPath = m_scenes.value(sceneId).thumbnailPath;
  if (thumbnailPath.isEmpty()) {
    return QString();
  }

  return QDir(m_projectPath).filePath(thumbnailPath);
}

// ============================================================================
// Validation
// ============================================================================

QStringList SceneRegistry::validateScenes() const {
  QStringList errors;

  if (m_projectPath.isEmpty()) {
    errors << "No project path set";
    return errors;
  }

  for (auto it = m_scenes.constBegin(); it != m_scenes.constEnd(); ++it) {
    const SceneMetadata& meta = it.value();

    // Check if document exists
    QString fullDocPath = QDir(m_projectPath).filePath(meta.documentPath);
    if (!QFile::exists(fullDocPath)) {
      errors << QString("Scene '%1' document not found: %2").arg(meta.id, meta.documentPath);
    }

    // Validate ID format
    if (meta.id.isEmpty()) {
      errors << QString("Scene has empty ID: %1").arg(meta.name);
    }

    // Validate name
    if (meta.name.isEmpty()) {
      errors << QString("Scene '%1' has empty name").arg(meta.id);
    }
  }

  return errors;
}

QStringList SceneRegistry::findOrphanedScenes() const {
  QStringList orphaned;

  if (m_projectPath.isEmpty()) {
    return orphaned;
  }

  // Find all .nmscene files in project
  QDir scenesDir(QDir(m_projectPath).filePath("Scenes"));
  if (!scenesDir.exists()) {
    return orphaned;
  }

  QStringList filters;
  filters << "*.nmscene";
  QFileInfoList sceneFiles = scenesDir.entryInfoList(filters, QDir::Files, QDir::Name);

  // Also check subdirectories
  QFileInfoList subDirs = scenesDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
  for (const QFileInfo& subDir : subDirs) {
    if (subDir.fileName() == ".thumbnails") {
      continue; // Skip thumbnails directory
    }
    QDir subDirectory(subDir.absoluteFilePath());
    sceneFiles.append(subDirectory.entryInfoList(filters, QDir::Files, QDir::Name));
  }

  for (const QFileInfo& fileInfo : sceneFiles) {
    // Get relative path
    QString relativePath = QDir(m_projectPath).relativeFilePath(fileInfo.absoluteFilePath());

    // Check if registered
    bool isRegistered = false;
    for (auto it = m_scenes.constBegin(); it != m_scenes.constEnd(); ++it) {
      if (it.value().documentPath == relativePath) {
        isRegistered = true;
        break;
      }
    }

    if (!isRegistered) {
      orphaned << fileInfo.absoluteFilePath();
    }
  }

  return orphaned;
}

QStringList SceneRegistry::findBrokenReferences() const {
  QStringList broken;

  if (m_projectPath.isEmpty()) {
    return broken;
  }

  for (auto it = m_scenes.constBegin(); it != m_scenes.constEnd(); ++it) {
    QString fullPath = QDir(m_projectPath).filePath(it.value().documentPath);
    if (!QFile::exists(fullPath)) {
      broken << it.key();
    }
  }

  return broken;
}

QStringList SceneRegistry::getOrphanedSceneDocuments() const {
  // Alias for findOrphanedScenes for API compatibility (issue #211)
  return findOrphanedScenes();
}

QStringList SceneRegistry::getInvalidSceneReferences() const {
  // Alias for findBrokenReferences for API compatibility (issue #211)
  return findBrokenReferences();
}

// ============================================================================
// Persistence
// ============================================================================

bool SceneRegistry::load(const QString& projectPath) {
  m_projectPath = projectPath;
  m_registryFilePath = QDir(projectPath).filePath(REGISTRY_FILENAME);
  m_scenes.clear();
  m_modified = false;

  QFile file(m_registryFilePath);
  if (!file.exists()) {
    // Registry doesn't exist yet - this is OK for new projects
    emit registryLoaded();
    return true;
  }

  if (!file.open(QIODevice::ReadOnly)) {
    qWarning() << "Failed to open scene registry:" << m_registryFilePath;
    return false;
  }

  QByteArray data = file.readAll();
  file.close();

  QJsonParseError error;
  QJsonDocument doc = QJsonDocument::fromJson(data, &error);
  if (error.error != QJsonParseError::NoError) {
    qWarning() << "Failed to parse scene registry JSON:" << error.errorString();
    return false;
  }

  if (!fromJson(doc.object())) {
    return false;
  }

  emit registryLoaded();
  return true;
}

bool SceneRegistry::save(const QString& projectPath) {
  QString savePath = projectPath.isEmpty() ? m_projectPath : projectPath;
  if (savePath.isEmpty()) {
    qWarning() << "No project path set for scene registry save";
    return false;
  }

  m_projectPath = savePath;
  m_registryFilePath = QDir(savePath).filePath(REGISTRY_FILENAME);

  QFile file(m_registryFilePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    qWarning() << "Failed to open scene registry for writing:" << m_registryFilePath;
    return false;
  }

  QJsonDocument doc(toJson());
  file.write(doc.toJson(QJsonDocument::Indented));
  file.close();

  m_modified = false;
  emit registrySaved();
  return true;
}

QJsonObject SceneRegistry::toJson() const {
  QJsonObject root;
  root.insert("version", REGISTRY_VERSION);

  QJsonArray scenesArray;
  for (auto it = m_scenes.constBegin(); it != m_scenes.constEnd(); ++it) {
    scenesArray.append(it.value().toJson());
  }
  root.insert("scenes", scenesArray);

  return root;
}

bool SceneRegistry::fromJson(const QJsonObject& json) {
  QString version = json.value("version").toString();
  if (version.isEmpty()) {
    qWarning() << "Scene registry missing version field";
    return false;
  }

  m_scenes.clear();

  const QJsonArray scenesArray = json.value("scenes").toArray();
  for (const auto& value : scenesArray) {
    SceneMetadata meta = SceneMetadata::fromJson(value.toObject());
    if (!meta.id.isEmpty()) {
      m_scenes.insert(meta.id, meta);
    }
  }

  m_modified = false;
  return true;
}

// ============================================================================
// Private Methods
// ============================================================================

QString SceneRegistry::generateUniqueSceneId(const QString& baseName) const {
  QString baseId = sanitizeForId(baseName);
  if (baseId.isEmpty()) {
    baseId = "scene";
  }

  // Check if base ID is unique
  if (!m_scenes.contains(baseId)) {
    return baseId;
  }

  // Append number to make unique
  int counter = 1;
  QString candidateId;
  do {
    candidateId = QString("%1_%2").arg(baseId).arg(counter++);
  } while (m_scenes.contains(candidateId));

  return candidateId;
}

void SceneRegistry::updateModifiedTime(const QString& sceneId) {
  if (m_scenes.contains(sceneId)) {
    m_scenes[sceneId].modified = QDateTime::currentDateTime();
  }
}

QString SceneRegistry::sanitizeForId(const QString& name) {
  // Convert to lowercase and replace spaces with underscores
  QString id = name.toLower().trimmed();

  // Replace spaces with underscores
  id.replace(' ', '_');

  // Remove characters that aren't alphanumeric or underscore
  static QRegularExpression invalidChars("[^a-z0-9_]");
  id.remove(invalidChars);

  // Remove consecutive underscores
  static QRegularExpression multiUnderscore("_+");
  id.replace(multiUnderscore, "_");

  // Remove leading/trailing underscores
  while (id.startsWith('_')) {
    id.remove(0, 1);
  }
  while (id.endsWith('_')) {
    id.chop(1);
  }

  return id;
}

} // namespace NovelMind::editor
