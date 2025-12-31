#include "NovelMind/editor/qt/nm_style_manager.hpp"
#include "NovelMind/editor/qt/panels/nm_scene_view_panel.hpp"

#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineF>
#include <QPainter>
#include <QPen>
#include <QVBoxLayout>
#include <QVector>
#include <QtMath>
#include <cmath>
#include <memory>

namespace NovelMind::editor::qt {

// ============================================================================
// NMSceneGraphicsScene
// ============================================================================

NMSceneGraphicsScene::NMSceneGraphicsScene(QObject *parent)
    : QGraphicsScene(parent) {
  // Set a large scene rect for scrolling
  setSceneRect(-5000, -5000, 10000, 10000);

  // Create gizmo with scene ownership via addItem()
  // MEM-2 fix: Use unique_ptr for exception-safe allocation, then transfer
  // ownership to scene
  auto gizmo = std::make_unique<NMTransformGizmo>();
  m_gizmo = gizmo.get();
  m_gizmo->setVisible(false);
  addItem(gizmo.release()); // Scene takes ownership
}

NMSceneGraphicsScene::~NMSceneGraphicsScene() {
  // Scene objects are owned by the scene and will be deleted automatically
}

void NMSceneGraphicsScene::setGridVisible(bool visible) {
  m_gridVisible = visible;
  invalidate(sceneRect(), BackgroundLayer);
}

void NMSceneGraphicsScene::setGridSize(qreal size) {
  m_gridSize = size;
  if (m_gridVisible) {
    invalidate(sceneRect(), BackgroundLayer);
  }
}

void NMSceneGraphicsScene::setStageGuidesVisible(bool visible) {
  m_stageGuidesVisible = visible;
  invalidate(sceneRect(), BackgroundLayer);
}

void NMSceneGraphicsScene::setSafeFrameVisible(bool visible) {
  m_safeFrameVisible = visible;
  invalidate(sceneRect(), BackgroundLayer);
}

void NMSceneGraphicsScene::setBaselineVisible(bool visible) {
  m_baselineVisible = visible;
  invalidate(sceneRect(), BackgroundLayer);
}

void NMSceneGraphicsScene::setSnapToGrid(bool enabled) {
  m_snapToGrid = enabled;
}

QRectF NMSceneGraphicsScene::stageRect() const {
  const QPointF topLeft(-m_stageSize.width() / 2.0,
                        -m_stageSize.height() / 2.0);
  return QRectF(topLeft, m_stageSize);
}

void NMSceneGraphicsScene::addSceneObject(NMSceneObject *object) {
  if (!object)
    return;

  // Avoid duplicate IDs to keep selection and lookup stable
  if (findSceneObject(object->id())) {
    removeSceneObject(object->id());
  }

  m_sceneObjects.append(object);
  addItem(object);

  // Note: Position tracking should be implemented through
  // NMSceneObject::itemChange() or via a custom signal/slot mechanism in
  // NMSceneObject if it inherits from QObject
}

void NMSceneGraphicsScene::removeSceneObject(const QString &objectId) {
  if (objectId == m_selectedObjectId) {
    clearSelection();
  }
  if (objectId == m_draggingObjectId) {
    resetDragTracking();
  }

  for (int i = 0; i < m_sceneObjects.size(); ++i) {
    if (m_sceneObjects[i]->id() == objectId) {
      NMSceneObject *obj = m_sceneObjects.takeAt(i);
      removeItem(obj);
      delete obj;
      break;
    }
  }
}

NMSceneObject *
NMSceneGraphicsScene::findSceneObject(const QString &objectId) const {
  for (auto *obj : m_sceneObjects) {
    if (obj->id() == objectId)
      return obj;
  }
  return nullptr;
}

NMSceneObject *NMSceneGraphicsScene::selectedObject() const {
  if (m_selectedObjectId.isEmpty()) {
    return nullptr;
  }
  return findSceneObject(m_selectedObjectId);
}

QPointF NMSceneGraphicsScene::getObjectPosition(const QString &objectId) const {
  if (auto *obj = findSceneObject(objectId)) {
    return obj->pos();
  }
  return {};
}

bool NMSceneGraphicsScene::setObjectPosition(const QString &objectId,
                                             const QPointF &pos) {
  if (auto *obj = findSceneObject(objectId)) {
    obj->setPos(pos);
    handleItemPositionChange(objectId, pos);
    return true;
  }
  return false;
}

bool NMSceneGraphicsScene::setObjectRotation(const QString &objectId,
                                             qreal degrees) {
  if (auto *obj = findSceneObject(objectId)) {
    obj->setRotation(degrees);
    updateGizmo();
    return true;
  }
  return false;
}

qreal NMSceneGraphicsScene::getObjectRotation(const QString &objectId) const {
  if (auto *obj = findSceneObject(objectId)) {
    return obj->rotation();
  }
  return 0.0;
}

bool NMSceneGraphicsScene::setObjectScale(const QString &objectId, qreal scaleX,
                                          qreal scaleY) {
  if (auto *obj = findSceneObject(objectId)) {
    obj->setScaleXY(scaleX, scaleY);
    updateGizmo();
    return true;
  }
  return false;
}

bool NMSceneGraphicsScene::setObjectOpacity(const QString &objectId,
                                            qreal opacity) {
  if (auto *obj = findSceneObject(objectId)) {
    obj->setOpacity(opacity);
    return true;
  }
  return false;
}

bool NMSceneGraphicsScene::setObjectVisible(const QString &objectId,
                                            bool visible) {
  if (auto *obj = findSceneObject(objectId)) {
    obj->setVisible(visible);
    return true;
  }
  return false;
}

bool NMSceneGraphicsScene::setObjectLocked(const QString &objectId,
                                           bool locked) {
  if (auto *obj = findSceneObject(objectId)) {
    obj->setLocked(locked);
    return true;
  }
  return false;
}

bool NMSceneGraphicsScene::setObjectZOrder(const QString &objectId,
                                           qreal zValue) {
  if (auto *obj = findSceneObject(objectId)) {
    obj->setZValue(zValue);
    return true;
  }
  return false;
}

bool NMSceneGraphicsScene::reparentObject(const QString &objectId,
                                          const QString &newParentId) {
  auto *obj = findSceneObject(objectId);
  if (!obj) {
    return false;
  }

  // Get old parent
  const QString oldParentId = obj->parentObjectId();

  // Check for cycle: prevent setting parent to self or any descendant
  if (!newParentId.isEmpty()) {
    if (newParentId == objectId) {
      // Cannot parent to self
      return false;
    }

    // Check if newParent is a descendant of obj
    QString checkId = newParentId;
    while (!checkId.isEmpty()) {
      if (checkId == objectId) {
        // Would create a cycle
        return false;
      }
      auto *checkObj = findSceneObject(checkId);
      if (!checkObj) {
        break;
      }
      checkId = checkObj->parentObjectId();
    }
  }

  // Remove from old parent's child list
  if (!oldParentId.isEmpty()) {
    if (auto *oldParent = findSceneObject(oldParentId)) {
      oldParent->removeChildObjectId(objectId);
    }
  }

  // Set new parent
  obj->setParentObjectId(newParentId);

  // Add to new parent's child list
  if (!newParentId.isEmpty()) {
    if (auto *newParent = findSceneObject(newParentId)) {
      newParent->addChildObjectId(objectId);
    }
  }

  return true;
}

QPointF NMSceneGraphicsScene::getObjectScale(const QString &objectId) const {
  if (auto *obj = findSceneObject(objectId)) {
    return {obj->scaleX(), obj->scaleY()};
  }
  return {1.0, 1.0};
}

bool NMSceneGraphicsScene::isObjectLocked(const QString &objectId) const {
  if (auto *obj = findSceneObject(objectId)) {
    return obj->isLocked();
  }
  return false;
}

void NMSceneGraphicsScene::selectObject(const QString &objectId) {
  if (objectId.isEmpty()) {
    clearSelection();
    return;
  }
  if (objectId == m_selectedObjectId) {
    if (auto *obj = findSceneObject(m_selectedObjectId)) {
      if (obj->isSelected()) {
        return;
      }
    }
  }
  // Clear previous selection
  if (!m_selectedObjectId.isEmpty()) {
    if (auto *prev = findSceneObject(m_selectedObjectId)) {
      prev->setSelected(false);
    }
  }

  // Select new object
  m_selectedObjectId = objectId;
  if (auto *obj = findSceneObject(m_selectedObjectId)) {
    obj->setSelected(true);
    updateGizmo();
    emit objectSelected(objectId);
  }
}

void NMSceneGraphicsScene::clearSelection() {
  if (m_selectedObjectId.isEmpty()) {
    return;
  }
  if (!m_selectedObjectId.isEmpty()) {
    if (auto *obj = findSceneObject(m_selectedObjectId)) {
      obj->setSelected(false);
    }
    resetDragTracking();
    m_selectedObjectId.clear();
    updateGizmo();
    emit objectSelected(QString());
  }
}

void NMSceneGraphicsScene::setGizmoMode(NMTransformGizmo::GizmoMode mode) {
  m_gizmo->setMode(mode);
}

void NMSceneGraphicsScene::drawBackground(QPainter *painter,
                                          const QRectF &rect) {
  const auto &palette = NMStyleManager::instance().palette();

  // Fill background
  painter->fillRect(rect, palette.bgDarkest);

  if (m_stageGuidesVisible || m_safeFrameVisible || m_baselineVisible) {
    const QRectF stage = stageRect();
    if (m_stageGuidesVisible) {
      QPen stagePen(palette.gridMajor);
      stagePen.setWidth(1);
      stagePen.setStyle(Qt::SolidLine);
      painter->setPen(stagePen);
      painter->setBrush(Qt::NoBrush);
      painter->drawRect(stage);

      QPen centerPen(palette.borderDark);
      centerPen.setStyle(Qt::DashLine);
      painter->setPen(centerPen);
      painter->drawLine(QLineF(stage.center().x(), stage.top(),
                               stage.center().x(), stage.bottom()));
      painter->drawLine(QLineF(stage.left(), stage.center().y(), stage.right(),
                               stage.center().y()));
    }

    if (m_safeFrameVisible) {
      QRectF safe = stage.adjusted(80, 60, -80, -60);
      QPen safePen(palette.gridLine);
      safePen.setStyle(Qt::DashLine);
      painter->setPen(safePen);
      painter->setBrush(Qt::NoBrush);
      painter->drawRect(safe);
    }

    if (m_baselineVisible) {
      const qreal baseline = stage.bottom() - 120.0;
      QPen basePen(palette.accentPrimary);
      basePen.setStyle(Qt::DotLine);
      painter->setPen(basePen);
      painter->drawLine(
          QLineF(stage.left(), baseline, stage.right(), baseline));
    }
  }

  if (!m_gridVisible) {
    return;
  }

  // Draw grid
  painter->setPen(QPen(palette.gridLine, 1));

  // Calculate grid bounds
  qreal left = rect.left() - std::fmod(rect.left(), m_gridSize);
  qreal top = rect.top() - std::fmod(rect.top(), m_gridSize);

  // Draw minor grid lines
  QVector<QLineF> lines;
  for (qreal x = left; x < rect.right(); x += m_gridSize) {
    lines.append(QLineF(x, rect.top(), x, rect.bottom()));
  }
  for (qreal y = top; y < rect.bottom(); y += m_gridSize) {
    lines.append(QLineF(rect.left(), y, rect.right(), y));
  }
  painter->drawLines(lines);

  // Draw major grid lines (every 8 minor lines)
  painter->setPen(QPen(palette.gridMajor, 1));
  qreal majorSize = m_gridSize * 8;
  left = rect.left() - std::fmod(rect.left(), majorSize);
  top = rect.top() - std::fmod(rect.top(), majorSize);

  lines.clear();
  for (qreal x = left; x < rect.right(); x += majorSize) {
    lines.append(QLineF(x, rect.top(), x, rect.bottom()));
  }
  for (qreal y = top; y < rect.bottom(); y += majorSize) {
    lines.append(QLineF(rect.left(), y, rect.right(), y));
  }
  painter->drawLines(lines);

  // Draw origin axes
  painter->setPen(QPen(palette.accentPrimary, 2));
  if (rect.left() <= 0 && rect.right() >= 0) {
    painter->drawLine(QLineF(0, rect.top(), 0, rect.bottom()));
  }
  if (rect.top() <= 0 && rect.bottom() >= 0) {
    painter->drawLine(QLineF(rect.left(), 0, rect.right(), 0));
  }
}

void NMSceneGraphicsScene::mousePressEvent(QGraphicsSceneMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    // Check if we clicked on a scene object
    QGraphicsItem *item = itemAt(event->scenePos(), QTransform());

    // Skip gizmo items
    while (item && item->parentItem()) {
      if (item->parentItem() == m_gizmo) {
        // Clicked on gizmo, let default handling work
        QGraphicsScene::mousePressEvent(event);
        return;
      }
      item = item->parentItem();
    }

    // Check if it's a scene object
    auto *sceneObj = dynamic_cast<NMSceneObject *>(item);
    if (sceneObj) {
      selectObject(sceneObj->id());
      if (!sceneObj->isLocked()) {
        m_draggingObjectId = sceneObj->id();
        m_dragStartPos = sceneObj->pos();
        m_isDraggingObject = true;
      } else {
        resetDragTracking();
      }
    } else {
      // Clicked on empty space, clear selection
      resetDragTracking();
      clearSelection();
    }
  }

