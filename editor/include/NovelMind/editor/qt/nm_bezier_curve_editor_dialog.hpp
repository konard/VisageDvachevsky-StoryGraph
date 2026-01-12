#pragma once

/**
 * @file nm_bezier_curve_editor_dialog.hpp
 * @brief Dialog for editing bezier curve easing on keyframes
 *
 * Provides:
 * - Visual bezier curve editing with draggable control points
 * - Preset curves (Linear, Ease In, Ease Out, Ease In-Out)
 * - Real-time preview
 * - Coordinate spinboxes for precise control
 */

#include "NovelMind/editor/qt/panels/nm_timeline_panel.hpp"

#include <QDialog>
#include <QDoubleSpinBox>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QPointF>
#include <QPushButton>
#include <QWidget>

namespace NovelMind::editor::qt {

/**
 * @brief Graphics view for bezier curve editing with custom event handling
 */
class NMBezierCurveView : public QGraphicsView {
  Q_OBJECT

public:
  explicit NMBezierCurveView(QGraphicsScene* scene, QWidget* parent = nullptr);

protected:
  void resizeEvent(QResizeEvent* event) override;

signals:
  void viewResized();
};

/**
 * @brief Draggable control point handle for bezier curves
 */
class NMBezierControlPointItem : public QObject, public QGraphicsEllipseItem {
  Q_OBJECT

public:
  enum PointType { StartPoint, EndPoint, ControlPoint1, ControlPoint2 };

  NMBezierControlPointItem(PointType type, qreal x, qreal y, QGraphicsItem* parent = nullptr);

  [[nodiscard]] PointType pointType() const { return m_type; }
  void setDraggable(bool draggable) { m_draggable = draggable; }

signals:
  void positionChanged(PointType type, QPointF newPos);
  void dragFinished();

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

private:
  PointType m_type;
  bool m_draggable = true;
  bool m_dragging = false;
  QColor m_normalColor;
  QColor m_hoverColor;
};

/**
 * @brief Result structure for bezier curve dialog
 */
struct BezierCurveResult {
  EasingType easingType = EasingType::Linear;
  float handleInX = 0.0f;
  float handleInY = 0.0f;
  float handleOutX = 0.0f;
  float handleOutY = 0.0f;
};

/**
 * @brief Dialog for editing bezier curve easing
 *
 * Provides a visual editor for customizing keyframe easing curves.
 * Shows a 300x300 canvas with:
 * - The bezier curve visualization
 * - Two draggable control points
 * - Preset buttons for common curves
 * - Coordinate spinboxes for precise editing
 */
class NMBezierCurveEditorDialog : public QDialog {
  Q_OBJECT

public:
  /**
   * @brief Construct dialog with initial keyframe easing values
   * @param keyframe Keyframe to edit (used to initialize control points)
   * @param parent Parent widget
   */
  explicit NMBezierCurveEditorDialog(const Keyframe* keyframe, QWidget* parent = nullptr);

  /**
   * @brief Get the result after dialog is accepted
   */
  [[nodiscard]] BezierCurveResult result() const { return m_result; }

  /**
   * @brief Get current control point 1
   */
  [[nodiscard]] QPointF controlPoint1() const;

  /**
   * @brief Get current control point 2
   */
  [[nodiscard]] QPointF controlPoint2() const;

  /**
   * @brief Static convenience method to show dialog and get result
   * @param parent Parent widget
   * @param keyframe Keyframe to edit
   * @param outResult Output result if accepted
   * @return true if user accepted, false if cancelled
   */
  static bool getEasing(QWidget* parent, const Keyframe* keyframe, BezierCurveResult& outResult);

private slots:
  void onControlPointMoved(NMBezierControlPointItem::PointType type, QPointF newPos);
  void onPresetLinear();
  void onPresetEaseIn();
  void onPresetEaseOut();
  void onPresetEaseInOut();
  void onSpinBoxChanged();
  void onViewResized();

private:
  void buildUi();
  void setupPresetButtons(QWidget* container);
  void setupSpinBoxes(QWidget* container);
  void updateCurveVisualization();
  void updateSpinBoxesFromControlPoints();
  void setControlPoints(const QPointF& cp1, const QPointF& cp2);

  // Coordinate conversion
  QPointF normalizedToScene(qreal x, qreal y) const;
  QPointF sceneToNormalized(const QPointF& scenePos) const;
  QRectF usableRect() const;

  // UI components
  NMBezierCurveView* m_curveView = nullptr;
  QGraphicsScene* m_curveScene = nullptr;

  // Control point items
  NMBezierControlPointItem* m_cp1Item = nullptr;
  NMBezierControlPointItem* m_cp2Item = nullptr;
  QGraphicsLineItem* m_handleLine1 = nullptr;
  QGraphicsLineItem* m_handleLine2 = nullptr;
  QGraphicsPathItem* m_curvePathItem = nullptr;

  // Spinboxes for precise control
  QDoubleSpinBox* m_cp1XSpin = nullptr;
  QDoubleSpinBox* m_cp1YSpin = nullptr;
  QDoubleSpinBox* m_cp2XSpin = nullptr;
  QDoubleSpinBox* m_cp2YSpin = nullptr;

  // Preset buttons
  QPushButton* m_linearBtn = nullptr;
  QPushButton* m_easeInBtn = nullptr;
  QPushButton* m_easeOutBtn = nullptr;
  QPushButton* m_easeInOutBtn = nullptr;

  // Control points (normalized 0-1)
  QPointF m_controlPoint1{0.42f, 0.0f}; // Default ease-in-out
  QPointF m_controlPoint2{0.58f, 1.0f};

  // Result
  BezierCurveResult m_result;

  // Layout constants
  static constexpr int CANVAS_SIZE = 300;
  static constexpr qreal MARGIN = 20.0;
  static constexpr qreal POINT_RADIUS = 6.0;
};

} // namespace NovelMind::editor::qt
