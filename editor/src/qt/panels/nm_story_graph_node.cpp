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
#include <QInputDialog>
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
// NMGraphNodeItem
// ============================================================================

NMGraphNodeItem::NMGraphNodeItem(const QString &title, const QString &nodeType)
    : m_title(title), m_nodeType(nodeType) {
  setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
}

void NMGraphNodeItem::setTitle(const QString &title) {
  m_title = title;
  update();
}

void NMGraphNodeItem::setNodeType(const QString &type) {
  m_nodeType = type;
  update();
}

void NMGraphNodeItem::setSelected(bool selected) {
  if (m_isSelected != selected) {
    // Notify Qt that geometry will change (boundingRect depends on selection
    // state)
    prepareGeometryChange();
    m_isSelected = selected;
    QGraphicsItem::setSelected(selected);
    update();
  }
}

void NMGraphNodeItem::setBreakpoint(bool hasBreakpoint) {
  m_hasBreakpoint = hasBreakpoint;
  // Only update if we're in a valid scene with views
  // The signal connection is queued, so this is safe
  if (scene() && !scene()->views().isEmpty()) {
    update();
  }
}

void NMGraphNodeItem::setCurrentlyExecuting(bool isExecuting) {
  if (m_isCurrentlyExecuting != isExecuting) {
    // Notify Qt that geometry will change (boundingRect depends on executing
    // state)
    prepareGeometryChange();
    m_isCurrentlyExecuting = isExecuting;
    // Only update if we're in a valid scene with views
    // The signal connection is queued, so this is safe
    if (scene() && !scene()->views().isEmpty()) {
      update();
    }
  }
}

void NMGraphNodeItem::setEntry(bool isEntry) {
  m_isEntry = isEntry;
  if (scene() && !scene()->views().isEmpty()) {
    update();
  }
}

QPointF NMGraphNodeItem::inputPortPosition() const {
  const qreal height = isSceneNode() ? SCENE_NODE_HEIGHT : NODE_HEIGHT;
  return mapToScene(QPointF(0, height / 2));
}

QPointF NMGraphNodeItem::outputPortPosition() const {
  const qreal height = isSceneNode() ? SCENE_NODE_HEIGHT : NODE_HEIGHT;
  return mapToScene(QPointF(NODE_WIDTH, height / 2));
}

bool NMGraphNodeItem::hitTestInputPort(const QPointF &scenePos) const {
  const QPointF portPos = inputPortPosition();
  const qreal hitRadius = PORT_RADIUS + 6;
  if (QLineF(portPos, scenePos).length() <= hitRadius) {
    return true;
  }

  const QPointF localPos = mapFromScene(scenePos);
  const qreal zoneWidth = 16.0;
  const qreal height = isSceneNode() ? SCENE_NODE_HEIGHT : NODE_HEIGHT;
  const QRectF inputZone(0.0, 0.0, zoneWidth, height);
  return inputZone.contains(localPos);
}

bool NMGraphNodeItem::hitTestOutputPort(const QPointF &scenePos) const {
  const QPointF portPos = outputPortPosition();
  const qreal hitRadius = PORT_RADIUS + 6;
  if (QLineF(portPos, scenePos).length() <= hitRadius) {
    return true;
  }

  const QPointF localPos = mapFromScene(scenePos);
  const qreal zoneWidth = 16.0;
  const qreal height = isSceneNode() ? SCENE_NODE_HEIGHT : NODE_HEIGHT;
  const QRectF outputZone(NODE_WIDTH - zoneWidth, 0.0, zoneWidth, height);
  return outputZone.contains(localPos);
}

QRectF NMGraphNodeItem::boundingRect() const {
  const qreal height = isSceneNode() ? SCENE_NODE_HEIGHT : NODE_HEIGHT;

  // Add margin to include selection highlight, executing glow, and other
  // effects that draw outside the base node rectangle
  const qreal margin =
      m_isCurrentlyExecuting ? 10.0 : (m_isSelected ? 4.0 : 2.0);

  return QRectF(0, 0, NODE_WIDTH, height)
      .adjusted(-margin, -margin, margin, margin);
}

