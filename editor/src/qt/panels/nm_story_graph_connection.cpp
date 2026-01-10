#include "NovelMind/editor/project_manager.hpp"
#include "NovelMind/editor/qt/nm_icon_manager.hpp"
#include "NovelMind/editor/qt/nm_play_mode_controller.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"
#include "NovelMind/editor/qt/nm_undo_manager.hpp"
#include "NovelMind/editor/qt/panels/nm_story_graph_panel.hpp"

#include <QAction>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineF>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
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
// NMGraphConnectionItem
// ============================================================================

NMGraphConnectionItem::NMGraphConnectionItem(NMGraphNodeItem* startNode, NMGraphNodeItem* endNode)
    : QGraphicsItem(), m_startNode(startNode), m_endNode(endNode) {
  setZValue(-1);                                  // Draw behind nodes
  setFlag(QGraphicsItem::ItemIsSelectable, true); // Issue #325: Allow selection for deletion
  // Don't call updatePath() in constructor - let the scene call it after adding
}

void NMGraphConnectionItem::updatePath() {
  if (!m_startNode || !m_endNode)
    return;

  // Safety check - ensure nodes are still in a scene
  if (!m_startNode->scene() || !m_endNode->scene())
    return;

  // Extra safety: ensure this connection is also in a scene
  if (!scene())
    return;

  QPointF start = m_startNode->outputPortPosition();
  QPointF end = m_endNode->inputPortPosition();

  // Notify Qt that geometry will change before modifying the path
  prepareGeometryChange();

  // Create bezier curve
  m_path = QPainterPath();
  m_path.moveTo(start);

  qreal dx = std::abs(end.x() - start.x()) * 0.5;
  m_path.cubicTo(start + QPointF(dx, 0), end + QPointF(-dx, 0), end);

  // Request redraw
  update();
}

QRectF NMGraphConnectionItem::boundingRect() const {
  // Expand bounding rect to include label area
  QRectF rect = m_path.boundingRect().adjusted(-5, -5, 5, 5);
  if (!m_label.isEmpty()) {
    // Add extra space for label text (centered on path)
    rect = rect.adjusted(-50, -20, 50, 20);
  }
  return rect;
}

void NMGraphConnectionItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* /*option*/,
                                  QWidget* /*widget*/) {
  // Save painter state to prevent state leakage to other items
  painter->save();

  const auto& palette = NMStyleManager::instance().palette();

  painter->setRenderHint(QPainter::Antialiasing);

  // Issue #345: Determine connection type for visual differentiation
  enum class ConnectionType { SameScene, SceneTransition, CrossScene };
  ConnectionType connType = ConnectionType::SameScene;

  if (m_startNode && m_endNode) {
    const bool startIsScene = m_startNode->isSceneNode();
    const bool endIsScene = m_endNode->isSceneNode();

    if (startIsScene || endIsScene) {
      // Connection involves a scene node
      connType = ConnectionType::SceneTransition;
    }
    if (startIsScene && endIsScene) {
      // Both ends are scene nodes - cross-scene connection
      connType = ConnectionType::CrossScene;
    }
  }

  // Issue #389: Determine connection color based on branch type and connection type
  QColor lineColor = palette.connectionLine;
  if (!m_label.isEmpty()) {
    // Use different colors for different branch types
    if (m_label.toLower() == "true") {
      lineColor = palette.connectionTrue;
    } else if (m_label.toLower() == "false") {
      lineColor = palette.connectionFalse;
    } else if (m_branchIndex >= 0) {
      // Use a color palette for choice options
      const QColor branchColors[] = {
          palette.connectionChoice1, palette.connectionChoice2, palette.connectionChoice3,
          palette.connectionChoice4, palette.connectionChoice5,
      };
      lineColor = branchColors[m_branchIndex % 5];
    }
  }

  // Issue #345: Apply connection type styling
  qreal lineWidth = 2;
  Qt::PenStyle penStyle = Qt::SolidLine;

  switch (connType) {
  case ConnectionType::SceneTransition:
    // Scene transitions: thicker, scene-colored line
    lineColor = palette.connectionSceneTransition; // Issue #389
    lineWidth = 2.5;
    break;
  case ConnectionType::CrossScene:
    // Cross-scene: dashed line indicating major scene change
    lineColor = palette.connectionCrossScene; // Issue #389
    lineWidth = 2.5;
    penStyle = Qt::DashLine;
    break;
  case ConnectionType::SameScene:
  default:
    // Same-scene connections: standard styling (already set)
    break;
  }

  // Issue #325: Visual feedback for selected connections
  if (isSelected()) {
    lineColor = lineColor.lighter(150); // Brighten when selected
    lineWidth = lineWidth + 1;          // Make thicker when selected
  }

  QPen connectionPen(lineColor, lineWidth, penStyle);
  if (penStyle == Qt::DashLine) {
    connectionPen.setDashPattern({6, 4});
  }
  painter->setPen(connectionPen);
  painter->setBrush(Qt::NoBrush);
  painter->drawPath(m_path);

  // Issue #345: Draw scene transition indicator icon at midpoint
  if (connType == ConnectionType::SceneTransition || connType == ConnectionType::CrossScene) {
    QPointF midPoint = m_path.pointAtPercent(0.5);
    drawSceneTransitionIndicator(painter, midPoint, connType == ConnectionType::CrossScene);
  }

  // Draw edge label if set
  if (!m_label.isEmpty()) {
    // Calculate label position (middle of the bezier curve)
    qreal t = 0.5;
    QPointF labelPos = m_path.pointAtPercent(t);

    // Draw label background
    QFont labelFont = NMStyleManager::instance().defaultFont();
    labelFont.setPointSize(labelFont.pointSize() - 1);
    painter->setFont(labelFont);

    QFontMetrics fm(labelFont);
    QString displayLabel = m_label;
    // Truncate long labels
    if (displayLabel.length() > 15) {
      displayLabel = displayLabel.left(12) + "...";
    }
    QRect textRect = fm.boundingRect(displayLabel);
    textRect.adjust(-4, -2, 4, 2);

    // Position label slightly above the path
    QPointF bgPos = labelPos - QPointF(textRect.width() / 2.0, textRect.height() + 4);
    QRectF bgRect(bgPos.x(), bgPos.y(), textRect.width(), textRect.height());

    // Draw rounded background
    QColor labelBg = palette.connectionLabelBg; // Issue #389
    labelBg.setAlpha(220);
    painter->setBrush(labelBg);
    painter->setPen(QPen(lineColor.darker(120), 1));
    painter->drawRoundedRect(bgRect, 4, 4);

    // Draw label text
    painter->setPen(lineColor.lighter(130));
    painter->drawText(bgRect, Qt::AlignCenter, displayLabel);
  }

  // Restore painter state
  painter->restore();
}

