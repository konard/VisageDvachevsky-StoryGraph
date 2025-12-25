#include "NovelMind/editor/qt/panels/nm_story_graph_minimap.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"
#include "NovelMind/editor/qt/panels/nm_story_graph_panel.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>

namespace NovelMind::editor::qt {

NMStoryGraphMinimap::NMStoryGraphMinimap(QWidget *parent)
    : QGraphicsView(parent) {
  setupView();

  // Setup update timer for deferred updates
  m_updateTimer = new QTimer(this);
  m_updateTimer->setSingleShot(true);
  m_updateTimer->setInterval(UPDATE_DELAY_MS);
  connect(m_updateTimer, &QTimer::timeout, this,
          &NMStoryGraphMinimap::performDeferredUpdate);
}

NMStoryGraphMinimap::~NMStoryGraphMinimap() = default;

void NMStoryGraphMinimap::setupView() {
  // Configure view for optimal minimap display
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setInteractive(true);
  setDragMode(QGraphicsView::NoDrag);
  setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
  setRenderHints(QPainter::Antialiasing);
  setTransformationAnchor(QGraphicsView::AnchorViewCenter);
  setResizeAnchor(QGraphicsView::AnchorViewCenter);

  // Set minimum size
  setMinimumSize(150, 150);
  setMaximumSize(300, 300);
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

void NMStoryGraphMinimap::setMainView(NMStoryGraphView *mainView) {
  // Disconnect old view - only disconnect the specific connections we made
  if (m_mainView) {
    disconnect(m_mainView, &QGraphicsView::rubberBandChanged, this,
               &NMStoryGraphMinimap::onMainViewTransformed);
    if (m_mainView->horizontalScrollBar()) {
      disconnect(m_mainView->horizontalScrollBar(), &QScrollBar::valueChanged,
                 this, &NMStoryGraphMinimap::onMainViewTransformed);
    }
    if (m_mainView->verticalScrollBar()) {
      disconnect(m_mainView->verticalScrollBar(), &QScrollBar::valueChanged,
                 this, &NMStoryGraphMinimap::onMainViewTransformed);
    }
  }

  m_mainView = mainView;

  // Connect to main view signals
  if (m_mainView) {
    connect(m_mainView, &QGraphicsView::rubberBandChanged, this,
            &NMStoryGraphMinimap::onMainViewTransformed);
    connect(m_mainView->horizontalScrollBar(), &QScrollBar::valueChanged, this,
            &NMStoryGraphMinimap::onMainViewTransformed);
    connect(m_mainView->verticalScrollBar(), &QScrollBar::valueChanged, this,
            &NMStoryGraphMinimap::onMainViewTransformed);

    // Initial update
    updateMinimap();
  }
}

void NMStoryGraphMinimap::setGraphScene(NMStoryGraphScene *scene) {
  // Disconnect old scene - only disconnect the specific connections we made
  if (m_graphScene) {
    disconnect(m_graphScene, &NMStoryGraphScene::nodeAdded, this,
               &NMStoryGraphMinimap::onSceneChanged);
    disconnect(m_graphScene, &NMStoryGraphScene::nodeDeleted, this,
               &NMStoryGraphMinimap::onSceneChanged);
    disconnect(m_graphScene, &NMStoryGraphScene::connectionAdded, this,
               &NMStoryGraphMinimap::onSceneChanged);
    disconnect(m_graphScene, &NMStoryGraphScene::connectionDeleted, this,
               &NMStoryGraphMinimap::onSceneChanged);
    disconnect(m_graphScene, &NMStoryGraphScene::nodesMoved, this,
               &NMStoryGraphMinimap::onSceneChanged);
  }

  m_graphScene = scene;
  setScene(scene);

  // Connect to scene signals
  if (m_graphScene) {
    connect(m_graphScene, &NMStoryGraphScene::nodeAdded, this,
            &NMStoryGraphMinimap::onSceneChanged);
    connect(m_graphScene, &NMStoryGraphScene::nodeDeleted, this,
            &NMStoryGraphMinimap::onSceneChanged);
    connect(m_graphScene, &NMStoryGraphScene::connectionAdded, this,
            &NMStoryGraphMinimap::onSceneChanged);
    connect(m_graphScene, &NMStoryGraphScene::connectionDeleted, this,
            &NMStoryGraphMinimap::onSceneChanged);
    connect(m_graphScene, &NMStoryGraphScene::nodesMoved, this,
            &NMStoryGraphMinimap::onSceneChanged);

    // Initial update
    updateMinimap();
  }
}

void NMStoryGraphMinimap::updateMinimap() {
  if (!m_graphScene) {
    return;
  }

  fitGraphInView();
  updateViewportRect();
}

void NMStoryGraphMinimap::updateViewportRect() {
  // Trigger redraw to show updated viewport rectangle
  viewport()->update();
}

void NMStoryGraphMinimap::onMainViewTransformed() {
  // Defer update to avoid too frequent redraws
  if (!m_updateTimer->isActive()) {
    m_updateTimer->start();
  }
}

void NMStoryGraphMinimap::onSceneChanged() {
  // Defer update
  if (!m_updateTimer->isActive()) {
    m_updateTimer->start();
  }
}

void NMStoryGraphMinimap::performDeferredUpdate() { updateMinimap(); }

void NMStoryGraphMinimap::fitGraphInView() {
  if (!m_graphScene || m_graphScene->nodes().isEmpty()) {
    return;
  }

  // Calculate bounding rect of all nodes
  QRectF graphBounds;
  for (auto *node : m_graphScene->nodes()) {
    if (graphBounds.isNull()) {
      graphBounds = node->sceneBoundingRect();
    } else {
      graphBounds = graphBounds.united(node->sceneBoundingRect());
    }
  }

  if (!graphBounds.isNull()) {
    // Add some padding
    graphBounds.adjust(-50, -50, 50, 50);
    fitInView(graphBounds, Qt::KeepAspectRatio);
  }
}

QRectF NMStoryGraphMinimap::getViewportRectInScene() const {
  if (!m_mainView) {
    return QRectF();
  }

  // Get the visible rect in the main view's scene coordinates
  QPointF topLeft = m_mainView->mapToScene(0, 0);
  QPointF bottomRight = m_mainView->mapToScene(
      m_mainView->viewport()->width(), m_mainView->viewport()->height());

  return QRectF(topLeft, bottomRight);
}

void NMStoryGraphMinimap::navigateToScenePos(const QPointF &scenePos) {
  if (m_mainView) {
    m_mainView->centerOn(scenePos);
    emit navigationRequested(scenePos);
  }
}

void NMStoryGraphMinimap::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    QPointF scenePos = mapToScene(event->pos());
    QRectF viewportRect = getViewportRectInScene();

    // Check if clicking inside viewport rect to start dragging
    if (viewportRect.contains(scenePos)) {
      m_isDraggingViewport = true;
      m_lastMousePos = scenePos;
    } else {
      // Click outside viewport - center on clicked position
      navigateToScenePos(scenePos);
    }

    event->accept();
    return;
  }

