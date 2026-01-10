#include "NovelMind/editor/qt/panels/nm_scene_dialogue_graph_panel.hpp"
#include "NovelMind/editor/project_manager.hpp"
#include "NovelMind/editor/qt/nm_dialogs.hpp"
#include "NovelMind/editor/qt/nm_icon_manager.hpp"
#include "NovelMind/editor/qt/nm_play_mode_controller.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"
#include "NovelMind/editor/qt/nm_undo_manager.hpp"
#include "NovelMind/editor/qt/panels/nm_story_graph_minimap.hpp"

#include <QAction>
#include <QDebug>
#include <QFile>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QQueue>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>
#include <filesystem>
#include <limits.h>

namespace NovelMind::editor::qt {

// ============================================================================
// NMSceneDialogueNodePalette
// ============================================================================

NMSceneDialogueNodePalette::NMSceneDialogueNodePalette(QWidget* parent) : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(4, 4, 4, 4);
  layout->setSpacing(4);

  auto* label = new QLabel(tr("Dialogue Nodes"), this);
  label->setStyleSheet("font-weight: bold; padding: 4px;");
  layout->addWidget(label);

  // Only show dialogue-appropriate node types
  createNodeButton("Dialogue", "node-dialogue");
  createNodeButton("Choice", "node-choice");
  createNodeButton("Wait", "node-event");
  createNodeButton("SetVariable", "node-variable");
  createNodeButton("PlaySound", "node-event");
  createNodeButton("ShowCharacter", "node-event");
  createNodeButton("HideCharacter", "node-event");

  layout->addStretch();
}

void NMSceneDialogueNodePalette::createNodeButton(const QString& nodeType, const QString& icon) {
  auto* btn = new QPushButton(NMIconManager::instance().getIcon(icon, 16), nodeType, this);
  btn->setFlat(true);
  btn->setStyleSheet("QPushButton { text-align: left; padding: 6px; } "
                     "QPushButton:hover { background: #3a3a3a; }");
  connect(btn, &QPushButton::clicked, this,
          [this, nodeType]() { emit nodeTypeSelected(nodeType); });
  qobject_cast<QVBoxLayout*>(layout())->addWidget(btn);
}

// ============================================================================
// NMSceneDialogueGraphScene
// ============================================================================

NMSceneDialogueGraphScene::NMSceneDialogueGraphScene(QObject* parent) : NMStoryGraphScene(parent) {}

bool NMSceneDialogueGraphScene::isAllowedNodeType(const QString& nodeType) {
  // Allow dialogue-related nodes, but not Scene nodes
  static const QStringList allowedTypes = {
      "Dialogue",  "Choice",        "ChoiceOption",  "Wait",           "SetVariable", "GetVariable",
      "PlaySound", "ShowCharacter", "HideCharacter", "ShowBackground", "Transition",  "PlayMusic",
      "StopMusic", "Expression",    "SceneStart",    "SceneEnd",       "Comment"};
  return allowedTypes.contains(nodeType, Qt::CaseInsensitive);
}

NMGraphNodeItem* NMSceneDialogueGraphScene::addNode(const QString& title, const QString& nodeType,
                                                    const QPointF& pos, uint64_t nodeId,
                                                    const QString& nodeIdString) {
  // Prevent Scene nodes in embedded dialogue graphs
  if (nodeType.compare("Scene", Qt::CaseInsensitive) == 0) {
    qWarning() << "[SceneDialogueGraph] Cannot add Scene nodes to embedded "
                  "dialogue graph";
    return nullptr;
  }

  if (!isAllowedNodeType(nodeType)) {
    qWarning() << "[SceneDialogueGraph] Node type not allowed in dialogue "
                  "graph:"
               << nodeType;
    return nullptr;
  }

  // Delegate to parent class
  return NMStoryGraphScene::addNode(title, nodeType, pos, nodeId, nodeIdString);
}

// ============================================================================
// NMSceneDialogueGraphPanel
// ============================================================================

NMSceneDialogueGraphPanel::NMSceneDialogueGraphPanel(QWidget* parent)
    : NMDockPanel(tr("Scene Dialogue Flow"), parent) {
  setPanelId("SceneDialogueGraph");
  setupContent();
  setupToolBar();
  setupBreadcrumb();
}

NMSceneDialogueGraphPanel::~NMSceneDialogueGraphPanel() = default;

