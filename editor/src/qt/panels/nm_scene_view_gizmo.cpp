#include "NovelMind/editor/qt/nm_style_manager.hpp"
#include "NovelMind/editor/qt/panels/nm_scene_view_panel.hpp"

#include <QBrush>
#include <QColor>
#include <QCursor>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsRectItem>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QLineF>
#include <QList>
#include <QPainter>
#include <QPen>
#include <QPolygonF>
#include <algorithm>
#include <cmath>

namespace NovelMind::editor::qt {

// ============================================================================
// NMGizmoHandle - Interactive handle for gizmo
// ============================================================================

class NMGizmoHandle : public QGraphicsEllipseItem {
public:
  using HandleType = NMTransformGizmo::HandleType;

  NMGizmoHandle(HandleType type, QGraphicsItem *parent = nullptr)
      : QGraphicsEllipseItem(parent), m_handleType(type) {
    setFlag(ItemIsMovable, false);
    setFlag(ItemIsSelectable, false);
    setFlag(ItemSendsGeometryChanges, true);
    setAcceptHoverEvents(true);
    setCursor(Qt::PointingHandCursor);
  }

  HandleType handleType() const { return m_handleType; }

protected:
  void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override {
    m_isHovered = true;
    update();
    QGraphicsEllipseItem::hoverEnterEvent(event);
  }

  void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override {
    m_isHovered = false;
    update();
    QGraphicsEllipseItem::hoverLeaveEvent(event);
  }

  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
             QWidget *widget) override {
    // Make the handle more visible when hovered
    if (m_isHovered) {
      QBrush originalBrush = brush();
      QColor highlightColor = originalBrush.color().lighter(150);
      painter->setBrush(highlightColor);
      painter->setPen(pen());
      painter->drawEllipse(rect());
    } else {
      QGraphicsEllipseItem::paint(painter, option, widget);
    }
  }

  void mousePressEvent(QGraphicsSceneMouseEvent *event) override {
    if (event->button() == Qt::LeftButton) {
      if (auto *gizmo = qgraphicsitem_cast<NMTransformGizmo *>(parentItem())) {
        gizmo->beginHandleDrag(m_handleType, event->scenePos());
      }
      event->accept();
      return;
    }
    QGraphicsEllipseItem::mousePressEvent(event);
  }

  void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override {
    if (auto *gizmo = qgraphicsitem_cast<NMTransformGizmo *>(parentItem())) {
      gizmo->updateHandleDrag(event->scenePos());
    }
    event->accept();
  }

  void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override {
    if (event->button() == Qt::LeftButton) {
      if (auto *gizmo = qgraphicsitem_cast<NMTransformGizmo *>(parentItem())) {
        gizmo->endHandleDrag();
      }
      event->accept();
      return;
    }
    QGraphicsEllipseItem::mouseReleaseEvent(event);
  }

private:
  HandleType m_handleType;
  bool m_isHovered = false;
};

class NMGizmoHitArea : public QGraphicsRectItem {
public:
  using HandleType = NMTransformGizmo::HandleType;

  NMGizmoHitArea(HandleType type, const QRectF &rect,
                 QGraphicsItem *parent = nullptr)
      : QGraphicsRectItem(rect, parent), m_handleType(type) {
    setFlag(ItemIsMovable, false);
    setFlag(ItemIsSelectable, false);
    setFlag(ItemSendsGeometryChanges, true);
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::LeftButton);
    setPen(Qt::NoPen);
    setBrush(Qt::NoBrush);
    setZValue(-1);
  }

  HandleType handleType() const { return m_handleType; }

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent *event) override {
    if (event->button() == Qt::LeftButton) {
      if (auto *gizmo = qgraphicsitem_cast<NMTransformGizmo *>(parentItem())) {
        gizmo->beginHandleDrag(m_handleType, event->scenePos());
      }
      event->accept();
      return;
    }
    QGraphicsRectItem::mousePressEvent(event);
  }

  void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override {
    if (auto *gizmo = qgraphicsitem_cast<NMTransformGizmo *>(parentItem())) {
      gizmo->updateHandleDrag(event->scenePos());
    }
    event->accept();
  }

  void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override {
    if (event->button() == Qt::LeftButton) {
      if (auto *gizmo = qgraphicsitem_cast<NMTransformGizmo *>(parentItem())) {
        gizmo->endHandleDrag();
      }
      event->accept();
      return;
    }
    QGraphicsRectItem::mouseReleaseEvent(event);
  }

private:
  HandleType m_handleType;
};

