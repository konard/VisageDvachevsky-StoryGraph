#include "NovelMind/editor/project_manager.hpp"
#include "NovelMind/editor/qt/nm_icon_manager.hpp"
#include "NovelMind/editor/qt/nm_play_mode_controller.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"
#include "NovelMind/editor/qt/nm_undo_manager.hpp"
#include "NovelMind/editor/qt/panels/nm_story_graph_panel.hpp"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineF>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QTextStream>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <algorithm>
#include <filesystem>

namespace NovelMind::editor::qt {

// ============================================================================
// NMStoryGraphView
// ============================================================================

NMStoryGraphView::NMStoryGraphView(QWidget *parent) : QGraphicsView(parent) {
  // Enable antialiasing for smooth rendering
  setRenderHint(QPainter::Antialiasing);
  setRenderHint(QPainter::TextAntialiasing);
  setRenderHint(QPainter::SmoothPixmapTransform);

  // Use FullViewportUpdate to prevent visual trails when dragging nodes
  // SmartViewportUpdate combined with CacheBackground can cause artifacts
  // when items are moved frequently (see issue #53)
  setViewportUpdateMode(FullViewportUpdate);

  // Disable background caching to prevent visual trails when nodes are dragged
  // CacheBackground can cause stale content to persist during frequent redraws
  setCacheMode(CacheNone);

  // Optimization flags
  setOptimizationFlag(DontSavePainterState, true);

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
      const bool wantsConnection = m_connectionModeEnabled ||
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
    if ((event->pos() - m_dragStartPos).manhattanLength() >=
        QApplication::startDragDistance()) {
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

// Issue #173: Drag-and-drop validation for StoryFlow editor
// Validates that dropped files are appropriate for the story graph

/**
 * @brief Check if a URL represents a valid droppable file for the story graph
 * @param url URL to validate
 * @return true if the file can be dropped on the story graph
 */
static bool isValidDroppableFile(const QUrl &url) {
  if (!url.isLocalFile()) {
    return false;
  }

  const QString path = url.toLocalFile();
  const QFileInfo fileInfo(path);

  if (!fileInfo.exists() || !fileInfo.isFile()) {
    return false;
  }

  // Accept NMScript files (.nms) for creating script nodes
  const QString suffix = fileInfo.suffix().toLower();
  return suffix == "nms";
}

void NMStoryGraphView::dragEnterEvent(QDragEnterEvent *event) {
  // Validate MIME data before accepting
  if (!event || !event->mimeData()) {
    QGraphicsView::dragEnterEvent(event);
    return;
  }

  const QMimeData *mimeData = event->mimeData();

  // Check for valid file URLs
  if (mimeData->hasUrls()) {
    bool hasValidFile = false;
    for (const QUrl &url : mimeData->urls()) {
      if (isValidDroppableFile(url)) {
        hasValidFile = true;
        break;
      }
    }

    if (hasValidFile) {
      event->acceptProposedAction();
      return;
    }
  }

  // Check for internal asset path drag
  if (mimeData->hasFormat("application/x-novelmind-asset")) {
    const QString assetPath =
        QString::fromUtf8(mimeData->data("application/x-novelmind-asset"));
    if (assetPath.endsWith(".nms", Qt::CaseInsensitive)) {
      event->acceptProposedAction();
      return;
    }
  }

  QGraphicsView::dragEnterEvent(event);
}

void NMStoryGraphView::dragMoveEvent(QDragMoveEvent *event) {
  // Continue to validate during drag move
  if (!event || !event->mimeData()) {
    QGraphicsView::dragMoveEvent(event);
    return;
  }

  const QMimeData *mimeData = event->mimeData();

  // Accept if we have valid files or internal assets
  if (mimeData->hasUrls()) {
    for (const QUrl &url : mimeData->urls()) {
      if (isValidDroppableFile(url)) {
        event->acceptProposedAction();
        return;
      }
    }
  }

  if (mimeData->hasFormat("application/x-novelmind-asset")) {
    const QString assetPath =
        QString::fromUtf8(mimeData->data("application/x-novelmind-asset"));
    if (assetPath.endsWith(".nms", Qt::CaseInsensitive)) {
      event->acceptProposedAction();
      return;
    }
  }

  QGraphicsView::dragMoveEvent(event);
}

void NMStoryGraphView::dropEvent(QDropEvent *event) {
  // Validate drop data before processing
  if (!event || !event->mimeData()) {
    QGraphicsView::dropEvent(event);
    return;
  }

  const QMimeData *mimeData = event->mimeData();
  const QPointF scenePos = mapToScene(event->position().toPoint());

  // Handle file URLs
  if (mimeData->hasUrls()) {
    for (const QUrl &url : mimeData->urls()) {
      if (isValidDroppableFile(url)) {
        const QString filePath = url.toLocalFile();
        emit scriptFileDropped(filePath, scenePos);
        event->acceptProposedAction();
        return;
      }
    }
  }

  // Handle internal asset drag
  if (mimeData->hasFormat("application/x-novelmind-asset")) {
    const QString assetPath =
        QString::fromUtf8(mimeData->data("application/x-novelmind-asset"));
    if (assetPath.endsWith(".nms", Qt::CaseInsensitive)) {
      // Convert to absolute path if relative
      QString fullPath = assetPath;
      if (!QFileInfo(assetPath).isAbsolute()) {
        const auto projectRoot =
            QString::fromStdString(ProjectManager::instance().getProjectRoot());
        if (!projectRoot.isEmpty()) {
          fullPath = QDir(projectRoot).absoluteFilePath(assetPath);
        }
      }
      emit scriptFileDropped(fullPath, scenePos);
      event->acceptProposedAction();
      return;
    }
  }

  QGraphicsView::dropEvent(event);
void NMStoryGraphView::hideEvent(QHideEvent *event) {
  // Issue #172 fix: Reset drag state when widget is hidden to prevent stale
  // state if closed during a drag operation. This avoids crashes from
  // accessing invalid state when the widget is shown again.
  resetDragState();
  QGraphicsView::hideEvent(event);
}

void NMStoryGraphView::resetDragState() {
  m_isPanning = false;
  m_isDrawingConnection = false;
  m_connectionStartNode = nullptr;
  m_connectionEndPoint = QPointF();
  m_possibleDrag = false;
  m_isDragging = false;
  m_lastPanPoint = QPoint();
  m_dragStartPos = QPoint();

  // Reset cursor if not in persistent connection mode
  if (!m_connectionModeEnabled) {
    setCursor(Qt::ArrowCursor);
  }
}

// ============================================================================
// NMNodePalette
// ============================================================================

NMNodePalette::NMNodePalette(QWidget *parent) : QWidget(parent) {
  // Main layout for the widget
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  const auto &palette = NMStyleManager::instance().palette();

  // Create scroll area for adaptive layout when panel height is small
  auto *scrollArea = new QScrollArea(this);
  scrollArea->setWidgetResizable(true);
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  scrollArea->setFrameShape(QFrame::NoFrame);

  // Style the scroll bar to be minimal
  scrollArea->setStyleSheet(
      QString("QScrollArea { background: transparent; border: none; }"
              "QScrollBar:vertical { width: 6px; background: %1; }"
              "QScrollBar::handle:vertical { background: %2; border-radius: "
              "3px; min-height: 20px; }"
              "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical "
              "{ height: 0; }"
              "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical "
              "{ background: none; }")
          .arg(NMStyleManager::colorToStyleString(palette.bgDarkest))
          .arg(NMStyleManager::colorToStyleString(palette.borderDefault)));

  // Create content widget inside scroll area
  auto *contentWidget = new QWidget(scrollArea);
  m_contentLayout = new QVBoxLayout(contentWidget);
  m_contentLayout->setContentsMargins(4, 4, 4, 4);
  m_contentLayout->setSpacing(4);

  // Title
  auto *titleLabel = new QLabel(tr("Create Node"), contentWidget);
  titleLabel->setStyleSheet(
      QString("color: %1; font-weight: bold; padding: 4px;")
          .arg(palette.textPrimary.name()));
  m_contentLayout->addWidget(titleLabel);

  // Separator
  auto *separator = new QFrame(contentWidget);
  separator->setFrameShape(QFrame::HLine);
  separator->setStyleSheet(
      QString("background-color: %1;").arg(palette.borderDark.name()));
  m_contentLayout->addWidget(separator);

  // Node type buttons - Core nodes
  createNodeButton("Entry", "node-start");
  createNodeButton("Dialogue", "node-dialogue");
  createNodeButton("Choice", "node-choice");
  createNodeButton("Scene", "panel-scene");

  // Separator for flow control nodes
  auto *separator2 = new QFrame(contentWidget);
  separator2->setFrameShape(QFrame::HLine);
  separator2->setStyleSheet(
      QString("background-color: %1;").arg(palette.borderDark.name()));
  m_contentLayout->addWidget(separator2);

  // Flow control nodes
  createNodeButton("Jump", "node-jump");
  createNodeButton("Label", "property-link");
  createNodeButton("Condition", "node-condition");
  createNodeButton("Random", "node-random");
  createNodeButton("End", "node-end");

  // Separator for advanced nodes
  auto *separator3 = new QFrame(contentWidget);
  separator3->setFrameShape(QFrame::HLine);
  separator3->setStyleSheet(
      QString("background-color: %1;").arg(palette.borderDark.name()));
  m_contentLayout->addWidget(separator3);

  // Advanced nodes
  createNodeButton("Script", "settings");
  createNodeButton("Variable", "node-variable");
  createNodeButton("Event", "node-event");

  m_contentLayout->addStretch();

  scrollArea->setWidget(contentWidget);
  mainLayout->addWidget(scrollArea);

  // Style the widget
  setStyleSheet(QString("QWidget { background-color: %1; border: 1px solid %2; "
                        "border-radius: 4px; }")
                    .arg(palette.bgDark.name())
                    .arg(palette.borderDark.name()));
  setMinimumWidth(120);
  setMaximumWidth(150);
}

void NMNodePalette::createNodeButton(const QString &nodeType,
                                     const QString &iconName) {
  if (!m_contentLayout)
    return;

  auto &iconMgr = NMIconManager::instance();
  auto *button = new QPushButton(nodeType, this);
  button->setIcon(iconMgr.getIcon(iconName, 16));
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

  m_contentLayout->insertWidget(m_contentLayout->count() - 1, button);
}

// ============================================================================

} // namespace NovelMind::editor::qt
