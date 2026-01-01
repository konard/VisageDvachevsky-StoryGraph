#include "NovelMind/editor/error_reporter.hpp"
#include "NovelMind/editor/project_manager.hpp"
#include "NovelMind/editor/qt/nm_dialogs.hpp"
#include "NovelMind/editor/qt/nm_play_mode_controller.hpp"
#include "NovelMind/editor/qt/nm_undo_manager.hpp"
#include "NovelMind/editor/qt/panels/nm_story_graph_panel.hpp"

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProgressDialog>
#include <QQueue>
#include <QSet>
#include <QTextStream>
#include <QThread>
#include <atomic>

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
  auto result = NMMessageDialog::showQuestion(
      this, tr("Auto Layout"),
      tr("This will automatically arrange all nodes in a hierarchical "
         "layout.\n\n"
         "Current manual positioning will be lost. Do you want to continue?"),
      {NMDialogButton::Yes, NMDialogButton::No}, NMDialogButton::No);

  if (result != NMDialogButton::Yes) {
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
    qreal totalWidth =
        static_cast<qreal>(nodesInLayer.size() - 1) * horizontalSpacing;
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

  // Single click should only select the node, not open Script Editor
  // Script Editor should only open on double-click (handled in
  // onNodeDoubleClicked)
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
    const QString sceneId =
        node->sceneId().isEmpty() ? node->nodeIdString() : node->sceneId();
    qDebug() << "[StoryGraph] Scene node double-clicked, emitting "
                "sceneNodeDoubleClicked:"
             << sceneId;
    emit sceneNodeDoubleClicked(sceneId);
  } else {
    // For non-Scene nodes, double-click should open Script Editor
    if (!node->scriptPath().isEmpty()) {
      emit scriptNodeRequested(node->scriptPath());
    }
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

  // Creating a node should keep focus in Story Graph panel
  // Script Editor should only open on explicit double-click

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

  // Collect all connections from this source node
  QList<NMGraphConnectionItem *> outgoingConns;
  for (auto *conn : m_scene->connections()) {
    if (!conn || !conn->startNode() || !conn->endNode()) {
      continue;
    }
    if (conn->startNode()->nodeId() == fromNodeId) {
      outgoingConns.append(conn);
    }
  }

  // Update connection labels for Choice nodes
  if (from->isChoiceNode()) {
    const QStringList choices = from->choiceOptions();
    QHash<QString, QString> newTargets;

    for (int i = 0; i < outgoingConns.size(); ++i) {
      auto *conn = outgoingConns[i];
      QString label;
      if (i < choices.size()) {
        label = choices[i];
        // Update the mapping
        newTargets.insert(label, conn->endNode()->nodeIdString());
      } else {
        label = QString("Option %1").arg(i + 1);
      }
      conn->setLabel(label);
      conn->setBranchIndex(i);
      conn->update();
    }

    // Update the node's choice targets mapping
    from->setChoiceTargets(newTargets);
  }

  // Update connection labels for Condition nodes
  if (from->isConditionNode()) {
    QStringList outputs = from->conditionOutputs();
    if (outputs.isEmpty()) {
      outputs << "true" << "false"; // Default outputs
    }

    QHash<QString, QString> newTargets;
    for (int i = 0; i < outgoingConns.size(); ++i) {
      auto *conn = outgoingConns[i];
      QString label;
      if (i < outputs.size()) {
        label = outputs[i];
        newTargets.insert(label, conn->endNode()->nodeIdString());
      } else {
        label = QString("branch_%1").arg(i + 1);
      }
      conn->setLabel(label);
      conn->setBranchIndex(i);
      conn->update();
    }

    // Update the node's condition targets mapping
    from->setConditionTargets(newTargets);
  }

  // Build targets list for script update
  QStringList targets;
  for (auto *conn : outgoingConns) {
    targets << conn->endNode()->nodeIdString();
  }

  detail::updateSceneGraphBlock(from->nodeIdString(),
                                detail::resolveScriptPath(from), targets);

  // Save the updated layout
  if (!m_isRebuilding) {
    LayoutNode layout = detail::buildLayoutFromNode(from);
    m_layoutNodes.insert(from->nodeIdString(), layout);
    detail::saveGraphLayout(m_layoutNodes, m_layoutEntryScene);
  }
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

  // Collect remaining connections from this source node
  QList<NMGraphConnectionItem *> outgoingConns;
  for (auto *conn : m_scene->connections()) {
    if (!conn || !conn->startNode() || !conn->endNode()) {
      continue;
    }
    if (conn->startNode()->nodeId() == fromNodeId) {
      outgoingConns.append(conn);
    }
  }

  // Update connection labels for Choice nodes
  if (from->isChoiceNode()) {
    const QStringList choices = from->choiceOptions();
    QHash<QString, QString> newTargets;

    for (int i = 0; i < outgoingConns.size(); ++i) {
      auto *conn = outgoingConns[i];
      QString label;
      if (i < choices.size()) {
        label = choices[i];
        newTargets.insert(label, conn->endNode()->nodeIdString());
      } else {
        label = QString("Option %1").arg(i + 1);
      }
      conn->setLabel(label);
      conn->setBranchIndex(i);
      conn->update();
    }

    from->setChoiceTargets(newTargets);
  }

  // Update connection labels for Condition nodes
  if (from->isConditionNode()) {
    QStringList outputs = from->conditionOutputs();
    if (outputs.isEmpty()) {
      outputs << "true" << "false";
    }

    QHash<QString, QString> newTargets;
    for (int i = 0; i < outgoingConns.size(); ++i) {
      auto *conn = outgoingConns[i];
      QString label;
      if (i < outputs.size()) {
        label = outputs[i];
        newTargets.insert(label, conn->endNode()->nodeIdString());
      } else {
        label = QString("branch_%1").arg(i + 1);
      }
      conn->setLabel(label);
      conn->setBranchIndex(i);
      conn->update();
    }

    from->setConditionTargets(newTargets);
  }

  // Build targets list for script update
  QStringList targets;
  for (auto *conn : outgoingConns) {
    targets << conn->endNode()->nodeIdString();
  }

  detail::updateSceneGraphBlock(from->nodeIdString(),
                                detail::resolveScriptPath(from), targets);

  // Save the updated layout
  if (!m_isRebuilding) {
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
      if (prevNode->scene() == m_scene && m_scene->items().contains(prevNode)) {
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

    QString fromName =
        fromNode ? fromNode->title() : QString::number(fromNodeId);
    QString toName = toNode ? toNode->title() : QString::number(toNodeId);

    QString message = tr("Cannot create connection: Adding connection from "
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
        out << "// ========================================\n";
        out << "// Generated from Story Graph\n";
        out << "// Do not edit manually - changes may be overwritten\n";
        out << "// ========================================\n";
        out << "// " << node->nodeIdString() << "\n";
        out << "scene " << node->nodeIdString() << " {\n";
        // Condition and Scene nodes are "silent" - they only handle branching,
        // not dialogue. Only Dialogue nodes get a default say statement.
        // This fixes issue #76 where Condition nodes incorrectly said text.
        if (node->isConditionNode()) {
          out << "  // Condition node - add branching logic here\n";
        } else if (node->isSceneNode()) {
          out << "  // Scene node - add scene content here\n";
        } else {
          out << "  say Narrator \"New script node\"\n";
        }
        out << "}\n";
      }
    }
  } else if (propertyName == "speaker") {
    node->setDialogueSpeaker(newValue);
    // Sync speaker change to the .nms script file
    const QString scriptPath = detail::resolveScriptPath(node);
    if (!scriptPath.isEmpty()) {
      detail::updateSceneSayStatement(node->nodeIdString(), scriptPath,
                                      newValue, node->dialogueText());
    }
  } else if (propertyName == "text") {
    node->setDialogueText(newValue);
    // Sync text change to the .nms script file
    const QString scriptPath = detail::resolveScriptPath(node);
    if (!scriptPath.isEmpty()) {
      detail::updateSceneSayStatement(node->nodeIdString(), scriptPath,
                                      node->dialogueSpeaker(), newValue);
    }
  } else if (propertyName == "choices") {
    node->setChoiceOptions(detail::splitChoiceLines(newValue));
  } else if (propertyName == "conditionExpression") {
    node->setConditionExpression(newValue);
  } else if (propertyName == "conditionOutputs") {
    node->setConditionOutputs(detail::splitChoiceLines(newValue));
  } else if (propertyName == "choiceTargets") {
    // Parse "OptionText=TargetNodeId" format
    QHash<QString, QString> targets;
    const QStringList lines = detail::splitChoiceLines(newValue);
    for (const QString &line : lines) {
      int eqPos = line.indexOf('=');
      if (eqPos > 0) {
        QString option = line.left(eqPos).trimmed();
        QString target = line.mid(eqPos + 1).trimmed();
        if (!option.isEmpty()) {
          targets.insert(option, target);
        }
      }
    }
    node->setChoiceTargets(targets);
  } else if (propertyName == "conditionTargets") {
    // Parse "OutputLabel=TargetNodeId" format
    QHash<QString, QString> targets;
    const QStringList lines = detail::splitChoiceLines(newValue);
    for (const QString &line : lines) {
      int eqPos = line.indexOf('=');
      if (eqPos > 0) {
        QString output = line.left(eqPos).trimmed();
        QString target = line.mid(eqPos + 1).trimmed();
        if (!output.isEmpty()) {
          targets.insert(output, target);
        }
      }
    }
    node->setConditionTargets(targets);
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
  if (!m_scene) {
    return;
  }

  // Collect all dialogue nodes and their text
  QStringList dialogueEntries;
  for (auto *item : m_scene->items()) {
    if (auto *node = qgraphicsitem_cast<NMGraphNodeItem *>(item)) {
      if (node->isDialogueNode() && !node->localizationKey().isEmpty()) {
        // Format: key,speaker,text
        QString line = QString("\"%1\",\"%2\",\"%3\"")
                           .arg(node->localizationKey())
                           .arg(node->dialogueSpeaker())
                           .arg(node->dialogueText().replace("\"", "\"\""));
        dialogueEntries.append(line);
      }
    }
  }

  if (dialogueEntries.isEmpty()) {
    qDebug() << "[StoryGraph] No dialogue entries to export";
    return;
  }

  // Emit signal for the localization panel to handle the actual export
  emit dialogueExportRequested(m_layoutEntryScene);

  qDebug() << "[StoryGraph] Exported" << dialogueEntries.size()
           << "dialogue entries";
}

void NMStoryGraphPanel::onGenerateLocalizationKeysClicked() {
  if (!m_scene) {
    return;
  }

  int keysGenerated = 0;

  for (auto *item : m_scene->items()) {
    if (auto *node = qgraphicsitem_cast<NMGraphNodeItem *>(item)) {
      // Generate keys for dialogue nodes that don't have one
      if (node->isDialogueNode() && node->localizationKey().isEmpty()) {
        // Generate key in format: scene.{sceneId}.dialogue.{nodeId}
        QString sceneId =
            node->sceneId().isEmpty() ? node->nodeIdString() : node->sceneId();
        QString key =
            QString("scene.%1.dialogue.%2").arg(sceneId).arg(node->nodeId());
        node->setLocalizationKey(key);
        ++keysGenerated;
      }

      // Generate keys for choice nodes
      if (node->nodeType().compare("Choice", Qt::CaseInsensitive) == 0) {
        QString sceneId =
            node->sceneId().isEmpty() ? node->nodeIdString() : node->sceneId();
        const QStringList &options = node->choiceOptions();
        for (int i = 0; i < options.size(); ++i) {
          // Each choice option gets its own key
          // Keys are stored as a property on the node
          QString key = QString("scene.%1.choice.%2.%3")
                            .arg(sceneId)
                            .arg(node->nodeId())
                            .arg(i);
          // Store choice keys - would need to extend node to store multiple
          // keys
          ++keysGenerated;
        }
      }

      node->update();
    }
  }

  qDebug() << "[StoryGraph] Generated" << keysGenerated << "localization keys";

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

// Helper struct for sync item data (Issue #96: async sync)
struct SyncItem {
  QString sceneId;
  QString scriptPath;
  QString speaker;
  QString dialogueText;
};

// Result struct for async sync operation
struct SyncResult {
  int nodesSynced = 0;
  int nodesSkipped = 0;
  QStringList syncErrors;
};

/**
 * @brief Worker class for async sync operation (Issue #96)
 *
 * This worker runs in a separate thread to perform file I/O operations
 * without blocking the UI thread.
 */
class SyncToScriptWorker : public QObject {
  Q_OBJECT

public:
  SyncToScriptWorker(QList<SyncItem> items,
                     std::shared_ptr<std::atomic<bool>> cancelled)
      : m_items(std::move(items)), m_cancelled(std::move(cancelled)) {}

public slots:
  void process() {
    SyncResult result;

    for (int i = 0; i < m_items.size(); ++i) {
      // Check for cancellation
      if (m_cancelled->load()) {
        result.syncErrors << tr("Operation cancelled by user");
        break;
      }

      const auto &item = m_items.at(i);

      // Perform the actual sync (file I/O in background thread)
      bool success = detail::updateSceneSayStatement(
          item.sceneId, item.scriptPath, item.speaker, item.dialogueText);

      if (success) {
        ++result.nodesSynced;
      } else {
        result.syncErrors << tr("Failed to sync node '%1' to '%2'")
                                 .arg(item.sceneId, item.scriptPath);
      }

      // Report progress
      emit progressUpdated(i + 1, static_cast<int>(m_items.size()));
    }

    emit finished(result);
  }

signals:
  void progressUpdated(int current, int total);
  void finished(const SyncResult &result);

private:
  QList<SyncItem> m_items;
  std::shared_ptr<std::atomic<bool>> m_cancelled;
};

void NMStoryGraphPanel::onSyncGraphToScript() {
  // Issue #82: Sync Story Graph data to NMScript files
  // Issue #96: Run sync asynchronously to prevent UI freeze
  // This ensures that visual edits made in the Story Graph are persisted
  // to the authoritative NMScript source files without blocking the UI.

  if (!m_scene) {
    return;
  }

  // Collect sync items from UI thread (fast operation)
  QList<SyncItem> syncItems;
  int nodesSkipped = 0;

  for (auto *item : m_scene->items()) {
    auto *node = qgraphicsitem_cast<NMGraphNodeItem *>(item);
    if (!node) {
      continue;
    }

    const QString sceneId = node->nodeIdString();
    const QString scriptPath = detail::resolveScriptPath(node);

    if (scriptPath.isEmpty()) {
      // Node has no associated script file - skip
      ++nodesSkipped;
      continue;
    }

    // Sync speaker and dialogue text to script
    const QString speaker = node->dialogueSpeaker();
    const QString dialogueText = node->dialogueText();

    // Only sync if we have meaningful content (not default placeholder)
    if (dialogueText.isEmpty() || dialogueText.trimmed() == "New scene") {
      ++nodesSkipped;
      continue;
    }

    syncItems.append({sceneId, scriptPath, speaker, dialogueText});
  }

  // If nothing to sync, show message immediately
  if (syncItems.isEmpty()) {
    QString message = tr("No nodes needed synchronization.\n"
                         "(%1 node(s) skipped - no script or empty content)")
                          .arg(nodesSkipped);
    NMMessageDialog::showInfo(this, tr("Sync Graph to Script"), message);
    return;
  }

  // Create progress dialog (Issue #96: keep UI responsive)
  auto *progressDialog = new QProgressDialog(
      tr("Synchronizing nodes to scripts..."), tr("Cancel"), 0,
      static_cast<int>(syncItems.size()), this);
  progressDialog->setWindowModality(Qt::WindowModal);
  progressDialog->setMinimumDuration(0); // Show immediately
  progressDialog->setValue(0);
  progressDialog->setAutoClose(false);
  progressDialog->setAutoReset(false);

  // Capture initial skip count for final message
  const int initialSkipped = nodesSkipped;

  // Create cancellation flag (shared with worker thread)
  auto cancelled = std::make_shared<std::atomic<bool>>(false);

  // Create worker thread
  auto *workerThread = new QThread(this);
  auto *worker = new SyncToScriptWorker(syncItems, cancelled);
  worker->moveToThread(workerThread);

  // Connect cancel button to set cancellation flag
  connect(progressDialog, &QProgressDialog::canceled, this,
          [cancelled, progressDialog]() {
            cancelled->store(true);
            progressDialog->setLabelText(tr("Cancelling..."));
          });

  // Connect signals for progress updates
  connect(worker, &SyncToScriptWorker::progressUpdated, progressDialog,
          [progressDialog](int current, int total) {
            progressDialog->setValue(current);
            progressDialog->setLabelText(
                QObject::tr("Synchronizing node %1 of %2...")
                    .arg(current)
                    .arg(total));
          });

  // Connect finished signal to handle results
  connect(
      worker, &SyncToScriptWorker::finished, this,
      [this, progressDialog, workerThread, worker,
       initialSkipped](const SyncResult &result) {
        progressDialog->close();
        progressDialog->deleteLater();

        // Clean up worker and thread
        workerThread->quit();
        workerThread->wait();
        worker->deleteLater();
        workerThread->deleteLater();

        // Report results to user
        QString message;
        const int totalSkipped = initialSkipped + result.nodesSkipped;

        if (result.syncErrors.isEmpty()) {
          if (result.nodesSynced > 0) {
            message = tr("Successfully synchronized %1 node(s) to NMScript "
                         "files.\n"
                         "(%2 node(s) skipped - no script or empty content)")
                          .arg(result.nodesSynced)
                          .arg(totalSkipped);
            qDebug() << "[StoryGraph] Sync to Script:" << result.nodesSynced
                     << "synced," << totalSkipped << "skipped";
          } else {
            message = tr("No nodes needed synchronization.\n"
                         "(%1 node(s) skipped - no script or empty content)")
                          .arg(totalSkipped);
          }
        } else {
          message = tr("Synchronization completed with errors:\n\n%1\n\n"
                       "(%2 node(s) synced, %3 failed)")
                        .arg(result.syncErrors.join("\n"))
                        .arg(result.nodesSynced)
                        .arg(result.syncErrors.size());
          ErrorReporter::instance().reportWarning(message.toStdString());
        }

        // Show notification (non-blocking info dialog)
        NMMessageDialog::showInfo(this, tr("Sync Graph to Script"), message);
      });

  // Connect thread started signal to start worker
  connect(workerThread, &QThread::started, worker,
          &SyncToScriptWorker::process);

  // Start the worker thread
  workerThread->start();
}

void NMStoryGraphPanel::onSyncScriptToGraph() {
  // Issue #127: Sync NMScript files to Story Graph
  // This parses .nms script files and creates/updates graph nodes

  if (!m_scene) {
    NMMessageDialog::showWarning(
        this, tr("Sync Script to Graph"),
        tr("Story Graph scene is not initialized."));
    return;
  }

  // Get the scripts folder path
  const auto &pm = ProjectManager::instance();
  if (!pm.hasOpenProject()) {
    NMMessageDialog::showWarning(this, tr("Sync Script to Graph"),
                                 tr("No project is currently open."));
    return;
  }

  const QString scriptsPath =
      QString::fromStdString(pm.getFolderPath(ProjectFolder::Scripts));

  if (scriptsPath.isEmpty()) {
    NMMessageDialog::showWarning(
        this, tr("Sync Script to Graph"),
        tr("Could not find scripts folder in project."));
    return;
  }

  // Check if scripts directory exists
  QDir scriptsDir(scriptsPath);
  if (!scriptsDir.exists()) {
    NMMessageDialog::showWarning(
        this, tr("Sync Script to Graph"),
        tr("Scripts folder does not exist:\n%1").arg(scriptsPath));
    return;
  }

  // Find all .nms files recursively
  QStringList nmsFiles;
  QDirIterator it(scriptsPath, QStringList() << "*.nms",
                  QDir::Files | QDir::Readable,
                  QDirIterator::Subdirectories);
  while (it.hasNext()) {
    nmsFiles.append(it.next());
  }

  if (nmsFiles.isEmpty()) {
    NMMessageDialog::showInfo(
        this, tr("Sync Script to Graph"),
        tr("No .nms script files found in:\n%1").arg(scriptsPath));
    return;
  }

  // Confirm with user before replacing nodes
  auto result = NMMessageDialog::showQuestion(
      this, tr("Sync Script to Graph"),
      tr("This will parse %1 script file(s) and update the Story Graph.\n\n"
         "Existing graph content will be replaced.\n\n"
         "Do you want to continue?")
          .arg(nmsFiles.size()),
      {NMDialogButton::Yes, NMDialogButton::No}, NMDialogButton::No);

  if (result != NMDialogButton::Yes) {
    return;
  }

  // Parse all script files
  QList<detail::ParsedNode> allNodes;
  QList<QPair<QString, QString>> allEdges;
  QString entryPoint;
  int filesProcessed = 0;
  int parseErrors = 0;
  QStringList errorMessages;

  for (const QString &filePath : nmsFiles) {
    detail::ParseResult parseResult = detail::parseNMScriptFile(filePath);

    if (!parseResult.success) {
      ++parseErrors;
      errorMessages.append(
          QString("%1: %2").arg(QFileInfo(filePath).fileName(),
                                parseResult.errorMessage));
      continue;
    }

    allNodes.append(parseResult.nodes);
    allEdges.append(parseResult.edges);

    // Use first file's entry point if not set
    if (entryPoint.isEmpty() && !parseResult.entryPoint.isEmpty()) {
      entryPoint = parseResult.entryPoint;
    }

    ++filesProcessed;
  }

  if (allNodes.isEmpty()) {
    QString message = tr("No scenes found in script files.");
    if (!errorMessages.isEmpty()) {
      message += tr("\n\nParse errors:\n") + errorMessages.join("\n");
    }
    NMMessageDialog::showWarning(this, tr("Sync Script to Graph"), message);
    return;
  }

  // Clear existing graph
  m_isRebuilding = true;
  m_scene->clearGraph();
  m_layoutNodes.clear();
  m_nodeIdToString.clear();

  // Create nodes from parsed data
  QHash<QString, NMGraphNodeItem *> nodeMap;
  int col = 0;
  int row = 0;
  const qreal horizontalSpacing = 260.0;
  const qreal verticalSpacing = 140.0;

  for (const detail::ParsedNode &parsedNode : allNodes) {
    // Calculate default position (grid layout)
    const QPointF defaultPos(col * horizontalSpacing, row * verticalSpacing);
    ++col;
    if (col >= 4) {
      col = 0;
      ++row;
    }

    // Create node in the scene
    QString nodeType = parsedNode.type;
    if (nodeType.isEmpty()) {
      nodeType = "Scene";
    }

    NMGraphNodeItem *node =
        m_scene->addNode(parsedNode.id, nodeType, defaultPos, 0, parsedNode.id);

    if (!node) {
      continue;
    }

    // Set node properties
    node->setSceneId(parsedNode.id);

    if (!parsedNode.speaker.isEmpty()) {
      node->setDialogueSpeaker(parsedNode.speaker);
    }
    if (!parsedNode.text.isEmpty()) {
      node->setDialogueText(parsedNode.text);
    }
    if (!parsedNode.choices.isEmpty()) {
      node->setChoiceOptions(parsedNode.choices);

      // Build choice targets mapping
      QHash<QString, QString> choiceTargets;
      for (int i = 0; i < parsedNode.choices.size() &&
                      i < parsedNode.targets.size();
           ++i) {
        choiceTargets.insert(parsedNode.choices[i], parsedNode.targets[i]);
      }
      node->setChoiceTargets(choiceTargets);
    }
    if (!parsedNode.conditionExpr.isEmpty()) {
      node->setConditionExpression(parsedNode.conditionExpr);
      node->setConditionOutputs(parsedNode.conditionOutputs);
    }

    // Mark entry node
    if (parsedNode.id == entryPoint) {
      node->setEntry(true);
    }

    nodeMap.insert(parsedNode.id, node);
    m_nodeIdToString.insert(node->nodeId(), node->nodeIdString());

    // Save layout node
    m_layoutNodes.insert(parsedNode.id, detail::buildLayoutFromNode(node));
  }

  // Create connections from edges
  int connectionsCreated = 0;
  for (const auto &edge : allEdges) {
    NMGraphNodeItem *fromNode = nodeMap.value(edge.first);
    NMGraphNodeItem *toNode = nodeMap.value(edge.second);

    if (fromNode && toNode) {
      // Check if connection already exists
      if (!m_scene->hasConnection(fromNode->nodeId(), toNode->nodeId())) {
        m_scene->addConnection(fromNode, toNode);
        ++connectionsCreated;
      }
    }
  }

  // Set entry scene
  m_layoutEntryScene = entryPoint;
  if (!entryPoint.isEmpty()) {
    ProjectManager::instance().setStartScene(entryPoint.toStdString());
  }

  // Save layout
  detail::saveGraphLayout(m_layoutNodes, m_layoutEntryScene);

  m_isRebuilding = false;

  // Fit view to content
  if (m_view && !m_scene->nodes().isEmpty()) {
    m_view->fitInView(m_scene->itemsBoundingRect().adjusted(-50, -50, 50, 50),
                      Qt::KeepAspectRatio);
  }

  // Show result
  QString message =
      tr("Successfully imported %1 node(s) with %2 connection(s) from %3 "
         "file(s).")
          .arg(allNodes.size())
          .arg(connectionsCreated)
          .arg(filesProcessed);

  if (parseErrors > 0) {
    message += tr("\n\n%1 file(s) had parse errors:\n%2")
                   .arg(parseErrors)
                   .arg(errorMessages.join("\n"));
  }

  qDebug() << "[StoryGraph] Sync Script to Graph completed:" << allNodes.size()
           << "nodes," << connectionsCreated << "connections from"
           << filesProcessed << "files";

  NMMessageDialog::showInfo(this, tr("Sync Script to Graph"), message);
}

} // namespace NovelMind::editor::qt

// Include MOC for Q_OBJECT class defined in this file
#include "nm_story_graph_panel_handlers.moc"
