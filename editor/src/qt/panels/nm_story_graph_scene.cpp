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
#include <functional>

namespace NovelMind::editor::qt {

// ============================================================================
// NMStoryGraphScene
// ============================================================================

NMStoryGraphScene::NMStoryGraphScene(QObject *parent) : QGraphicsScene(parent) {
  setSceneRect(-5000, -5000, 10000, 10000);
}

NMGraphNodeItem *NMStoryGraphScene::addNode(const QString &title,
                                            const QString &nodeType,
                                            const QPointF &pos, uint64_t nodeId,
                                            const QString &nodeIdString) {
  auto *node = new NMGraphNodeItem(title, nodeType);
  node->setPos(pos);

  if (nodeId == 0) {
    nodeId = m_nextNodeId++;
  } else {
    m_nextNodeId = std::max(m_nextNodeId, nodeId + 1);
  }
  node->setNodeId(nodeId);
  if (!nodeIdString.isEmpty()) {
    node->setNodeIdString(nodeIdString);
  } else {
    node->setNodeIdString(QString("node_%1").arg(nodeId));
  }

  const bool isEntryNode =
      (nodeType.compare("Entry", Qt::CaseInsensitive) == 0);
  if (!isEntryNode) {
    const auto scriptsDir = QString::fromStdString(
        ProjectManager::instance().getFolderPath(ProjectFolder::Scripts));
    if (!scriptsDir.isEmpty()) {
      const QString scriptPathAbs =
          scriptsDir + "/" + node->nodeIdString() + ".nms";
      const QString scriptPathRel =
          QString::fromStdString(ProjectManager::instance().toRelativePath(
              scriptPathAbs.toStdString()));
      node->setScriptPath(scriptPathRel);

      QFile scriptFile(scriptPathAbs);
      if (!scriptFile.exists()) {
        if (scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
          QTextStream out(&scriptFile);
          out << "// " << node->nodeIdString() << "\n";
          out << "scene " << node->nodeIdString() << " {\n";
          out << "}\n";
        }
      }
    }
  } else {
    node->setScriptPath(QString());
  }

  addItem(node);
  m_nodes.append(node);
  m_nodeLookup.insert(nodeId, node);
  emit nodeAdded(node->nodeId(), node->nodeIdString(), node->nodeType());
  return node;
}

NMGraphConnectionItem *NMStoryGraphScene::addConnection(NMGraphNodeItem *from,
                                                        NMGraphNodeItem *to) {
  if (!from || !to) {
    return nullptr;
  }
  return addConnection(from->nodeId(), to->nodeId());
}

NMGraphConnectionItem *NMStoryGraphScene::addConnection(uint64_t fromNodeId,
                                                        uint64_t toNodeId) {
  auto *from = findNode(fromNodeId);
  auto *to = findNode(toNodeId);
  if (!from || !to || hasConnection(fromNodeId, toNodeId)) {
    return nullptr;
  }

  auto *connection = new NMGraphConnectionItem(from, to);
  addItem(connection);
  m_connections.append(connection);

  // Now update the path after the connection is added to the scene
  connection->updatePath();
  emit connectionAdded(fromNodeId, toNodeId);

  return connection;
}

void NMStoryGraphScene::clearGraph() {
  for (auto *conn : m_connections) {
    removeItem(conn);
    delete conn;
  }
  m_connections.clear();

  for (auto *node : m_nodes) {
    removeItem(node);
    delete node;
  }
  m_nodes.clear();
  m_nodeLookup.clear();
  m_nextNodeId = 1;
}

void NMStoryGraphScene::removeNode(NMGraphNodeItem *node) {
  if (!node)
    return;

  // Get bounding rect before removal for proper update
  QRectF nodeRect = node->sceneBoundingRect();

  // Remove all connections attached to this node
  auto connections = findConnectionsForNode(node);
  for (auto *conn : connections) {
    removeConnection(conn);
  }

  // Remove from list and scene
  m_nodes.removeAll(node);
  m_nodeLookup.remove(node->nodeId());
  removeItem(node);
  emit nodeDeleted(node ? node->nodeId() : 0);
  delete node;

  // Force update of the area where the node was to clear artifacts
  update(nodeRect);
}

void NMStoryGraphScene::removeConnection(NMGraphConnectionItem *connection) {
  if (!connection)
    return;

  // Get bounding rect before removal for proper update
  QRectF connRect = connection->sceneBoundingRect();

  m_connections.removeAll(connection);
  removeItem(connection);
  if (connection && connection->startNode() && connection->endNode()) {
    emit connectionDeleted(connection->startNode()->nodeId(),
                           connection->endNode()->nodeId());
  }
  delete connection;

  // Force update of the area where the connection was to clear artifacts
  update(connRect);
}

bool NMStoryGraphScene::removeConnection(uint64_t fromNodeId,
                                         uint64_t toNodeId) {
  for (auto *conn : m_connections) {
    if (conn->startNode() && conn->endNode() &&
        conn->startNode()->nodeId() == fromNodeId &&
        conn->endNode()->nodeId() == toNodeId) {
      removeConnection(conn);
      return true;
    }
  }
  return false;
}

QList<NMGraphConnectionItem *>
NMStoryGraphScene::findConnectionsForNode(NMGraphNodeItem *node) const {
  QList<NMGraphConnectionItem *> result;
  for (auto *conn : m_connections) {
    if (conn->startNode() == node || conn->endNode() == node) {
      result.append(conn);
    }
  }
  return result;
}

NMGraphNodeItem *NMStoryGraphScene::findNode(uint64_t nodeId) const {
  return m_nodeLookup.value(nodeId, nullptr);
}

void NMStoryGraphScene::requestEntryNode(const QString &nodeIdString) {
  emit entryNodeRequested(nodeIdString);
}

bool NMStoryGraphScene::hasConnection(uint64_t fromNodeId,
                                      uint64_t toNodeId) const {
  for (auto *conn : m_connections) {
    if (conn->startNode() && conn->endNode() &&
        conn->startNode()->nodeId() == fromNodeId &&
        conn->endNode()->nodeId() == toNodeId) {
      return true;
    }
  }
  return false;
}

bool NMStoryGraphScene::wouldCreateCycle(uint64_t fromNodeId,
                                         uint64_t toNodeId) const {
  // If adding an edge from -> to would create a cycle, we need to check
  // if there's already a path from 'to' back to 'from'
  if (fromNodeId == toNodeId) {
    return true; // Self-loop
  }

  // Build adjacency list
  QHash<uint64_t, QList<uint64_t>> adj;
  for (auto *conn : m_connections) {
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

  for (auto *node : m_nodes) {
    allNodes.insert(node->nodeId());
  }

  for (auto *conn : m_connections) {
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
  for (auto *node : m_nodes) {
    if (node->isEntry()) {
      entryNodes.append(node->nodeId());
    }
  }

  // If no entry nodes, all nodes are potentially unreachable
  if (entryNodes.isEmpty()) {
    for (auto *node : m_nodes) {
      unreachable.append(node->nodeId());
    }
    return unreachable;
  }

  // Build adjacency list
  QHash<uint64_t, QList<uint64_t>> adj;
  for (auto *conn : m_connections) {
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
  for (auto *node : m_nodes) {
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
  for (auto *node : m_nodes) {
    if (node->isEntry()) {
      hasEntry = true;
      break;
    }
  }
  if (!hasEntry && !m_nodes.isEmpty()) {
    errors.append(
        tr("No entry node defined. Set one node as the starting point."));
  }

  // Check for cycles
  auto cycles = detectCycles();
  for (const auto &cycle : cycles) {
    QStringList nodeNames;
    for (uint64_t id : cycle) {
      auto *node = findNode(id);
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
      auto *node = findNode(id);
      if (node) {
        nodeNames.append(node->title());
      }
    }
    errors.append(tr("Unreachable nodes: %1").arg(nodeNames.join(", ")));
  }

  // Check for dead ends (nodes with no outgoing connections except End nodes)
  for (auto *node : m_nodes) {
    bool hasOutgoing = false;
    for (auto *conn : m_connections) {
      if (conn->startNode() == node) {
        hasOutgoing = true;
        break;
      }
    }

    // End nodes don't need outgoing connections
    if (!hasOutgoing &&
        !node->nodeType().contains("End", Qt::CaseInsensitive)) {
      // Also skip entry nodes if they have no other nodes
      if (!node->isEntry() || m_nodes.size() > 1) {
        errors.append(tr("Dead end: '%1' has no outgoing connections")
                          .arg(node->title()));
      }
    }
  }

  return errors;
}

void NMStoryGraphScene::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
    emit deleteSelectionRequested();
    event->accept();
    return;
  }

  QGraphicsScene::keyPressEvent(event);
}

void NMStoryGraphScene::mousePressEvent(QGraphicsSceneMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    // Track starting positions for all selected nodes (or the clicked node)
    m_dragStartPositions.clear();
    QList<QGraphicsItem *> targets;
    QGraphicsItem *clicked = itemAt(event->scenePos(), QTransform());
    if (auto *node = qgraphicsitem_cast<NMGraphNodeItem *>(clicked)) {
      // Include all selected nodes if the clicked node is part of selection
      targets = selectedItems().contains(node) ? selectedItems()
                                               : QList<QGraphicsItem *>{node};
    }
    for (auto *item : targets) {
      if (auto *node = qgraphicsitem_cast<NMGraphNodeItem *>(item)) {
        m_dragStartPositions.insert(node->nodeId(), node->pos());
      }
    }
    m_isDraggingNodes = !m_dragStartPositions.isEmpty();
  }

  QGraphicsScene::mousePressEvent(event);
}

void NMStoryGraphScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
  if (event->button() == Qt::LeftButton && m_isDraggingNodes) {
    QVector<GraphNodeMove> moves;
    for (auto it = m_dragStartPositions.constBegin();
         it != m_dragStartPositions.constEnd(); ++it) {
      const uint64_t nodeId = it.key();
      auto *node = findNode(nodeId);
      if (!node) {
        continue;
      }
      const QPointF oldPos = it.value();
      const QPointF newPos = node->pos();
      if (!qFuzzyCompare(oldPos.x(), newPos.x()) ||
          !qFuzzyCompare(oldPos.y(), newPos.y())) {
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

void NMStoryGraphScene::drawBackground(QPainter *painter, const QRectF &rect) {
  const auto &palette = NMStyleManager::instance().palette();

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
}

// ============================================================================

} // namespace NovelMind::editor::qt
