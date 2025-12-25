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
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>
#include <filesystem>

namespace NovelMind::editor::qt {

// ============================================================================
// NMSceneDialogueNodePalette
// ============================================================================

NMSceneDialogueNodePalette::NMSceneDialogueNodePalette(QWidget *parent)
    : QWidget(parent) {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(4, 4, 4, 4);
  layout->setSpacing(4);

  auto *label = new QLabel(tr("Dialogue Nodes"), this);
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

void NMSceneDialogueNodePalette::createNodeButton(const QString &nodeType,
                                                  const QString &icon) {
  auto *btn = new QPushButton(NMIconManager::instance().getIcon(icon, 16),
                              nodeType, this);
  btn->setFlat(true);
  btn->setStyleSheet("QPushButton { text-align: left; padding: 6px; } "
                     "QPushButton:hover { background: #3a3a3a; }");
  connect(btn, &QPushButton::clicked, this,
          [this, nodeType]() { emit nodeTypeSelected(nodeType); });
  qobject_cast<QVBoxLayout *>(layout())->addWidget(btn);
}

// ============================================================================
// NMSceneDialogueGraphScene
// ============================================================================

NMSceneDialogueGraphScene::NMSceneDialogueGraphScene(QObject *parent)
    : NMStoryGraphScene(parent) {}

bool NMSceneDialogueGraphScene::isAllowedNodeType(const QString &nodeType) {
  // Allow dialogue-related nodes, but not Scene nodes
  static const QStringList allowedTypes = {
      "Dialogue",      "Choice",         "ChoiceOption", "Wait",
      "SetVariable",   "GetVariable",    "PlaySound",    "ShowCharacter",
      "HideCharacter", "ShowBackground", "Transition",   "PlayMusic",
      "StopMusic",     "Expression",     "SceneStart",   "SceneEnd",
      "Comment"};
  return allowedTypes.contains(nodeType, Qt::CaseInsensitive);
}

NMGraphNodeItem *NMSceneDialogueGraphScene::addNode(
    const QString &title, const QString &nodeType, const QPointF &pos,
    uint64_t nodeId, const QString &nodeIdString) {
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

NMSceneDialogueGraphPanel::NMSceneDialogueGraphPanel(QWidget *parent)
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
    connect(m_dialogueScene, &NMSceneDialogueGraphScene::connectionDeleted,
            this, &NMSceneDialogueGraphPanel::onConnectionDeleted);
    connect(m_dialogueScene,
            &NMSceneDialogueGraphScene::deleteSelectionRequested, this,
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

void NMSceneDialogueGraphPanel::loadSceneDialogue(const QString &sceneId) {
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

  auto &iconMgr = NMIconManager::instance();

  QAction *actionConnect =
      m_toolBar->addAction(iconMgr.getIcon("connection", 16), tr("Connect"));
  actionConnect->setToolTip(tr("Connection mode"));
  actionConnect->setCheckable(true);
  connect(actionConnect, &QAction::toggled, this, [this](bool enabled) {
    if (m_view) {
      m_view->setConnectionModeEnabled(enabled);
    }
  });

  m_toolBar->addSeparator();

  QAction *actionZoomIn =
      m_toolBar->addAction(iconMgr.getIcon("zoom-in", 16), tr("Zoom In"));
  connect(actionZoomIn, &QAction::triggered, this,
          &NMSceneDialogueGraphPanel::onZoomIn);

  QAction *actionZoomOut =
      m_toolBar->addAction(iconMgr.getIcon("zoom-out", 16), tr("Zoom Out"));
  connect(actionZoomOut, &QAction::triggered, this,
          &NMSceneDialogueGraphPanel::onZoomOut);

  QAction *actionZoomReset =
      m_toolBar->addAction(iconMgr.getIcon("zoom-reset", 16), tr("Reset Zoom"));
  connect(actionZoomReset, &QAction::triggered, this,
          &NMSceneDialogueGraphPanel::onZoomReset);

  QAction *actionFit =
      m_toolBar->addAction(iconMgr.getIcon("zoom-fit", 16), tr("Fit"));
  connect(actionFit, &QAction::triggered, this,
          &NMSceneDialogueGraphPanel::onFitToGraph);

  m_toolBar->addSeparator();

  QAction *actionAutoLayout = m_toolBar->addAction(
      iconMgr.getIcon("layout-auto", 16), tr("Auto Layout"));
  connect(actionAutoLayout, &QAction::triggered, this,
          &NMSceneDialogueGraphPanel::onAutoLayout);

  // Note: Toolbar is set in the parent QDockWidget, not explicitly needed here
}

void NMSceneDialogueGraphPanel::setupContent() {
  m_contentWidget = new QWidget(this);
  auto *mainLayout = new QVBoxLayout(m_contentWidget);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // Create graph scene and view
  m_dialogueScene = new NMSceneDialogueGraphScene(this);
  m_view = new NMStoryGraphView(m_contentWidget);
  m_view->setScene(m_dialogueScene);

  // Apply distinct visual styling for dialogue graph
  const auto &palette = NMStyleManager::instance().palette();
  m_view->setStyleSheet(QString("QGraphicsView { background: %1; }")
                            .arg(palette.bgDark.darker(105).name()));

  // Create minimap
  m_minimap = new NMStoryGraphMinimap(m_contentWidget);
  m_minimap->setMainView(m_view);

  // Create node palette
  m_nodePalette = new NMSceneDialogueNodePalette(m_contentWidget);
  connect(m_nodePalette, &NMSceneDialogueNodePalette::nodeTypeSelected, this,
          &NMSceneDialogueGraphPanel::onNodeTypeSelected);

  // Layout: [Palette | View] with minimap overlay
  auto *graphLayout = new QHBoxLayout();
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
  auto *breadcrumbLayout = new QHBoxLayout(m_breadcrumbWidget);
  breadcrumbLayout->setContentsMargins(8, 4, 8, 4);
  breadcrumbLayout->setSpacing(8);

  auto &iconMgr = NMIconManager::instance();

  // Return button
  m_returnButton = new QPushButton(iconMgr.getIcon("arrow-left", 16),
                                   tr("Story Graph"), m_breadcrumbWidget);
  m_returnButton->setFlat(true);
  m_returnButton->setStyleSheet("QPushButton { padding: 4px 8px; } "
                                "QPushButton:hover { background: #3a3a3a; }");
  connect(m_returnButton, &QPushButton::clicked, this,
          &NMSceneDialogueGraphPanel::onReturnToStoryGraph);
  breadcrumbLayout->addWidget(m_returnButton);

  // Breadcrumb separator
  auto *separator = new QLabel(tr(">"), m_breadcrumbWidget);
  separator->setStyleSheet("color: #888; padding: 0 4px;");
  breadcrumbLayout->addWidget(separator);

  // Scene name label
  m_breadcrumbLabel = new QLabel(m_breadcrumbWidget);
  m_breadcrumbLabel->setStyleSheet("font-weight: bold; color: #64C896;");
  breadcrumbLayout->addWidget(m_breadcrumbLabel);

  breadcrumbLayout->addStretch();

  // Insert breadcrumb at top of content
  if (m_contentWidget && m_contentWidget->layout()) {
    qobject_cast<QVBoxLayout *>(m_contentWidget->layout())
        ->insertWidget(0, m_breadcrumbWidget);
  }

  updateBreadcrumb();
}

void NMSceneDialogueGraphPanel::updateBreadcrumb() {
  if (m_breadcrumbLabel) {
    if (m_currentSceneId.isEmpty()) {
      m_breadcrumbLabel->setText(tr("No scene loaded"));
    } else {
      m_breadcrumbLabel->setText(
          tr("Scene: %1 > Dialogue Flow").arg(m_currentSceneId));
    }
  }
}

void NMSceneDialogueGraphPanel::createNode(const QString &nodeType) {
  if (!m_dialogueScene || !m_view) {
    return;
  }

  if (m_currentSceneId.isEmpty()) {
    NMMessageDialog::showWarning(
        this, tr("No Scene Loaded"),
        tr("Please load a scene first before adding nodes."));
    return;
  }

  // Get center of view
  const QPointF sceneCenter =
      m_view->mapToScene(m_view->viewport()->rect().center());

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
  for (const auto *node : m_dialogueScene->nodes()) {
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
  for (const auto *conn : m_dialogueScene->connections()) {
    if (!conn || !conn->startNode() || !conn->endNode())
      continue;

    QJsonObject connObj;
    connObj["from"] = QString::number(conn->startNode()->nodeId());
    connObj["to"] = QString::number(conn->endNode()->nodeId());
    connectionsArray.append(connObj);
  }
  graphData["connections"] = connectionsArray;

  // Save to project folder: .novelmind/scenes/{sceneId}_dialogue.json
  const QString projectPath =
      QString::fromStdString(ProjectManager::instance().getProjectPath());
  const QString scenesDir = projectPath + "/.novelmind/scenes";

  std::filesystem::create_directories(scenesDir.toStdString());

  const QString filePath =
      QString("%1/%2_dialogue.json").arg(scenesDir, m_currentSceneId);

  QFile file(filePath);
  if (file.open(QIODevice::WriteOnly)) {
    file.write(QJsonDocument(graphData).toJson(QJsonDocument::Indented));
    file.close();
    m_hasUnsavedChanges.store(false, std::memory_order_relaxed);
    qDebug() << "[SceneDialogueGraph] Saved dialogue graph to:" << filePath;
  } else {
    qWarning() << "[SceneDialogueGraph] Failed to save dialogue graph:"
               << filePath;
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
  const QString projectPath =
      QString::fromStdString(ProjectManager::instance().getProjectPath());
  const QString filePath = QString("%1/.novelmind/scenes/%2_dialogue.json")
                               .arg(projectPath, m_currentSceneId);

  QFile file(filePath);
  if (!file.exists()) {
    qDebug() << "[SceneDialogueGraph] No dialogue graph file found for scene:"
             << m_currentSceneId;
    return;
  }

  if (!file.open(QIODevice::ReadOnly)) {
    qWarning() << "[SceneDialogueGraph] Failed to open dialogue graph file:"
               << filePath;
    return;
  }

  QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  file.close();

  if (!doc.isObject()) {
    qWarning() << "[SceneDialogueGraph] Invalid dialogue graph format";
    return;
  }

  QJsonObject graphData = doc.object();

  // Load nodes
  QHash<QString, NMGraphNodeItem *> nodeMap;
  QJsonArray nodesArray = graphData["nodes"].toArray();
  for (const QJsonValue &nodeVal : nodesArray) {
    QJsonObject nodeObj = nodeVal.toObject();

    const QString idString = nodeObj["idString"].toString();
    const QString nodeType = nodeObj["type"].toString();
    const QString title = nodeObj["title"].toString();
    const QPointF pos(nodeObj["x"].toDouble(), nodeObj["y"].toDouble());
    const uint64_t nodeId = nodeObj["id"].toString().toULongLong();

    auto *node =
        m_dialogueScene->addNode(title, nodeType, pos, nodeId, idString);
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
  for (const QJsonValue &connVal : connectionsArray) {
    QJsonObject connObj = connVal.toObject();

    const uint64_t fromId = connObj["from"].toString().toULongLong();
    const uint64_t toId = connObj["to"].toString().toULongLong();

    m_dialogueScene->addConnection(fromId, toId);
  }

  qDebug() << "[SceneDialogueGraph] Loaded dialogue graph for scene:"
           << m_currentSceneId << "with" << nodesArray.size() << "nodes and"
           << connectionsArray.size() << "connections";
}

void NMSceneDialogueGraphPanel::updateDialogueCount() {
  if (m_currentSceneId.isEmpty() || !m_dialogueScene) {
    return;
  }

  // Count Dialogue nodes
  int count = 0;
  for (const auto *node : m_dialogueScene->nodes()) {
    if (node && node->isDialogueNode()) {
      ++count;
    }
  }

  emit dialogueCountChanged(m_currentSceneId, count);
}

void NMSceneDialogueGraphPanel::markAsModified() {
  bool expected = false;
  if (m_hasUnsavedChanges.compare_exchange_strong(expected, true,
                                                  std::memory_order_relaxed)) {
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

void NMSceneDialogueGraphPanel::onAutoLayout() {
  if (!m_dialogueScene || m_dialogueScene->nodes().isEmpty()) {
    qDebug() << "[SceneDialogueGraph] No nodes to layout";
    return;
  }

  qDebug() << "[SceneDialogueGraph] Running hierarchical auto-layout";

  // Hierarchical layered graph layout algorithm (Sugiyama-style)
  const qreal LAYER_SPACING = 200.0;
  const qreal NODE_SPACING = 80.0;
  const qreal START_X = 100.0;
  const qreal START_Y = 100.0;

  // Step 1: Find entry nodes (nodes with no incoming connections)
  QList<NMGraphNodeItem *> entryNodes;
  QHash<NMGraphNodeItem *, int> inDegree;
  QHash<NMGraphNodeItem *, QList<NMGraphNodeItem *>> children;

  // Initialize in-degree map
  for (auto *node : m_dialogueScene->nodes()) {
    inDegree[node] = 0;
  }

  // Count in-degrees and build children map
  for (auto *conn : m_dialogueScene->connections()) {
    auto *startNode = conn->startNode();
    auto *endNode = conn->endNode();
    if (startNode && endNode) {
      inDegree[endNode]++;
      children[startNode].append(endNode);
    }
  }

  // Find entry nodes
  for (auto *node : m_dialogueScene->nodes()) {
    if (inDegree[node] == 0) {
      entryNodes.append(node);
    }
  }

  // If no entry nodes found, use the first node
  if (entryNodes.isEmpty() && !m_dialogueScene->nodes().isEmpty()) {
    entryNodes.append(m_dialogueScene->nodes().first());
  }

  // Step 2: Assign nodes to layers using BFS from entry nodes
  QHash<NMGraphNodeItem *, int> nodeLayer;
  QHash<int, QList<NMGraphNodeItem *>> layers;
  QSet<NMGraphNodeItem *> visited;
  QList<NMGraphNodeItem *> queue;

  // Start BFS from all entry nodes
  for (auto *entry : entryNodes) {
    nodeLayer[entry] = 0;
    layers[0].append(entry);
    queue.append(entry);
    visited.insert(entry);
  }

  while (!queue.isEmpty()) {
    auto *current = queue.takeFirst();
    int currentLayer = nodeLayer[current];

    // Process all children
    for (auto *child : children[current]) {
      if (!visited.contains(child)) {
        int childLayer = currentLayer + 1;
        nodeLayer[child] = childLayer;
        layers[childLayer].append(child);
        queue.append(child);
        visited.insert(child);
      }
    }
  }

  // Handle any disconnected nodes
  for (auto *node : m_dialogueScene->nodes()) {
    if (!visited.contains(node)) {
      int layer = layers.keys().isEmpty() ? 0 : layers.keys().last() + 1;
      nodeLayer[node] = layer;
      layers[layer].append(node);
    }
  }

  // Step 3: Position nodes based on their layer
  for (int layer : layers.keys()) {
    const QList<NMGraphNodeItem *> &nodesInLayer = layers[layer];
    const qreal layerY = START_Y + layer * LAYER_SPACING;
    const qreal totalWidth =
        (static_cast<qreal>(nodesInLayer.size()) - 1.0) * NODE_SPACING;
    const qreal startX = START_X - totalWidth / 2.0;

    for (int i = 0; i < nodesInLayer.size(); ++i) {
      auto *node = nodesInLayer[i];
      const qreal x = startX + i * NODE_SPACING;
      node->setPos(x, layerY);
    }
  }

  // Step 4: Center the view on the laid out graph
  if (m_view) {
    m_view->centerOnGraph();
  }

  markAsModified();
  qDebug() << "[SceneDialogueGraph] Auto-layout complete:" << layers.size()
           << "layers," << m_dialogueScene->nodes().size() << "nodes";
}

void NMSceneDialogueGraphPanel::onReturnToStoryGraph() {
  if (m_hasUnsavedChanges) {
    saveDialogueGraphToScene();
  }
  emit returnToStoryGraphRequested();
}

void NMSceneDialogueGraphPanel::onNodeTypeSelected(const QString &nodeType) {
  createNode(nodeType);
}

void NMSceneDialogueGraphPanel::onNodeAdded(uint64_t nodeId,
                                            const QString &nodeIdString,
                                            const QString & /*nodeType*/) {
  m_nodeIdToString.insert(nodeId, nodeIdString);
  markAsModified();
}

void NMSceneDialogueGraphPanel::onNodeDeleted(uint64_t nodeId) {
  m_nodeIdToString.remove(nodeId);
  markAsModified();
}

void NMSceneDialogueGraphPanel::onConnectionAdded(uint64_t /*fromNodeId*/,
                                                  uint64_t /*toNodeId*/) {
  markAsModified();
}

void NMSceneDialogueGraphPanel::onConnectionDeleted(uint64_t /*fromNodeId*/,
                                                    uint64_t /*toNodeId*/) {
  markAsModified();
}

void NMSceneDialogueGraphPanel::onRequestConnection(uint64_t fromNodeId,
                                                    uint64_t toNodeId) {
  if (m_dialogueScene) {
    m_dialogueScene->addConnection(fromNodeId, toNodeId);
  }
}

void NMSceneDialogueGraphPanel::onDeleteSelected() {
  // Delete selected nodes and connections
  if (!m_dialogueScene) {
    return;
  }

  QList<NMGraphNodeItem *> selectedNodes;
  for (auto *item : m_dialogueScene->selectedItems()) {
    if (auto *node = qgraphicsitem_cast<NMGraphNodeItem *>(item)) {
      selectedNodes.append(node);
    }
  }

  for (auto *node : selectedNodes) {
    m_dialogueScene->removeNode(node);
  }

  if (!selectedNodes.isEmpty()) {
    markAsModified();
  }
}

void NMSceneDialogueGraphPanel::onNodesMoved(
    const QVector<GraphNodeMove> & /*moves*/) {
  markAsModified();
}

} // namespace NovelMind::editor::qt
