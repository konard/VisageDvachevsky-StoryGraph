#include "NovelMind/editor/qt/panels/nm_story_graph_panel.hpp"
#include "NovelMind/editor/qt/nm_play_mode_controller.hpp"
#include "NovelMind/editor/qt/nm_undo_manager.hpp"
#include "NovelMind/editor/project_manager.hpp"
#include "NovelMind/editor/error_reporter.hpp"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QQueue>
#include <QSet>
#include <QTextStream>

#include "nm_story_graph_panel_detail.hpp"

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
  QMessageBox msgBox;
  msgBox.setIcon(QMessageBox::Question);
  msgBox.setWindowTitle(tr("Auto Layout"));
  msgBox.setText(tr("This will automatically arrange all nodes in a hierarchical layout."));
  msgBox.setInformativeText(tr("Current manual positioning will be lost. Do you want to continue?"));
  msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
  msgBox.setDefaultButton(QMessageBox::No);

  if (msgBox.exec() != QMessageBox::Yes) {
    return;
  }

  // Simple hierarchical layout algorithm
  // 1. Find entry nodes (nodes with no incoming connections or marked as entry)
  // 2. Assign layers using breadth-first traversal
  // 3. Position nodes in each layer horizontally with spacing

  const qreal horizontalSpacing = 250.0;
  const qreal verticalSpacing = 150.0;
  const qreal startX = 100.0;
  const qreal startY = 100.0;

  // Build adjacency list
  QHash<uint64_t, QList<uint64_t>> adjacencyList;
  QHash<uint64_t, int> inDegree;

  for (auto *node : nodes) {
    adjacencyList[node->nodeId()] = QList<uint64_t>();
    inDegree[node->nodeId()] = 0;
  }

  for (auto *conn : m_scene->connections()) {
    uint64_t fromId = conn->startNode()->nodeId();
    uint64_t toId = conn->endNode()->nodeId();
    adjacencyList[fromId].append(toId);
    inDegree[toId]++;
  }

  // Find entry nodes
  QList<uint64_t> entryNodes;
  for (auto *node : nodes) {
    if (inDegree[node->nodeId()] == 0 || node->isEntry()) {
      entryNodes.append(node->nodeId());
    }
  }

  if (entryNodes.isEmpty() && !nodes.isEmpty()) {
    // If no entry nodes found, use the first node
    entryNodes.append(nodes.first()->nodeId());
  }

  // BFS to assign layers
  QHash<uint64_t, int> nodeLayers;
  QHash<int, QList<uint64_t>> layerNodes;
  QQueue<QPair<uint64_t, int>> queue;
  QSet<uint64_t> visited;

  for (uint64_t entryId : entryNodes) {
    queue.enqueue(qMakePair(entryId, 0));
  }

  while (!queue.isEmpty()) {
    auto [nodeId, layer] = queue.dequeue();

    if (visited.contains(nodeId)) {
      continue;
    }
    visited.insert(nodeId);

    nodeLayers[nodeId] = layer;
    layerNodes[layer].append(nodeId);

    for (uint64_t childId : adjacencyList[nodeId]) {
      if (!visited.contains(childId)) {
        queue.enqueue(qMakePair(childId, layer + 1));
      }
    }
  }

  // Position unvisited nodes (orphaned nodes)
  int maxLayer = 0;
  for (int layer : nodeLayers.values()) {
    if (layer > maxLayer) {
      maxLayer = layer;
    }
  }

  for (auto *node : nodes) {
    if (!visited.contains(node->nodeId())) {
      maxLayer++;
      nodeLayers[node->nodeId()] = maxLayer;
      layerNodes[maxLayer].append(node->nodeId());
    }
  }

  // Position nodes
  for (int layer : layerNodes.keys()) {
    const auto &nodesInLayer = layerNodes[layer];
    qreal y = startY + layer * verticalSpacing;
    qreal totalWidth = static_cast<qreal>(nodesInLayer.size() - 1) * horizontalSpacing;
    qreal x = startX - totalWidth / 2.0;

    for (int i = 0; i < nodesInLayer.size(); ++i) {
      uint64_t nodeId = nodesInLayer[i];
      auto *node = m_scene->findNode(nodeId);
      if (node) {
        node->setPos(x + i * horizontalSpacing, y);
      }
    }
  }

  // Update all connection paths
  for (auto *conn : m_scene->connections()) {
    conn->updatePath();
  }

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
  auto *node = findNodeById(nodeId);
  if (!node) {
    return;
  }
  m_nodeIdToString.insert(nodeId, node->nodeIdString());

  emit nodeSelected(node->nodeIdString());

  if (!node->scriptPath().isEmpty()) {
    emit scriptNodeRequested(node->scriptPath());
  }
}

void NMStoryGraphPanel::onNodeDoubleClicked(uint64_t nodeId) {
  auto *node = findNodeById(nodeId);
  if (!node) {
    return;
  }

  if (m_scene) {
    m_scene->clearSelection();
  }
  node->setSelected(true);
  if (m_view) {
    m_view->centerOn(node);
  }

  emit nodeSelected(node->nodeIdString());
  emit nodeActivated(node->nodeIdString());

  // Scene Node specific: emit signal to open Scene View
  if (node->isSceneNode()) {
    const QString sceneId = node->sceneId().isEmpty()
                                ? node->nodeIdString()
                                : node->sceneId();
    qDebug() << "[StoryGraph] Scene node double-clicked, emitting sceneNodeDoubleClicked:"
             << sceneId;
    emit sceneNodeDoubleClicked(sceneId);
  }
}

