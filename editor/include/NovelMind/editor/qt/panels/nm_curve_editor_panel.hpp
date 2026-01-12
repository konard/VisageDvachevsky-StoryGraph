#pragma once

/**
 * @file nm_curve_editor_panel.hpp
 * @brief Curve editor for animation curves and interpolation
 *
 * Provides:
 * - Visual curve display with grid
 * - Add/delete/move points
 * - Interpolation mode selection
 * - Integration with Inspector and Timeline
 *
 * Architecture (MVC):
 * - Model: CurveData (points with unique IDs)
 * - View: NMCurveScene + QGraphicsView
 * - Controller: NMCurveEditorPanel (handles input, toolbar, signals)
 */

#include "NovelMind/editor/qt/nm_dock_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_curve_point_item.hpp"

#include <QComboBox>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QPushButton>
#include <QToolBar>
#include <QWidget>

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

class QToolBar;
class QPushButton;
class QComboBox;

namespace NovelMind::editor::qt {

/**
 * @brief Interpolation type for curve segments
 */
enum class CurveInterpolation { Linear, EaseIn, EaseOut, EaseInOut, Bezier };

/**
 * @brief A single point in the curve data model
 */
struct CurveDataPoint {
  CurvePointId id; ///< Unique stable identifier
  qreal time;      ///< Normalized time [0, 1]
  qreal value;     ///< Normalized value [0, 1]
  CurveInterpolation interpolation = CurveInterpolation::Linear;
};

/**
 * @brief Curve data model with stable point IDs
 *
 * Maintains a list of points sorted by time.
 * Points have stable unique IDs for UI binding.
 */
class CurveData {
public:
  CurveData();

  /**
   * @brief Add a new point to the curve
   * @param time Normalized time [0, 1]
   * @param value Normalized value [0, 1]
   * @param interpolation Interpolation type
   * @return The ID of the newly added point
   */
  CurvePointId addPoint(qreal time, qreal value,
                        CurveInterpolation interpolation = CurveInterpolation::Linear);

  /**
   * @brief Remove a point by ID
   * @param id Point ID to remove
   * @return true if point was found and removed
   */
  bool removePoint(CurvePointId id);

  /**
   * @brief Update point position
   * @param id Point ID
   * @param time New normalized time
   * @param value New normalized value
   * @return true if point was found and updated
   */
  bool updatePoint(CurvePointId id, qreal time, qreal value);

  /**
   * @brief Update point interpolation
   * @param id Point ID
   * @param interpolation New interpolation type
   * @return true if point was found and updated
   */
  bool updatePointInterpolation(CurvePointId id, CurveInterpolation interpolation);

  /**
   * @brief Get point by ID
   * @param id Point ID
   * @return Pointer to point or nullptr if not found
   */
  CurveDataPoint* getPoint(CurvePointId id);
  const CurveDataPoint* getPoint(CurvePointId id) const;

  /**
   * @brief Get all points (sorted by time)
   */
  const std::vector<CurveDataPoint>& points() const { return m_points; }

  /**
   * @brief Get point count
   */
  size_t pointCount() const { return m_points.size(); }

  /**
   * @brief Evaluate the curve at a given time
   * @param t Normalized time [0, 1]
   * @return Interpolated value
   */
  qreal evaluate(qreal t) const;

  /**
   * @brief Clear all points
   */
  void clear();

  /**
   * @brief Create default linear curve (two endpoints)
   */
  void createDefault();

  /**
   * @brief Sort points by time (called after modifications)
   */
  void sortByTime();

  /**
   * @brief Check if curve is valid (at least 2 points)
   */
  bool isValid() const { return m_points.size() >= 2; }

private:
  std::vector<CurveDataPoint> m_points;
  CurvePointId m_nextId = 1;
};

/**
 * @brief Custom graphics view for curve editing
 *
 * Handles:
 * - Background click to add points
 * - Mouse wheel zoom
 * - Coordinate system (Y-up)
 */
class NMCurveView : public QGraphicsView {
  Q_OBJECT

public:
  explicit NMCurveView(QGraphicsScene* scene, QWidget* parent = nullptr);

signals:
  /**
   * @brief Emitted when user clicks on empty area
   * @param time Normalized time at click position
   * @param value Normalized value at click position
   */
  void addPointRequested(qreal time, qreal value);

