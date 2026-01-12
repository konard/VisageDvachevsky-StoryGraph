#pragma once

/**
 * @file nm_curve_point_item.hpp
 * @brief Custom QGraphicsItem for interactive curve point editing
 *
 * Provides:
 * - Drag with clamping to normalized [0,1] range
 * - Selection support
 * - Stable point ID binding to data model
 * - Visual feedback for interaction states
 */

#include <QColor>
#include <QGraphicsObject>
#include <cstdint>
#include <functional>

namespace NovelMind::editor::qt {

/**
 * @brief Unique identifier for a curve point
 */
using CurvePointId = uint64_t;

/**
 * @brief Custom graphics item for curve point editing
 *
 * Inherits from QGraphicsObject to support signals/slots.
 * Handles mouse interaction for dragging and selection.
 * Uses stable point IDs rather than indices for data binding.
 */
class NMCurvePointItem : public QGraphicsObject {
  Q_OBJECT

public:
  /**
   * @brief Construct curve point item
   * @param pointId Unique identifier for this point
   * @param time Normalized time [0,1]
   * @param value Normalized value [0,1]
   * @param color Color of the point
   * @param parent Parent graphics item
   */
  explicit NMCurvePointItem(CurvePointId pointId, qreal time, qreal value,
                            const QColor& color = QColor("#0078d4"),
                            QGraphicsItem* parent = nullptr);

  /**
   * @brief Get the bounding rectangle
   */
  QRectF boundingRect() const override;

  /**
   * @brief Paint the point
   */
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
             QWidget* widget = nullptr) override;

  /**
   * @brief Get point ID
   */
  CurvePointId pointId() const { return m_pointId; }

  /**
   * @brief Get/set normalized time [0,1]
   */
  qreal time() const { return m_time; }
  void setTime(qreal time);

  /**
   * @brief Get/set normalized value [0,1]
   */
  qreal value() const { return m_value; }
  void setValue(qreal value);

  /**
   * @brief Get/set selection state
   */
  bool isPointSelected() const { return m_selected; }
  void setPointSelected(bool selected);

  /**
   * @brief Set coordinate conversion functions
   * @param normalizedToScene Converts (time, value) to scene coordinates
   * @param sceneToNormalized Converts scene coordinates to (time, value)
   */
  void setCoordinateConverter(std::function<QPointF(qreal, qreal)> normalizedToScene,
                              std::function<QPointF(QPointF)> sceneToNormalized) {
    m_normalizedToScene = normalizedToScene;
    m_sceneToNormalized = sceneToNormalized;
  }

  /**
   * @brief Update position from normalized coordinates
   */
  void updatePositionFromNormalized();

  /**
   * @brief Set point color
   */
  void setColor(const QColor& color);

  /**
   * @brief Set point radius
   */
  void setRadius(qreal radius) { m_radius = radius; }

signals:
  /**
   * @brief Emitted when point position changes
   * @param pointId Point identifier
   * @param time New normalized time
   * @param value New normalized value
   */
  void positionChanged(CurvePointId pointId, qreal time, qreal value);

  /**
   * @brief Emitted when point is clicked
   * @param pointId Point identifier
   * @param additiveSelection True if Ctrl is held
   */
  void clicked(CurvePointId pointId, bool additiveSelection);

  /**
   * @brief Emitted when point drag ends
   * @param pointId Point identifier
   */
  void dragFinished(CurvePointId pointId);

  /**
   * @brief Emitted when point is double-clicked
   * @param pointId Point identifier
   */
  void doubleClicked(CurvePointId pointId);

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

private:
  CurvePointId m_pointId;
  qreal m_time;
  qreal m_value;
  QColor m_color;
  qreal m_radius = 6.0;

  bool m_selected = false;
  bool m_hovered = false;
  bool m_dragging = false;

  // Coordinate conversion
  std::function<QPointF(qreal, qreal)> m_normalizedToScene;
  std::function<QPointF(QPointF)> m_sceneToNormalized;
};

} // namespace NovelMind::editor::qt
