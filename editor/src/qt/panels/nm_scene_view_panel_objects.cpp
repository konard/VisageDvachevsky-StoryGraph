#include "NovelMind/editor/qt/nm_dialogs.hpp"
#include "NovelMind/editor/qt/nm_undo_manager.hpp"
#include "NovelMind/editor/qt/panels/nm_scene_view_panel.hpp"

#include <QDateTime>
#include <QFileInfo>
#include <QLineEdit>

namespace NovelMind::editor::qt {

bool NMSceneViewPanel::createObject(const QString &id, NMSceneObjectType type,
                                    const QPointF &pos, qreal scale) {
  if (!m_scene || id.isEmpty()) {
    return false;
  }

  if (m_scene->findSceneObject(id)) {
    return false;
  }

  auto *obj = new NMSceneObject(id, type);
  obj->setName(id);
  obj->setPos(pos);
  obj->setUniformScale(scale);
  m_scene->addSceneObject(obj);
  m_scene->selectObject(id);
  emit sceneObjectsChanged();
  return true;
}

bool NMSceneViewPanel::deleteObject(const QString &id) {
  if (!m_scene || id.isEmpty()) {
    return false;
  }
  if (!m_scene->findSceneObject(id)) {
    return false;
  }
  m_scene->removeSceneObject(id);
  emit sceneObjectsChanged();
  return true;
}

bool NMSceneViewPanel::moveObject(const QString &id, const QPointF &pos) {
  if (!m_scene || id.isEmpty()) {
    return false;
  }
  return m_scene->setObjectPosition(id, pos);
}

bool NMSceneViewPanel::rotateObject(const QString &id, qreal rotation) {
  if (!m_scene || id.isEmpty()) {
    return false;
  }
  return m_scene->setObjectRotation(id, rotation);
}

bool NMSceneViewPanel::scaleObject(const QString &id, qreal scaleX,
                                   qreal scaleY) {
  if (!m_scene || id.isEmpty()) {
    return false;
  }
  return m_scene->setObjectScale(id, scaleX, scaleY);
}

bool NMSceneViewPanel::setObjectOpacity(const QString &id, qreal opacity) {
  if (!m_scene || id.isEmpty()) {
    return false;
  }
  const bool ok = m_scene->setObjectOpacity(id, opacity);
  if (ok) {
    emit sceneObjectsChanged();
  }
  return ok;
}

bool NMSceneViewPanel::setObjectVisible(const QString &id, bool visible) {
  if (!m_scene || id.isEmpty()) {
    return false;
  }
  const bool ok = m_scene->setObjectVisible(id, visible);
  if (ok) {
    emit sceneObjectsChanged();
  }
  return ok;
}

bool NMSceneViewPanel::setObjectLocked(const QString &id, bool locked) {
  if (!m_scene || id.isEmpty()) {
    return false;
  }
  const bool ok = m_scene->setObjectLocked(id, locked);
  if (ok) {
    emit sceneObjectsChanged();
  }
  return ok;
}

bool NMSceneViewPanel::setObjectZOrder(const QString &id, qreal zValue) {
  if (!m_scene || id.isEmpty()) {
    return false;
  }
  const bool ok = m_scene->setObjectZOrder(id, zValue);
  if (ok) {
    emit sceneObjectsChanged();
  }
  return ok;
}

bool NMSceneViewPanel::reparentObject(const QString &id,
                                      const QString &newParentId) {
  if (!m_scene || id.isEmpty()) {
    return false;
  }
  const bool ok = m_scene->reparentObject(id, newParentId);
  if (ok) {
    emit sceneObjectsChanged();
  }
  return ok;
}

bool NMSceneViewPanel::applyObjectTransform(const QString &id,
                                            const QPointF &pos, qreal rotation,
                                            qreal scaleX, qreal scaleY) {
  if (!m_scene || id.isEmpty()) {
    return false;
  }
  bool ok = m_scene->setObjectPosition(id, pos);
  ok = m_scene->setObjectRotation(id, rotation) && ok;
  ok = m_scene->setObjectScale(id, scaleX, scaleY) && ok;
  return ok;
}

bool NMSceneViewPanel::renameObject(const QString &id, const QString &name) {
  if (!m_scene || id.isEmpty()) {
    return false;
  }

  NMSceneObject *obj = m_scene->findSceneObject(id);
  if (!obj) {
    return false;
  }

  obj->setName(name);
  if (obj->isObjectSelected() && m_infoOverlay) {
    m_infoOverlay->setSelectedObjectInfo(name, obj->pos());
  }
  emit objectNameChanged(id, name);
  emit sceneObjectsChanged();
  return true;
}

bool NMSceneViewPanel::duplicateObject(const QString &id) {
  if (!canEditScene() || !m_scene || id.isEmpty()) {
    return false;
  }

  auto *obj = m_scene->findSceneObject(id);
  if (!obj) {
    return false;
  }

  SceneObjectSnapshot snapshot = snapshotFromObject(obj);
  snapshot.id = generateObjectId(snapshot.type);
  snapshot.name =
      snapshot.name.isEmpty() ? tr("Copy") : tr("%1 Copy").arg(snapshot.name);
  snapshot.position = obj->pos() + QPointF(32.0, 32.0);

  NMUndoManager::instance().pushCommand(new AddObjectCommand(this, snapshot));
  return true;
}

void NMSceneViewPanel::selectObjectById(const QString &id) {
  if (!m_scene) {
    return;
  }
  if (id.isEmpty()) {
    m_scene->clearSelection();
    return;
  }
  if (m_scene->findSceneObject(id)) {
    m_scene->selectObject(id);
  }
}

NMSceneObject *NMSceneViewPanel::findObjectById(const QString &id) const {
  if (!m_scene || id.isEmpty()) {
    return nullptr;
  }
  return m_scene->findSceneObject(id);
}

bool NMSceneViewPanel::setObjectAsset(const QString &id,
                                      const QString &assetPath) {
  if (!m_scene || id.isEmpty()) {
    return false;
  }
  auto *obj = m_scene->findSceneObject(id);
  if (!obj) {
    return false;
  }

  QString normalized = normalizeAssetPath(assetPath);

  obj->setAssetPath(normalized);
  obj->setPixmap(loadPixmapForAsset(normalized, obj->objectType()));
  emit sceneObjectsChanged();
  return true;
}

bool NMSceneViewPanel::addObjectFromAsset(const QString &assetPath,
                                          const QPointF &scenePos) {
  const NMSceneObjectType type = guessObjectTypeForAsset(assetPath);
  return addObjectFromAsset(assetPath, scenePos, type);
}

bool NMSceneViewPanel::addObjectFromAsset(const QString &assetPath,
                                          const QPointF &scenePos,
                                          NMSceneObjectType type) {
  if (!m_scene || assetPath.isEmpty()) {
    return false;
  }
  const QString ext = QFileInfo(assetPath).suffix().toLower();
  const bool isImage = (ext == "png" || ext == "jpg" || ext == "jpeg" ||
                        ext == "bmp" || ext == "gif");
  if (!isImage) {
    return false;
  }

  const QString normalized = normalizeAssetPath(assetPath);

  SceneObjectSnapshot snapshot;
  const qint64 stamp = QDateTime::currentMSecsSinceEpoch();
  QString prefix;
  switch (type) {
  case NMSceneObjectType::Background:
    prefix = "background";
    break;
  case NMSceneObjectType::Character:
    prefix = "character";
    break;
  case NMSceneObjectType::Effect:
    prefix = "effect";
    break;
  default:
    prefix = "ui";
    break;
  }
  snapshot.id = QString("%1_%2").arg(prefix).arg(stamp);
  snapshot.name = QFileInfo(normalized).completeBaseName();
  snapshot.type = type;
  snapshot.position = scenePos;
  snapshot.scaleX = 1.0;
  snapshot.scaleY = 1.0;
  snapshot.rotation = 0.0;
  snapshot.opacity = 1.0;
  snapshot.visible = true;
  snapshot.zValue = 0.0;
  snapshot.assetPath = normalized;

  NMUndoManager::instance().pushCommand(new AddObjectCommand(this, snapshot));
  return true;
}

bool NMSceneViewPanel::canEditScene() const {
  return m_scene && !m_playModeActive && !m_runtimePreviewActive;
}

SceneObjectSnapshot
NMSceneViewPanel::snapshotFromObject(const NMSceneObject *obj) const {
  SceneObjectSnapshot snapshot;
  if (!obj) {
    return snapshot;
  }
  snapshot.id = obj->id();
  snapshot.name = obj->name();
  snapshot.type = obj->objectType();
  snapshot.position = obj->pos();
  snapshot.rotation = obj->rotation();
  snapshot.scaleX = obj->scaleX();
  snapshot.scaleY = obj->scaleY();
  snapshot.opacity = obj->opacity();
  snapshot.visible = obj->isVisible();
  snapshot.zValue = obj->zValue();
  snapshot.assetPath = obj->assetPath();
  return snapshot;
}

QString NMSceneViewPanel::generateObjectId(NMSceneObjectType type) const {
  const qint64 stamp = QDateTime::currentMSecsSinceEpoch();
  QString prefix;
  switch (type) {
  case NMSceneObjectType::Background:
    prefix = "background";
    break;
  case NMSceneObjectType::Character:
    prefix = "character";
    break;
  case NMSceneObjectType::Effect:
    prefix = "effect";
    break;
  default:
    prefix = "ui";
    break;
  }
  return QString("%1_%2").arg(prefix).arg(stamp);
}

void NMSceneViewPanel::copySelectedObject() {
  if (!canEditScene()) {
    return;
  }
  auto *obj = m_scene ? m_scene->selectedObject() : nullptr;
  if (!obj) {
    return;
  }
  m_sceneClipboard = snapshotFromObject(obj);
  m_sceneClipboardValid = !m_sceneClipboard.id.isEmpty();
}

bool NMSceneViewPanel::pasteClipboardObject() {
  if (!canEditScene() || !m_scene || !m_sceneClipboardValid) {
    return false;
  }

  SceneObjectSnapshot snapshot = m_sceneClipboard;
  snapshot.id = generateObjectId(snapshot.type);
  snapshot.name =
      snapshot.name.isEmpty() ? tr("Copy") : tr("%1 Copy").arg(snapshot.name);

  QPointF basePos = snapshot.position;
  if (m_view && m_view->viewport()) {
    basePos = m_view->mapToScene(m_view->viewport()->rect().center());
  }
  const QPointF offset(32.0, 32.0);
  snapshot.position = basePos + offset;

  NMUndoManager::instance().pushCommand(new AddObjectCommand(this, snapshot));
  return true;
}

bool NMSceneViewPanel::duplicateSelectedObject() {
  if (!canEditScene() || !m_scene) {
    return false;
  }

  auto *obj = m_scene->selectedObject();
  if (!obj) {
    return false;
  }

  SceneObjectSnapshot snapshot = snapshotFromObject(obj);
  snapshot.id = generateObjectId(snapshot.type);
  snapshot.name =
      snapshot.name.isEmpty() ? tr("Copy") : tr("%1 Copy").arg(snapshot.name);
  snapshot.position = obj->pos() + QPointF(32.0, 32.0);

  NMUndoManager::instance().pushCommand(new AddObjectCommand(this, snapshot));
  return true;
}

void NMSceneViewPanel::deleteSelectedObject() {
  if (!canEditScene() || !m_scene) {
    return;
  }
  if (auto *obj = m_scene->selectedObject()) {
    onDeleteRequested(obj->id());
  }
}

void NMSceneViewPanel::renameSelectedObject() {
  if (!canEditScene() || !m_scene) {
    return;
  }

  auto *obj = m_scene->selectedObject();
  if (!obj) {
    return;
  }

  bool ok = false;
  const QString current = obj->name().isEmpty() ? obj->id() : obj->name();
  const QString name =
      NMInputDialog::getText(this, tr("Rename Object"), tr("Name:"),
                             QLineEdit::Normal, current, &ok)
          .trimmed();
  if (ok && !name.isEmpty() && name != obj->name()) {
    renameObject(obj->id(), name);
  }
}

void NMSceneViewPanel::toggleSelectedVisibility() {
  if (!canEditScene() || !m_scene) {
    return;
  }
  if (auto *obj = m_scene->selectedObject()) {
    setObjectVisible(obj->id(), !obj->isVisible());
  }
}

void NMSceneViewPanel::toggleSelectedLocked() {
  if (!canEditScene() || !m_scene) {
    return;
  }
  if (auto *obj = m_scene->selectedObject()) {
    setObjectLocked(obj->id(), !obj->isLocked());
  }
}

} // namespace NovelMind::editor::qt