  QGraphicsScene::mousePressEvent(event);
}

void NMSceneGraphicsScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
  if (event->button() == Qt::LeftButton && m_isDraggingObject) {
    if (auto *obj = findSceneObject(m_draggingObjectId)) {
      const QPointF newPos = obj->pos();
      if (!qFuzzyCompare(m_dragStartPos.x(), newPos.x()) ||
          !qFuzzyCompare(m_dragStartPos.y(), newPos.y())) {
        emit objectMoveFinished(m_draggingObjectId, m_dragStartPos, newPos);
      }
    }
    resetDragTracking();
  }

  QGraphicsScene::mouseReleaseEvent(event);
}

void NMSceneGraphicsScene::keyPressEvent(QKeyEvent *event) {
  if ((event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) &&
      !m_selectedObjectId.isEmpty()) {
    emit deleteRequested(m_selectedObjectId);
    event->accept();
    return;
  }

  QGraphicsScene::keyPressEvent(event);
}

void NMSceneGraphicsScene::updateGizmo() {
  m_gizmo->setTargetObjectId(m_selectedObjectId);
}

void NMSceneGraphicsScene::handleItemPositionChange(const QString &objectId,
                                                    const QPointF &newPos) {
  if (m_selectedObjectId == objectId) {
    updateGizmo();
  }
  emit objectPositionChanged(objectId, newPos);
}