void NMGraphConnectionItem::drawSceneTransitionIndicator(QPainter* painter, const QPointF& pos,
                                                         bool isCrossScene) {
  // Issue #345: Draw a small visual indicator showing scene transition
  // For scene transitions: overlapping rectangle icon
  // For cross-scene: double rectangle icon with arrow

  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);

  const auto &palette = NMStyleManager::instance().palette();

  const qreal size = 14.0;
  // Issue #389: Use palette colors for scene transition indicators
  QColor iconColor =
      isCrossScene ? palette.connectionCrossScene : palette.connectionSceneTransition;
  iconColor.setAlpha(220);
  const QColor borderColor = iconColor.darker(130);

  // Draw two overlapping rectangles representing scene transition
  QRectF rect1(pos.x() - size / 2 - 2, pos.y() - size / 2 - 2, size * 0.6, size * 0.6);
  QRectF rect2(pos.x() - size / 2 + 4, pos.y() - size / 2 + 2, size * 0.6, size * 0.6);

  // Draw background for icon area
  QColor iconBg = palette.sceneIconBg; // Issue #389
  iconBg.setAlpha(200);
  painter->setBrush(iconBg);
  painter->setPen(Qt::NoPen);
  painter->drawEllipse(pos, size * 0.6, size * 0.6);

  // Draw first rectangle (back)
  painter->setBrush(iconColor.darker(140));
  painter->setPen(QPen(borderColor.darker(120), 1));
  painter->drawRoundedRect(rect1, 2, 2);

  // Draw second rectangle (front)
  painter->setBrush(iconColor);
  painter->setPen(QPen(borderColor, 1));
  painter->drawRoundedRect(rect2, 2, 2);

  // For cross-scene, add a small arrow indicator
  if (isCrossScene) {
    painter->setPen(QPen(QColor(255, 255, 255, 200), 1.5));
    QPointF arrowStart = pos + QPointF(size * 0.3, 0);
    QPointF arrowEnd = pos + QPointF(size * 0.5, 0);
    painter->drawLine(arrowStart, arrowEnd);
    // Arrow head
    painter->drawLine(arrowEnd, arrowEnd + QPointF(-3, -2));
    painter->drawLine(arrowEnd, arrowEnd + QPointF(-3, 2));
  }

  painter->restore();
}

void NMGraphConnectionItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* event) {
  // Issue #325: Context menu for connection deletion
  QMenu menu;
  auto& iconMgr = NMIconManager::instance();

  // Delete Connection action
  QAction* deleteAction = menu.addAction("Delete Connection");
  deleteAction->setIcon(iconMgr.getIcon("edit-delete", 16));
  deleteAction->setToolTip("Remove this connection (Del)");

  // Show menu and handle action
  QAction* selectedAction = menu.exec(event->screenPos());

  if (selectedAction == deleteAction) {
    if (auto* graphScene = qobject_cast<NMStoryGraphScene*>(scene())) {
      if (m_startNode && m_endNode) {
        NMUndoManager::instance().pushCommand(new DisconnectGraphNodesCommand(
            graphScene, m_startNode->nodeId(), m_endNode->nodeId()));
      }
    }
  }
}

// ============================================================================

} // namespace NovelMind::editor::qt
