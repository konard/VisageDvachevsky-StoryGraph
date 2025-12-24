#include "NovelMind/editor/qt/panels/nm_story_graph_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_story_graph_minimap.hpp"
#include "NovelMind/editor/qt/nm_play_mode_controller.hpp"
#include "NovelMind/editor/project_manager.hpp"

#include <QAction>
#include <QFile>
#include <QHBoxLayout>
#include <QList>
#include <QPair>
#include <QRegularExpression>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>
#include <filesystem>

#include "nm_story_graph_panel_detail.hpp"

namespace NovelMind::editor::qt {

// ============================================================================
// NMStoryGraphPanel
// ============================================================================

NMStoryGraphPanel::NMStoryGraphPanel(QWidget *parent)
    : NMDockPanel(tr("Story Graph"), parent) {
  setPanelId("StoryGraph");
  setupContent();
  setupToolBar();
  setupNodePalette();
}

NMStoryGraphPanel::~NMStoryGraphPanel() = default;

void NMStoryGraphPanel::onInitialize() {
  if (m_scene) {
    m_scene->clearGraph();
  }

  if (m_view) {
    m_view->centerOnGraph();
  }

  // Connect to Play Mode Controller signals with queued connection
  // This defers slot execution to next event loop iteration, avoiding
  // crashes from synchronous signal emission during state transitions
  auto &playController = NMPlayModeController::instance();
  connect(&playController, &NMPlayModeController::currentNodeChanged, this,
          &NMStoryGraphPanel::onCurrentNodeChanged, Qt::QueuedConnection);
  connect(&playController, &NMPlayModeController::breakpointsChanged, this,
          &NMStoryGraphPanel::onBreakpointsChanged, Qt::QueuedConnection);
  connect(&playController, &NMPlayModeController::projectLoaded, this,
          &NMStoryGraphPanel::rebuildFromProjectScripts, Qt::QueuedConnection);

  rebuildFromProjectScripts();
}

void NMStoryGraphPanel::onUpdate(double /*deltaTime*/) {
  // Update connections when nodes move
  // For now, this is handled reactively
}

void NMStoryGraphPanel::rebuildFromProjectScripts() {
  if (!m_scene) {
    return;
  }

  m_isRebuilding = true;
  detail::loadGraphLayout(m_layoutNodes, m_layoutEntryScene);

  m_scene->clearGraph();
  m_currentExecutingNode.clear();
  m_nodeIdToString.clear();

  const QString scriptsRoot = QString::fromStdString(
      ProjectManager::instance().getFolderPath(ProjectFolder::Scripts));
  if (scriptsRoot.isEmpty()) {
    m_isRebuilding = false;
    return;
  }

  namespace fs = std::filesystem;
  fs::path base(scriptsRoot.toStdString());
  if (!fs::exists(base)) {
    m_isRebuilding = false;
    return;
  }

  QHash<QString, QString> sceneToFile;
  QList<QPair<QString, QString>> edges;

  const QRegularExpression sceneRe(
      "\\bscene\\s+([A-Za-z_][A-Za-z0-9_]*)");
  const QRegularExpression gotoRe(
      "\\bgoto\\s+([A-Za-z_][A-Za-z0-9_]*)");
  const QRegularExpression choiceRe(
      "\"([^\"]+)\"\\s*->\\s*([A-Za-z_][A-Za-z0-9_]*)");

  for (const auto &entry : fs::recursive_directory_iterator(base)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".nms") {
      continue;
    }

    QFile file(QString::fromStdString(entry.path().string()));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      continue;
    }
    const QString content = QString::fromUtf8(file.readAll());
    file.close();

    const QString relPath = QString::fromStdString(
        ProjectManager::instance().toRelativePath(
            entry.path().string()));

