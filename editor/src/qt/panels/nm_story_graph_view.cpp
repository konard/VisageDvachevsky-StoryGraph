#include "NovelMind/editor/qt/panels/nm_story_graph_panel.hpp"
#include "NovelMind/editor/qt/nm_icon_manager.hpp"
#include "NovelMind/editor/qt/nm_play_mode_controller.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"
#include "NovelMind/editor/qt/nm_undo_manager.hpp"
#include "NovelMind/editor/project_manager.hpp"

#include <QAction>
#include <QApplication>
#include <QFrame>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineF>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QPushButton>
#include <QScrollBar>
#include <QTextStream>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <algorithm>
#include <QSet>
#include <filesystem>


namespace NovelMind::editor::qt {

// ============================================================================
// NMStoryGraphView
// ============================================================================

NMStoryGraphView::NMStoryGraphView(QWidget *parent) : QGraphicsView(parent) {
  setRenderHint(QPainter::Antialiasing);
  setRenderHint(QPainter::SmoothPixmapTransform);
  setViewportUpdateMode(FullViewportUpdate);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setTransformationAnchor(AnchorUnderMouse);
  setResizeAnchor(AnchorViewCenter);
  setDragMode(RubberBandDrag);
}

void NMStoryGraphView::setConnectionModeEnabled(bool enabled) {
  m_connectionModeEnabled = enabled;
  if (enabled) {
    setDragMode(NoDrag);
    setCursor(Qt::CrossCursor);
  } else {
    setDragMode(RubberBandDrag);
    setCursor(Qt::ArrowCursor);
  }
}

void NMStoryGraphView::setConnectionDrawingMode(bool enabled) {
  m_isDrawingConnection = enabled;
  if (!enabled) {
    m_connectionStartNode = nullptr;
  }
  viewport()->update();
}

void NMStoryGraphView::setZoomLevel(qreal zoom) {
  zoom = qBound(0.1, zoom, 5.0);
  if (qFuzzyCompare(m_zoomLevel, zoom))
    return;

  qreal scaleFactor = zoom / m_zoomLevel;
  m_zoomLevel = zoom;

  scale(scaleFactor, scaleFactor);
  emit zoomChanged(m_zoomLevel);
}

void NMStoryGraphView::centerOnGraph() {
  if (scene() && !scene()->items().isEmpty()) {
    centerOn(scene()->itemsBoundingRect().center());
  } else {
    centerOn(0, 0);
  }
}

void NMStoryGraphView::wheelEvent(QWheelEvent *event) {
  qreal factor = 1.15;
  if (event->angleDelta().y() < 0) {
    factor = 1.0 / factor;
  }

  setZoomLevel(m_zoomLevel * factor);
  event->accept();
}

void NMStoryGraphView::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::MiddleButton) {
    m_isPanning = true;
    m_lastPanPoint = event->pos();
    setCursor(Qt::ClosedHandCursor);
    event->accept();
    return;
  }

  if (event->button() == Qt::LeftButton) {
    // Track potential drag start position
    m_dragStartPos = event->pos();
    m_possibleDrag = true;
    m_isDragging = false;

    QPointF scenePos = mapToScene(event->pos());
    auto *item = scene()->itemAt(scenePos, transform());
    if (auto *node = qgraphicsitem_cast<NMGraphNodeItem *>(item)) {
      const bool wantsConnection =
          m_connectionModeEnabled ||
          (event->modifiers() & Qt::ControlModifier) ||
          node->hitTestOutputPort(scenePos);
      if (wantsConnection) {
        m_isDrawingConnection = true;
        m_connectionStartNode = node;
        m_connectionEndPoint = scenePos;
        setCursor(Qt::CrossCursor);
        event->accept();
        return;
      }
      emit nodeClicked(node->nodeId());
    }
  }

  QGraphicsView::mousePressEvent(event);
}

void NMStoryGraphView::mouseDoubleClickEvent(QMouseEvent *event) {
  // Ignore double-click if user is dragging nodes
  if (m_isDragging) {
    event->ignore();
    return;
  }

  if (event->button() == Qt::LeftButton) {
    QPointF scenePos = mapToScene(event->pos());
    auto *item = scene()->itemAt(scenePos, transform());
    if (auto *node = qgraphicsitem_cast<NMGraphNodeItem *>(item)) {
      emit nodeDoubleClicked(node->nodeId());
      event->accept();
      return;
    }
  }

  QGraphicsView::mouseDoubleClickEvent(event);
}

void NMStoryGraphView::mouseMoveEvent(QMouseEvent *event) {
  if (m_isPanning) {
    QPoint delta = event->pos() - m_lastPanPoint;
    m_lastPanPoint = event->pos();

    horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
    verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
    event->accept();
    return;
  }

  // Update connection drawing line
  if (m_isDrawingConnection && m_connectionStartNode) {
    m_connectionEndPoint = mapToScene(event->pos());
    viewport()->update();
    event->accept();
    return;
  }

  // Track if user is dragging (moved beyond Qt's drag threshold)
  if (m_possibleDrag) {
    if ((event->pos() - m_dragStartPos).manhattanLength() >= QApplication::startDragDistance()) {
      m_isDragging = true;
      m_possibleDrag = false;
    }
  }

  QGraphicsView::mouseMoveEvent(event);
}

