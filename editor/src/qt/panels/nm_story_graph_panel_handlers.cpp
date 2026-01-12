#include "NovelMind/editor/error_reporter.hpp"
#include "NovelMind/editor/project_manager.hpp"
#include "NovelMind/editor/qt/nm_dialogs.hpp"
#include "NovelMind/editor/qt/nm_play_mode_controller.hpp"
#include "NovelMind/editor/qt/nm_undo_manager.hpp"
#include "NovelMind/editor/qt/panels/nm_story_graph_panel.hpp"

#include "nm_story_graph_panel_detail.hpp"
#include "story_graph_edge_manager.hpp"
#include "story_graph_layout.hpp"
#include "story_graph_node_factory.hpp"
#include "story_graph_property_manager.hpp"
#include "story_graph_serialization.hpp"

#include <QDebug>
#include <QFile>
#include <QFileInfo>

namespace NovelMind::editor::qt {

void NMStoryGraphPanel::onZoomIn() {
  if (m_view) {
    m_view->setZoomLevel(m_view->zoomLevel() * 1.25);
  }
}

void NMStoryGraphPanel::onZoomOut() {
  if (m_view) {
    m_view->setZoomLevel(m_view->zoomLevel() / 1.25);
  }
}

void NMStoryGraphPanel::onZoomReset() {
  if (m_view) {
    m_view->setZoomLevel(1.0);
    m_view->centerOnGraph();
  }
}

void NMStoryGraphPanel::onFitToGraph() {
  if (m_view && m_scene && !m_scene->items().isEmpty()) {
    m_view->fitInView(m_scene->itemsBoundingRect().adjusted(-50, -50, 50, 50),
                      Qt::KeepAspectRatio);
  }
}

void NMStoryGraphPanel::onAutoLayout() {
  if (!m_scene) {
    return;
  }

  const auto &nodes = m_scene->nodes();
  if (nodes.isEmpty()) {
    return;
  }

  // Ask for confirmation before rearranging
  auto result = NMMessageDialog::showQuestion(
      this, tr("Auto Layout"),
      tr("This will automatically arrange all nodes in a hierarchical "
         "layout.\n\n"
         "Current manual positioning will be lost. Do you want to continue?"),
      {NMDialogButton::Yes, NMDialogButton::No}, NMDialogButton::No);

  if (result != NMDialogButton::Yes) {
    return;
  }

  // Delegate to layout module
  layout::applyAutoLayout(m_scene, nodes, m_scene->connections());

  // Fit the graph to view
  if (m_view) {
    m_view->centerOnGraph();
  }
}

void NMStoryGraphPanel::onCurrentNodeChanged(const QString &nodeId) {
  updateCurrentNode(nodeId);
}

void NMStoryGraphPanel::onBreakpointsChanged() { updateNodeBreakpoints(); }

void NMStoryGraphPanel::onNodeClicked(uint64_t nodeId) {
  m_nodeIdToString.insert(nodeId, findNodeById(nodeId)->nodeIdString());
  node_factory::handleNodeClick(this, nodeId);
}

void NMStoryGraphPanel::onNodeDoubleClicked(uint64_t nodeId) {
  node_factory::handleNodeDoubleClick(this, nodeId);
}

void NMStoryGraphPanel::onNodeAdded(uint64_t nodeId,
                                    const QString &nodeIdString,
                                    const QString &nodeType) {
  if (m_isRebuilding) {
    return;
  }

  node_factory::handleNodeAdded(this, nodeId, nodeIdString, nodeType);

  // Save layout
  auto *node = findNodeById(nodeId);
  if (node) {
    LayoutNode layout = detail::buildLayoutFromNode(node);
    m_layoutNodes.insert(node->nodeIdString(), layout);
    detail::saveGraphLayout(m_layoutNodes, m_layoutEntryScene);
  }

  if (m_markNextNodeAsEntry) {
    m_markNextNodeAsEntry = false;
    onEntryNodeRequested(nodeIdString);
  }
}

void NMStoryGraphPanel::onNodeDeleted(uint64_t nodeId) {
  if (m_isRebuilding) {
    return;
  }

  const QString idString = m_nodeIdToString.take(nodeId);

  if (!idString.isEmpty()) {
    m_layoutNodes.remove(idString);
    if (m_layoutEntryScene == idString) {
      m_layoutEntryScene.clear();
      ProjectManager::instance().setStartScene("");
    }
    detail::saveGraphLayout(m_layoutNodes, m_layoutEntryScene);
  }
}

void NMStoryGraphPanel::onConnectionAdded(uint64_t fromNodeId,
                                          uint64_t toNodeId) {
  if (m_isRebuilding) {
    return;
  }

  edge_manager::handleConnectionAdded(this, m_scene, fromNodeId, toNodeId);

  // Save the updated layout
  auto *from = findNodeById(fromNodeId);
  if (from && !m_isRebuilding) {
    LayoutNode layout = detail::buildLayoutFromNode(from);
    m_layoutNodes.insert(from->nodeIdString(), layout);
    detail::saveGraphLayout(m_layoutNodes, m_layoutEntryScene);
  }
}

void NMStoryGraphPanel::onConnectionDeleted(uint64_t fromNodeId,
                                            uint64_t toNodeId) {
  if (m_isRebuilding) {
    return;
  }

  edge_manager::handleConnectionDeleted(this, m_scene, fromNodeId, toNodeId);

  // Save the updated layout
  auto *from = findNodeById(fromNodeId);
  if (from && !m_isRebuilding) {
    LayoutNode layout = detail::buildLayoutFromNode(from);
    m_layoutNodes.insert(from->nodeIdString(), layout);
    detail::saveGraphLayout(m_layoutNodes, m_layoutEntryScene);
  }
}

NMGraphNodeItem *
NMStoryGraphPanel::findNodeByIdString(const QString &id) const {
  if (!m_scene)
    return nullptr;

  const auto items = m_scene->items();
  for (auto *item : items) {
    if (auto *node = qgraphicsitem_cast<NMGraphNodeItem *>(item)) {
      if (node->nodeIdString() == id) {
        return node;
      }
    }
  }
  return nullptr;
}

NMGraphNodeItem *NMStoryGraphPanel::findNodeById(uint64_t nodeId) const {
  if (!m_scene) {
    return nullptr;
  }
  return m_scene->findNode(nodeId);
}

void NMStoryGraphPanel::updateNodeBreakpoints() {
  if (!m_scene)
    return;

  auto &playController = NMPlayModeController::instance();
  const QSet<QString> breakpoints = playController.breakpoints();

  // Update all nodes - make a copy of the list to avoid iterator invalidation
  QList<QGraphicsItem *> itemsCopy = m_scene->items();
  for (auto *item : itemsCopy) {
    // Check if item is still valid (not deleted)
    if (!item || !m_scene->items().contains(item))
      continue;

    if (auto *node = qgraphicsitem_cast<NMGraphNodeItem *>(item)) {
      // Extra safety: ensure node is still in scene
      if (node->scene() != m_scene)
        continue;

      bool hasBreakpoint = breakpoints.contains(node->nodeIdString());
      node->setBreakpoint(hasBreakpoint);
    }
  }
}

void NMStoryGraphPanel::updateCurrentNode(const QString &nodeId) {
  if (!m_scene) {
    qWarning() << "[StoryGraph] updateCurrentNode: scene is null!";
    return;
  }

  qDebug() << "[StoryGraph] updateCurrentNode:" << nodeId << "(prev was"
           << m_currentExecutingNode << ")";

  // Clear previous execution state
  if (!m_currentExecutingNode.isEmpty()) {
    auto *prevNode = findNodeByIdString(m_currentExecutingNode);
    if (prevNode) {
      // Double-check that node is still valid and in scene
      if (prevNode->scene() == m_scene &&
          m_scene->items().contains(prevNode)) {
        qDebug() << "[StoryGraph] Clearing execution state on"
                 << m_currentExecutingNode;
        prevNode->setCurrentlyExecuting(false);
      } else {
        qWarning() << "[StoryGraph] Previous node" << m_currentExecutingNode
                   << "found but no longer valid in scene!";
      }
    } else {
      qDebug() << "[StoryGraph] Warning: Previous node"
               << m_currentExecutingNode
               << "not found in graph (may have been deleted)";
    }
  }

  // Set new execution state
  m_currentExecutingNode = nodeId;
  if (!nodeId.isEmpty()) {
    auto *currentNode = findNodeByIdString(nodeId);
    if (currentNode) {
      // Double-check that node is still valid and in scene
      if (currentNode->scene() == m_scene &&
          m_scene->items().contains(currentNode)) {
        qDebug() << "[StoryGraph] Setting execution state on" << nodeId;
        currentNode->setCurrentlyExecuting(true);

        // Issue #339: Deferred centering with follow mode support
        // Only center if follow mode is enabled
        if (m_followCurrentNode) {
          if (m_view && !m_view->isHidden()) {
            qDebug() << "[StoryGraph] Centering view on" << nodeId;
            m_view->centerOn(currentNode);
            m_pendingCenterNode.clear(); // Clear any pending operation
          } else {
            // View is hidden, queue the centering operation
            qDebug() << "[StoryGraph] View is hidden, queueing center "
                        "operation for"
                     << nodeId;
            m_pendingCenterNode = nodeId;
          }
        } else {
          qDebug() << "[StoryGraph] Follow mode disabled, skipping center";
        }
      } else {
        qWarning() << "[StoryGraph] Current node" << nodeId
                   << "found but no longer valid in scene!";
      }
    } else {
      qDebug() << "[StoryGraph] Warning: Current node" << nodeId
               << "not found in graph (may not be loaded yet)";
    }
  } else {
    qDebug() << "[StoryGraph] Clearing current node (empty nodeId)";
    m_pendingCenterNode.clear(); // Clear any pending operation
  }
}

void NMStoryGraphPanel::createNode(const QString &nodeType) {
  if (!m_scene || !m_view)
    return;

  // Get center of visible area
  QPointF centerPos = m_view->mapToScene(m_view->viewport()->rect().center());

  QString effectiveType = nodeType;
  if (nodeType.compare("Entry", Qt::CaseInsensitive) == 0) {
    effectiveType = "Scene";
    m_markNextNodeAsEntry = true;
  }

  NMUndoManager::instance().pushCommand(
      new CreateGraphNodeCommand(m_scene, effectiveType, centerPos));
}

void NMStoryGraphPanel::onNodeTypeSelected(const QString &nodeType) {
  createNode(nodeType);
}

void NMStoryGraphPanel::onRequestConnection(uint64_t fromNodeId,
                                            uint64_t toNodeId) {
  if (!m_scene || fromNodeId == 0 || toNodeId == 0 || fromNodeId == toNodeId)
    return;

  if (m_scene->hasConnection(fromNodeId, toNodeId)) {
    return;
  }

  // Check if this connection would create a cycle
  if (m_scene->wouldCreateCycle(fromNodeId, toNodeId)) {
    auto *fromNode = findNodeById(fromNodeId);
    auto *toNode = findNodeById(toNodeId);

    QString fromName =
        fromNode ? fromNode->title() : QString::number(fromNodeId);
    QString toName = toNode ? toNode->title() : QString::number(toNodeId);

    QString message =
        tr("Cannot create connection: Adding connection from "
           "'%1' to '%2' would create a cycle in the graph.")
            .arg(fromName, toName);

    // Report to diagnostics system
    ErrorReporter::instance().reportGraphError(
        message.toStdString(),
        QString("Connection: %1 -> %2").arg(fromName, toName).toStdString());

    // Show user feedback
    NMMessageDialog::showWarning(this, tr("Cycle Detected"), message);
    return;
  }

  NMUndoManager::instance().pushCommand(
      new ConnectGraphNodesCommand(m_scene, fromNodeId, toNodeId));
}

void NMStoryGraphPanel::applyNodePropertyChange(const QString &nodeIdString,
                                                const QString &propertyName,
                                                const QString &newValue) {
  property_manager::applyNodePropertyChange(this, nodeIdString, propertyName,
                                            newValue);

  if (!m_isRebuilding) {
    auto *node = findNodeByIdString(nodeIdString);
    if (node) {
      LayoutNode layout = detail::buildLayoutFromNode(node);
      m_layoutNodes.insert(nodeIdString, layout);
      detail::saveGraphLayout(m_layoutNodes, m_layoutEntryScene);
    }
  }
}

void NMStoryGraphPanel::onDeleteSelected() {
  if (!m_scene)
    return;

  const auto selected = m_scene->selectedItems();
  QSet<uint64_t> nodesToDelete;
  QList<NMGraphConnectionItem *> connectionsToDelete;
  QHash<uint64_t, QString> scriptFilesToDelete;

  for (auto *item : selected) {
    if (auto *node = qgraphicsitem_cast<NMGraphNodeItem *>(item)) {
      nodesToDelete.insert(node->nodeId());
      const QString scriptPath = detail::resolveScriptPath(node);
      if (!scriptPath.isEmpty()) {
        QFileInfo info(scriptPath);
        if (info.exists() && info.baseName() == node->nodeIdString()) {
          scriptFilesToDelete.insert(node->nodeId(), info.absoluteFilePath());
        }
      }
    } else if (auto *conn = qgraphicsitem_cast<NMGraphConnectionItem *>(item)) {
      connectionsToDelete.append(conn);
    }
  }

  // Delete connections not covered by node deletion
  for (auto *conn : connectionsToDelete) {
    if (!conn || !conn->startNode() || !conn->endNode()) {
      continue;
    }
    const auto fromId = conn->startNode()->nodeId();
    const auto toId = conn->endNode()->nodeId();
    if (nodesToDelete.contains(fromId) || nodesToDelete.contains(toId)) {
      continue; // Will be handled by node deletion
    }
    NMUndoManager::instance().pushCommand(
        new DisconnectGraphNodesCommand(m_scene, fromId, toId));
  }

  for (auto nodeId : nodesToDelete) {
    NMUndoManager::instance().pushCommand(
        new DeleteGraphNodeCommand(m_scene, nodeId));
    if (scriptFilesToDelete.contains(nodeId)) {
      QFile::remove(scriptFilesToDelete.value(nodeId));
    }
  }
}

void NMStoryGraphPanel::onNodesMoved(const QVector<GraphNodeMove> &moves) {
  if (!m_scene || moves.isEmpty()) {
    return;
  }
  NMUndoManager::instance().pushCommand(
      new MoveGraphNodesCommand(m_scene, moves));

  if (m_isRebuilding) {
    return;
  }

  for (const auto &move : moves) {
    if (auto *node = findNodeById(move.nodeId)) {
      m_layoutNodes.insert(node->nodeIdString(),
                           detail::buildLayoutFromNode(node));
    }
  }
  detail::saveGraphLayout(m_layoutNodes, m_layoutEntryScene);
}

void NMStoryGraphPanel::onEntryNodeRequested(const QString &nodeIdString) {
  if (!m_scene || nodeIdString.isEmpty()) {
    return;
  }

  m_layoutEntryScene = nodeIdString;
  ProjectManager::instance().setStartScene(nodeIdString.toStdString());

  for (auto *item : m_scene->items()) {
    if (auto *node = qgraphicsitem_cast<NMGraphNodeItem *>(item)) {
      node->setEntry(!m_layoutEntryScene.isEmpty() &&
                     node->nodeIdString() == m_layoutEntryScene);
    }
  }

  detail::saveGraphLayout(m_layoutNodes, m_layoutEntryScene);
}

void NMStoryGraphPanel::onLocalePreviewChanged(int index) {
  if (!m_localePreviewSelector || !m_scene) {
    return;
  }

  m_currentPreviewLocale = m_localePreviewSelector->itemData(index).toString();
  emit localePreviewChanged(m_currentPreviewLocale);

  // Update dialogue nodes to show translated text or highlight missing
  for (auto *item : m_scene->items()) {
    if (auto *node = qgraphicsitem_cast<NMGraphNodeItem *>(item)) {
      if (node->isDialogueNode()) {
        // If source locale is selected, show original text
        if (m_currentPreviewLocale.isEmpty()) {
          node->setLocalizedText(node->dialogueText());
          node->setTranslationStatus(2); // Translated (showing source)
        } else {
          // For other locales, would query LocalizationManager
          // For now, mark as untranslated if different from source
          node->setTranslationStatus(1); // Untranslated
          node->setLocalizedText(QString("[%1]").arg(node->localizationKey()));
        }
        node->update();
      }
    }
  }
}

void NMStoryGraphPanel::onExportDialogueClicked() {
  property_manager::handleExportDialogue(this);
  emit dialogueExportRequested(m_layoutEntryScene);
}

void NMStoryGraphPanel::onGenerateLocalizationKeysClicked() {
  property_manager::handleGenerateLocalizationKeys(this);

  if (!m_isRebuilding) {
    // Save layout with new keys
    for (auto *item : m_scene->items()) {
      if (auto *node = qgraphicsitem_cast<NMGraphNodeItem *>(item)) {
        m_layoutNodes.insert(node->nodeIdString(),
                             detail::buildLayoutFromNode(node));
      }
    }
    detail::saveGraphLayout(m_layoutNodes, m_layoutEntryScene);
  }
}

void NMStoryGraphPanel::onSyncGraphToScript() {
  serialization::syncGraphToScript(this, this);
}

void NMStoryGraphPanel::onSyncScriptToGraph() {
  // Set rebuilding flag before delegating to serialization module
  m_isRebuilding = true;
  m_layoutNodes.clear();
  m_nodeIdToString.clear();

  serialization::syncScriptToGraph(this, this);

  // Update internal state after sync
  if (m_scene) {
    for (auto *node : m_scene->nodes()) {
      m_nodeIdToString.insert(node->nodeId(), node->nodeIdString());
      m_layoutNodes.insert(node->nodeIdString(),
                           detail::buildLayoutFromNode(node));
    }

    // Find entry scene
    for (auto *node : m_scene->nodes()) {
      if (node->isEntry()) {
        m_layoutEntryScene = node->nodeIdString();
        ProjectManager::instance().setStartScene(
            m_layoutEntryScene.toStdString());
        break;
      }
    }

    detail::saveGraphLayout(m_layoutNodes, m_layoutEntryScene);
  }

  m_isRebuilding = false;
}

void NMStoryGraphPanel::onScriptFileCreationFailed(
    uint64_t nodeId, const QString &nodeIdString,
    const QString &errorMessage) {
  // Show error dialog to notify the user
  const QString title = tr("Script File Creation Failed");
  const QString message =
      tr("Failed to create script file for node '%1' (ID: %2).\n\n"
         "Error: %3\n\n"
         "Possible causes:\n"
         "• Insufficient disk space\n"
         "• Permission denied for the Scripts folder\n"
         "• Invalid characters in node name\n"
         "• Directory does not exist\n\n"
         "The node has been marked with an error indicator. "
         "You can hover over it to see the error details.")
          .arg(nodeIdString)
          .arg(nodeId)
          .arg(errorMessage);

  qWarning() << "[StoryGraphPanel] Script file creation failed for node"
             << nodeIdString << ":" << errorMessage;

  NMMessageDialog::showError(this, title, message);
}

} // namespace NovelMind::editor::qt