    // Collect scene blocks with ranges
    struct SceneBlock {
      QString id;
      qsizetype start = 0;
      qsizetype end = 0;
    };
    QVector<SceneBlock> blocks;
    QRegularExpressionMatchIterator it = sceneRe.globalMatch(content);
    while (it.hasNext()) {
      const QRegularExpressionMatch match = it.next();
      SceneBlock block;
      block.id = match.captured(1);
      block.start = match.capturedStart();
      blocks.push_back(block);
      if (!sceneToFile.contains(block.id)) {
        sceneToFile.insert(block.id, relPath);
      }
    }
    for (int i = 0; i < blocks.size(); ++i) {
      blocks[i].end =
          (i + 1 < blocks.size()) ? blocks[i + 1].start : content.size();
    }

    // For each scene block, gather transitions
    for (const auto &block : blocks) {
      const QString slice = content.mid(block.start, block.end - block.start);

      QRegularExpressionMatchIterator gotoIt = gotoRe.globalMatch(slice);
      while (gotoIt.hasNext()) {
        const QString target = gotoIt.next().captured(1);
        if (!target.isEmpty()) {
          edges.append({block.id, target});
        }
      }

      QRegularExpressionMatchIterator choiceIt =
          choiceRe.globalMatch(slice);
      while (choiceIt.hasNext()) {
        const QString target = choiceIt.next().captured(2);
        if (!target.isEmpty()) {
          edges.append({block.id, target});
        }
      }
    }
  }

  // Build nodes
  QHash<QString, NMGraphNodeItem *> nodeMap;
  int index = 0;
  auto ensureNode = [&](const QString &sceneId) -> NMGraphNodeItem * {
    if (nodeMap.contains(sceneId)) {
      return nodeMap.value(sceneId);
    }
    const int col = index % 4;
    const int row = index / 4;
    const QPointF defaultPos(col * 260.0, row * 140.0);
    const auto layoutIt = m_layoutNodes.constFind(sceneId);
    const QPointF pos =
        (layoutIt != m_layoutNodes.constEnd()) ? layoutIt->position
                                               : defaultPos;
    const QString type =
        (layoutIt != m_layoutNodes.constEnd() && !layoutIt->type.isEmpty())
            ? layoutIt->type
            : QString("Scene");
    const QString title =
        (layoutIt != m_layoutNodes.constEnd() && !layoutIt->title.isEmpty())
            ? layoutIt->title
            : sceneId;
    auto *node = m_scene->addNode(title, type, pos, 0, sceneId);
    if (node) {
      if (sceneToFile.contains(sceneId)) {
        node->setScriptPath(sceneToFile.value(sceneId));
      } else if (layoutIt != m_layoutNodes.constEnd() &&
                 !layoutIt->scriptPath.isEmpty()) {
        node->setScriptPath(layoutIt->scriptPath);
      }
      if (layoutIt != m_layoutNodes.constEnd()) {
        node->setDialogueSpeaker(layoutIt->speaker);
        node->setDialogueText(layoutIt->dialogueText);
        node->setChoiceOptions(layoutIt->choices);
        // Scene Node specific properties
        node->setSceneId(layoutIt->sceneId);
        node->setHasEmbeddedDialogue(layoutIt->hasEmbeddedDialogue);
        node->setDialogueCount(layoutIt->dialogueCount);
        node->setThumbnailPath(layoutIt->thumbnailPath);
      }
      nodeMap.insert(sceneId, node);
      m_nodeIdToString.insert(node->nodeId(), node->nodeIdString());
      m_layoutNodes.insert(sceneId, detail::buildLayoutFromNode(node));
    }
    ++index;
    return node;
  };

  for (auto it = sceneToFile.constBegin(); it != sceneToFile.constEnd(); ++it) {
    ensureNode(it.key());
  }

  for (auto it = m_layoutNodes.constBegin(); it != m_layoutNodes.constEnd();
       ++it) {
    ensureNode(it.key());
  }

  // Build edges
  for (const auto &edge : edges) {
    auto *from = ensureNode(edge.first);
    auto *to = ensureNode(edge.second);
    if (from && to) {
      m_scene->addConnection(from, to);
    }
  }

  // Resolve entry scene from project metadata if available
  const QString projectEntry =
      QString::fromStdString(ProjectManager::instance().getStartScene());
  if (!projectEntry.isEmpty()) {
    m_layoutEntryScene = projectEntry;
  } else if (!m_layoutEntryScene.isEmpty()) {
    ProjectManager::instance().setStartScene(m_layoutEntryScene.toStdString());
  }

  // Update entry highlight
  for (auto *item : m_scene->items()) {
    if (auto *node = qgraphicsitem_cast<NMGraphNodeItem *>(item)) {
      node->setEntry(!m_layoutEntryScene.isEmpty() &&
                     node->nodeIdString() == m_layoutEntryScene);
    }
  }

  if (m_view && !m_scene->items().isEmpty()) {
    m_view->centerOnGraph();
  }

  updateNodeBreakpoints();
  updateCurrentNode(QString());

  detail::saveGraphLayout(m_layoutNodes, m_layoutEntryScene);
  m_isRebuilding = false;
}