void NMGraphNodeItem::paint(QPainter *painter,
                            const QStyleOptionGraphicsItem * /*option*/,
                            QWidget * /*widget*/) {
  // Save painter state to prevent state leakage to other items
  painter->save();

  const auto &palette = NMStyleManager::instance().palette();

  painter->setRenderHint(QPainter::Antialiasing);

  const bool isScene = isSceneNode();
  const qreal nodeHeight = isScene ? SCENE_NODE_HEIGHT : NODE_HEIGHT;

  // Calculate margin offset to position node correctly within boundingRect
  const qreal margin =
      m_isCurrentlyExecuting ? 10.0 : (m_isSelected ? 4.0 : 2.0);
  painter->translate(margin, margin);

  // Node background - Scene nodes get a distinct gradient
  QColor bgColor = m_isSelected ? palette.nodeSelected : palette.nodeDefault;
  if (isScene) {
    // Scene nodes have a gradient background
    QLinearGradient gradient(0, 0, 0, nodeHeight);
    gradient.setColorAt(0, bgColor);
    gradient.setColorAt(1, bgColor.darker(110));
    painter->setBrush(gradient);
    painter->setPen(QPen(QColor(100, 200, 150), 2)); // Green border for scenes
  } else {
    painter->setBrush(bgColor);
    painter->setPen(QPen(palette.borderLight, 1));
  }
  painter->drawRoundedRect(QRectF(0, 0, NODE_WIDTH, nodeHeight), CORNER_RADIUS,
                           CORNER_RADIUS);

  // Header bar with icon
  QRectF headerRect(0, 0, NODE_WIDTH, 28);
  painter->setBrush(isScene ? QColor(45, 65, 55)
                            : palette.bgDark); // Greenish header for scenes
  painter->setPen(Qt::NoPen);
  QPainterPath headerPath;
  headerPath.addRoundedRect(headerRect, CORNER_RADIUS, CORNER_RADIUS);
  // Clip to top corners only
  QPainterPath clipPath;
  clipPath.addRect(QRectF(0, CORNER_RADIUS, NODE_WIDTH, 28 - CORNER_RADIUS));
  headerPath = headerPath.united(clipPath);
  painter->drawPath(headerPath);

  // Node type icon + text (header)
  QString iconName = "node-dialogue"; // default
  QColor iconColor = palette.textSecondary;

  // Map node types to icons and colors
  if (isScene) {
    iconName = "panel-scene-view";     // Use scene view icon
    iconColor = QColor(100, 220, 150); // Green
  } else if (m_nodeType.contains("Dialogue", Qt::CaseInsensitive)) {
    iconName = "node-dialogue";
    iconColor = QColor(100, 180, 255); // Blue
  } else if (m_nodeType.contains("Choice", Qt::CaseInsensitive)) {
    iconName = "node-choice";
    iconColor = QColor(255, 180, 100); // Orange
  } else if (m_nodeType.contains("Event", Qt::CaseInsensitive)) {
    iconName = "node-event";
    iconColor = QColor(255, 220, 100); // Yellow
  } else if (m_nodeType.contains("Condition", Qt::CaseInsensitive)) {
    iconName = "node-condition";
    iconColor = QColor(200, 100, 255); // Purple
  } else if (m_nodeType.contains("Random", Qt::CaseInsensitive)) {
    iconName = "node-random";
    iconColor = QColor(100, 255, 180); // Green
  } else if (m_nodeType.contains("Start", Qt::CaseInsensitive)) {
    iconName = "node-start";
    iconColor = QColor(100, 255, 100); // Bright Green
  } else if (m_nodeType.contains("End", Qt::CaseInsensitive)) {
    iconName = "node-end";
    iconColor = QColor(255, 100, 100); // Red
  } else if (m_nodeType.contains("Jump", Qt::CaseInsensitive)) {
    iconName = "node-jump";
    iconColor = QColor(180, 180, 255); // Light Blue
  } else if (m_nodeType.contains("Variable", Qt::CaseInsensitive)) {
    iconName = "node-variable";
    iconColor = QColor(255, 180, 255); // Pink
  }

  // Draw icon (with null check to prevent segfault if icon fails to load)
  QPixmap iconPixmap =
      NMIconManager::instance().getPixmap(iconName, 18, iconColor);
  if (!iconPixmap.isNull()) {
    painter->drawPixmap(6, static_cast<int>(headerRect.center().y()) - 9,
                        iconPixmap);
  }

  // Draw node type text
  painter->setPen(isScene ? QColor(100, 220, 150) : palette.textSecondary);
  painter->setFont(NMStyleManager::instance().defaultFont());
  painter->drawText(headerRect.adjusted(28, 0, -8, 0),
                    Qt::AlignVCenter | Qt::AlignLeft, m_nodeType);

  if (m_isEntry) {
    QPolygonF marker;
    marker << QPointF(NODE_WIDTH - 18, 6) << QPointF(NODE_WIDTH - 6, 14)
           << QPointF(NODE_WIDTH - 18, 22);
    painter->setBrush(QColor(80, 200, 120));
    painter->setPen(Qt::NoPen);
    painter->drawPolygon(marker);
  }

  // Node title (body)
  QRectF titleRect(8, 34, NODE_WIDTH - 16, nodeHeight - 42);
  painter->setPen(palette.textPrimary);
  QFont boldFont = NMStyleManager::instance().defaultFont();
  boldFont.setBold(true);
  painter->setFont(boldFont);
  painter->drawText(titleRect, Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap,
                    m_title);

  // Scene-specific: Draw dialogue count badge
  if (isScene && m_dialogueCount > 0) {
    const QString countText = QString("[%1 dialogues]").arg(m_dialogueCount);
    QFont smallFont = NMStyleManager::instance().defaultFont();
    smallFont.setPointSize(smallFont.pointSize() - 1);
    painter->setFont(smallFont);
    painter->setPen(QColor(150, 200, 180));
    painter->drawText(QRectF(8, nodeHeight - 22, NODE_WIDTH - 16, 18),
                      Qt::AlignBottom | Qt::AlignLeft, countText);
  }

  // Scene-specific: Draw embedded dialogue indicator
  if (isScene && m_hasEmbeddedDialogue) {
    // Small graph icon in bottom-right corner
    QRectF indicatorRect(NODE_WIDTH - 24, nodeHeight - 22, 16, 16);
    painter->setPen(QColor(100, 180, 255));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(indicatorRect.adjusted(2, 2, -2, -2));
    // Draw mini node icons
    painter->drawEllipse(indicatorRect.center() - QPointF(3, 3), 2, 2);
    painter->drawEllipse(indicatorRect.center() + QPointF(3, 3), 2, 2);
    painter->drawLine(indicatorRect.center() - QPointF(1, 1),
                      indicatorRect.center() + QPointF(1, 1));
  }

  // Dialogue-specific: Draw voice-over indicators
  if (isDialogueNode()) {
    const qreal bottomY = nodeHeight - 24;
    const qreal iconSize = 16;

    // Voice clip status indicator
    if (hasVoiceClip()) {
      // Show play button for preview
      QRectF playButtonRect(NODE_WIDTH - 44, bottomY, iconSize, iconSize);
      QColor playColor;

      // Color based on binding status
      switch (m_voiceBindingStatus) {
      case 1:                              // Bound
        playColor = QColor(100, 220, 150); // Green
        break;
      case 2:                              // MissingFile
        playColor = QColor(220, 100, 100); // Red
        break;
      case 3:                              // AutoMapped
        playColor = QColor(100, 180, 255); // Blue
        break;
      default:                             // Unbound/Pending
        playColor = QColor(180, 180, 180); // Gray
        break;
      }

      // Draw play icon (triangle)
      painter->setBrush(playColor);
      painter->setPen(QPen(playColor.darker(120), 1));
      QPolygonF playTriangle;
      const QPointF playCenter = playButtonRect.center();
      playTriangle << playCenter + QPointF(-4, -5)
                   << playCenter + QPointF(-4, 5) << playCenter + QPointF(5, 0);
      painter->drawPolygon(playTriangle);
    }

    // Show record button (always visible for dialogue nodes)
    QRectF recordButtonRect(NODE_WIDTH - 22, bottomY, iconSize, iconSize);
    QColor recordColor =
        hasVoiceClip() ? QColor(220, 100, 100) : QColor(255, 140, 140);

    // Draw record icon (circle)
    painter->setBrush(recordColor);
    painter->setPen(QPen(recordColor.darker(120), 1));
    painter->drawEllipse(recordButtonRect.center(), 6, 6);

    // Small waveform indicator if voice is present
    if (hasVoiceClip() && m_voiceBindingStatus == 1) {
      QFont tinyFont = NMStyleManager::instance().defaultFont();
      tinyFont.setPointSize(7);
      painter->setFont(tinyFont);
      painter->setPen(QColor(150, 220, 180));
      painter->drawText(QRectF(8, bottomY, 60, 16),
                        Qt::AlignVCenter | Qt::AlignLeft, "Voice");
    }

    // Localization status indicator
    if (!m_localizationKey.isEmpty()) {
      const qreal locIndicatorX = 8;
      const qreal locIndicatorY = nodeHeight - 8;
      const qreal indicatorSize = 6;

      QColor locColor;
      QString locTooltip;

      switch (m_translationStatus) {
      case 0: // NotLocalizable
        // No indicator for non-localizable content
        break;
      case 1:                             // Untranslated
        locColor = QColor(255, 180, 100); // Orange warning
        locTooltip = "Untranslated";
        break;
      case 2:                             // Translated
        locColor = QColor(100, 220, 150); // Green success
        locTooltip = "Translated";
        break;
      case 3:                             // NeedsReview
        locColor = QColor(180, 180, 255); // Light blue
        locTooltip = "Needs Review";
        break;
      case 4:                             // Missing
        locColor = QColor(255, 100, 100); // Red error
        locTooltip = "Missing Translation";
        break;
      default:
        locColor = QColor(180, 180, 180); // Gray
        break;
      }

      if (m_translationStatus > 0) {
        // Draw localization status dot
        painter->setBrush(locColor);
        painter->setPen(QPen(locColor.darker(120), 1));
        painter->drawEllipse(QPointF(locIndicatorX, locIndicatorY),
                             indicatorSize / 2, indicatorSize / 2);

        // Draw localization key text (abbreviated)
        QFont keyFont = NMStyleManager::instance().defaultFont();
        keyFont.setPointSize(6);
        painter->setFont(keyFont);
        painter->setPen(palette.textMuted);
        QString displayKey = m_localizationKey;
        if (displayKey.length() > 20) {
          displayKey = "..." + displayKey.right(17);
        }
        painter->drawText(
            QRectF(locIndicatorX + 8, locIndicatorY - 6, NODE_WIDTH - 80, 12),
            Qt::AlignVCenter | Qt::AlignLeft, displayKey);
      }
    }
  }

  // Input/output ports
  const QPointF inputPort(0, nodeHeight / 2);
  const QPointF outputPort(NODE_WIDTH, nodeHeight / 2);
  painter->setBrush(palette.bgDark);
  painter->setPen(QPen(palette.borderLight, 1));
  painter->drawEllipse(inputPort, PORT_RADIUS, PORT_RADIUS);
  painter->setBrush(palette.accentPrimary);
  painter->drawEllipse(outputPort, PORT_RADIUS, PORT_RADIUS);

  // Selection highlight
  if (m_isSelected) {
    painter->setPen(QPen(palette.accentPrimary, 2));
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(boundingRect().adjusted(1, 1, -1, -1),
                             CORNER_RADIUS, CORNER_RADIUS);
  }

  // Breakpoint indicator (red circle in top-left corner)
  if (m_hasBreakpoint) {
    const qreal radius = 8.0;
    const QPointF center(radius + 4, radius + 4);

    painter->setBrush(QColor(220, 60, 60)); // Red
    painter->setPen(QPen(QColor(180, 40, 40), 2));
    painter->drawEllipse(center, radius, radius);

    // Inner highlight for 3D effect
    painter->setBrush(QColor(255, 100, 100, 80));
    painter->setPen(Qt::NoPen);
    painter->drawEllipse(center - QPointF(2, 2), radius * 0.4, radius * 0.4);
  }

  // Currently executing indicator (pulsing green border + glow)
  if (m_isCurrentlyExecuting) {
    // Outer glow effect
    for (int i = 3; i >= 0; --i) {
      int alpha = 40 - (i * 10);
      QColor glowColor(60, 220, 120, alpha);
      painter->setPen(QPen(glowColor, 3 + i * 2));
      painter->setBrush(Qt::NoBrush);
      painter->drawRoundedRect(boundingRect().adjusted(-i, -i, i, i),
                               CORNER_RADIUS + i, CORNER_RADIUS + i);
    }

    // Solid green border
    painter->setPen(QPen(QColor(60, 220, 120), 3));
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(boundingRect().adjusted(1, 1, -1, -1),
                             CORNER_RADIUS, CORNER_RADIUS);

    // Execution arrow indicator in top-right corner
    const qreal arrowSize = 16.0;
    const QPointF arrowCenter(NODE_WIDTH - arrowSize - 4, arrowSize / 2 + 4);

    QPainterPath arrowPath;
    arrowPath.moveTo(arrowCenter + QPointF(-arrowSize / 2, -arrowSize / 3));
    arrowPath.lineTo(arrowCenter + QPointF(arrowSize / 2, 0));
    arrowPath.lineTo(arrowCenter + QPointF(-arrowSize / 2, arrowSize / 3));
    arrowPath.closeSubpath();

    painter->setBrush(QColor(60, 220, 120));
    painter->setPen(QPen(QColor(40, 180, 90), 2));
    painter->drawPath(arrowPath);
  }

  // Restore painter state
  painter->restore();
}

