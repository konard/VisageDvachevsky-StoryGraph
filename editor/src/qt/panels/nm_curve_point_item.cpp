#include "NovelMind/editor/qt/panels/nm_curve_point_item.hpp"

#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <algorithm>

namespace NovelMind::editor::qt {

NMCurvePointItem::NMCurvePointItem(CurvePointId pointId, qreal time, qreal value,
                                   const QColor& color, QGraphicsItem* parent)
    : QGraphicsObject(parent), m_pointId(pointId), m_time(time), m_value(value), m_color(color) {
  setFlag(QGraphicsItem::ItemIsMovable, false);
  setFlag(QGraphicsItem::ItemIsSelectable, false);
  setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
  setAcceptHoverEvents(true);
  setCursor(Qt::SizeAllCursor);
  setZValue(20); // Points should be on top of the curve
}

QRectF NMCurvePointItem::boundingRect() const {
  qreal radius = m_radius;
  if (m_selected || m_hovered) {
    radius += 2;
  }
  return QRectF(-radius, -radius, radius * 2, radius * 2);
}

void NMCurvePointItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                             QWidget* widget) {
  Q_UNUSED(option);
  Q_UNUSED(widget);

  painter->setRenderHint(QPainter::Antialiasing);

  qreal radius = m_radius;
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
  painter->drawEllipse(QPointF(0, 0), radius, radius);
}

void NMCurvePointItem::setTime(qreal time) {
  if (m_time != time) {
    m_time = std::clamp(time, 0.0, 1.0);
    updatePositionFromNormalized();
  }
}

void NMCurvePointItem::setValue(qreal value) {
  if (m_value != value) {
    m_value = std::clamp(value, 0.0, 1.0);
    updatePositionFromNormalized();
  }
}

void NMCurvePointItem::setPointSelected(bool selected) {
  if (m_selected != selected) {
    m_selected = selected;
    update();
  }
}

void NMCurvePointItem::setColor(const QColor& color) {
  if (m_color != color) {
    m_color = color;
    update();
  }
}

void NMCurvePointItem::updatePositionFromNormalized() {
  if (m_normalizedToScene) {
    QPointF scenePos = m_normalizedToScene(m_time, m_value);
    setPos(scenePos);
  }
}

void NMCurvePointItem::mousePressEvent(QGraphicsSceneMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    m_dragging = true;
    bool additiveSelection = event->modifiers() & Qt::ControlModifier;
    emit clicked(m_pointId, additiveSelection);
    event->accept();
  } else {
    QGraphicsObject::mousePressEvent(event);
  }
}

void NMCurvePointItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
  if (m_dragging && m_sceneToNormalized) {
    QPointF scenePos = event->scenePos();
    QPointF normalized = m_sceneToNormalized(scenePos);

    // Clamp to [0, 1] range
    qreal newTime = std::clamp(normalized.x(), 0.0, 1.0);
    qreal newValue = std::clamp(normalized.y(), 0.0, 1.0);

    if (newTime != m_time || newValue != m_value) {
      m_time = newTime;
      m_value = newValue;
      updatePositionFromNormalized();
      emit positionChanged(m_pointId, m_time, m_value);
    }

    event->accept();
  } else {
    QGraphicsObject::mouseMoveEvent(event);
  }
}

void NMCurvePointItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    if (m_dragging) {
      m_dragging = false;
      emit dragFinished(m_pointId);
    }
    event->accept();
  } else {
    QGraphicsObject::mouseReleaseEvent(event);
  }
}

void NMCurvePointItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    emit doubleClicked(m_pointId);
    event->accept();
  } else {
    QGraphicsObject::mouseDoubleClickEvent(event);
  }
}

void NMCurvePointItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event) {
  Q_UNUSED(event);
  m_hovered = true;
  update();
}

void NMCurvePointItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event) {
  Q_UNUSED(event);
  m_hovered = false;
  update();
}

} // namespace NovelMind::editor::qt
