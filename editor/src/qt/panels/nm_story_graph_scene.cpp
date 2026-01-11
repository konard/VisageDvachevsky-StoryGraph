#include "NovelMind/editor/project_manager.hpp"
#include "NovelMind/editor/qt/nm_icon_manager.hpp"
#include "NovelMind/editor/qt/nm_play_mode_controller.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"
#include "NovelMind/editor/qt/nm_undo_manager.hpp"
#include "NovelMind/editor/qt/panels/nm_story_graph_panel.hpp"

#include <QAction>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSet>
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
#include <functional>
#include <memory>

namespace NovelMind::editor::qt {

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Sanitizes a string to be a valid NMScript identifier.
 *
 * Valid identifiers must:
 * - Start with a letter (ASCII or Unicode) or underscore
 * - Contain only letters, digits, or underscores
 *
 * This function:
 * - Replaces invalid characters with underscores
 * - Prepends an underscore if the string starts with a digit
 * - Converts empty strings to a default identifier
 *
 * @param input The input string to sanitize
 * @param defaultPrefix Prefix to use for empty or all-invalid inputs
 * @return A valid NMScript identifier string
 */
static QString sanitizeToIdentifier(const QString& input, const QString& defaultPrefix = "node") {
  if (input.isEmpty()) {
    return defaultPrefix;
  }

  QString result;
  result.reserve(input.length() + 1);

  bool firstChar = true;
  for (const QChar& ch : input) {
    if (ch.isLetter() || ch == '_') {
      // Valid character
      result.append(ch);
      firstChar = false;
    } else if (ch.isDigit()) {
      if (firstChar) {
        // Can't start with a digit, prepend underscore
        result.append('_');
        firstChar = false;
      }
      result.append(ch);
    } else if (ch.isSpace() || ch == '-' || ch == '.') {
      // Replace common separators with underscore
      if (!result.isEmpty() && result.back() != '_') {
        result.append('_');
      }
      firstChar = false;
    }
    // Other characters are simply dropped
  }

  // Trim trailing underscores
  while (result.endsWith('_')) {
    result.chop(1);
  }

  // If result is empty, use default
  if (result.isEmpty()) {
    return defaultPrefix;
  }

  return result;
}

// ============================================================================
// NMStoryGraphScene
// ============================================================================

NMStoryGraphScene::NMStoryGraphScene(QObject* parent) : QGraphicsScene(parent) {
  setSceneRect(-5000, -5000, 10000, 10000);
}

NMGraphNodeItem* NMStoryGraphScene::addNode(const QString& title, const QString& nodeType,
                                            const QPointF& pos, uint64_t nodeId,
                                            const QString& nodeIdString) {
  // MEM-1 fix: Use unique_ptr for exception-safe allocation, then transfer
  // ownership to scene via addItem()
  auto nodePtr = std::make_unique<NMGraphNodeItem>(title, nodeType);
  auto* node = nodePtr.get();
  node->setPos(pos);

  if (nodeId == 0) {
    nodeId = m_nextNodeId++;
  } else {
    m_nextNodeId = std::max(m_nextNodeId, nodeId + 1);
  }
  node->setNodeId(nodeId);

  // Set the node ID string, ensuring it's a valid identifier
  // The node ID string is used as the scene name in NMScript, so it must be
  // valid
  if (!nodeIdString.isEmpty()) {
    // Sanitize the provided ID string to ensure it's a valid identifier
    const QString sanitized = sanitizeToIdentifier(nodeIdString, "node");
    // If sanitization changed it significantly or it's the same as another
    // node, make it unique with the numeric ID
    if (sanitized != nodeIdString) {
      node->setNodeIdString(sanitized + QString("_%1").arg(nodeId));
    } else {
      node->setNodeIdString(nodeIdString);
    }
  } else {
    // No ID string provided - generate from title or use default
    const QString sanitizedTitle = sanitizeToIdentifier(title, "scene");
    if (sanitizedTitle != "scene" && sanitizedTitle != "node") {
      // Use sanitized title with numeric suffix for uniqueness
      node->setNodeIdString(sanitizedTitle + QString("_%1").arg(nodeId));
    } else {
      node->setNodeIdString(QString("node_%1").arg(nodeId));
    }
  }

  const bool isEntryNode = (nodeType.compare("Entry", Qt::CaseInsensitive) == 0);
  if (!isEntryNode) {
    const auto scriptsDir = QString::fromStdString(
        ProjectManager::instance().getFolderPath(ProjectFolder::ScriptsGenerated));
    if (!scriptsDir.isEmpty()) {
      const QString scriptPathAbs = scriptsDir + "/" + node->nodeIdString() + ".nms";
      const QString scriptPathRel = QString::fromStdString(
          ProjectManager::instance().toRelativePath(scriptPathAbs.toStdString()));
      node->setScriptPath(scriptPathRel);

      QFile scriptFile(scriptPathAbs);
      if (!scriptFile.exists()) {
        if (scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
          QTextStream out(&scriptFile);
          out << "// ========================================\n";
          out << "// Generated from Story Graph\n";
          out << "// Do not edit manually - changes may be overwritten\n";
          out << "// ========================================\n";
          out << "// " << node->nodeIdString() << "\n";
          out << "scene " << node->nodeIdString() << " {\n";
          // Condition and Scene nodes are "silent" - they only handle
          // branching, not dialogue. Only Dialogue nodes get a default say
          // statement. This fixes issue #76 where Condition nodes incorrectly
          // said text.
          if (node->isConditionNode()) {
            out << "    // Condition node - add branching logic here\n";
          } else if (node->isSceneNode()) {
            out << "    // Scene node - add scene content here\n";
          } else {
            out << "    say \"New scene\"\n";
          }
          out << "}\n";

          // Flush the stream and close the file to ensure data is written to
          // disk This fixes issue #97 where runtime couldn't find scenes
          // because script files weren't properly flushed before
          // validation/compilation
          out.flush();
          scriptFile.close();

          // Verify the file was written successfully
          if (scriptFile.error() != QFile::NoError) {
            const QString errorMsg = scriptFile.errorString();
            qWarning() << "[StoryGraph] Failed to write script file for node"
                       << node->nodeIdString() << ":" << errorMsg;

            // Mark the node with an error state
            node->setScriptFileError(true);
            node->setScriptFileErrorMessage(
                QString("Failed to write script file: %1").arg(errorMsg));

            // Emit signal to notify that there was an error
            emit scriptFileCreationFailed(node->nodeId(), node->nodeIdString(), errorMsg);
          } else {
            qDebug() << "[StoryGraph] Successfully created script file:" << scriptPathRel;
          }
        } else {
          const QString errorMsg = scriptFile.errorString();
          qWarning() << "[StoryGraph] Failed to open script file for writing:" << scriptPathAbs
                     << "-" << errorMsg;

          // Mark the node with an error state
          node->setScriptFileError(true);
          node->setScriptFileErrorMessage(
              QString("Failed to create script file: %1").arg(errorMsg));

          // Emit signal to notify that there was an error
          emit scriptFileCreationFailed(node->nodeId(), node->nodeIdString(), errorMsg);
        }
      }
    }
  } else {
    node->setScriptPath(QString());
  }

  addItem(nodePtr.release()); // Scene takes ownership
  m_nodes.append(node);
  m_nodeLookup.insert(nodeId, node);
  emit nodeAdded(node->nodeId(), node->nodeIdString(), node->nodeType());
  return node;
}

NMGraphConnectionItem* NMStoryGraphScene::addConnection(NMGraphNodeItem* from,
                                                        NMGraphNodeItem* to) {
  if (!from || !to) {
    return nullptr;
  }
  return addConnection(from->nodeId(), to->nodeId());
}

NMGraphConnectionItem* NMStoryGraphScene::addConnection(uint64_t fromNodeId, uint64_t toNodeId) {
  auto* from = findNode(fromNodeId);
  auto* to = findNode(toNodeId);
  if (!from || !to || hasConnection(fromNodeId, toNodeId)) {
    return nullptr;
  }

  // MEM-1 fix: Use unique_ptr for exception-safe allocation, then transfer
  // ownership to scene via addItem()
  auto connectionPtr = std::make_unique<NMGraphConnectionItem>(from, to);
  auto* connection = connectionPtr.get();
  addItem(connectionPtr.release()); // Scene takes ownership
  m_connections.append(connection);

  // Now update the path after the connection is added to the scene
  connection->updatePath();
  emit connectionAdded(fromNodeId, toNodeId);

  return connection;
}

void NMStoryGraphScene::clearGraph() {
  for (auto* conn : m_connections) {
    removeItem(conn);
    delete conn;
  }
  m_connections.clear();

  for (auto* node : m_nodes) {
    removeItem(node);
    delete node;
  }
  m_nodes.clear();
  m_nodeLookup.clear();
  m_nextNodeId = 1;
}

void NMStoryGraphScene::removeNode(NMGraphNodeItem* node) {
  if (!node)
    return;

  // Get bounding rect before removal for proper update
  QRectF nodeRect = node->sceneBoundingRect();

  // Remove all connections attached to this node
  auto connections = findConnectionsForNode(node);
  for (auto* conn : connections) {
    removeConnection(conn);
  }

  // Emit signal BEFORE removing from lookup so listeners can still find the node
  // This prevents memory leaks in listeners that need to query node properties
  // Issue #568: Scene Mediator needs to access node->sceneId() and node->nodeIdString()
  emit nodeDeleted(node ? node->nodeId() : 0);

  // Remove from list and scene
  m_nodes.removeAll(node);
  m_nodeLookup.remove(node->nodeId());
  removeItem(node);
  delete node;

  // Force update of the area where the node was to clear artifacts
  update(nodeRect);
}

void NMStoryGraphScene::removeConnection(NMGraphConnectionItem* connection) {
  if (!connection)
    return;

  // Get bounding rect before removal for proper update
  QRectF connRect = connection->sceneBoundingRect();

  m_connections.removeAll(connection);
  removeItem(connection);
  if (connection && connection->startNode() && connection->endNode()) {
    emit connectionDeleted(connection->startNode()->nodeId(), connection->endNode()->nodeId());
  }
  delete connection;

  // Force update of the area where the connection was to clear artifacts
  update(connRect);
}

bool NMStoryGraphScene::removeConnection(uint64_t fromNodeId, uint64_t toNodeId) {
  for (auto* conn : m_connections) {
    if (conn->startNode() && conn->endNode() && conn->startNode()->nodeId() == fromNodeId &&
        conn->endNode()->nodeId() == toNodeId) {
      removeConnection(conn);
      return true;
    }
  }
  return false;
}

QList<NMGraphConnectionItem*>
NMStoryGraphScene::findConnectionsForNode(NMGraphNodeItem* node) const {
  QList<NMGraphConnectionItem*> result;
  for (auto* conn : m_connections) {
    if (conn->startNode() == node || conn->endNode() == node) {
      result.append(conn);
    }
  }
  return result;
}

NMGraphNodeItem* NMStoryGraphScene::findNode(uint64_t nodeId) const {
  return m_nodeLookup.value(nodeId, nullptr);
}

void NMStoryGraphScene::requestEntryNode(const QString& nodeIdString) {
  emit entryNodeRequested(nodeIdString);
}

bool NMStoryGraphScene::hasConnection(uint64_t fromNodeId, uint64_t toNodeId) const {
  for (auto* conn : m_connections) {
    if (conn->startNode() && conn->endNode() && conn->startNode()->nodeId() == fromNodeId &&
        conn->endNode()->nodeId() == toNodeId) {
      return true;
    }
  }
  return false;
}

bool NMStoryGraphScene::wouldCreateCycle(uint64_t fromNodeId, uint64_t toNodeId) const {
  // If adding an edge from -> to would create a cycle, we need to check
  // if there's already a path from 'to' back to 'from'
  if (fromNodeId == toNodeId) {
    return true; // Self-loop
  }

  // Build adjacency list
  QHash<uint64_t, QList<uint64_t>> adj;
  for (auto* conn : m_connections) {
    if (conn->startNode() && conn->endNode()) {
      adj[conn->startNode()->nodeId()].append(conn->endNode()->nodeId());
    }
  }

  // Add the proposed edge
  adj[fromNodeId].append(toNodeId);

  // DFS from 'to' to see if we can reach 'from'
  QSet<uint64_t> visited;
  QList<uint64_t> stack;
  stack.append(toNodeId);

  while (!stack.isEmpty()) {
    uint64_t current = stack.takeLast();
    if (current == fromNodeId) {
      return true; // Found a cycle
    }
    if (visited.contains(current)) {
      continue;
    }
    visited.insert(current);

    for (uint64_t next : adj.value(current)) {
      if (!visited.contains(next)) {
        stack.append(next);
      }
    }
  }

  return false;
}

QList<QList<uint64_t>> NMStoryGraphScene::detectCycles() const {
  QList<QList<uint64_t>> cycles;

  // Build adjacency list
  QHash<uint64_t, QList<uint64_t>> adj;
  QSet<uint64_t> allNodes;

  for (auto* node : m_nodes) {
    allNodes.insert(node->nodeId());
  }

  for (auto* conn : m_connections) {
    if (conn->startNode() && conn->endNode()) {
      adj[conn->startNode()->nodeId()].append(conn->endNode()->nodeId());
    }
  }

  // Use Tarjan's algorithm to find strongly connected components
  QHash<uint64_t, int> index;
  QHash<uint64_t, int> lowlink;
  QSet<uint64_t> onStack;
  QList<uint64_t> stack;
  int nextIndex = 0;

  std::function<void(uint64_t)> strongconnect = [&](uint64_t v) {
    index[v] = nextIndex;
    lowlink[v] = nextIndex;
    nextIndex++;
    stack.append(v);
    onStack.insert(v);

    for (uint64_t w : adj.value(v)) {
      if (!index.contains(w)) {
        strongconnect(w);
        lowlink[v] = qMin(lowlink[v], lowlink[w]);
      } else if (onStack.contains(w)) {
        lowlink[v] = qMin(lowlink[v], index[w]);
      }
    }

    // If v is a root node, pop the stack and generate an SCC
    if (lowlink[v] == index[v]) {
      QList<uint64_t> component;
      uint64_t w;
      do {
        w = stack.takeLast();
        onStack.remove(w);
        component.append(w);
      } while (w != v);

      // Only report SCCs with more than one node (actual cycles)
      if (component.size() > 1) {
        cycles.append(component);
      }
    }
  };

  for (uint64_t nodeId : allNodes) {
    if (!index.contains(nodeId)) {
      strongconnect(nodeId);
    }
  }

  return cycles;
}

QList<uint64_t> NMStoryGraphScene::findUnreachableNodes() const {
  QList<uint64_t> unreachable;

  // Find entry nodes
  QList<uint64_t> entryNodes;
  for (auto* node : m_nodes) {
    if (node->isEntry()) {
      entryNodes.append(node->nodeId());
    }
  }

  // If no entry nodes, all nodes are potentially unreachable
  if (entryNodes.isEmpty()) {
    for (auto* node : m_nodes) {
      unreachable.append(node->nodeId());
    }
    return unreachable;
  }

  // Build adjacency list
  QHash<uint64_t, QList<uint64_t>> adj;
  for (auto* conn : m_connections) {
    if (conn->startNode() && conn->endNode()) {
      adj[conn->startNode()->nodeId()].append(conn->endNode()->nodeId());
    }
  }

  // BFS from all entry nodes
  QSet<uint64_t> visited;
  QList<uint64_t> queue = entryNodes;

  while (!queue.isEmpty()) {
    uint64_t current = queue.takeFirst();
    if (visited.contains(current)) {
      continue;
    }
    visited.insert(current);

    for (uint64_t next : adj.value(current)) {
      if (!visited.contains(next)) {
        queue.append(next);
      }
    }
  }

  // Find nodes not visited
  for (auto* node : m_nodes) {
    if (!visited.contains(node->nodeId())) {
      unreachable.append(node->nodeId());
    }
  }

  return unreachable;
}

QStringList NMStoryGraphScene::validateGraph() const {
  QStringList errors;

  // Check for entry node
  bool hasEntry = false;
  for (auto* node : m_nodes) {
    if (node->isEntry()) {
      hasEntry = true;
      break;
    }
  }
  if (!hasEntry && !m_nodes.isEmpty()) {
    errors.append(tr("No entry node defined. Set one node as the starting point."));
  }

  // Check for cycles
  auto cycles = detectCycles();
  for (const auto& cycle : cycles) {
    QStringList nodeNames;
    for (uint64_t id : cycle) {
      auto* node = findNode(id);
      if (node) {
        nodeNames.append(node->title());
      }
    }
    errors.append(tr("Cycle detected: %1").arg(nodeNames.join(" -> ")));
  }

  // Check for unreachable nodes
  auto unreachable = findUnreachableNodes();
  if (!unreachable.isEmpty()) {
    QStringList nodeNames;
    for (uint64_t id : unreachable) {
      auto* node = findNode(id);
      if (node) {
        nodeNames.append(node->title());
      }
    }
    errors.append(tr("Unreachable nodes: %1").arg(nodeNames.join(", ")));
  }

  // Check for dead ends (nodes with no outgoing connections except End nodes)
  for (auto* node : m_nodes) {
    bool hasOutgoing = false;
    for (auto* conn : m_connections) {
      if (conn->startNode() == node) {
        hasOutgoing = true;
        break;
      }
    }

    // End nodes don't need outgoing connections
    if (!hasOutgoing && !node->nodeType().contains("End", Qt::CaseInsensitive)) {
      errors.append(tr("Dead end: '%1' has no outgoing connections").arg(node->title()));
    }
  }

  // Validate scene references from project manager
  const QString projectPath = QString::fromStdString(ProjectManager::instance().getProjectPath());
  if (!projectPath.isEmpty()) {
    errors.append(validateSceneReferences(projectPath));
  }

  return errors;
}

QStringList NMStoryGraphScene::validateSceneReferences(const QString& projectPath) const {
  QStringList errors;

  if (projectPath.isEmpty()) {
    return errors;
  }

  // Build path to Scenes folder
  const QString scenesPath = projectPath + "/Scenes";
  QDir scenesDir(scenesPath);

  if (!scenesDir.exists()) {
    // If Scenes folder doesn't exist yet, don't report errors
    return errors;
  }

  // Collect all available .nmscene files
  QSet<QString> availableScenes;
  QDirIterator it(scenesPath, QStringList() << "*.nmscene", QDir::Files,
                  QDirIterator::Subdirectories);
  while (it.hasNext()) {
    QString filePath = it.next();
    QFileInfo fileInfo(filePath);
    // Get scene ID from filename (without extension)
    QString sceneId = fileInfo.completeBaseName();
    availableScenes.insert(sceneId);
  }

  // Check each scene node for valid references
  for (auto* node : m_nodes) {
    if (!node->isSceneNode()) {
      continue;
    }

    const QString sceneId = node->sceneId();

    // Check if scene ID is empty
    if (sceneId.isEmpty()) {
      errors.append(tr("Scene node '%1' has no scene ID assigned").arg(node->title()));
      continue;
    }

    // Check if .nmscene file exists
    if (!availableScenes.contains(sceneId)) {
      errors.append(
          tr("Scene '%1' not found - Missing file: Scenes/%2.nmscene").arg(node->title(), sceneId));
    }
  }

  return errors;
}

void NMStoryGraphScene::updateSceneValidationState(const QString& projectPath) {
  if (projectPath.isEmpty()) {
    // Clear validation state if no project
    for (auto* node : m_nodes) {
      if (node->isSceneNode()) {
        node->setSceneValidationError(false);
        node->setSceneValidationWarning(false);
        node->setSceneValidationMessage(QString());
      }
    }
    return;
  }

  // Build path to Scenes folder
  const QString scenesPath = projectPath + "/Scenes";
  QDir scenesDir(scenesPath);

  // Collect all available .nmscene files
  QSet<QString> availableScenes;
  if (scenesDir.exists()) {
    QDirIterator it(scenesPath, QStringList() << "*.nmscene", QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
      QString filePath = it.next();
      QFileInfo fileInfo(filePath);
      QString sceneId = fileInfo.completeBaseName();
      availableScenes.insert(sceneId);
    }
  }

  // Update validation state for each scene node
  for (auto* node : m_nodes) {
    if (!node->isSceneNode()) {
      // Clear validation for non-scene nodes
      node->setSceneValidationError(false);
      node->setSceneValidationWarning(false);
      node->setSceneValidationMessage(QString());
      continue;
    }

    const QString sceneId = node->sceneId();

    // Check if scene ID is empty
    if (sceneId.isEmpty()) {
      node->setSceneValidationError(true);
      node->setSceneValidationWarning(false);
      node->setSceneValidationMessage(tr("No scene ID assigned"));
      continue;
    }

    // Check if .nmscene file exists
    if (!availableScenes.contains(sceneId)) {
      node->setSceneValidationError(true);
      node->setSceneValidationWarning(false);
      node->setSceneValidationMessage(tr("Scene file not found: Scenes/%1.nmscene").arg(sceneId));
    } else {
      // Scene is valid
      node->setSceneValidationError(false);
      node->setSceneValidationWarning(false);
      node->setSceneValidationMessage(QString());
    }
  }

  // Trigger visual update
  update();
}

void NMStoryGraphScene::setReadOnly(bool readOnly) {
  m_readOnly = readOnly;

  // Update item flags for all nodes
  for (auto* node : m_nodes) {
    if (readOnly) {
      // Allow selection but not movement
      node->setFlag(QGraphicsItem::ItemIsMovable, false);
    } else {
      // Normal mode - allow movement
      node->setFlag(QGraphicsItem::ItemIsMovable, true);
    }
  }
}

// ============================================================================
// Scene Container Visualization (Issue #345)
// ============================================================================

void NMStoryGraphScene::setSceneContainersVisible(bool enabled) {
  if (m_showSceneContainers == enabled) {
    return;
  }
  m_showSceneContainers = enabled;
  update(); // Trigger repaint
}

QList<NMGraphNodeItem*>
NMStoryGraphScene::findDialogueNodesInScene(NMGraphNodeItem* sceneNode) const {
  QList<NMGraphNodeItem*> result;
  if (!sceneNode || !sceneNode->isSceneNode()) {
    return result;
  }

  // Build adjacency list for outgoing connections
  QHash<uint64_t, QList<uint64_t>> adj;
  for (auto* conn : m_connections) {
    if (conn->startNode() && conn->endNode()) {
      adj[conn->startNode()->nodeId()].append(conn->endNode()->nodeId());
    }
  }

  // BFS from scene node, stop at other Scene nodes
  QSet<uint64_t> visited;
  QList<uint64_t> queue;
  queue.append(sceneNode->nodeId());
  visited.insert(sceneNode->nodeId());

  while (!queue.isEmpty()) {
    uint64_t currentId = queue.takeFirst();

    for (uint64_t nextId : adj.value(currentId)) {
      if (visited.contains(nextId)) {
        continue;
      }
      visited.insert(nextId);

      auto* nextNode = findNode(nextId);
      if (!nextNode) {
        continue;
      }

      // Stop at Scene nodes - they belong to their own container
      if (nextNode->isSceneNode()) {
        continue;
      }

      result.append(nextNode);
      queue.append(nextId);
    }
  }

  return result;
}

void NMStoryGraphScene::keyPressEvent(QKeyEvent* event) {
  // Block delete in read-only mode
  if (m_readOnly) {
    event->accept();
    return;
  }

  if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
    emit deleteSelectionRequested();
    event->accept();
    return;
  }