void NMSceneDialogueGraphPanel::onInitialize() {
  if (m_dialogueScene) {
    m_dialogueScene->clearGraph();
  }

  if (m_view) {
    m_view->centerOnGraph();
  }

  // Connect to graph scene signals
  if (m_dialogueScene) {
    connect(m_dialogueScene, &NMSceneDialogueGraphScene::nodeAdded, this,
            &NMSceneDialogueGraphPanel::onNodeAdded);
    connect(m_dialogueScene, &NMSceneDialogueGraphScene::nodeDeleted, this,
            &NMSceneDialogueGraphPanel::onNodeDeleted);
    connect(m_dialogueScene, &NMSceneDialogueGraphScene::connectionAdded, this,
            &NMSceneDialogueGraphPanel::onConnectionAdded);
    connect(m_dialogueScene, &NMSceneDialogueGraphScene::connectionDeleted, this,
            &NMSceneDialogueGraphPanel::onConnectionDeleted);
    connect(m_dialogueScene, &NMSceneDialogueGraphScene::deleteSelectionRequested, this,
            &NMSceneDialogueGraphPanel::onDeleteSelected);
    connect(m_dialogueScene, &NMSceneDialogueGraphScene::nodesMoved, this,
            &NMSceneDialogueGraphPanel::onNodesMoved);
  }

  // Connect to view signals
  if (m_view) {
    connect(m_view, &NMStoryGraphView::requestConnection, this,
            &NMSceneDialogueGraphPanel::onRequestConnection);
  }
}

void NMSceneDialogueGraphPanel::onUpdate(double /*deltaTime*/) {
  // Update handled reactively
}

void NMSceneDialogueGraphPanel::loadSceneDialogue(const QString& sceneId) {
  if (m_hasUnsavedChanges) {
    auto reply = NMMessageDialog::showQuestion(
        this, tr("Unsaved Changes"),
        tr("The current dialogue graph has unsaved changes. Discard them?"),
        {NMDialogButton::Yes, NMDialogButton::No}, NMDialogButton::No);
    if (reply == NMDialogButton::No) {
      return;
    }
  }

  m_currentSceneId = sceneId;
  m_hasUnsavedChanges.store(false, std::memory_order_relaxed);

  updateBreadcrumb();
  loadDialogueGraphFromScene();

  if (m_view && !m_dialogueScene->nodes().isEmpty()) {
    m_view->centerOnGraph();
  }
}

void NMSceneDialogueGraphPanel::clear() {
  if (m_dialogueScene) {
    m_dialogueScene->clearGraph();
  }
  m_currentSceneId.clear();
  m_hasUnsavedChanges.store(false, std::memory_order_relaxed);
  m_nodeIdToString.clear();
  updateBreadcrumb();
}

void NMSceneDialogueGraphPanel::setupToolBar() {
  m_toolBar = new QToolBar(this);
  m_toolBar->setObjectName("SceneDialogueGraphToolBar");
  m_toolBar->setIconSize(QSize(16, 16));

  auto& iconMgr = NMIconManager::instance();

  QAction* actionConnect = m_toolBar->addAction(iconMgr.getIcon("connection", 16), tr("Connect"));
  actionConnect->setToolTip(tr("Connection mode"));
  actionConnect->setCheckable(true);
  connect(actionConnect, &QAction::toggled, this, [this](bool enabled) {
    if (m_view) {
      m_view->setConnectionModeEnabled(enabled);
    }
  });

  m_toolBar->addSeparator();

  QAction* actionZoomIn = m_toolBar->addAction(iconMgr.getIcon("zoom-in", 16), tr("Zoom In"));
  connect(actionZoomIn, &QAction::triggered, this, &NMSceneDialogueGraphPanel::onZoomIn);

  QAction* actionZoomOut = m_toolBar->addAction(iconMgr.getIcon("zoom-out", 16), tr("Zoom Out"));
  connect(actionZoomOut, &QAction::triggered, this, &NMSceneDialogueGraphPanel::onZoomOut);

  QAction* actionZoomReset =
      m_toolBar->addAction(iconMgr.getIcon("zoom-reset", 16), tr("Reset Zoom"));
  connect(actionZoomReset, &QAction::triggered, this, &NMSceneDialogueGraphPanel::onZoomReset);

  QAction* actionFit = m_toolBar->addAction(iconMgr.getIcon("zoom-fit", 16), tr("Fit"));
  connect(actionFit, &QAction::triggered, this, &NMSceneDialogueGraphPanel::onFitToGraph);

  m_toolBar->addSeparator();

  QAction* actionAutoLayout =
      m_toolBar->addAction(iconMgr.getIcon("layout-auto", 16), tr("Auto Layout"));
  connect(actionAutoLayout, &QAction::triggered, this, &NMSceneDialogueGraphPanel::onAutoLayout);

  // Note: Toolbar is set in the parent QDockWidget, not explicitly needed here
}