void NMSceneGraphicsScene::resetDragTracking() {
  m_isDraggingObject = false;
  m_draggingObjectId.clear();
  m_dragStartPos = QPointF();
}

// ============================================================================
// NMSceneInfoOverlay
// ============================================================================

NMSceneInfoOverlay::NMSceneInfoOverlay(QWidget *parent) : QWidget(parent) {
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setAttribute(Qt::WA_NoSystemBackground);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(10, 10, 10, 10);
  layout->setSpacing(5);

  const auto &palette = NMStyleManager::instance().palette();

  // Scene label
  m_sceneLabel = new QLabel(this);
  m_sceneLabel->setStyleSheet(
      QString("QLabel {"
              "  background-color: rgba(20, 20, 24, 210);"
              "  color: %1;"
              "  padding: 6px 12px;"
              "  border-radius: 4px;"
              "  border: 1px solid %2;"
              "  font-weight: 600;"
              "  letter-spacing: 0.4px;"
              "}")
          .arg(palette.textPrimary.name())
          .arg(palette.accentPrimary.name()));
  m_sceneLabel->setVisible(false);
  layout->addWidget(m_sceneLabel);

  // Cursor position label
  m_cursorLabel = new QLabel(this);
  m_cursorLabel->setStyleSheet(
      QString("QLabel {"
              "  background-color: rgba(45, 45, 48, 200);"
              "  color: %1;"
              "  padding: 5px 10px;"
              "  border-radius: 3px;"
              "  font-family: 'Consolas', 'Monaco', monospace;"
              "  font-size: 11px;"
              "}")
          .arg(palette.textPrimary.name()));
  layout->addWidget(m_cursorLabel);

  // Selected object label
  m_objectLabel = new QLabel(this);
  m_objectLabel->setStyleSheet(
      QString("QLabel {"
              "  background-color: rgba(0, 120, 212, 200);"
              "  color: %1;"
              "  padding: 5px 10px;"
              "  border-radius: 3px;"
              "  font-family: 'Consolas', 'Monaco', monospace;"
              "  font-size: 11px;"
              "}")
          .arg(palette.textPrimary.name()));
  m_objectLabel->setVisible(false);
  layout->addWidget(m_objectLabel);

  layout->addStretch();

  updateDisplay();
}

