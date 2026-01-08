#include "NovelMind/editor/event_bus.hpp"
#include "NovelMind/editor/events/panel_events.hpp"
#include "NovelMind/editor/project_manager.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"
#include "NovelMind/editor/qt/panels/nm_scene_view_panel.hpp"
#include "NovelMind/editor/scene_document.hpp"

#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QPainter>
#include <QPixmap>

namespace NovelMind::editor::qt {

bool NMSceneViewPanel::loadSceneDocument(const QString &sceneId) {
  if (!m_scene || sceneId.isEmpty()) {
    return false;
  }

  if (!m_currentSceneId.isEmpty() && m_currentSceneId != sceneId) {
    if (!m_suppressSceneSave) {
      saveSceneDocument();
    }
  }

  if (m_runtimePreviewActive) {
    clearRuntimePreview();
  }

  m_isLoadingScene = true;
  m_currentSceneId = sceneId;
  if (m_infoOverlay) {
    m_infoOverlay->setSceneInfo(sceneId);
  }

  const QString scenesRoot = QString::fromStdString(
      ProjectManager::instance().getFolderPath(ProjectFolder::Scenes));
  if (scenesRoot.isEmpty()) {
    m_isLoadingScene = false;
    return false;
  }

  QDir().mkpath(scenesRoot);

  const QString scenePath = QDir(scenesRoot).filePath(sceneId + ".nmscene");
  const auto result =
      ::NovelMind::editor::loadSceneDocument(scenePath.toStdString());

  for (auto *obj : m_scene->sceneObjects()) {
    if (!obj) {
      continue;
    }
    m_scene->removeSceneObject(obj->id());
  }

  if (result.isOk()) {
    const auto &doc = result.value();
    for (const auto &item : doc.objects) {
      NMSceneObjectType type = NMSceneObjectType::UI;
      if (item.type == "Background") {
        type = NMSceneObjectType::Background;
      } else if (item.type == "Character") {
        type = NMSceneObjectType::Character;
      } else if (item.type == "Effect") {
        type = NMSceneObjectType::Effect;
      }

      const QString id = QString::fromStdString(item.id);
      auto *obj = new NMSceneObject(id, type);
      obj->setName(QString::fromStdString(item.name));
      obj->setPos(QPointF(item.x, item.y));
      obj->setRotation(item.rotation);
      obj->setScaleXY(item.scaleX, item.scaleY);
      obj->setOpacity(item.alpha);
      obj->setVisible(item.visible);
      obj->setZValue(item.zOrder);

      QString asset;
      auto it = item.properties.find("textureId");
      if (it != item.properties.end()) {
        asset = QString::fromStdString(it->second);
      } else {
        it = item.properties.find("asset");
        if (it != item.properties.end()) {
          asset = QString::fromStdString(it->second);
        }
      }
      obj->setAssetPath(asset);
      if (!asset.isEmpty()) {
        obj->setPixmap(loadPixmapForAsset(asset, obj->objectType()));
      }
      m_scene->addSceneObject(obj);
    }
  } else {
    ::NovelMind::editor::SceneDocument empty;
    empty.sceneId = sceneId.toStdString();
    ::NovelMind::editor::saveSceneDocument(empty, scenePath.toStdString());
  }

  restoreEditorObjectsAfterRuntime();
  emit sceneObjectsChanged();
  m_isLoadingScene = false;
  return true;
}

bool NMSceneViewPanel::saveSceneDocument() {
  if (!m_scene || m_currentSceneId.isEmpty()) {
    return false;
  }

  const QString scenesRoot = QString::fromStdString(
      ProjectManager::instance().getFolderPath(ProjectFolder::Scenes));
  if (scenesRoot.isEmpty()) {
    return false;
  }

  ::NovelMind::editor::SceneDocument doc;
  doc.sceneId = m_currentSceneId.toStdString();

  const auto objects = m_scene->sceneObjects();
  doc.objects.reserve(static_cast<size_t>(objects.size()));
  for (const auto *obj : objects) {
    if (!obj || obj->id().startsWith("runtime_")) {
      continue;
    }

    ::NovelMind::editor::SceneDocumentObject item;
    item.id = obj->id().toStdString();
    item.name = obj->name().toStdString();
    switch (obj->objectType()) {
    case NMSceneObjectType::Background:
      item.type = "Background";
      break;
    case NMSceneObjectType::Character:
      item.type = "Character";
      break;
    case NMSceneObjectType::Effect:
      item.type = "Effect";
      break;
    default:
      item.type = "UI";
      break;
    }
    item.x = static_cast<float>(obj->pos().x());
    item.y = static_cast<float>(obj->pos().y());
    item.rotation = static_cast<float>(obj->rotation());
    item.scaleX = static_cast<float>(obj->scaleX());
    item.scaleY = static_cast<float>(obj->scaleY());
    item.alpha = static_cast<float>(obj->opacity());
    item.visible = obj->isVisible();
    item.zOrder = static_cast<int>(obj->zValue());
    item.properties.emplace("name", item.name);
    if (!obj->assetPath().isEmpty()) {
      item.properties.emplace("textureId", obj->assetPath().toStdString());
    }
    doc.objects.push_back(std::move(item));
  }

  const QString scenePath =
      QDir(scenesRoot).filePath(m_currentSceneId + ".nmscene");
  const auto result =
      ::NovelMind::editor::saveSceneDocument(doc, scenePath.toStdString());

  // Emit SceneDocumentModifiedEvent to notify other panels (Issue #223)
  if (result.isOk()) {
    events::SceneDocumentModifiedEvent event;
    event.sceneId = m_currentSceneId;
    EventBus::instance().publish(event);
    qDebug() << "[SceneView] Published SceneDocumentModifiedEvent for" << m_currentSceneId;
  }

  return result.isOk();
}

QString NMSceneViewPanel::normalizeAssetPath(const QString &assetPath) const {
  QString normalized = assetPath;
  if (!assetPath.isEmpty()) {
    QFileInfo info(assetPath);
    if (info.isAbsolute()) {
      auto &pm = ProjectManager::instance();
      if (pm.isPathInProject(assetPath.toStdString())) {
        normalized =
            QString::fromStdString(pm.toRelativePath(assetPath.toStdString()));
      }
    }
  }
  return normalized;
}

NMSceneObjectType
NMSceneViewPanel::guessObjectTypeForAsset(const QString &assetPath) const {
  const QString lower = assetPath.toLower();
  if (lower.contains("/background") || lower.contains("/bg/") ||
      lower.contains("background")) {
    return NMSceneObjectType::Background;
  }

  QString absPath = assetPath;
  QFileInfo info(assetPath);
  if (!info.isAbsolute()) {
    absPath = QString::fromStdString(
        ProjectManager::instance().toAbsolutePath(assetPath.toStdString()));
  }

  QImageReader reader(absPath);
  QSize size = reader.size();
  if (size.isValid()) {
    const qreal aspect = size.height() == 0
                             ? 1.0
                             : static_cast<qreal>(size.width()) /
                                   static_cast<qreal>(size.height());
    if (size.width() >= 1024 || aspect >= 1.4) {
      return NMSceneObjectType::Background;
    }
  }

  return NMSceneObjectType::Character;
}

QPixmap NMSceneViewPanel::loadPixmapForAsset(const QString &hint,
                                             NMSceneObjectType type) {
  if (hint.isEmpty()) {
    return QPixmap();
  }

  if (m_assetsRoot.isEmpty()) {
    m_assetsRoot = QString::fromStdString(
        ProjectManager::instance().getFolderPath(ProjectFolder::Assets));
  }

  if (m_textureCache.contains(hint)) {
    return m_textureCache.value(hint);
  }

  QString baseName = hint;
  QFileInfo hintInfo(hint);

  QStringList candidates;
  if (hintInfo.isAbsolute()) {
    candidates << hintInfo.absoluteFilePath();
  } else {
    if (ProjectManager::instance().hasOpenProject()) {
      const QString abs = QString::fromStdString(
          ProjectManager::instance().toAbsolutePath(hint.toStdString()));
      candidates << abs;
    }
    candidates << hint;
  }

  QString trimmed = hint;
  if (trimmed.startsWith("Assets/", Qt::CaseInsensitive)) {
    trimmed = trimmed.mid(QString("Assets/").size());
  }
  if (!m_assetsRoot.isEmpty()) {
    candidates << (m_assetsRoot + "/" + trimmed);
  }

  for (const auto &path : candidates) {
    if (!path.isEmpty() && QFileInfo::exists(path)) {
      QPixmap pix(path);
      if (!pix.isNull()) {
        m_textureCache.insert(hint, pix);
        return pix;
      }
    }
  }

  QStringList exts = {"", ".png", ".jpg", ".jpeg"};
  QStringList prefixes = {m_assetsRoot + "/", m_assetsRoot + "/Images/",
                          m_assetsRoot + "/images/", QString()};
  for (const auto &prefix : prefixes) {
    for (const auto &ext : exts) {
      const QString path = prefix + baseName + ext;
      if (!path.isEmpty() && QFileInfo::exists(path)) {
        QPixmap pix(path);
        if (!pix.isNull()) {
          m_textureCache.insert(hint, pix);
          return pix;
        }
      }
    }
  }

  const auto &palette = NMStyleManager::instance().palette();
  QSize sz = (type == NMSceneObjectType::Background) ? QSize(1280, 720)
                                                     : QSize(400, 600);
  QImage img(sz, QImage::Format_ARGB32_Premultiplied);
  if (img.isNull()) {
    QPixmap fallback(1, 1);
    fallback.fill(Qt::transparent);
    return fallback;
  }
  QColor fill = (type == NMSceneObjectType::Background) ? palette.bgMedium
                                                        : palette.bgLight;
  img.fill(fill);
  QPainter p(&img);
  if (!p.isActive()) {
    QPixmap fallback(1, 1);
    fallback.fill(Qt::transparent);
    return fallback;
  }
  p.setPen(palette.accentPrimary);
  p.setFont(NMStyleManager::instance().defaultFont());
  p.drawRect(QRect(QPoint(0, 0), sz - QSize(1, 1)));
  p.drawText(QRect(QPoint(0, 0), sz), Qt::AlignCenter, hint);
  p.end();
  QPixmap fallback = QPixmap::fromImage(img);
  m_textureCache.insert(hint, fallback);
  return fallback;
}

} // namespace NovelMind::editor::qt