void NMSceneDialogueGraphPanel::setupContent() {
  m_contentWidget = new QWidget(this);
  auto* mainLayout = new QVBoxLayout(m_contentWidget);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // Create graph scene and view
  m_dialogueScene = new NMSceneDialogueGraphScene(this);
  m_view = new NMStoryGraphView(m_contentWidget);
  m_view->setScene(m_dialogueScene);

  // Apply distinct visual styling for dialogue graph
  const auto& palette = NMStyleManager::instance().palette();
  m_view->setStyleSheet(
      QString("QGraphicsView { background: %1; }").arg(palette.bgDark.darker(105).name()));

  // Create minimap
  m_minimap = new NMStoryGraphMinimap(m_contentWidget);
  m_minimap->setMainView(m_view);

  // Create node palette
  m_nodePalette = new NMSceneDialogueNodePalette(m_contentWidget);
  connect(m_nodePalette, &NMSceneDialogueNodePalette::nodeTypeSelected, this,
          &NMSceneDialogueGraphPanel::onNodeTypeSelected);

  // Layout: [Palette | View] with minimap overlay
  auto* graphLayout = new QHBoxLayout();
  graphLayout->setContentsMargins(0, 0, 0, 0);
  graphLayout->setSpacing(0);
  graphLayout->addWidget(m_nodePalette, 0);
  graphLayout->addWidget(m_view, 1);

  mainLayout->addLayout(graphLayout);

  // Position minimap in bottom-right corner
  m_minimap->setParent(m_view);
  m_minimap->setFixedSize(200, 150);
  m_minimap->move(m_view->width() - 210, m_view->height() - 160);
  m_minimap->raise();

  setContentWidget(m_contentWidget);
}

void NMSceneDialogueGraphPanel::setupBreadcrumb() {
  m_breadcrumbWidget = new QWidget(this);
  auto* breadcrumbLayout = new QHBoxLayout(m_breadcrumbWidget);
  breadcrumbLayout->setContentsMargins(8, 4, 8, 4);
  breadcrumbLayout->setSpacing(8);

  auto& iconMgr = NMIconManager::instance();

  // Return button
  m_returnButton =
      new QPushButton(iconMgr.getIcon("arrow-left", 16), tr("Story Graph"), m_breadcrumbWidget);
  m_returnButton->setFlat(true);
  m_returnButton->setStyleSheet("QPushButton { padding: 4px 8px; } "
                                "QPushButton:hover { background: #3a3a3a; }");
  connect(m_returnButton, &QPushButton::clicked, this,
          &NMSceneDialogueGraphPanel::onReturnToStoryGraph);
  breadcrumbLayout->addWidget(m_returnButton);

  // Breadcrumb separator
  auto* separator = new QLabel(tr(">"), m_breadcrumbWidget);
  separator->setStyleSheet("color: #888; padding: 0 4px;");
  breadcrumbLayout->addWidget(separator);

  // Scene name label
  m_breadcrumbLabel = new QLabel(m_breadcrumbWidget);
  m_breadcrumbLabel->setStyleSheet("font-weight: bold; color: #64C896;");
  breadcrumbLayout->addWidget(m_breadcrumbLabel);

  breadcrumbLayout->addStretch();

  // Insert breadcrumb at top of content
  if (m_contentWidget && m_contentWidget->layout()) {
    qobject_cast<QVBoxLayout*>(m_contentWidget->layout())->insertWidget(0, m_breadcrumbWidget);
  }

  updateBreadcrumb();
}