QVariant NMGraphNodeItem::itemChange(GraphicsItemChange change,
                                     const QVariant &value) {
  if (change == ItemPositionHasChanged && scene()) {
    // Update all connections attached to this node
    auto *graphScene = qobject_cast<NMStoryGraphScene *>(scene());
    if (graphScene) {
      const auto connections = graphScene->findConnectionsForNode(this);
      for (auto *conn : connections) {
        if (conn) {
          conn->updatePath();
        }
      }
    }
  } else if (change == ItemSelectedHasChanged) {
    // Geometry changes when selection changes (boundingRect includes selection
    // highlight)
    if (m_isSelected != value.toBool()) {
      prepareGeometryChange();
      m_isSelected = value.toBool();
      update();
    }
  }
  return QGraphicsItem::itemChange(change, value);
}

void NMGraphNodeItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event) {
  QMenu menu;
  auto &iconMgr = NMIconManager::instance();

  const bool isScene = isSceneNode();
  const bool isDialogue = isDialogueNode();

  // Scene-specific actions
  QAction *editLayoutAction = nullptr;
  QAction *editDialogueFlowAction = nullptr;
  QAction *openScriptAction = nullptr;

  QAction *editAnimationsAction = nullptr;

  if (isScene) {
    editLayoutAction = menu.addAction("Edit Scene Layout");
    editLayoutAction->setIcon(iconMgr.getIcon("panel-scene-view", 16));
    editLayoutAction->setToolTip("Open Scene View to edit visual layout");

    editDialogueFlowAction = menu.addAction("Edit Dialogue Flow");
    editDialogueFlowAction->setIcon(iconMgr.getIcon("node-dialogue", 16));
    editDialogueFlowAction->setToolTip("Edit embedded dialogue graph");

    editAnimationsAction = menu.addAction("Edit Animations");
    editAnimationsAction->setIcon(iconMgr.getIcon("panel-timeline", 16));
    editAnimationsAction->setToolTip("Open Timeline to edit scene animations");

    if (!m_scriptPath.isEmpty()) {
      openScriptAction = menu.addAction("Open Script");
      openScriptAction->setIcon(iconMgr.getIcon("panel-script-editor", 16));
      openScriptAction->setToolTip("Open .nms script file");
    }

    menu.addSeparator();
  }

  // Dialogue-specific voice-over actions
  QAction *assignVoiceAction = nullptr;
  QAction *previewVoiceAction = nullptr;
  QAction *recordVoiceAction = nullptr;
  QAction *clearVoiceAction = nullptr;
  QAction *autoDetectVoiceAction = nullptr;

  if (isDialogue) {
    assignVoiceAction = menu.addAction("Assign Voice Clip...");
    assignVoiceAction->setIcon(iconMgr.getIcon("audio-file", 16));
    assignVoiceAction->setToolTip("Drag-drop or browse for voice audio file");

    autoDetectVoiceAction = menu.addAction("Auto-Detect Voice");
    autoDetectVoiceAction->setIcon(iconMgr.getIcon("search", 16));
    autoDetectVoiceAction->setToolTip(
        "Auto-detect voice file based on localization key");

    if (hasVoiceClip()) {
      previewVoiceAction = menu.addAction("Preview Voice");
      previewVoiceAction->setIcon(iconMgr.getIcon("play", 16));
      previewVoiceAction->setToolTip("Play voice clip preview");

      clearVoiceAction = menu.addAction("Clear Voice Clip");
      clearVoiceAction->setIcon(iconMgr.getIcon("edit-delete", 16));
      clearVoiceAction->setToolTip("Remove voice clip assignment");
    }

    recordVoiceAction = menu.addAction(hasVoiceClip() ? "Re-record Voice..."
                                                      : "Record Voice...");
    recordVoiceAction->setIcon(iconMgr.getIcon("record", 16));
    recordVoiceAction->setToolTip("Open Recording Studio to record voice");

    menu.addSeparator();
  }

  // Toggle Breakpoint action
  QAction *breakpointAction =
      menu.addAction(m_hasBreakpoint ? "Remove Breakpoint" : "Add Breakpoint");
  breakpointAction->setIcon(
      iconMgr.getIcon(m_hasBreakpoint ? "remove" : "breakpoint", 16));

  menu.addSeparator();

  // Edit Node action
  QAction *editAction = menu.addAction("Edit Node Properties");
  editAction->setIcon(iconMgr.getIcon("panel-inspector", 16));

  // Rename Scene action (Scene nodes only)
  QAction *renameAction = nullptr;
  if (isScene) {
    renameAction = menu.addAction("Rename Scene");
    renameAction->setIcon(iconMgr.getIcon("edit-rename", 16));
  }

  // Set as Entry action
  QAction *entryAction = menu.addAction("Set as Entry");
  entryAction->setIcon(iconMgr.getIcon("node-start", 16));
  if (m_isEntry) {
    entryAction->setEnabled(false);
  }

  menu.addSeparator();

  // Duplicate Scene action (Scene nodes only)
  QAction *duplicateAction = nullptr;
  if (isScene) {
    duplicateAction = menu.addAction("Duplicate Scene");
    duplicateAction->setIcon(iconMgr.getIcon("edit-copy", 16));
  }

  // Delete Node action
  QAction *deleteAction = menu.addAction("Delete Node");
  deleteAction->setIcon(iconMgr.getIcon("edit-delete", 16));

  // Show menu and handle action
  QAction *selectedAction = menu.exec(event->screenPos());

  if (selectedAction == breakpointAction) {
    // Toggle breakpoint via Play Mode Controller
    // Only toggle if we have a valid node ID string
    if (!m_nodeIdString.isEmpty()) {
      NMPlayModeController::instance().toggleBreakpoint(m_nodeIdString);
      // Update visual state immediately
      setBreakpoint(
          NMPlayModeController::instance().hasBreakpoint(m_nodeIdString));
    }
  } else if (selectedAction == deleteAction) {
    if (auto *graphScene = qobject_cast<NMStoryGraphScene *>(scene())) {
      NMUndoManager::instance().pushCommand(
          new DeleteGraphNodeCommand(graphScene, nodeId()));
    }
  } else if (selectedAction == entryAction) {
    if (auto *graphScene = qobject_cast<NMStoryGraphScene *>(scene())) {
      graphScene->requestEntryNode(m_nodeIdString);
    }
  } else if (selectedAction == editAction) {
    if (scene() && !scene()->views().isEmpty()) {
      if (auto *view =
              qobject_cast<NMStoryGraphView *>(scene()->views().first())) {
        view->emitNodeClicked(nodeId());
      }
    }
  } else if (isScene && selectedAction == editLayoutAction) {
    // Emit signal to open Scene View for this scene
    if (scene() && !scene()->views().isEmpty()) {
      if (auto *view =
              qobject_cast<NMStoryGraphView *>(scene()->views().first())) {
        emit view->nodeDoubleClicked(nodeId()); // Reuse double-click signal
      }
    }
  } else if (isScene && selectedAction == editDialogueFlowAction) {
    // Emit signal to open embedded dialogue graph editor
    if (scene() && !scene()->views().isEmpty()) {
      if (auto *graphScene = qobject_cast<NMStoryGraphScene *>(scene())) {
        // Find the parent panel to emit the signal
        for (QObject *obj = graphScene; obj; obj = obj->parent()) {
          if (auto *panel = qobject_cast<NMStoryGraphPanel *>(obj)) {
            emit panel->editDialogueFlowRequested(m_sceneId);
            break;
          }
        }
      }
    }
    qDebug() << "[StoryGraph] Edit dialogue flow for scene:" << m_sceneId;
  } else if (isScene && editAnimationsAction &&
             selectedAction == editAnimationsAction) {
    // Emit signal to open Timeline and Scene View for animation editing
    if (scene() && !scene()->views().isEmpty()) {
      if (auto *view =
              qobject_cast<NMStoryGraphView *>(scene()->views().first())) {
        emit view->nodeDoubleClicked(nodeId()); // Reuse double-click signal
      }
    }
  } else if (isScene && openScriptAction &&
             selectedAction == openScriptAction) {
    // Emit signal to open script editor
    if (scene() && !scene()->views().isEmpty()) {
      if (auto *graphScene = qobject_cast<NMStoryGraphScene *>(scene())) {
        // Find the parent panel to emit the signal
        for (QObject *obj = graphScene; obj; obj = obj->parent()) {
          if (auto *panel = qobject_cast<NMStoryGraphPanel *>(obj)) {
            emit panel->openSceneScriptRequested(m_sceneId, m_scriptPath);
            break;
          }
        }
      }
    }
    qDebug() << "[StoryGraph] Open script:" << m_scriptPath;
  } else if (isScene && duplicateAction && selectedAction == duplicateAction) {
    // Implement scene duplication
    if (auto *graphScene = qobject_cast<NMStoryGraphScene *>(scene())) {
      // Create a duplicate node with offset position
      const QPointF offset(50, 50);
      const QString newTitle = m_title + " (Copy)";

      // Add the duplicated node
      auto *newNode = graphScene->addNode(newTitle, m_nodeType, pos() + offset);
      if (newNode) {
        // Copy all properties from the original node
        newNode->setSceneId(m_sceneId + "_copy");
        newNode->setScriptPath(m_scriptPath);
        newNode->setHasEmbeddedDialogue(m_hasEmbeddedDialogue);
        newNode->setDialogueCount(m_dialogueCount);
        newNode->setThumbnailPath(m_thumbnailPath);

        qDebug() << "[StoryGraph] Duplicated scene:" << m_sceneId << "to"
                 << newNode->sceneId();
      }
    }
  } else if (isScene && renameAction && selectedAction == renameAction) {
    // Implement scene renaming with input dialog
    bool ok = false;
    QString newName = QInputDialog::getText(
        nullptr, "Rename Scene", "Enter new scene name:", QLineEdit::Normal,
        m_title, &ok);

    if (ok && !newName.isEmpty() && newName != m_title) {
      setTitle(newName);
      qDebug() << "[StoryGraph] Renamed scene:" << m_sceneId << "to" << newName;
    }
  } else if (isDialogue && assignVoiceAction &&
             selectedAction == assignVoiceAction) {
    // Emit signal to open voice clip assignment dialog
    if (scene() && !scene()->views().isEmpty()) {
      if (auto *graphScene = qobject_cast<NMStoryGraphScene *>(scene())) {
        // Find the parent panel to emit the signal
        for (QObject *obj = graphScene; obj; obj = obj->parent()) {
          if (auto *panel = qobject_cast<NMStoryGraphPanel *>(obj)) {
            emit panel->voiceClipAssignRequested(m_nodeIdString,
                                                 m_voiceClipPath);
            break;
          }
        }
      }
    }
    qDebug() << "[StoryGraph] Assign voice clip to dialogue node:"
             << m_nodeIdString;
  } else if (isDialogue && autoDetectVoiceAction &&
             selectedAction == autoDetectVoiceAction) {
    // Emit signal to auto-detect voice file based on localization key
    if (scene() && !scene()->views().isEmpty()) {
      if (auto *graphScene = qobject_cast<NMStoryGraphScene *>(scene())) {
        // Find the parent panel to emit the signal
        for (QObject *obj = graphScene; obj; obj = obj->parent()) {
          if (auto *panel = qobject_cast<NMStoryGraphPanel *>(obj)) {
            emit panel->voiceAutoDetectRequested(m_nodeIdString,
                                                 m_localizationKey);
            break;
          }
        }
      }
    }
    qDebug() << "[StoryGraph] Auto-detect voice for dialogue node:"
             << m_nodeIdString;
  } else if (isDialogue && previewVoiceAction &&
             selectedAction == previewVoiceAction) {
    // Emit signal to preview voice clip
    if (scene() && !scene()->views().isEmpty()) {
      if (auto *graphScene = qobject_cast<NMStoryGraphScene *>(scene())) {
        // Find the parent panel to emit the signal
        for (QObject *obj = graphScene; obj; obj = obj->parent()) {
          if (auto *panel = qobject_cast<NMStoryGraphPanel *>(obj)) {
            emit panel->voiceClipPreviewRequested(m_nodeIdString,
                                                  m_voiceClipPath);
            break;
          }
        }
      }
    }
    qDebug() << "[StoryGraph] Preview voice:" << m_voiceClipPath;
  } else if (isDialogue && recordVoiceAction &&
             selectedAction == recordVoiceAction) {
    // Emit signal to open Recording Studio panel with this dialogue line
    if (scene() && !scene()->views().isEmpty()) {
      if (auto *graphScene = qobject_cast<NMStoryGraphScene *>(scene())) {
        // Find the parent panel to emit the signal
        for (QObject *obj = graphScene; obj; obj = obj->parent()) {
          if (auto *panel = qobject_cast<NMStoryGraphPanel *>(obj)) {
            emit panel->voiceRecordingRequested(m_nodeIdString, m_dialogueText,
                                                m_dialogueSpeaker);
            break;
          }
        }
      }
    }
    qDebug() << "[StoryGraph] Record voice for dialogue node:"
             << m_nodeIdString;
  } else if (isDialogue && clearVoiceAction &&
             selectedAction == clearVoiceAction) {
    // Clear voice clip assignment
    setVoiceClipPath("");
    setVoiceBindingStatus(0); // Unbound
    qDebug() << "[StoryGraph] Cleared voice clip for dialogue node:"
             << m_nodeIdString;
    update();
  }

  event->accept();
}

// ============================================================================

} // namespace NovelMind::editor::qt
