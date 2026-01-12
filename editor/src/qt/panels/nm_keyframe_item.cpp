#include "NovelMind/editor/qt/panels/nm_keyframe_item.hpp"

#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QStyleOptionGraphicsItem>
#include <QToolTip>

namespace NovelMind::editor::qt {

NMKeyframeItem::NMKeyframeItem(int trackIndex, int frame, const QColor& color,
                               QGraphicsItem* parent)
    : QGraphicsObject(parent), m_color(color) {
  m_id.trackIndex = trackIndex;
  m_id.frame = frame;

  setFlag(QGraphicsItem::ItemIsMovable, false);
  setFlag(QGraphicsItem::ItemIsSelectable, false);
  setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
  setAcceptHoverEvents(true);
  setCursor(Qt::PointingHandCursor);
  setZValue(10);
}

QRectF NMKeyframeItem::boundingRect() const {
  qreal radius = KEYFRAME_RADIUS;
  if (m_selected || m_hovered)
    radius += 2;
  return QRectF(-radius, -radius, radius * 2, radius * 2);
}

void NMKeyframeItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                           QWidget* widget) {
  Q_UNUSED(option);
  Q_UNUSED(widget);

  painter->setRenderHint(QPainter::Antialiasing);

  qreal radius = KEYFRAME_RADIUS;
  QColor fillColor = m_color;
  QColor borderColor = m_color.lighter(150);

  if (m_selected) {
    radius += 2;
    borderColor = QColor("#FFD700"); // Gold for selection
    painter->setPen(QPen(borderColor, 3));
  } else if (m_hovered) {
    radius += 1;
    fillColor = m_color.lighter(120);
    painter->setPen(QPen(borderColor, 2));
  } else {
    painter->setPen(QPen(borderColor, 2));
  }

  painter->setBrush(QBrush(fillColor));

  // Draw shape based on easing type for visual distinction
  // 0 = Linear (circle), 1-4 = Ease In/Out (diamond), 15 = Custom (square)
  if (m_easingType == 0) {
    // Linear: simple circle
    painter->drawEllipse(QPointF(0, 0), radius, radius);
  } else if (m_easingType >= 1 && m_easingType <= 14) {
    // Ease types: diamond shape
    QPolygonF diamond;
    diamond << QPointF(0, -radius) << QPointF(radius, 0) << QPointF(0, radius)
            << QPointF(-radius, 0);
    painter->drawPolygon(diamond);
  } else if (m_easingType == 15) {
    // Custom bezier: rounded square
    qreal half = radius * 0.85;
    painter->drawRoundedRect(QRectF(-half, -half, half * 2, half * 2), radius * 0.3, radius * 0.3);

    // Draw small bezier curve icon inside
    if (radius >= 4) {
      painter->setPen(QPen(borderColor.darker(120), 1));
      QPainterPath curvePath;
      qreal iconSize = half * 0.7;
      curvePath.moveTo(-iconSize, iconSize * 0.5);
      curvePath.cubicTo(-iconSize * 0.3, -iconSize * 0.5, iconSize * 0.3, iconSize * 0.5, iconSize,
                        -iconSize * 0.5);
      painter->drawPath(curvePath);
    }
  } else {
    // Default to circle
    painter->drawEllipse(QPointF(0, 0), radius, radius);
  }
}

void NMKeyframeItem::setEasingType(int easing) {
  if (m_easingType != easing) {
    m_easingType = easing;
    update();
  }
}

void NMKeyframeItem::setSelected(bool selected) {
  if (m_selected != selected) {
    m_selected = selected;
    update();
  }
}

void NMKeyframeItem::setFrame(int frame) {
  if (m_id.frame != frame) {
    m_id.frame = frame;
    if (m_frameToX) {
      setPos(m_frameToX(frame), pos().y());
    }
  }
}

void NMKeyframeItem::mousePressEvent(QGraphicsSceneMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    m_dragging = true;
    m_dragStartPos = event->scenePos();
    m_dragStartFrame = m_id.frame;

    bool additiveSelection = event->modifiers() & Qt::ControlModifier;
    bool rangeSelection = event->modifiers() & Qt::ShiftModifier;
    emit clicked(additiveSelection, rangeSelection, m_id);

    // Emit drag started signal
    emit dragStarted(m_id);

    event->accept();
  } else {
    QGraphicsObject::mousePressEvent(event);
  }
}

void NMKeyframeItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
  if (m_dragging && m_xToFrame && m_frameToX) {
    int newFrame = m_xToFrame(static_cast<int>(event->scenePos().x()));

    // Apply snapping
    if (m_snapToGrid && m_gridSize > 0) {
      newFrame = snapToGrid(newFrame);
    }

    // Clamp to non-negative frames
    if (newFrame < 0)
      newFrame = 0;

    if (newFrame != m_id.frame) {
      int oldFrame = m_id.frame;
      m_id.frame = newFrame;
      setPos(m_frameToX(newFrame), pos().y());
      emit moved(oldFrame, newFrame, m_id.trackIndex);
    }

    // Show tooltip with current frame number
    QToolTip::showText(event->screenPos(), QString("Frame: %1").arg(newFrame), nullptr);

    event->accept();
  } else {
    QGraphicsObject::mouseMoveEvent(event);
  }
}

void NMKeyframeItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    if (m_dragging) {
      m_dragging = false;

      // Hide tooltip on release
      QToolTip::hideText();

      // Emit drag ended signal
      emit dragEnded();
    }
    event->accept();
  } else {
    QGraphicsObject::mouseReleaseEvent(event);
  }
}

void NMKeyframeItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    emit doubleClicked(m_id.trackIndex, m_id.frame);
    event->accept();
  } else {
    QGraphicsObject::mouseDoubleClickEvent(event);
  }
}

void NMKeyframeItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event) {
  Q_UNUSED(event);
  m_hovered = true;
  update();
}

void NMKeyframeItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event) {
  Q_UNUSED(event);
  m_hovered = false;
  update();
}

int NMKeyframeItem::snapToGrid(int frame) const {
  if (m_gridSize <= 0)
    return frame;

  int remainder = frame % m_gridSize;
  if (remainder < m_gridSize / 2) {
    return frame - remainder;
  } else {
    return frame + (m_gridSize - remainder);
  }
}

} // namespace NovelMind::editor::qt
