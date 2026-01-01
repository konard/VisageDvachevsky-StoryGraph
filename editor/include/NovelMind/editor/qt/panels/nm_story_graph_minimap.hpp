#pragma once

/**
 * @file nm_story_graph_minimap.hpp
 * @brief Minimap widget for story graph navigation
 *
 * Provides:
 * - Simplified overview of the entire graph
 * - Viewport rectangle showing current view
 * - Click-to-center navigation
 * - Efficient rendering for large graphs
 */

#include <QGraphicsView>
#include <QTimer>

namespace NovelMind::editor::qt {

class NMStoryGraphScene;
class NMStoryGraphView;

/**
 * @brief Minimap widget for story graph overview and navigation
 *
 * The minimap shows a simplified version of the entire graph with:
 * - Small rectangles for nodes (no text rendering)
 * - Simplified connection lines
 * - Viewport indicator (current visible area)
 * - Click-to-center navigation
 *
 * Performance optimizations:
 * - Minimal detail rendering (no text, simple shapes)
 * - Deferred updates using timer
 * - Caching of transformation matrices
 */
class NMStoryGraphMinimap : public QGraphicsView {
  Q_OBJECT

public:
  explicit NMStoryGraphMinimap(QWidget *parent = nullptr);
  ~NMStoryGraphMinimap() override;

  /**
   * @brief Set the main graph view to track
   */
  void setMainView(NMStoryGraphView *mainView);

  /**
   * @brief Set the scene to display
   */
  void setGraphScene(NMStoryGraphScene *scene);

  /**
   * @brief Update the minimap (called when graph changes)
   */
  void updateMinimap();

  /**
   * @brief Update viewport rectangle
   */
  void updateViewportRect();

signals:
  /**
   * @brief Emitted when user clicks on minimap to navigate
   * @param scenePos Position in scene coordinates to center on
   */
  void navigationRequested(const QPointF &scenePos);

protected:
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void drawForeground(QPainter *painter, const QRectF &rect) override;
  void resizeEvent(QResizeEvent *event) override;
  void hideEvent(QHideEvent *event) override;

private slots:
  void onMainViewTransformed();
  void onSceneChanged();
  void performDeferredUpdate();

private:
  void setupView();
  void fitGraphInView();
  QRectF getViewportRectInScene() const;
  void navigateToScenePos(const QPointF &scenePos);

  NMStoryGraphView *m_mainView = nullptr;
  NMStoryGraphScene *m_graphScene = nullptr;
  QTimer *m_updateTimer = nullptr;
  bool m_isDraggingViewport = false;
  QPointF m_lastMousePos;

  // Visual settings
  static constexpr qreal MINIMAP_NODE_SIZE = 8.0;
  static constexpr qreal MINIMAP_CONNECTION_WIDTH = 1.0;
  static constexpr qreal VIEWPORT_BORDER_WIDTH = 2.0;
  static constexpr int UPDATE_DELAY_MS = 100;
};

} // namespace NovelMind::editor::qt