void NMSceneDialogueGraphPanel::updateBreadcrumb() {
  if (m_breadcrumbLabel) {
    if (m_currentSceneId.isEmpty()) {
      m_breadcrumbLabel->setText(tr("No scene loaded"));
    } else {
      m_breadcrumbLabel->setText(tr("Scene: %1 > Dialogue Flow").arg(m_currentSceneId));
    }
  }
}

void NMSceneDialogueGraphPanel::createNode(const QString& nodeType) {
  if (!m_dialogueScene || !m_view) {
    return;
  }

  if (m_currentSceneId.isEmpty()) {
    NMMessageDialog::showWarning(this, tr("No Scene Loaded"),
                                 tr("Please load a scene first before adding nodes."));
    return;
  }

  // Get center of view
  const QPointF sceneCenter = m_view->mapToScene(m_view->viewport()->rect().center());

  // Create node with auto-generated title
  const QString title = QString("%1 Node").arg(nodeType);
  m_dialogueScene->addNode(title, nodeType, sceneCenter);
}

void NMSceneDialogueGraphPanel::saveDialogueGraphToScene() {
  if (m_currentSceneId.isEmpty() || !m_dialogueScene) {
    return;
  }

  // Build JSON representation of dialogue graph
  QJsonObject graphData;
  graphData["sceneId"] = m_currentSceneId;

  QJsonArray nodesArray;
  for (const auto* node : m_dialogueScene->nodes()) {
    if (!node)
      continue;

    QJsonObject nodeObj;
    nodeObj["id"] = QString::number(node->nodeId());
    nodeObj["idString"] = node->nodeIdString();
    nodeObj["type"] = node->nodeType();
    nodeObj["title"] = node->title();
    nodeObj["x"] = node->pos().x();
    nodeObj["y"] = node->pos().y();

    // Dialogue-specific properties
    if (node->isDialogueNode()) {
      nodeObj["speaker"] = node->dialogueSpeaker();
      nodeObj["text"] = node->dialogueText();
    }

    nodesArray.append(nodeObj);
  }
  graphData["nodes"] = nodesArray;

  QJsonArray connectionsArray;
  for (const auto* conn : m_dialogueScene->connections()) {
    if (!conn || !conn->startNode() || !conn->endNode())
      continue;

    QJsonObject connObj;
    connObj["from"] = QString::number(conn->startNode()->nodeId());
    connObj["to"] = QString::number(conn->endNode()->nodeId());
    connectionsArray.append(connObj);
  }
  graphData["connections"] = connectionsArray;

  // Save to project folder: .novelmind/scenes/{sceneId}_dialogue.json
  const QString projectPath = QString::fromStdString(ProjectManager::instance().getProjectPath());
  const QString scenesDir = projectPath + "/.novelmind/scenes";

  std::filesystem::create_directories(scenesDir.toStdString());

  const QString filePath = QString("%1/%2_dialogue.json").arg(scenesDir, m_currentSceneId);

  QFile file(filePath);
  if (file.open(QIODevice::WriteOnly)) {
    file.write(QJsonDocument(graphData).toJson(QJsonDocument::Indented));
    file.close();
    m_hasUnsavedChanges.store(false, std::memory_order_relaxed);
    qDebug() << "[SceneDialogueGraph] Saved dialogue graph to:" << filePath;
  } else {
    const QString errorMsg = file.errorString();
    qWarning() << "[SceneDialogueGraph] Failed to save dialogue graph:" << filePath
               << "Error:" << errorMsg;
    NMMessageDialog::showError(this, tr("Save Failed"),
                               tr("Failed to save dialogue graph for scene '%1'.\n\n"
                                  "File: %2\n"
                                  "Reason: %3\n\n"
                                  "Please check file permissions and available disk space.")
                                   .arg(m_currentSceneId, filePath, errorMsg));
  }

  // Update dialogue count
  updateDialogueCount();
}