void NMStoryGraphPanel::setupToolBar() {
  m_toolBar = new QToolBar(this);
  m_toolBar->setObjectName("StoryGraphToolBar");
  m_toolBar->setIconSize(QSize(16, 16));

  QAction *actionConnect = m_toolBar->addAction(tr("Connect"));
  actionConnect->setToolTip(tr("Connection mode"));
  actionConnect->setCheckable(true);
  connect(actionConnect, &QAction::toggled, this, [this](bool enabled) {
    if (m_view) {
      m_view->setConnectionModeEnabled(enabled);
    }
  });

  m_toolBar->addSeparator();

  QAction *actionZoomIn = m_toolBar->addAction(tr("+"));
  actionZoomIn->setToolTip(tr("Zoom In"));
  connect(actionZoomIn, &QAction::triggered, this,
          &NMStoryGraphPanel::onZoomIn);

  QAction *actionZoomOut = m_toolBar->addAction(tr("-"));
  actionZoomOut->setToolTip(tr("Zoom Out"));
  connect(actionZoomOut, &QAction::triggered, this,
          &NMStoryGraphPanel::onZoomOut);

  QAction *actionZoomReset = m_toolBar->addAction(tr("1:1"));
  actionZoomReset->setToolTip(tr("Reset Zoom"));
  connect(actionZoomReset, &QAction::triggered, this,
          &NMStoryGraphPanel::onZoomReset);

  QAction *actionFit = m_toolBar->addAction(tr("Fit"));
  actionFit->setToolTip(tr("Fit Graph to View"));
  connect(actionFit, &QAction::triggered, this,
          &NMStoryGraphPanel::onFitToGraph);

  m_toolBar->addSeparator();

  QAction *actionAutoLayout = m_toolBar->addAction(tr("Auto Layout"));
  actionAutoLayout->setToolTip(tr("Automatically arrange nodes (hierarchical layout)"));
  connect(actionAutoLayout, &QAction::triggered, this,
          &NMStoryGraphPanel::onAutoLayout);

  if (auto *layout = qobject_cast<QVBoxLayout *>(m_contentWidget->layout())) {
    layout->insertWidget(0, m_toolBar);
  }
}