void NMStoryGraphPanel::onNodeAdded(uint64_t nodeId,
                                    const QString &nodeIdString,
                                    const QString &nodeType) {
  Q_UNUSED(nodeIdString);
  Q_UNUSED(nodeType);
  if (m_isRebuilding) {
    return;
  }
  auto *node = findNodeById(nodeId);
  if (!node) {
    return;
  }

  if (m_scene) {
    m_scene->clearSelection();
  }
  node->setSelected(true);
  if (m_view) {
    m_view->centerOn(node);
  }
  emit nodeSelected(node->nodeIdString());

  if (!node->scriptPath().isEmpty()) {
    emit scriptNodeRequested(node->scriptPath());
  }

  LayoutNode layout = detail::buildLayoutFromNode(node);
  if ((nodeType.contains("Dialogue", Qt::CaseInsensitive) ||
       nodeType.contains("Choice", Qt::CaseInsensitive)) &&
      layout.speaker.isEmpty()) {
    layout.speaker = "Narrator";
    node->setDialogueSpeaker(layout.speaker);
  }
  m_layoutNodes.insert(node->nodeIdString(), layout);
  detail::saveGraphLayout(m_layoutNodes, m_layoutEntryScene);

  if (m_markNextNodeAsEntry) {
    m_markNextNodeAsEntry = false;
    onEntryNodeRequested(node->nodeIdString());
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
  auto *from = findNodeById(fromNodeId);
  auto *to = findNodeById(toNodeId);
  if (!from || !to) {
    return;
  }

  QStringList targets;
  for (auto *conn : m_scene->connections()) {
    if (!conn || !conn->startNode() || !conn->endNode()) {
      continue;
    }
    if (conn->startNode()->nodeId() == fromNodeId) {
      targets << conn->endNode()->nodeIdString();
    }
  }

  detail::updateSceneGraphBlock(from->nodeIdString(),
                                detail::resolveScriptPath(from), targets);
}

void NMStoryGraphPanel::onConnectionDeleted(uint64_t fromNodeId,
                                            uint64_t toNodeId) {
  Q_UNUSED(toNodeId);
  if (m_isRebuilding) {
    return;
  }
  auto *from = findNodeById(fromNodeId);
  if (!from) {
    return;
  }

  QStringList targets;
  for (auto *conn : m_scene->connections()) {
    if (!conn || !conn->startNode() || !conn->endNode()) {
      continue;
    }
    if (conn->startNode()->nodeId() == fromNodeId) {
      targets << conn->endNode()->nodeIdString();
    }
  }

  detail::updateSceneGraphBlock(from->nodeIdString(),
                                detail::resolveScriptPath(from), targets);
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
      if (prevNode->scene() == m_scene && m_scene->items().contains(prevNode)) {
        qDebug() << "[StoryGraph] Clearing execution state on"
                 << m_currentExecutingNode;
        prevNode->setCurrentlyExecuting(false);
      } else {
        qWarning() << "[StoryGraph] Previous node" << m_currentExecutingNode
                   << "found but no longer valid in scene!";
      }
    } else {
      qDebug() << "[StoryGraph] Warning: Previous node" << m_currentExecutingNode
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

        // Center view on executing node with safety check
        if (m_view && !m_view->isHidden()) {
          qDebug() << "[StoryGraph] Centering view on" << nodeId;
          m_view->centerOn(currentNode);
        } else {
          qWarning() << "[StoryGraph] View is null or hidden, cannot center!";
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

    QString fromName = fromNode ? fromNode->title() : QString::number(fromNodeId);
    QString toName = toNode ? toNode->title() : QString::number(toNodeId);

    QString message = tr("Cannot create connection: Adding connection from '%1' to '%2' would create a cycle in the graph.")
                        .arg(fromName, toName);

    // Report to diagnostics system
    ErrorReporter::instance().reportGraphError(
      message.toStdString(),
      QString("Connection: %1 -> %2").arg(fromName, toName).toStdString()
    );

    // Show user feedback
    QMessageBox::warning(this, tr("Cycle Detected"), message);
    return;
  }

  NMUndoManager::instance().pushCommand(
      new ConnectGraphNodesCommand(m_scene, fromNodeId, toNodeId));
}

void NMStoryGraphPanel::applyNodePropertyChange(const QString &nodeIdString,
                                                const QString &propertyName,
                                                const QString &newValue) {
  auto *node = findNodeByIdString(nodeIdString);
  if (!node) {
    return;
  }

  if (propertyName == "title") {
    node->setTitle(newValue);
  } else if (propertyName == "type") {
    node->setNodeType(newValue);
  } else if (propertyName == "scriptPath") {
    node->setScriptPath(newValue);
    QString scriptPath = newValue;
    if (QFileInfo(newValue).isRelative()) {
      scriptPath = QString::fromStdString(
          ProjectManager::instance().toAbsolutePath(newValue.toStdString()));
    }
    QFile scriptFile(scriptPath);
    if (!scriptPath.isEmpty() && !scriptFile.exists()) {
      if (scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&scriptFile);
        out << "// " << node->nodeIdString() << "\n";
        out << "scene " << node->nodeIdString() << " {\n";
        out << "  say Narrator \"New script node\"\n";
        out << "}\n";
      }
    }
  } else if (propertyName == "speaker") {
    node->setDialogueSpeaker(newValue);
  } else if (propertyName == "text") {
    node->setDialogueText(newValue);
  } else if (propertyName == "choices") {
    node->setChoiceOptions(detail::splitChoiceLines(newValue));
  }

  if (!m_isRebuilding) {
    LayoutNode layout = detail::buildLayoutFromNode(node);
    m_layoutNodes.insert(nodeIdString, layout);
    detail::saveGraphLayout(m_layoutNodes, m_layoutEntryScene);
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

} // namespace NovelMind::editor::qt