void NMSceneDialogueGraphPanel::loadDialogueGraphFromScene() {
  if (m_currentSceneId.isEmpty() || !m_dialogueScene) {
    return;
  }

  m_dialogueScene->clearGraph();
  m_nodeIdToString.clear();

  // Load from: .novelmind/scenes/{sceneId}_dialogue.json
  const QString projectPath = QString::fromStdString(ProjectManager::instance().getProjectPath());
  const QString filePath =
      QString("%1/.novelmind/scenes/%2_dialogue.json").arg(projectPath, m_currentSceneId);

  QFile file(filePath);
  if (!file.exists()) {
    qDebug() << "[SceneDialogueGraph] No dialogue graph file found for scene:" << m_currentSceneId;
    return;
  }

  if (!file.open(QIODevice::ReadOnly)) {
    const QString errorMsg = file.errorString();
    qWarning() << "[SceneDialogueGraph] Failed to open dialogue graph file:" << filePath
               << "Error:" << errorMsg;
    NMMessageDialog::showError(this, tr("Load Failed"),
                               tr("Failed to load dialogue graph for scene '%1'.\n\n"
                                  "File: %2\n"
                                  "Reason: %3\n\n"
                                  "Please check if the file exists and you have read permissions.")
                                   .arg(m_currentSceneId, filePath, errorMsg));
    return;
  }

  QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  file.close();

  if (!doc.isObject()) {
    qWarning() << "[SceneDialogueGraph] Invalid dialogue graph format";
    NMMessageDialog::showError(this, tr("Load Failed"),
                               tr("Failed to load dialogue graph for scene '%1'.\n\n"
                                  "File: %2\n"
                                  "Reason: Invalid JSON format\n\n"
                                  "The dialogue graph file may be corrupted. "
                                  "Try creating a new dialogue graph or restoring from a backup.")
                                   .arg(m_currentSceneId, filePath));
    return;
  }

  QJsonObject graphData = doc.object();

  // Load nodes
  QHash<QString, NMGraphNodeItem*> nodeMap;
  QJsonArray nodesArray = graphData["nodes"].toArray();
  for (const QJsonValue& nodeVal : nodesArray) {
    QJsonObject nodeObj = nodeVal.toObject();

    const QString idString = nodeObj["idString"].toString();
    const QString nodeType = nodeObj["type"].toString();
    const QString title = nodeObj["title"].toString();
    const QPointF pos(nodeObj["x"].toDouble(), nodeObj["y"].toDouble());
    const uint64_t nodeId = nodeObj["id"].toString().toULongLong();

    auto* node = m_dialogueScene->addNode(title, nodeType, pos, nodeId, idString);
    if (node) {
      // Set dialogue-specific properties
      if (node->isDialogueNode()) {
        node->setDialogueSpeaker(nodeObj["speaker"].toString());
        node->setDialogueText(nodeObj["text"].toString());
      }

      nodeMap.insert(idString, node);
      m_nodeIdToString.insert(nodeId, idString);
    }
  }

  // Load connections
  QJsonArray connectionsArray = graphData["connections"].toArray();
  for (const QJsonValue& connVal : connectionsArray) {
    QJsonObject connObj = connVal.toObject();

    const uint64_t fromId = connObj["from"].toString().toULongLong();
    const uint64_t toId = connObj["to"].toString().toULongLong();

    m_dialogueScene->addConnection(fromId, toId);
  }

  qDebug() << "[SceneDialogueGraph] Loaded dialogue graph for scene:" << m_currentSceneId << "with"
           << nodesArray.size() << "nodes and" << connectionsArray.size() << "connections";
}

void NMSceneDialogueGraphPanel::updateDialogueCount() {
  if (m_currentSceneId.isEmpty() || !m_dialogueScene) {
    return;
  }

  // Count Dialogue nodes
  int count = 0;
  for (const auto* node : m_dialogueScene->nodes()) {
    if (node && node->isDialogueNode()) {
      ++count;
    }
  }

  emit dialogueCountChanged(m_currentSceneId, count);
}

void NMSceneDialogueGraphPanel::markAsModified() {
  bool expected = false;
  if (m_hasUnsavedChanges.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
    // First modification - trigger auto-save
    saveDialogueGraphToScene();
  }
}

// ============================================================================
// Slots
// ============================================================================

void NMSceneDialogueGraphPanel::onZoomIn() {
  if (m_view) {
    m_view->setZoomLevel(m_view->zoomLevel() * 1.2);
  }
}

void NMSceneDialogueGraphPanel::onZoomOut() {
  if (m_view) {
    m_view->setZoomLevel(m_view->zoomLevel() / 1.2);
  }
}

void NMSceneDialogueGraphPanel::onZoomReset() {
  if (m_view) {
    m_view->setZoomLevel(1.0);
  }
}

void NMSceneDialogueGraphPanel::onFitToGraph() {
  if (m_view) {
    m_view->centerOnGraph();
  }
}