  /**
   * @brief Emitted when view size changes
   */
  void viewResized();

protected:
  void resizeEvent(QResizeEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
};

/**
 * @brief Curve editor panel for editing animation curves
 */
class NMCurveEditorPanel : public NMDockPanel {
  Q_OBJECT

public:
  explicit NMCurveEditorPanel(QWidget* parent = nullptr);
  ~NMCurveEditorPanel() override;

  void onInitialize() override;
  void onShutdown() override;
  void onUpdate(double deltaTime) override;

  /**
   * @brief Set the curve data to edit
   * @param curveId Optional curve identifier for external tracking
   */
  void setCurve(const QString& curveId = QString());

  /**
   * @brief Get current curve ID
   */
  const QString& curveId() const { return m_curveId; }

  /**
   * @brief Get curve data model
   */
  CurveData& curveData() { return m_curveData; }
  const CurveData& curveData() const { return m_curveData; }

signals:
  /**
   * @brief Emitted when curve data changes
   * @param curveId Curve identifier
   */
  void curveChanged(const QString& curveId);

  /**
   * @brief Emitted to request opening from Inspector
   * @param propertyName The property requesting the curve editor
   * @param curveId Curve data identifier
   */
  void openCurveEditorRequested(const QString& propertyName, const QString& curveId);

public slots:
  /**
   * @brief Add a point at the specified normalized coordinates
   */
  void addPointAt(qreal time, qreal value);

  /**
   * @brief Delete selected points
   */
  void deleteSelectedPoints();

  /**
   * @brief Select all points
   */
  void selectAllPoints();

  /**
   * @brief Clear selection
   */
  void clearSelection();

private slots:
  void onAddPointClicked();
  void onDeletePointClicked();
  void onInterpolationChanged(int index);
  void onPointPositionChanged(CurvePointId id, qreal time, qreal value);
  void onPointClicked(CurvePointId id, bool additive);
  void onPointDragFinished(CurvePointId id);
  void onAddPointRequested(qreal time, qreal value);
  void onViewResized();

private:
  void setupUI();
  void setupToolbar();

  /**
   * @brief Rebuild the visual representation of the curve
   */
  void rebuildCurveVisuals();

  /**
   * @brief Update scene rect based on view size
   */
  void updateSceneRect();

  /**
   * @brief Draw grid lines
   */
  void drawGrid();

  /**
   * @brief Draw the curve path
   */
  void drawCurvePath();

  /**
   * @brief Create/update point items
   */
  void updatePointItems();

  /**
   * @brief Convert normalized coordinates to scene coordinates
   */
  QPointF normalizedToScene(qreal time, qreal value) const;

  /**
   * @brief Convert scene coordinates to normalized coordinates
   */
  QPointF sceneToNormalized(QPointF scenePos) const;

  /**
   * @brief Get usable area rect (excluding margins)
   */
  QRectF usableRect() const;

  // UI components
  NMCurveView* m_curveView = nullptr;
  QGraphicsScene* m_curveScene = nullptr;
  QToolBar* m_toolbar = nullptr;
  QPushButton* m_addPointBtn = nullptr;
  QPushButton* m_deletePointBtn = nullptr;
  QComboBox* m_interpCombo = nullptr;

  // Data model
  CurveData m_curveData;
  QString m_curveId;

  // Visual elements
  QGraphicsPathItem* m_curvePathItem = nullptr;
  std::vector<QGraphicsLineItem*> m_gridLines;
  std::unordered_map<CurvePointId, NMCurvePointItem*> m_pointItems;

  // Selection state
  std::vector<CurvePointId> m_selectedPoints;

  // Drag tracking for undo
  std::unordered_map<CurvePointId, std::pair<qreal, qreal>> m_dragStartPositions;

  // Layout constants
  static constexpr qreal MARGIN = 40.0;
  static constexpr int GRID_DIVISIONS = 10;
};

} // namespace NovelMind::editor::qt
