#include "NovelMind/editor/qt/panels/nm_scene_view_panel.hpp"

#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QUrl>
#include <QWheelEvent>
#include <QtMath>

namespace NovelMind::editor::qt {

// ============================================================================
// NMSceneGraphicsView
// ============================================================================

NMSceneGraphicsView::NMSceneGraphicsView(QWidget *parent)
    : QGraphicsView(parent) {
  setRenderHint(QPainter::Antialiasing);
  setRenderHint(QPainter::SmoothPixmapTransform);
  setViewportUpdateMode(FullViewportUpdate);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setTransformationAnchor(AnchorUnderMouse);
  setResizeAnchor(AnchorViewCenter);
  setDragMode(NoDrag);

  // Set background
  setBackgroundBrush(Qt::NoBrush);

  // Enable mouse tracking for cursor position
  setMouseTracking(true);
  setAcceptDrops(true);
}

void NMSceneGraphicsView::setZoomLevel(qreal zoom) {
  zoom = qBound(0.1, zoom, 10.0);
  if (qFuzzyCompare(m_zoomLevel, zoom))
    return;

  qreal scaleFactor = zoom / m_zoomLevel;
  m_zoomLevel = zoom;

  scale(scaleFactor, scaleFactor);
  emit zoomChanged(m_zoomLevel);
}

void NMSceneGraphicsView::centerOnScene() { centerOn(0, 0); }

void NMSceneGraphicsView::fitToScene() {
  if (scene() && !scene()->items().isEmpty()) {
    fitInView(scene()->itemsBoundingRect(), Qt::KeepAspectRatio);
    m_zoomLevel = transform().m11();
    emit zoomChanged(m_zoomLevel);
  }
}

void NMSceneGraphicsView::wheelEvent(QWheelEvent *event) {
  // Zoom with mouse wheel
  qreal factor = 1.15;
  if (event->angleDelta().y() < 0) {
    factor = 1.0 / factor;
  }

  setZoomLevel(m_zoomLevel * factor);
  event->accept();
}

void NMSceneGraphicsView::updateCursor(const QCursor &newCursor) {
  // Only set cursor if it's actually different to avoid redundant calls
  if (m_currentCursor.shape() != newCursor.shape()) {
    m_currentCursor = newCursor;
    setCursor(newCursor);
  }
}

void NMSceneGraphicsView::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::MiddleButton) {
    // Start panning
    m_isPanning = true;
    m_lastPanPoint = event->pos();
    updateCursor(Qt::ClosedHandCursor);
    event->accept();
    return;
  }

  QGraphicsView::mousePressEvent(event);
}

void NMSceneGraphicsView::mouseMoveEvent(QMouseEvent *event) {
  // Emit cursor position in scene coordinates
  QPointF scenePos = mapToScene(event->pos());
  emit cursorPositionChanged(scenePos);

  if (m_isPanning) {
    QPoint delta = event->pos() - m_lastPanPoint;
    m_lastPanPoint = event->pos();

    horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
    verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
    event->accept();
    return;
  }

  QGraphicsView::mouseMoveEvent(event);
}

void NMSceneGraphicsView::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() == Qt::MiddleButton && m_isPanning) {
    m_isPanning = false;
    updateCursor(Qt::ArrowCursor);
    event->accept();
    return;
  }

  QGraphicsView::mouseReleaseEvent(event);
}

void NMSceneGraphicsView::dragEnterEvent(QDragEnterEvent *event) {
  if (event->mimeData() && event->mimeData()->hasUrls()) {
    event->acceptProposedAction();
    emit dragActiveChanged(true);
    return;
  }
  QGraphicsView::dragEnterEvent(event);
}

void NMSceneGraphicsView::dragMoveEvent(QDragMoveEvent *event) {
  if (event->mimeData() && event->mimeData()->hasUrls()) {
    event->acceptProposedAction();
    return;
  }
  QGraphicsView::dragMoveEvent(event);
}

void NMSceneGraphicsView::dropEvent(QDropEvent *event) {
  if (event->mimeData() && event->mimeData()->hasUrls()) {
    QStringList paths;
    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl &url : urls) {
      if (url.isLocalFile()) {
        paths.append(url.toLocalFile());
      }
    }
    if (!paths.isEmpty()) {
      const QPointF scenePos = mapToScene(event->position().toPoint());
      emit assetsDropped(paths, scenePos);
      event->acceptProposedAction();
      emit dragActiveChanged(false);
      return;
    }
  }
  emit dragActiveChanged(false);
  QGraphicsView::dropEvent(event);
}

void NMSceneGraphicsView::dragLeaveEvent(QDragLeaveEvent *event) {
  emit dragActiveChanged(false);
  QGraphicsView::dragLeaveEvent(event);
}

void NMSceneGraphicsView::contextMenuEvent(QContextMenuEvent *event) {
  if (!event) {
    return;
  }
  emit contextMenuRequested(event->globalPos(), mapToScene(event->pos()));
  event->accept();
}

} // namespace NovelMind::editor::qt
