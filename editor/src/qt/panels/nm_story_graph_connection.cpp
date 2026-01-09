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

NMGraphConnectionItem::NMGraphConnectionItem(NMGraphNodeItem *startNode,
                                             NMGraphNodeItem *endNode)
    : QGraphicsItem(), m_startNode(startNode), m_endNode(endNode) {
  setZValue(-1); // Draw behind nodes
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

void NMGraphConnectionItem::paint(QPainter *painter,
                                  const QStyleOptionGraphicsItem * /*option*/,
                                  QWidget * /*widget*/) {
  // Save painter state to prevent state leakage to other items
  painter->save();

  const auto &palette = NMStyleManager::instance().palette();

  painter->setRenderHint(QPainter::Antialiasing);

  // Determine connection color based on branch type
  QColor lineColor = palette.connectionLine;
  if (!m_label.isEmpty()) {
    // Use different colors for different branch types
    if (m_label.toLower() == "true") {
      lineColor = QColor(100, 200, 100); // Green for true
    } else if (m_label.toLower() == "false") {
      lineColor = QColor(200, 100, 100); // Red for false
    } else if (m_branchIndex >= 0) {
      // Use a color palette for choice options
      static const QColor branchColors[] = {
          QColor(100, 180, 255), // Blue
          QColor(255, 180, 100), // Orange
          QColor(180, 100, 255), // Purple
          QColor(255, 100, 180), // Pink
          QColor(100, 255, 180), // Cyan
      };
      lineColor = branchColors[m_branchIndex % 5];
    }
  }

  // Issue #325: Visual feedback for selected connections
  qreal lineWidth = 2;
  if (isSelected()) {
    lineColor = lineColor.lighter(150); // Brighten when selected
    lineWidth = 3; // Make thicker when selected
  }

  painter->setPen(QPen(lineColor, lineWidth));
  painter->setBrush(Qt::NoBrush);
  painter->drawPath(m_path);

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
    painter->setBrush(QColor(40, 44, 52, 220)); // Semi-transparent dark bg
    painter->setPen(QPen(lineColor.darker(120), 1));
    painter->drawRoundedRect(bgRect, 4, 4);

    // Draw label text
    painter->setPen(lineColor.lighter(130));
    painter->drawText(bgRect, Qt::AlignCenter, displayLabel);
  }

  // Restore painter state
  painter->restore();
}

void NMGraphConnectionItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event) {
  // Issue #325: Context menu for connection deletion
  QMenu menu;
  auto &iconMgr = NMIconManager::instance();

  // Delete Connection action
  QAction *deleteAction = menu.addAction("Delete Connection");
  deleteAction->setIcon(iconMgr.getIcon("edit-delete", 16));
  deleteAction->setToolTip("Remove this connection (Del)");

  // Show menu and handle action
  QAction *selectedAction = menu.exec(event->screenPos());

  if (selectedAction == deleteAction) {
    if (auto *graphScene = qobject_cast<NMStoryGraphScene *>(scene())) {
      if (m_startNode && m_endNode) {
        NMUndoManager::instance().pushCommand(
            new DisconnectGraphNodesCommand(graphScene,
                                           m_startNode->nodeId(),
                                           m_endNode->nodeId()));
      }
    }
  }
}

// ============================================================================

} // namespace NovelMind::editor::qt