  QGraphicsScene::keyPressEvent(event);
}

void NMStoryGraphScene::mousePressEvent(QGraphicsSceneMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    // Track starting positions for all selected nodes (or the clicked node)
    m_dragStartPositions.clear();
    QList<QGraphicsItem*> targets;
    QGraphicsItem* clicked = itemAt(event->scenePos(), QTransform());
    if (auto* node = qgraphicsitem_cast<NMGraphNodeItem*>(clicked)) {
      // Include all selected nodes if the clicked node is part of selection
      targets = selectedItems().contains(node) ? selectedItems() : QList<QGraphicsItem*>{node};
    }
    for (auto* item : targets) {
      if (auto* node = qgraphicsitem_cast<NMGraphNodeItem*>(item)) {
        m_dragStartPositions.insert(node->nodeId(), node->pos());
      }
    }
    m_isDraggingNodes = !m_dragStartPositions.isEmpty();
  }

  QGraphicsScene::mousePressEvent(event);
}

void NMStoryGraphScene::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
  if (event->button() == Qt::LeftButton && m_isDraggingNodes) {
    QVector<GraphNodeMove> moves;
    for (auto it = m_dragStartPositions.constBegin(); it != m_dragStartPositions.constEnd(); ++it) {
      const uint64_t nodeId = it.key();
      auto* node = findNode(nodeId);
      if (!node) {
        continue;
      }
      const QPointF oldPos = it.value();
      const QPointF newPos = node->pos();
      if (!qFuzzyCompare(oldPos.x(), newPos.x()) || !qFuzzyCompare(oldPos.y(), newPos.y())) {
        moves.push_back(GraphNodeMove{nodeId, oldPos, newPos});
      }
    }
    if (!moves.isEmpty()) {
      emit nodesMoved(moves);
    }
    m_dragStartPositions.clear();
    m_isDraggingNodes = false;
  }

  QGraphicsScene::mouseReleaseEvent(event);
}