// ============================================================================
// NMTransformGizmo
// ============================================================================

NMTransformGizmo::NMTransformGizmo(QGraphicsItem *parent)
    : QGraphicsItemGroup(parent) {
  setFlag(ItemIgnoresTransformations, true);
  setFlag(ItemHasNoContents, false);
  setHandlesChildEvents(false); // Allow children to handle their own events
  setZValue(1000);
  createMoveGizmo();
}

NMTransformGizmo::~NMTransformGizmo() {
  // Clean up all child items to prevent memory leaks.
  // This ensures proper cleanup even if the gizmo is destroyed
  // without explicitly calling clearGizmo().
  clearGizmo();
}

void NMTransformGizmo::setMode(GizmoMode mode) {
  if (m_mode == mode)
    return;

  m_mode = mode;
  clearGizmo();

  switch (mode) {
  case GizmoMode::Move:
    createMoveGizmo();
    break;
  case GizmoMode::Rotate:
    createRotateGizmo();
    break;
  case GizmoMode::Scale:
    createScaleGizmo();
    break;
  }

  updatePosition();
}

void NMTransformGizmo::setTargetObjectId(const QString &objectId) {
  m_targetObjectId = objectId;
  updatePosition();
  setVisible(!m_targetObjectId.isEmpty());
}

void NMTransformGizmo::updatePosition() {
  if (m_targetObjectId.isEmpty()) {
    return;
  }

  // Resolve the target object from the owning scene at use-time to avoid
  // dangling pointers if the item was deleted.
  auto *scenePtr = scene();
  if (!scenePtr) {
    return;
  }

  auto *nmScene = qobject_cast<NMSceneGraphicsScene *>(scenePtr);
  if (!nmScene) {
    return;
  }

  if (auto *target = nmScene->findSceneObject(m_targetObjectId)) {
    setPos(target->sceneBoundingRect().center());
  } else {
    setVisible(false);
  }
}

NMSceneObject *NMTransformGizmo::resolveTarget() const {
  auto *scenePtr = scene();
  if (!scenePtr) {
    return nullptr;
  }
  auto *nmScene = qobject_cast<NMSceneGraphicsScene *>(scenePtr);
  if (!nmScene) {
    return nullptr;
  }
  auto *target = nmScene->findSceneObject(m_targetObjectId);
  if (target && target->isLocked()) {
    return nullptr;
  }
  return target;
}

void NMTransformGizmo::beginHandleDrag(HandleType type,
                                       const QPointF &scenePos) {
  auto *target = resolveTarget();
  if (!target) {
    return;
  }

  m_isDragging = true;
  m_activeHandle = type;
  m_dragStartScenePos = scenePos;
  m_dragStartTargetPos = target->pos();
  m_dragStartRotation = target->rotation();
  m_dragStartScaleX = target->scaleX();
  m_dragStartScaleY = target->scaleY();

  constexpr qreal kMinGizmoRadius = 40.0;
  const QPointF center = target->sceneBoundingRect().center();
  m_dragStartDistance =
      std::max(kMinGizmoRadius, QLineF(center, scenePos).length());
}

void NMTransformGizmo::updateHandleDrag(const QPointF &scenePos) {
  if (!m_isDragging) {
    return;
  }

  auto *target = resolveTarget();
  if (!target) {
    return;
  }

  const QPointF delta = scenePos - m_dragStartScenePos;

  if (m_mode == GizmoMode::Move) {
    QPointF newPos = m_dragStartTargetPos;
    switch (m_activeHandle) {
    case HandleType::XAxis:
      newPos.setX(m_dragStartTargetPos.x() + delta.x());
      break;
    case HandleType::YAxis:
      newPos.setY(m_dragStartTargetPos.y() + delta.y());
      break;
    case HandleType::XYPlane:
    default:
      newPos = m_dragStartTargetPos + delta;
      break;
    }
    target->setPos(newPos);
  } else if (m_mode == GizmoMode::Rotate) {
    const QPointF center = target->sceneBoundingRect().center();
    constexpr qreal kMinGizmoRadius = 40.0;
    auto clampRadius = [&](const QPointF &point) {
      QLineF line(center, point);
      if (line.length() < kMinGizmoRadius) {
        line.setLength(kMinGizmoRadius);
        return line.p2();
      }
      return point;
    };
    const QPointF startPoint = clampRadius(m_dragStartScenePos);
    const QPointF currentPoint = clampRadius(scenePos);
    const qreal startAngle = QLineF(center, startPoint).angle();
    const qreal currentAngle = QLineF(center, currentPoint).angle();
    const qreal deltaAngle = startAngle - currentAngle;
    target->setRotation(m_dragStartRotation + deltaAngle);
    updatePosition();
  } else if (m_mode == GizmoMode::Scale) {
    const QPointF center = target->sceneBoundingRect().center();
    constexpr qreal kMinGizmoRadius = 40.0;
    constexpr qreal kMinScale = 0.1;
    constexpr qreal kMaxScale = 10.0;
    const qreal currentDistance =
        std::max(kMinGizmoRadius, QLineF(center, scenePos).length());
    const qreal rawFactor = currentDistance / m_dragStartDistance;
    const qreal scaleFactor = std::pow(rawFactor, 0.6);
    const qreal newScaleX =
        std::clamp(m_dragStartScaleX * scaleFactor, kMinScale, kMaxScale);
    const qreal newScaleY =
        std::clamp(m_dragStartScaleY * scaleFactor, kMinScale, kMaxScale);
    target->setScaleXY(newScaleX, newScaleY);
    updatePosition();
  }
}