/**
 * @brief Improved auto-layout using Sugiyama-style hierarchical graph layout
 *
 * Issue #378: Complete the auto-layout implementation with:
 * - Proper Sugiyama algorithm with longest path layer assignment
 * - Edge crossing minimization using barycenter heuristic
 * - Proper coordinate assignment with all nodes in positive space
 * - User confirmation before rearranging
 *
 * This implementation follows the same algorithm as the Story Graph Panel
 * (see nm_story_graph_panel_handlers.cpp, Issue #326)
 */
void NMSceneDialogueGraphPanel::onAutoLayout() {
  if (!m_dialogueScene || m_dialogueScene->nodes().isEmpty()) {
    qDebug() << "[SceneDialogueGraph] No nodes to layout";
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

  qDebug() << "[SceneDialogueGraph] Running hierarchical auto-layout";

  const auto& nodes = m_dialogueScene->nodes();

  // Layout constants
  const qreal horizontalSpacing = 280.0;
  const qreal verticalSpacing = 160.0;
  const qreal startX = 100.0;
  const qreal startY = 100.0;

  // Build adjacency lists (forward and reverse)
  QHash<uint64_t, QList<uint64_t>> successors;
  QHash<uint64_t, QList<uint64_t>> predecessors;
  QHash<uint64_t, int> inDegree;
  QHash<uint64_t, int> outDegree;
  QSet<uint64_t> entryNodeSet;

  for (auto* node : nodes) {
    successors[node->nodeId()] = QList<uint64_t>();
    predecessors[node->nodeId()] = QList<uint64_t>();
    inDegree[node->nodeId()] = 0;
    outDegree[node->nodeId()] = 0;
  }

  for (auto* conn : m_dialogueScene->connections()) {
    uint64_t fromId = conn->startNode()->nodeId();
    uint64_t toId = conn->endNode()->nodeId();
    successors[fromId].append(toId);
    predecessors[toId].append(fromId);
    inDegree[toId]++;
    outDegree[fromId]++;
  }

  // ===== STEP 1: Find entry nodes (sources) =====
  // Entry nodes have no incoming edges
  QList<uint64_t> entryNodes;
  for (auto* node : nodes) {
    if (inDegree[node->nodeId()] == 0) {
      entryNodes.append(node->nodeId());
      entryNodeSet.insert(node->nodeId());
    }
  }

  // ===== STEP 2: Layer assignment using longest path =====
  // This ensures nodes are placed at the maximum depth from any source
  QHash<uint64_t, int> nodeLayers;
  QSet<uint64_t> visited;

  // Use topological sort with longest path calculation
  QList<uint64_t> topoOrder;
  QHash<uint64_t, int> tempInDegree = inDegree;
  QQueue<uint64_t> zeroInDegreeQueue;

  // Initialize queue with sources
  for (auto* node : nodes) {
    if (tempInDegree[node->nodeId()] == 0) {
      zeroInDegreeQueue.enqueue(node->nodeId());
      nodeLayers[node->nodeId()] = 0;
    }
  }

  // Process topologically
  while (!zeroInDegreeQueue.isEmpty()) {
    uint64_t nodeId = zeroInDegreeQueue.dequeue();
    topoOrder.append(nodeId);
    visited.insert(nodeId);

    for (uint64_t childId : successors[nodeId]) {
      // Update child's layer to max(current layer, parent layer + 1)
      int newLayer = nodeLayers[nodeId] + 1;
      if (!nodeLayers.contains(childId) || newLayer > nodeLayers[childId]) {
        nodeLayers[childId] = newLayer;
      }

      tempInDegree[childId]--;
      if (tempInDegree[childId] == 0) {
        zeroInDegreeQueue.enqueue(childId);
      }
    }
  }

  // Handle cycles: nodes not in topo order are part of cycles
  for (auto* node : nodes) {
    uint64_t id = node->nodeId();
    if (!visited.contains(id)) {
      // Find the minimum layer of any successor that's already placed
      int minSuccessorLayer = INT_MAX;
      for (uint64_t succId : successors[id]) {
        if (nodeLayers.contains(succId)) {
          minSuccessorLayer = qMin(minSuccessorLayer, nodeLayers[succId]);
        }
      }

      // Find the maximum layer of any predecessor that's already placed
      int maxPredecessorLayer = -1;
      for (uint64_t predId : predecessors[id]) {
        if (nodeLayers.contains(predId)) {
          maxPredecessorLayer = qMax(maxPredecessorLayer, nodeLayers[predId]);
        }
      }

      // Place between predecessor and successor layers if possible
      if (maxPredecessorLayer >= 0 && minSuccessorLayer < INT_MAX) {
        nodeLayers[id] = maxPredecessorLayer + 1;
      } else if (maxPredecessorLayer >= 0) {
        nodeLayers[id] = maxPredecessorLayer + 1;
      } else if (minSuccessorLayer < INT_MAX) {
        nodeLayers[id] = qMax(0, minSuccessorLayer - 1);
      } else {
        // Isolated node in cycle, place at layer 0
        nodeLayers[id] = 0;
      }
      visited.insert(id);
    }
  }

  // ===== STEP 3: Identify orphaned nodes (disconnected components) =====
  QList<uint64_t> orphanedNodes;
  QSet<uint64_t> connectedNodes;

  for (auto* node : nodes) {
    uint64_t id = node->nodeId();
    if (inDegree[id] == 0 && outDegree[id] == 0) {
      orphanedNodes.append(id);
    } else {
      connectedNodes.insert(id);
    }
  }

  // ===== STEP 4: Build layer-to-nodes mapping for connected nodes only =====
  QHash<int, QList<uint64_t>> layerNodes;
  int maxLayer = 0;

  for (uint64_t nodeId : connectedNodes) {
    int layer = nodeLayers[nodeId];
    layerNodes[layer].append(nodeId);
    maxLayer = qMax(maxLayer, layer);
  }

  // ===== STEP 5: Edge crossing minimization using barycenter heuristic =====
  // First, order the first layer: put entry nodes first, then by out-degree
  if (layerNodes.contains(0)) {
    QList<uint64_t>& layer0 = layerNodes[0];
    std::sort(layer0.begin(), layer0.end(), [&entryNodeSet, &outDegree](uint64_t a, uint64_t b) {
      // Entry nodes come first
      bool aIsEntry = entryNodeSet.contains(a);
      bool bIsEntry = entryNodeSet.contains(b);
      if (aIsEntry != bIsEntry) {
        return aIsEntry;
      }
      // Then sort by out-degree (more connections = more central)
      return outDegree[a] > outDegree[b];
    });
  }

  // Apply barycenter heuristic for subsequent layers
  const int iterations = 4;
  for (int iter = 0; iter < iterations; ++iter) {
    // Forward sweep (layer 1 to maxLayer)
    for (int layer = 1; layer <= maxLayer; ++layer) {
      if (!layerNodes.contains(layer)) {
        continue;
      }

      QList<uint64_t>& currentLayer = layerNodes[layer];
      QHash<uint64_t, qreal> barycenter;

      // Calculate barycenter for each node based on predecessor positions
      for (uint64_t nodeId : currentLayer) {
        qreal sum = 0.0;
        int count = 0;

        for (uint64_t predId : predecessors[nodeId]) {
          if (nodeLayers.contains(predId) && nodeLayers[predId] == layer - 1 &&
              layerNodes.contains(layer - 1)) {
            int predIndex = static_cast<int>(layerNodes[layer - 1].indexOf(predId));
            if (predIndex >= 0) {
              sum += predIndex;
              ++count;
            }
          }
        }

        if (count > 0) {
          barycenter[nodeId] = sum / count;
        } else {
          barycenter[nodeId] = static_cast<qreal>(currentLayer.indexOf(nodeId));
        }
      }

      // Sort by barycenter
      std::sort(currentLayer.begin(), currentLayer.end(),
                [&barycenter](uint64_t a, uint64_t b) { return barycenter[a] < barycenter[b]; });
    }

    // Backward sweep (maxLayer-1 down to 0)
    for (int layer = maxLayer - 1; layer >= 0; --layer) {
      if (!layerNodes.contains(layer)) {
        continue;
      }

      QList<uint64_t>& currentLayer = layerNodes[layer];
      QHash<uint64_t, qreal> barycenter;

      // Calculate barycenter for each node based on successor positions
      for (uint64_t nodeId : currentLayer) {
        qreal sum = 0.0;
        int count = 0;

        for (uint64_t succId : successors[nodeId]) {
          if (nodeLayers.contains(succId) && nodeLayers[succId] == layer + 1 &&
              layerNodes.contains(layer + 1)) {
            int succIndex = static_cast<int>(layerNodes[layer + 1].indexOf(succId));
            if (succIndex >= 0) {
              sum += succIndex;
              ++count;
            }
          }
        }

        if (count > 0) {
          barycenter[nodeId] = sum / count;
        } else {
          barycenter[nodeId] = static_cast<qreal>(currentLayer.indexOf(nodeId));
        }
      }

      // Sort by barycenter
      std::sort(currentLayer.begin(), currentLayer.end(),
                [&barycenter](uint64_t a, uint64_t b) { return barycenter[a] < barycenter[b]; });
    }
  }

  // Handle any remaining unvisited nodes
  for (auto* node : nodes) {
    if (!visited.contains(node->nodeId())) {
      maxLayer++;
      nodeLayers[node->nodeId()] = maxLayer;
      layerNodes[maxLayer].append(node->nodeId());
    }
  }

  // ===== STEP 6: Position nodes =====
  for (int layer : layerNodes.keys()) {
    const auto& nodesInLayer = layerNodes[layer];
    qreal y = startY + layer * verticalSpacing;
    qreal totalWidth = static_cast<qreal>(nodesInLayer.size() - 1) * horizontalSpacing;
    qreal x = startX - totalWidth / 2.0;

    for (int i = 0; i < nodesInLayer.size(); ++i) {
      uint64_t nodeId = nodesInLayer[i];
      auto* node = m_dialogueScene->findNode(nodeId);
      if (node) {
        node->setPos(x + i * horizontalSpacing, y);
      }
    }
  }

  // Update all connection paths
  for (auto* conn : m_dialogueScene->connections()) {
    conn->updatePath();
  }

  // Center the view on the laid out graph
  if (m_view) {
    m_view->centerOnGraph();
  }

  markAsModified();
  qDebug() << "[SceneDialogueGraph] Auto-layout complete:" << layerNodes.size() << "layers,"
           << m_dialogueScene->nodes().size() << "nodes";
}

void NMSceneDialogueGraphPanel::onReturnToStoryGraph() {
  if (m_hasUnsavedChanges) {
    saveDialogueGraphToScene();
  }
  emit returnToStoryGraphRequested();
}

void NMSceneDialogueGraphPanel::onNodeTypeSelected(const QString& nodeType) {
  createNode(nodeType);
}

void NMSceneDialogueGraphPanel::onNodeAdded(uint64_t nodeId, const QString& nodeIdString,
                                            const QString& /*nodeType*/) {
  m_nodeIdToString.insert(nodeId, nodeIdString);
  markAsModified();
}

void NMSceneDialogueGraphPanel::onNodeDeleted(uint64_t nodeId) {
  m_nodeIdToString.remove(nodeId);
  markAsModified();
}

void NMSceneDialogueGraphPanel::onConnectionAdded(uint64_t /*fromNodeId*/, uint64_t /*toNodeId*/) {
  markAsModified();
}

void NMSceneDialogueGraphPanel::onConnectionDeleted(uint64_t /*fromNodeId*/,
                                                    uint64_t /*toNodeId*/) {
  markAsModified();
}

void NMSceneDialogueGraphPanel::onRequestConnection(uint64_t fromNodeId, uint64_t toNodeId) {
  if (m_dialogueScene) {
    m_dialogueScene->addConnection(fromNodeId, toNodeId);
  }
}

void NMSceneDialogueGraphPanel::onDeleteSelected() {
  // Delete selected nodes and connections
  if (!m_dialogueScene) {
    return;
  }

  QList<NMGraphNodeItem*> selectedNodes;
  for (auto* item : m_dialogueScene->selectedItems()) {
    if (auto* node = qgraphicsitem_cast<NMGraphNodeItem*>(item)) {
      selectedNodes.append(node);
    }
  }

  for (auto* node : selectedNodes) {
    m_dialogueScene->removeNode(node);
  }

  if (!selectedNodes.isEmpty()) {
    markAsModified();
  }
}

void NMSceneDialogueGraphPanel::onNodesMoved(const QVector<GraphNodeMove>& /*moves*/) {
  markAsModified();
}

} // namespace NovelMind::editor::qt