void NMSceneInfoOverlay::setCursorPosition(const QPointF &pos) {
  m_cursorPos = pos;
  updateDisplay();
}

void NMSceneInfoOverlay::setSceneInfo(const QString &sceneId) {
  m_sceneId = sceneId;
  updateDisplay();
}

void NMSceneInfoOverlay::setPlayModeActive(bool active) {
  m_playModeActive = active;
  updateDisplay();
}

void NMSceneInfoOverlay::setSelectedObjectInfo(const QString &name,
                                               const QPointF &pos) {
  m_objectName = name;
  m_objectPos = pos;
  m_hasSelection = true;
  updateDisplay();
}

void NMSceneInfoOverlay::clearSelectedObjectInfo() {
  m_hasSelection = false;
  updateDisplay();
}

void NMSceneInfoOverlay::updateDisplay() {
  if (!m_sceneId.isEmpty()) {
    m_sceneLabel->setText(QString("Node: %1").arg(m_sceneId));
    m_sceneLabel->setVisible(true);
  } else {
    m_sceneLabel->setVisible(false);
  }

  m_cursorLabel->setVisible(!m_playModeActive);
  if (!m_playModeActive) {
    m_cursorLabel->setText(QString("Cursor: X: %1  Y: %2")
                               .arg(m_cursorPos.x(), 7, 'f', 1)
                               .arg(m_cursorPos.y(), 7, 'f', 1));
  }

  if (m_hasSelection) {
    m_objectLabel->setText(QString("%1 - X: %2  Y: %3")
                               .arg(m_objectName)
                               .arg(m_objectPos.x(), 7, 'f', 1)
                               .arg(m_objectPos.y(), 7, 'f', 1));
    m_objectLabel->setVisible(!m_playModeActive);
  } else {
    m_objectLabel->setVisible(false);
  }
}

} // namespace NovelMind::editor::qt