void NMTransformGizmo::endHandleDrag() {
  if (!m_isDragging) {
    return;
  }

  auto *target = resolveTarget();
  if (target) {
    if (auto *nmScene = qobject_cast<NMSceneGraphicsScene *>(scene())) {
      nmScene->objectTransformFinished(
          target->id(), m_dragStartTargetPos, target->pos(),
          m_dragStartRotation, target->rotation(), m_dragStartScaleX,
          target->scaleX(), m_dragStartScaleY, target->scaleY());
    }
  }

  m_isDragging = false;
}

void NMTransformGizmo::createMoveGizmo() {
  const auto &palette = NMStyleManager::instance().palette();
  qreal arrowLength = 60;
  qreal arrowHeadSize = 12;
  qreal handleSize = 14;

  // X axis (Red) - make it thicker for easier clicking
  auto *xLine = new QGraphicsLineItem(0, 0, arrowLength, 0, this);
  xLine->setPen(QPen(QColor(220, 50, 50), 5));
  xLine->setAcceptHoverEvents(true);
  xLine->setCursor(Qt::SizeHorCursor);
  addToGroup(xLine);

  auto *xHit = new NMGizmoHitArea(HandleType::XAxis,
                                  QRectF(0, -8, arrowLength, 16), this);
  xHit->setCursor(Qt::SizeHorCursor);
  addToGroup(xHit);

  auto *xHandle = new NMGizmoHandle(NMTransformGizmo::HandleType::XAxis, this);
  xHandle->setRect(arrowLength - handleSize / 2, -handleSize / 2, handleSize,
                   handleSize);
  xHandle->setBrush(QColor(220, 50, 50));
  xHandle->setPen(Qt::NoPen);
  xHandle->setCursor(Qt::SizeHorCursor);
  addToGroup(xHandle);

  QPolygonF xArrow;
  xArrow << QPointF(arrowLength, 0)
         << QPointF(arrowLength - arrowHeadSize, -arrowHeadSize / 2)
         << QPointF(arrowLength - arrowHeadSize, arrowHeadSize / 2);
  auto *xArrowHead = new QGraphicsPolygonItem(xArrow, this);
  xArrowHead->setBrush(QColor(220, 50, 50));
  xArrowHead->setPen(Qt::NoPen);
  xArrowHead->setAcceptHoverEvents(true);
  xArrowHead->setCursor(Qt::SizeHorCursor);
  addToGroup(xArrowHead);

  // Y axis (Green) - make it thicker for easier clicking
  auto *yLine = new QGraphicsLineItem(0, 0, 0, arrowLength, this);
  yLine->setPen(QPen(QColor(50, 220, 50), 5));
  yLine->setAcceptHoverEvents(true);
  yLine->setCursor(Qt::SizeVerCursor);
  addToGroup(yLine);

  auto *yHit = new NMGizmoHitArea(HandleType::YAxis,
                                  QRectF(-8, 0, 16, arrowLength), this);
  yHit->setCursor(Qt::SizeVerCursor);
  addToGroup(yHit);

  auto *yHandle = new NMGizmoHandle(NMTransformGizmo::HandleType::YAxis, this);
  yHandle->setRect(-handleSize / 2, arrowLength - handleSize / 2, handleSize,
                   handleSize);
  yHandle->setBrush(QColor(50, 220, 50));
  yHandle->setPen(Qt::NoPen);
  yHandle->setCursor(Qt::SizeVerCursor);
  addToGroup(yHandle);

  QPolygonF yArrow;
  yArrow << QPointF(0, arrowLength)
         << QPointF(-arrowHeadSize / 2, arrowLength - arrowHeadSize)
         << QPointF(arrowHeadSize / 2, arrowLength - arrowHeadSize);
  auto *yArrowHead = new QGraphicsPolygonItem(yArrow, this);
  yArrowHead->setBrush(QColor(50, 220, 50));
  yArrowHead->setPen(Qt::NoPen);
  yArrowHead->setAcceptHoverEvents(true);
  yArrowHead->setCursor(Qt::SizeVerCursor);
  addToGroup(yArrowHead);

  // Center circle - for XY plane movement
  auto *center = new QGraphicsEllipseItem(-8, -8, 16, 16, this);
  center->setBrush(palette.accentPrimary);
  center->setPen(QPen(palette.textPrimary, 2));
  center->setAcceptHoverEvents(true);
  center->setCursor(Qt::SizeAllCursor);
  addToGroup(center);

  auto *centerHandle =
      new NMGizmoHandle(NMTransformGizmo::HandleType::XYPlane, this);
  centerHandle->setRect(-10, -10, 20, 20);
  centerHandle->setBrush(palette.accentPrimary);
  centerHandle->setPen(QPen(palette.textPrimary, 2));
  centerHandle->setCursor(Qt::SizeAllCursor);
  addToGroup(centerHandle);
}