void NMStoryGraphView::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() == Qt::MiddleButton && m_isPanning) {
    m_isPanning = false;
    setCursor(Qt::ArrowCursor);
    event->accept();
    return;
  }

  // Finish drawing connection
  if (event->button() == Qt::LeftButton && m_isDrawingConnection &&
      m_connectionStartNode) {
    QPointF scenePos = mapToScene(event->pos());
    auto *item = scene()->itemAt(scenePos, transform());
    if (auto *endNode = qgraphicsitem_cast<NMGraphNodeItem *>(item)) {
      if (endNode != m_connectionStartNode) {
        // Emit signal to create connection
        if (m_connectionStartNode && endNode) {
          emit requestConnection(m_connectionStartNode->nodeId(),
                                 endNode->nodeId());
        }
      }
    }

    m_isDrawingConnection = false;
    m_connectionStartNode = nullptr;
    if (!m_connectionModeEnabled) {
      setCursor(Qt::ArrowCursor);
    }
    viewport()->update();
    event->accept();
    return;
  }

  // Reset drag tracking
  if (event->button() == Qt::LeftButton) {
    m_isDragging = false;
    m_possibleDrag = false;
  }

  QGraphicsView::mouseReleaseEvent(event);
}

void NMStoryGraphView::drawForeground(QPainter *painter,
                                      const QRectF & /*rect*/) {
  // Draw connection line being created
  if (m_isDrawingConnection && m_connectionStartNode) {
    const auto &palette = NMStyleManager::instance().palette();

    QPointF start = m_connectionStartNode->outputPortPosition();
    QPointF end = m_connectionEndPoint;

    // Draw bezier curve
    QPainterPath path;
    path.moveTo(start);

    qreal dx = std::abs(end.x() - start.x()) * 0.5;
    path.cubicTo(start + QPointF(dx, 0), end + QPointF(-dx, 0), end);

    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(QPen(palette.accentPrimary, 2, Qt::DashLine));
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(path);
  }
}

// ============================================================================
// NMNodePalette
// ============================================================================

NMNodePalette::NMNodePalette(QWidget *parent) : QWidget(parent) {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(4, 4, 4, 4);
  layout->setSpacing(4);

  const auto &palette = NMStyleManager::instance().palette();

  // Title
  auto *titleLabel = new QLabel(tr("Create Node"), this);
  titleLabel->setStyleSheet(
      QString("color: %1; font-weight: bold; padding: 4px;")
          .arg(palette.textPrimary.name()));
  layout->addWidget(titleLabel);

  // Separator
  auto *separator = new QFrame(this);
  separator->setFrameShape(QFrame::HLine);
  separator->setStyleSheet(
      QString("background-color: %1;").arg(palette.borderDark.name()));
  layout->addWidget(separator);

  // Node type buttons - Core nodes
  createNodeButton("Entry", "▶");
  createNodeButton("Dialogue", "💬");
  createNodeButton("Choice", "⚑");
  createNodeButton("Scene", "🎬");

  // Separator for flow control nodes
  auto *separator2 = new QFrame(this);
  separator2->setFrameShape(QFrame::HLine);
  separator2->setStyleSheet(
      QString("background-color: %1;").arg(palette.borderDark.name()));
  layout->addWidget(separator2);

  // Flow control nodes
  createNodeButton("Jump", "↗");
  createNodeButton("Label", "🏷");
  createNodeButton("Condition", "❓");
  createNodeButton("Random", "🎲");
  createNodeButton("End", "⏹");

  // Separator for advanced nodes
  auto *separator3 = new QFrame(this);
  separator3->setFrameShape(QFrame::HLine);
  separator3->setStyleSheet(
      QString("background-color: %1;").arg(palette.borderDark.name()));
  layout->addWidget(separator3);

  // Advanced nodes
  createNodeButton("Script", "⚙");
  createNodeButton("Variable", "📝");
  createNodeButton("Event", "⚡");

  layout->addStretch();

  // Style the widget
  setStyleSheet(QString("QWidget { background-color: %1; border: 1px solid %2; "
                        "border-radius: 4px; }")
                    .arg(palette.bgDark.name())
                    .arg(palette.borderDark.name()));
  setMinimumWidth(120);
  setMaximumWidth(150);
}

void NMNodePalette::createNodeButton(const QString &nodeType,
                                     const QString &icon) {
  auto *layout = qobject_cast<QVBoxLayout *>(this->layout());
  if (!layout)
    return;

  auto *button = new QPushButton(QString("%1 %2").arg(icon, nodeType), this);
  button->setMinimumHeight(32);

  const auto &palette = NMStyleManager::instance().palette();
  button->setStyleSheet(QString("QPushButton {"
                                "  background-color: %1;"
                                "  color: %2;"
                                "  border: 1px solid %3;"
                                "  border-radius: 4px;"
                                "  padding: 6px 12px;"
                                "  text-align: left;"
                                "}"
                                "QPushButton:hover {"
                                "  background-color: %4;"
                                "  border-color: %5;"
                                "}"
                                "QPushButton:pressed {"
                                "  background-color: %6;"
                                "}")
                            .arg(palette.bgMedium.name())
                            .arg(palette.textPrimary.name())
                            .arg(palette.borderDark.name())
                            .arg(palette.bgLight.name())
                            .arg(palette.accentPrimary.name())
                            .arg(palette.bgDark.name()));

  connect(button, &QPushButton::clicked, this,
          [this, nodeType]() { emit nodeTypeSelected(nodeType); });

  layout->insertWidget(layout->count() - 1, button);
}

// ============================================================================

} // namespace NovelMind::editor::qt