void NMStoryGraphScene::drawBackground(QPainter* painter, const QRectF& rect) {
  const auto& palette = NMStyleManager::instance().palette();

  // Fill background
  painter->fillRect(rect, palette.bgDarkest);

  // Draw grid (dots pattern for graph view)
  painter->setPen(palette.gridLine);

  qreal gridSize = 32.0;
  qreal left = rect.left() - std::fmod(rect.left(), gridSize);
  qreal top = rect.top() - std::fmod(rect.top(), gridSize);

  for (qreal x = left; x < rect.right(); x += gridSize) {
    for (qreal y = top; y < rect.bottom(); y += gridSize) {
      painter->drawPoint(QPointF(x, y));
    }
  }

  // Draw origin
  painter->setPen(QPen(palette.accentPrimary, 1));
  if (rect.left() <= 0 && rect.right() >= 0) {
    painter->drawLine(QLineF(0, rect.top(), 0, rect.bottom()));
  }
  if (rect.top() <= 0 && rect.bottom() >= 0) {
    painter->drawLine(QLineF(rect.left(), 0, rect.right(), 0));
  }

  // Issue #345: Draw scene containers behind nodes
  if (m_showSceneContainers) {
    drawSceneContainers(painter, rect);
  }
}

void NMStoryGraphScene::drawSceneContainers(QPainter* painter, const QRectF& viewRect) {
  // Colors for scene containers - use scene's green accent with transparency
  const QColor containerFill(100, 200, 150, 25);   // Very transparent green fill
  const QColor containerBorder(100, 200, 150, 80); // Semi-transparent green border
  const QColor labelColor(100, 200, 150, 160);     // Scene label color
  constexpr qreal containerPadding = 25.0;
  constexpr qreal cornerRadius = 16.0;

  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);

  // Draw containers for each scene node
  for (auto* node : m_nodes) {
    if (!node->isSceneNode()) {
      continue;
    }

    // Calculate container bounds including all dialogue nodes in this scene
    QRectF containerBounds = node->sceneBoundingRect();

    QList<NMGraphNodeItem*> dialogueNodes = findDialogueNodesInScene(node);
    for (auto* dialogueNode : dialogueNodes) {
      containerBounds = containerBounds.united(dialogueNode->sceneBoundingRect());
    }

    // Add padding
    containerBounds.adjust(-containerPadding, -containerPadding - 20, // Extra top for label
                           containerPadding, containerPadding);

    // Skip if container is not visible in view
    if (!viewRect.intersects(containerBounds)) {
      continue;
    }

    // Draw container fill
    painter->setBrush(containerFill);
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(containerBounds, cornerRadius, cornerRadius);

    // Draw container border (dashed line)
    QPen borderPen(containerBorder, 1.5, Qt::DashLine);
    borderPen.setDashPattern({6, 4});
    painter->setPen(borderPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(containerBounds, cornerRadius, cornerRadius);

    // Draw scene label in top-left corner of container
    const QString sceneLabel = node->sceneId().isEmpty() ? node->title() : node->sceneId();
    painter->setPen(labelColor);
    QFont labelFont = painter->font();
    labelFont.setPointSize(9);
    labelFont.setWeight(QFont::Medium);
    painter->setFont(labelFont);

    QRectF labelRect(containerBounds.left() + 10, containerBounds.top() + 4,
                     containerBounds.width() - 20, 18);
    painter->drawText(labelRect, Qt::AlignLeft | Qt::AlignTop, sceneLabel);

    // If there are embedded dialogue nodes, show count indicator
    if (!dialogueNodes.isEmpty()) {
      QString countText = QString("(%1 nodes)").arg(dialogueNodes.size());
      QFontMetrics fm(labelFont);
      int labelWidth = fm.horizontalAdvance(sceneLabel);

      painter->setPen(QColor(100, 200, 150, 100));
      painter->drawText(labelRect.adjusted(labelWidth + 10, 0, 0, 0), Qt::AlignLeft | Qt::AlignTop,
                        countText);
    }
  }

  painter->restore();
}

// ============================================================================

} // namespace NovelMind::editor::qt