void NMTransformGizmo::createRotateGizmo() {
  const auto &palette = NMStyleManager::instance().palette();
  qreal radius = 60;

  // Outer circle
  auto *circle =
      new QGraphicsEllipseItem(-radius, -radius, radius * 2, radius * 2, this);
  circle->setPen(QPen(palette.accentPrimary, 3));
  circle->setBrush(Qt::NoBrush);
  addToGroup(circle);

  auto *rotateHit = new NMGizmoHitArea(
      HandleType::Rotation, QRectF(-radius, -radius, radius * 2, radius * 2),
      this);
  rotateHit->setCursor(Qt::CrossCursor);
  addToGroup(rotateHit);

  auto *handle =
      new NMGizmoHandle(NMTransformGizmo::HandleType::Rotation, this);
  handle->setRect(-8, -radius - 8, 16, 16);
  handle->setBrush(palette.accentPrimary);
  handle->setPen(Qt::NoPen);
  handle->setCursor(Qt::CrossCursor);
  addToGroup(handle);
}

void NMTransformGizmo::createScaleGizmo() {
  const auto &palette = NMStyleManager::instance().palette();
  qreal size = 50;

  // Bounding box
  auto *box = new QGraphicsRectItem(-size, -size, size * 2, size * 2, this);
  box->setPen(QPen(palette.accentPrimary, 2, Qt::DashLine));
  box->setBrush(Qt::NoBrush);
  addToGroup(box);

  auto *scaleHit = new NMGizmoHitArea(
      HandleType::Corner, QRectF(-size, -size, size * 2, size * 2), this);
  scaleHit->setCursor(Qt::SizeFDiagCursor);
  addToGroup(scaleHit);

  // Corner handles
  QList<QPointF> corners = {QPointF(-size, -size), QPointF(size, -size),
                            QPointF(-size, size), QPointF(size, size)};

  for (const auto &corner : corners) {
    auto *handle =
        new NMGizmoHandle(NMTransformGizmo::HandleType::Corner, this);
    handle->setRect(corner.x() - 8, corner.y() - 8, 16, 16);
    handle->setBrush(palette.accentPrimary);
    handle->setPen(Qt::NoPen);
    handle->setCursor(Qt::SizeFDiagCursor);
    addToGroup(handle);
  }
}

void NMTransformGizmo::clearGizmo() {
  // Create a copy of the child items list since we'll be modifying it
  // as we iterate. This prevents iterator invalidation and ensures
  // all items are properly cleaned up.
  auto children = childItems();

  // Remove all items from the group and delete them.
  // We must remove from group first to properly unparent the item,
  // then delete it. This ensures Qt's internal bookkeeping is correct.
  for (auto *item : children) {
    if (item) {
      removeFromGroup(item);
      // After removeFromGroup, the item's parent is set to this group's parent.
      // We need to explicitly set parent to nullptr before deletion to avoid
      // any potential double-deletion issues.
      item->setParentItem(nullptr);
      delete item;
    }
  }
}

} // namespace NovelMind::editor::qt