void NMStoryGraphPanel::setupContent() {
  m_contentWidget = new QWidget(this);
  auto *mainLayout = new QVBoxLayout(m_contentWidget);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // Create horizontal layout for node palette + graph view
  auto *hLayout = new QHBoxLayout();
  hLayout->setContentsMargins(0, 0, 0, 0);
  hLayout->setSpacing(4);

  m_scene = new NMStoryGraphScene(this);
  m_view = new NMStoryGraphView(m_contentWidget);
  m_view->setScene(m_scene);

  hLayout->addWidget(m_view, 1); // Graph view takes most space

  // Create vertical layout for minimap on the right
  auto *rightLayout = new QVBoxLayout();
  rightLayout->setContentsMargins(0, 0, 0, 0);
  rightLayout->setSpacing(4);

  // Create minimap
  m_minimap = new NMStoryGraphMinimap(m_contentWidget);
  m_minimap->setGraphScene(m_scene);
  m_minimap->setMainView(m_view);

  rightLayout->addWidget(m_minimap);
  rightLayout->addStretch(); // Push minimap to top

  hLayout->addLayout(rightLayout);

  mainLayout->addLayout(hLayout);

  setContentWidget(m_contentWidget);

  // Connect view signals
  connect(m_view, &NMStoryGraphView::requestConnection, this,
          &NMStoryGraphPanel::onRequestConnection);
  connect(m_view, &NMStoryGraphView::nodeClicked, this,
          &NMStoryGraphPanel::onNodeClicked);
  connect(m_view, &NMStoryGraphView::nodeDoubleClicked, this,
          &NMStoryGraphPanel::onNodeDoubleClicked);
  connect(m_scene, &NMStoryGraphScene::deleteSelectionRequested, this,
          &NMStoryGraphPanel::onDeleteSelected);
  connect(m_scene, &NMStoryGraphScene::nodesMoved, this,
          &NMStoryGraphPanel::onNodesMoved);
  connect(m_scene, &NMStoryGraphScene::nodeAdded, this,
          &NMStoryGraphPanel::onNodeAdded);
  connect(m_scene, &NMStoryGraphScene::nodeDeleted, this,
          &NMStoryGraphPanel::onNodeDeleted);
  connect(m_scene, &NMStoryGraphScene::connectionAdded, this,
          &NMStoryGraphPanel::onConnectionAdded);
  connect(m_scene, &NMStoryGraphScene::connectionDeleted, this,
          &NMStoryGraphPanel::onConnectionDeleted);
  connect(m_scene, &NMStoryGraphScene::entryNodeRequested, this,
          &NMStoryGraphPanel::onEntryNodeRequested);
}

void NMStoryGraphPanel::setupNodePalette() {
  if (!m_contentWidget)
    return;

  // Find the horizontal layout
  auto *mainLayout = qobject_cast<QVBoxLayout *>(m_contentWidget->layout());
  if (!mainLayout)
    return;

  QHBoxLayout *hLayout = nullptr;
  for (int i = 0; i < mainLayout->count(); ++i) {
    hLayout = qobject_cast<QHBoxLayout *>(mainLayout->itemAt(i)->layout());
    if (hLayout)
      break;
  }

  if (!hLayout)
    return;

  // Create and add node palette
  m_nodePalette = new NMNodePalette(m_contentWidget);
  hLayout->insertWidget(0, m_nodePalette); // Add to left side

  // Connect signals
  connect(m_nodePalette, &NMNodePalette::nodeTypeSelected, this,
          &NMStoryGraphPanel::onNodeTypeSelected);
}

bool NMStoryGraphPanel::navigateToNode(const QString &nodeIdString) {
  if (nodeIdString.isEmpty() || !m_scene || !m_view) {
    return false;
  }

  // Find the node
  NMGraphNodeItem *node = findNodeByIdString(nodeIdString);
  if (!node) {
    qWarning() << "[StoryGraph] Node not found for navigation:" << nodeIdString;
    return false;
  }

  // Clear previous selection
  for (auto *item : m_scene->items()) {
    if (auto *n = qgraphicsitem_cast<NMGraphNodeItem *>(item)) {
      n->setSelected(false);
    }
  }

  // Select and highlight the target node
  node->setSelected(true);

  // Center the view on the node
  m_view->centerOn(node);

  // Show and raise the panel
  show();
  raise();
  setFocus();

  qDebug() << "[StoryGraph] Navigated to node:" << nodeIdString;
  return true;
}

} // namespace NovelMind::editor::qt