  QGraphicsView::mousePressEvent(event);
}

void NMStoryGraphMinimap::mouseMoveEvent(QMouseEvent *event) {
  if (m_isDraggingViewport && (event->buttons() & Qt::LeftButton)) {
    QPointF scenePos = mapToScene(event->pos());
    QPointF delta = scenePos - m_lastMousePos;

    if (m_mainView) {
      // Move the main view's center by the delta
      QPointF currentCenter =
          m_mainView->mapToScene(m_mainView->viewport()->rect().center());
      m_mainView->centerOn(currentCenter + delta);
    }

    m_lastMousePos = scenePos;
    event->accept();
    return;
  }

  QGraphicsView::mouseMoveEvent(event);
}

void NMStoryGraphMinimap::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    m_isDraggingViewport = false;
    event->accept();
    return;
  }

  QGraphicsView::mouseReleaseEvent(event);
}

void NMStoryGraphMinimap::drawForeground(QPainter *painter,
                                         const QRectF &rect) {
  Q_UNUSED(rect);

  if (!m_mainView || !m_graphScene) {
    return;
  }

  const auto &palette = NMStyleManager::instance().palette();

  // Draw viewport rectangle
  QRectF viewportRect = getViewportRectInScene();
  if (!viewportRect.isNull()) {
    // Fill with semi-transparent color
    QColor fillColor = palette.accentPrimary;
    fillColor.setAlpha(30);
    painter->fillRect(viewportRect, fillColor);

    // Draw border
    QPen borderPen(palette.accentPrimary, VIEWPORT_BORDER_WIDTH);
    borderPen.setCosmetic(true); // Width in device pixels, not scene units
    painter->setPen(borderPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(viewportRect);
  }
}

void NMStoryGraphMinimap::resizeEvent(QResizeEvent *event) {
  QGraphicsView::resizeEvent(event);
  // Refit the graph when minimap is resized
  fitGraphInView();
}

} // namespace NovelMind::editor::qt
