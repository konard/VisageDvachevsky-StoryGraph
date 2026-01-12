#include "NovelMind/editor/qt/panels/nm_story_graph_panel.hpp"
#include "NovelMind/editor/event_bus.hpp"
#include "NovelMind/editor/events/panel_events.hpp"
#include "NovelMind/editor/project_manager.hpp"
#include "NovelMind/editor/qt/nm_icon_manager.hpp"
#include "NovelMind/editor/qt/nm_play_mode_controller.hpp"
#include "NovelMind/editor/qt/nm_scrollable_toolbar.hpp"
#include "NovelMind/editor/qt/panels/nm_story_graph_minimap.hpp"

#include <QAction>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QList>
#include <QListWidget>
#include <QMessageBox>
#include <QPair>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>
#include <filesystem>

#include "nm_story_graph_panel_detail.hpp"

namespace NovelMind::editor::qt {

// ============================================================================
// NMStoryGraphPanel
// ============================================================================

NMStoryGraphPanel::NMStoryGraphPanel(QWidget* parent) : NMDockPanel(tr("Story Graph"), parent) {
  setPanelId("StoryGraph");

  // Story Graph needs enough space to display nodes with their labels,
  // the toolbar, node palette, and minimap without cramping
  setMinimumPanelSize(400, 300);

  setupContent();
  setupToolBar();
  setupNodePalette();
}

NMStoryGraphPanel::~NMStoryGraphPanel() {
  // Unsubscribe from all events
  auto& bus = EventBus::instance();
  for (const auto& subId : m_eventSubscriptions) {
    bus.unsubscribe(subId);
  }
  m_eventSubscriptions.clear();
}

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
  auto& playController = NMPlayModeController::instance();
  connect(&playController, &NMPlayModeController::currentNodeChanged, this,
          &NMStoryGraphPanel::onCurrentNodeChanged, Qt::QueuedConnection);
  connect(&playController, &NMPlayModeController::breakpointsChanged, this,
          &NMStoryGraphPanel::onBreakpointsChanged, Qt::QueuedConnection);
  connect(&playController, &NMPlayModeController::projectLoaded, this,
          &NMStoryGraphPanel::rebuildFromProjectScripts, Qt::QueuedConnection);

  // Issue #339: Auto-show panel when debugging starts
  connect(
      &playController, &NMPlayModeController::playModeChanged, this,
      [this](NMPlayModeController::PlayMode mode) {
        if (mode == NMPlayModeController::Playing || mode == NMPlayModeController::Paused) {
          // Debugging started, show and raise the Story Graph panel
          if (!isVisible()) {
            qDebug() << "[StoryGraph] Auto-showing panel for debugging";
            show();
            raise();
          }
        }
      },
      Qt::QueuedConnection);

  // Issue #339: Handle visibility changes to process queued centering operations
  connect(this, &QDockWidget::visibilityChanged, this, [this](bool visible) {
    if (visible && !m_pendingCenterNode.isEmpty() && m_followCurrentNode) {
      qDebug() << "[StoryGraph] Panel became visible, processing pending center operation";
      auto* node = findNodeByIdString(m_pendingCenterNode);
      if (node && m_view) {
        qDebug() << "[StoryGraph] Centering on" << m_pendingCenterNode;
        m_view->centerOn(node);
      }
      m_pendingCenterNode.clear();
    }
  });

  // Subscribe to scene auto-sync events (Issue #223)
  auto& bus = EventBus::instance();

  m_eventSubscriptions.push_back(bus.subscribe<events::SceneThumbnailUpdatedEvent>(
      [this](const events::SceneThumbnailUpdatedEvent& event) {
        onSceneThumbnailUpdated(event.sceneId, event.thumbnailPath);
      }));

  m_eventSubscriptions.push_back(
      bus.subscribe<events::SceneRenamedEvent>([this](const events::SceneRenamedEvent& event) {
        onSceneRenamed(event.sceneId, event.newName);
      }));

  m_eventSubscriptions.push_back(bus.subscribe<events::SceneDeletedEvent>(
      [this](const events::SceneDeletedEvent& event) { onSceneDeleted(event.sceneId); }));

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

  const QString scriptsRoot =
      QString::fromStdString(ProjectManager::instance().getFolderPath(ProjectFolder::Scripts));
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

  // Use Unicode-aware patterns to support Cyrillic, Greek, CJK, and other
  // non-ASCII identifiers in scene names, goto targets, and choice targets.
  // \p{L} matches any Unicode letter, \p{N} matches any Unicode digit.
  const QRegularExpression sceneRe("\\bscene\\s+([\\p{L}_][\\p{L}\\p{N}_]*)",
                                   QRegularExpression::UseUnicodePropertiesOption);
  const QRegularExpression gotoRe("\\bgoto\\s+([\\p{L}_][\\p{L}\\p{N}_]*)",
                                  QRegularExpression::UseUnicodePropertiesOption);
  const QRegularExpression choiceRe("\"([^\"]+)\"\\s*->\\s*([\\p{L}_][\\p{L}\\p{N}_]*)",
                                    QRegularExpression::UseUnicodePropertiesOption);

  for (const auto& entry : fs::recursive_directory_iterator(base)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".nms") {
      continue;
    }

    QFile file(QString::fromStdString(entry.path().string()));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      continue;
    }
    const QString content = QString::fromUtf8(file.readAll());
    file.close();

    const QString relPath =
        QString::fromStdString(ProjectManager::instance().toRelativePath(entry.path().string()));

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
      blocks[i].end = (i + 1 < blocks.size()) ? blocks[i + 1].start : content.size();
    }

    // For each scene block, gather transitions
    for (const auto& block : blocks) {
      const QString slice = content.mid(block.start, block.end - block.start);

      QRegularExpressionMatchIterator gotoIt = gotoRe.globalMatch(slice);
      while (gotoIt.hasNext()) {
        const QString target = gotoIt.next().captured(1);
        if (!target.isEmpty()) {
          edges.append({block.id, target});
        }
      }

      QRegularExpressionMatchIterator choiceIt = choiceRe.globalMatch(slice);
      while (choiceIt.hasNext()) {
        const QString target = choiceIt.next().captured(2);
        if (!target.isEmpty()) {
          edges.append({block.id, target});
        }
      }
    }
  }

  // Build nodes
  QHash<QString, NMGraphNodeItem*> nodeMap;
  int index = 0;
  auto ensureNode = [&](const QString& sceneId) -> NMGraphNodeItem* {
    if (nodeMap.contains(sceneId)) {
      return nodeMap.value(sceneId);
    }
    const int col = index % 4;
    const int row = index / 4;
    const QPointF defaultPos(col * 260.0, row * 140.0);
    const auto layoutIt = m_layoutNodes.constFind(sceneId);
    const QPointF pos = (layoutIt != m_layoutNodes.constEnd()) ? layoutIt->position : defaultPos;
    const QString type = (layoutIt != m_layoutNodes.constEnd() && !layoutIt->type.isEmpty())
                             ? layoutIt->type
                             : QString("Scene");
    const QString title = (layoutIt != m_layoutNodes.constEnd() && !layoutIt->title.isEmpty())
                              ? layoutIt->title
                              : sceneId;
    auto* node = m_scene->addNode(title, type, pos, 0, sceneId);
    if (node) {
      // Always set sceneId from the parameter to ensure it's never empty
      node->setSceneId(sceneId);

      if (sceneToFile.contains(sceneId)) {
        node->setScriptPath(sceneToFile.value(sceneId));
      } else if (layoutIt != m_layoutNodes.constEnd() && !layoutIt->scriptPath.isEmpty()) {
        node->setScriptPath(layoutIt->scriptPath);
      }
      if (layoutIt != m_layoutNodes.constEnd()) {
        node->setDialogueSpeaker(layoutIt->speaker);
        node->setDialogueText(layoutIt->dialogueText);
        node->setChoiceOptions(layoutIt->choices);
        // Scene Node specific properties - override with layout data if
        // available
        if (!layoutIt->sceneId.isEmpty()) {
          node->setSceneId(layoutIt->sceneId);
        }
        node->setHasEmbeddedDialogue(layoutIt->hasEmbeddedDialogue);
        node->setDialogueCount(layoutIt->dialogueCount);
        node->setThumbnailPath(layoutIt->thumbnailPath);
        // Condition Node specific properties
        node->setConditionExpression(layoutIt->conditionExpression);
        node->setConditionOutputs(layoutIt->conditionOutputs);
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

  for (auto it = m_layoutNodes.constBegin(); it != m_layoutNodes.constEnd(); ++it) {
    ensureNode(it.key());
  }

  // Build edges
  for (const auto& edge : edges) {
    auto* from = ensureNode(edge.first);
    auto* to = ensureNode(edge.second);
    if (from && to) {
      m_scene->addConnection(from, to);
    }
  }

  // Resolve entry scene from project metadata if available
  const QString projectEntry = QString::fromStdString(ProjectManager::instance().getStartScene());
  if (!projectEntry.isEmpty()) {
    m_layoutEntryScene = projectEntry;
  } else if (!m_layoutEntryScene.isEmpty()) {
    ProjectManager::instance().setStartScene(m_layoutEntryScene.toStdString());
  }

  // Update entry highlight
  for (auto* item : m_scene->items()) {
    if (auto* node = qgraphicsitem_cast<NMGraphNodeItem*>(item)) {
      node->setEntry(!m_layoutEntryScene.isEmpty() && node->nodeIdString() == m_layoutEntryScene);
    }
  }

  if (m_view && !m_scene->items().isEmpty()) {
    m_view->centerOnGraph();
  }

  updateNodeBreakpoints();
  updateCurrentNode(QString());

  // Update scene validation state for all scene nodes
  const QString projectPath = QString::fromStdString(ProjectManager::instance().getProjectPath());
  if (!projectPath.isEmpty() && m_scene) {
    m_scene->updateSceneValidationState(projectPath);
    // Update validation status in toolbar (Issue #332)
    updateValidationStatus();
  }

  detail::saveGraphLayout(m_layoutNodes, m_layoutEntryScene);
  m_isRebuilding = false;
}

void NMStoryGraphPanel::setupToolBar() {
  // Wrap the toolbar in a scrollable container for adaptive layout
  // This ensures all toolbar controls remain accessible when panel is small
  m_scrollableToolBar = new NMScrollableToolBar(this);
  m_toolBar = m_scrollableToolBar->toolbar();
  m_scrollableToolBar->setToolBarObjectName("StoryGraphToolBar");
  m_scrollableToolBar->setIconSize(QSize(16, 16));

  QAction* actionConnect = m_toolBar->addAction(tr("Connect"));
  actionConnect->setToolTip(tr("Connection mode"));
  actionConnect->setCheckable(true);
  connect(actionConnect, &QAction::toggled, this, [this](bool enabled) {
    if (m_view) {
      m_view->setConnectionModeEnabled(enabled);
    }
  });

  m_toolBar->addSeparator();

  QAction* actionZoomIn = m_toolBar->addAction(tr("+"));
  actionZoomIn->setToolTip(tr("Zoom In"));
  connect(actionZoomIn, &QAction::triggered, this, &NMStoryGraphPanel::onZoomIn);

  QAction* actionZoomOut = m_toolBar->addAction(tr("-"));
  actionZoomOut->setToolTip(tr("Zoom Out"));
  connect(actionZoomOut, &QAction::triggered, this, &NMStoryGraphPanel::onZoomOut);

  QAction* actionZoomReset = m_toolBar->addAction(tr("1:1"));
  actionZoomReset->setToolTip(tr("Reset Zoom"));
  connect(actionZoomReset, &QAction::triggered, this, &NMStoryGraphPanel::onZoomReset);

  QAction* actionFit = m_toolBar->addAction(tr("Fit"));
  actionFit->setToolTip(tr("Fit Graph to View"));
  connect(actionFit, &QAction::triggered, this, &NMStoryGraphPanel::onFitToGraph);

  // Issue #339: Follow Current Node toggle
  m_followNodeAction = m_toolBar->addAction(tr("Follow"));
  m_followNodeAction->setToolTip(tr("Auto-center view on current executing node during debugging"));
  m_followNodeAction->setCheckable(true);
  m_followNodeAction->setChecked(m_followCurrentNode);
  connect(m_followNodeAction, &QAction::toggled, this, [this](bool enabled) {
    m_followCurrentNode = enabled;
    qDebug() << "[StoryGraph] Follow current node:" << enabled;

    // If enabling and we have a pending center operation, execute it now
    if (enabled && !m_pendingCenterNode.isEmpty() && m_view && !m_view->isHidden()) {
      auto* node = findNodeByIdString(m_pendingCenterNode);
      if (node) {
        qDebug() << "[StoryGraph] Executing pending center on" << m_pendingCenterNode;
        m_view->centerOn(node);
      }
      m_pendingCenterNode.clear();
    }
  });

  m_toolBar->addSeparator();

  QAction* actionAutoLayout = m_toolBar->addAction(tr("Auto Layout"));
  actionAutoLayout->setToolTip(tr("Automatically arrange nodes (hierarchical layout)"));
  connect(actionAutoLayout, &QAction::triggered, this, &NMStoryGraphPanel::onAutoLayout);

  // Issue #345: Scene container visualization toggle
  m_toolBar->addSeparator();

  QAction* actionSceneContainers = m_toolBar->addAction(tr("Scenes"));
  actionSceneContainers->setCheckable(true);
  actionSceneContainers->setChecked(true); // On by default
  actionSceneContainers->setToolTip(
      tr("Toggle scene container visualization.\n"
         "When enabled, semi-transparent boxes show which nodes\n"
         "belong to each scene, making the story structure clearer."));
  connect(actionSceneContainers, &QAction::toggled, this, [this](bool enabled) {
    if (m_scene) {
      m_scene->setSceneContainersVisible(enabled);
    }
  });

  // Localization controls section
  m_toolBar->addSeparator();

  // Locale preview selector
  QLabel* localeLabel = new QLabel(tr("Preview:"), m_toolBar);
  m_toolBar->addWidget(localeLabel);

  m_localePreviewSelector = new QComboBox(m_toolBar);
  m_localePreviewSelector->setMinimumWidth(80);
  m_localePreviewSelector->addItem(tr("Source"), "");
  m_localePreviewSelector->setToolTip(tr("Preview dialogue text in selected locale"));
  connect(m_localePreviewSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &NMStoryGraphPanel::onLocalePreviewChanged);
  m_toolBar->addWidget(m_localePreviewSelector);

  // Generate localization keys button
  m_generateKeysBtn = new QPushButton(tr("Gen Keys"), m_toolBar);
  m_generateKeysBtn->setIcon(NMIconManager::instance().getIcon("locale-key", 16));
  m_generateKeysBtn->setToolTip(tr("Generate localization keys for all dialogue nodes"));
  connect(m_generateKeysBtn, &QPushButton::clicked, this,
          &NMStoryGraphPanel::onGenerateLocalizationKeysClicked);
  m_toolBar->addWidget(m_generateKeysBtn);

  // Export dialogue button
  m_exportDialogueBtn = new QPushButton(tr("Export"), m_toolBar);
  m_exportDialogueBtn->setIcon(NMIconManager::instance().getIcon("export", 16));
  m_exportDialogueBtn->setToolTip(tr("Export all dialogue to localization files"));
  connect(m_exportDialogueBtn, &QPushButton::clicked, this,
          &NMStoryGraphPanel::onExportDialogueClicked);
  m_toolBar->addWidget(m_exportDialogueBtn);

  // Sync section (issue #82, #127)
  m_toolBar->addSeparator();

  m_syncGraphToScriptBtn = new QPushButton(tr("Sync to Script"), m_toolBar);
  m_syncGraphToScriptBtn->setIcon(NMIconManager::instance().getIcon("refresh", 16));
  m_syncGraphToScriptBtn->setToolTip(
      tr("Synchronize Story Graph changes to NMScript files.\n"
         "This writes graph node data (speaker, dialogue) to .nms files.\n"
         "Use in Graph Mode when the Story Graph is the source of truth."));
  connect(m_syncGraphToScriptBtn, &QPushButton::clicked, this,
          &NMStoryGraphPanel::onSyncGraphToScript);
  m_toolBar->addWidget(m_syncGraphToScriptBtn);

  // Issue #127: Sync Script to Graph button
  m_syncScriptToGraphBtn = new QPushButton(tr("Sync to Graph"), m_toolBar);
  m_syncScriptToGraphBtn->setIcon(NMIconManager::instance().getIcon("refresh", 16));
  m_syncScriptToGraphBtn->setToolTip(
      tr("Import NMScript content to Story Graph.\n"
         "This parses .nms script files and creates/updates graph nodes.\n"
         "Use in Script Mode to visualize script content."));
  connect(m_syncScriptToGraphBtn, &QPushButton::clicked, this,
          &NMStoryGraphPanel::onSyncScriptToGraph);
  m_toolBar->addWidget(m_syncScriptToGraphBtn);

  // Validation status section (Issue #332)
  m_toolBar->addSeparator();

  // Validation status label
  m_validationStatusLabel = new QLabel(m_toolBar);
  m_validationStatusLabel->setToolTip(
      tr("Scene validation status - shows count of nodes with orphaned scene references"));
  m_toolBar->addWidget(m_validationStatusLabel);

  // Fix issues button
  m_fixIssuesBtn = new QPushButton(tr("Fix Issues"), m_toolBar);
  m_fixIssuesBtn->setIcon(NMIconManager::instance().getIcon("alert-triangle", 16));
  m_fixIssuesBtn->setToolTip(tr("Show and fix scene validation issues"));
  m_fixIssuesBtn->setVisible(false); // Hidden initially until issues are detected
  connect(m_fixIssuesBtn, &QPushButton::clicked, this,
          &NMStoryGraphPanel::showValidationIssuesDialog);
  m_toolBar->addWidget(m_fixIssuesBtn);

  if (auto* layout = qobject_cast<QVBoxLayout*>(m_contentWidget->layout())) {
    layout->insertWidget(0, m_scrollableToolBar);
  }

  // Initialize button visibility based on current workflow mode
  updateSyncButtonsVisibility();

  // Initialize validation status
  updateValidationStatus();
}

void NMStoryGraphPanel::setupContent() {
  m_contentWidget = new QWidget(this);
  auto* mainLayout = new QVBoxLayout(m_contentWidget);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // Create horizontal layout for node palette + graph view
  auto* hLayout = new QHBoxLayout();
  hLayout->setContentsMargins(0, 0, 0, 0);
  hLayout->setSpacing(4);

  m_scene = new NMStoryGraphScene(this);
  m_view = new NMStoryGraphView(m_contentWidget);
  m_view->setScene(m_scene);

  hLayout->addWidget(m_view, 1); // Graph view takes most space

  // Create vertical layout for minimap on the right
  auto* rightLayout = new QVBoxLayout();
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
  connect(m_view, &NMStoryGraphView::nodeClicked, this, &NMStoryGraphPanel::onNodeClicked);
  connect(m_view, &NMStoryGraphView::nodeDoubleClicked, this,
          &NMStoryGraphPanel::onNodeDoubleClicked);
  connect(m_scene, &NMStoryGraphScene::deleteSelectionRequested, this,
          &NMStoryGraphPanel::onDeleteSelected);
  connect(m_scene, &NMStoryGraphScene::nodesMoved, this, &NMStoryGraphPanel::onNodesMoved);
  connect(m_scene, &NMStoryGraphScene::nodeAdded, this, &NMStoryGraphPanel::onNodeAdded);
  connect(m_scene, &NMStoryGraphScene::nodeDeleted, this, &NMStoryGraphPanel::onNodeDeleted);
  connect(m_scene, &NMStoryGraphScene::connectionAdded, this,
          &NMStoryGraphPanel::onConnectionAdded);
  connect(m_scene, &NMStoryGraphScene::connectionDeleted, this,
          &NMStoryGraphPanel::onConnectionDeleted);
  connect(m_scene, &NMStoryGraphScene::entryNodeRequested, this,
          &NMStoryGraphPanel::onEntryNodeRequested);
  connect(m_scene, &NMStoryGraphScene::scriptFileCreationFailed, this,
          &NMStoryGraphPanel::onScriptFileCreationFailed);
}

void NMStoryGraphPanel::setupNodePalette() {
  if (!m_contentWidget)
    return;

  // Find the horizontal layout
  auto* mainLayout = qobject_cast<QVBoxLayout*>(m_contentWidget->layout());
  if (!mainLayout)
    return;

  QHBoxLayout* hLayout = nullptr;
  for (int i = 0; i < mainLayout->count(); ++i) {
    hLayout = qobject_cast<QHBoxLayout*>(mainLayout->itemAt(i)->layout());
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

bool NMStoryGraphPanel::navigateToNode(const QString& nodeIdString) {
  if (nodeIdString.isEmpty() || !m_scene || !m_view) {
    qWarning() << "[StoryGraph] Navigation failed: Invalid parameters"
               << "(nodeIdString.isEmpty():" << nodeIdString.isEmpty()
               << ", m_scene:" << (m_scene != nullptr) << ", m_view:" << (m_view != nullptr) << ")";
    return false;
  }

  // Find the node
  NMGraphNodeItem* node = findNodeByIdString(nodeIdString);
  if (!node) {
    // Issue #335: Show user-visible notification when navigation target is not found
    qWarning() << "[StoryGraph] Node not found for navigation:" << nodeIdString
               << "- Available nodes:" << m_nodeIdToString.size() << "- Graph has"
               << m_scene->nodes().size() << "nodes";

    // Show user-visible error message
    QMessageBox::warning(this, tr("Node Not Found"),
                         tr("Cannot navigate to node '%1'.\n\n"
                            "The node does not exist in the Story Graph.\n"
                            "This may happen if:\n"
                            "• The node was deleted\n"
                            "• The graph hasn't been synchronized with scripts\n"
                            "• The node ID is incorrect\n\n"
                            "Try rebuilding the graph or check the node reference.")
                             .arg(nodeIdString));

    return false;
  }

  // Clear previous selection
  for (auto* item : m_scene->items()) {
    if (auto* n = qgraphicsitem_cast<NMGraphNodeItem*>(item)) {
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

  qDebug() << "[StoryGraph] Navigated to node:" << nodeIdString << "at position:" << node->pos();
  return true;
}

void NMStoryGraphPanel::setReadOnly(bool readOnly, const QString& reason) {
  if (m_readOnly == readOnly) {
    return;
  }

  m_readOnly = readOnly;

  // Create or update the read-only banner
  if (readOnly) {
    if (!m_readOnlyBanner) {
      m_readOnlyBanner = new QFrame(m_contentWidget);
      m_readOnlyBanner->setObjectName("WorkflowReadOnlyBanner");
      m_readOnlyBanner->setStyleSheet("QFrame#WorkflowReadOnlyBanner {"
                                      "  background-color: #3d5a80;"
                                      "  border: 1px solid #98c1d9;"
                                      "  border-radius: 4px;"
                                      "  padding: 6px 12px;"
                                      "  margin: 4px 8px;"
                                      "}");

      auto* bannerLayout = new QHBoxLayout(m_readOnlyBanner);
      bannerLayout->setContentsMargins(8, 4, 8, 4);
      bannerLayout->setSpacing(8);

      // Info icon (using text for now)
      auto* iconLabel = new QLabel(QString::fromUtf8("\xE2\x84\xB9"), // ℹ
                                   m_readOnlyBanner);
      iconLabel->setStyleSheet("font-size: 14px; color: #e0fbfc;");
      bannerLayout->addWidget(iconLabel);

      m_readOnlyLabel = new QLabel(m_readOnlyBanner);
      m_readOnlyLabel->setStyleSheet("color: #e0fbfc; font-weight: bold;");
      bannerLayout->addWidget(m_readOnlyLabel);

      bannerLayout->addStretch();

      // Insert banner at the top of the content widget
      if (auto* layout = qobject_cast<QVBoxLayout*>(m_contentWidget->layout())) {
        // Insert after toolbar (index 1)
        layout->insertWidget(1, m_readOnlyBanner);
      }
    }

    // Update banner text
    QString bannerText = tr("Read-only mode");
    if (!reason.isEmpty()) {
      bannerText += QString(" (%1)").arg(reason);
    }
    bannerText += tr(" - Graph editing is disabled. Use 'Sync to Script' to update.");
    m_readOnlyLabel->setText(bannerText);
    m_readOnlyBanner->setVisible(true);
  } else if (m_readOnlyBanner) {
    m_readOnlyBanner->setVisible(false);
  }

  // Disable/enable editing controls
  if (m_nodePalette) {
    m_nodePalette->setEnabled(!readOnly);
  }

  if (m_scene) {
    m_scene->setReadOnly(readOnly);
  }

  // Update toolbar buttons
  if (m_generateKeysBtn) {
    m_generateKeysBtn->setEnabled(!readOnly);
  }

  // Update sync buttons based on workflow mode (issue #127)
  updateSyncButtonsVisibility();

  qDebug() << "[StoryGraph] Read-only mode:" << readOnly << "reason:" << reason;
}

void NMStoryGraphPanel::updateSyncButtonsVisibility() {
  // Issue #127: Mode-specific button visibility
  // - Script Mode: Only "Sync to Graph" visible (Scripts are authoritative)
  // - Graph Mode: Only "Sync to Script" visible (Graph is authoritative)
  // - Mixed Mode: Both visible with appropriate tooltips

  const auto& pm = ProjectManager::instance();
  if (!pm.hasOpenProject()) {
    // No project - show both buttons with default state
    if (m_syncGraphToScriptBtn) {
      m_syncGraphToScriptBtn->setVisible(true);
      m_syncGraphToScriptBtn->setEnabled(false);
    }
    if (m_syncScriptToGraphBtn) {
      m_syncScriptToGraphBtn->setVisible(true);
      m_syncScriptToGraphBtn->setEnabled(false);
    }
    return;
  }

  const PlaybackSourceMode mode = pm.getMetadata().playbackSourceMode;

  switch (mode) {
  case PlaybackSourceMode::Script:
    // In Script Mode, scripts are authoritative.
    // User can sync script content to graph for visualization.
    if (m_syncGraphToScriptBtn) {
      m_syncGraphToScriptBtn->setVisible(false);
    }
    if (m_syncScriptToGraphBtn) {
      m_syncScriptToGraphBtn->setVisible(true);
      m_syncScriptToGraphBtn->setEnabled(true);
      m_syncScriptToGraphBtn->setToolTip(
          tr("Import NMScript content to Story Graph.\n"
             "In Script Mode, scripts are authoritative.\n"
             "This updates the graph visualization from script files."));
    }
    break;

  case PlaybackSourceMode::Graph:
    // In Graph Mode, graph is authoritative.
    // User can sync graph content to scripts for export/backup.
    if (m_syncGraphToScriptBtn) {
      m_syncGraphToScriptBtn->setVisible(true);
      m_syncGraphToScriptBtn->setEnabled(true);
      m_syncGraphToScriptBtn->setToolTip(tr("Export Story Graph content to NMScript files.\n"
                                            "In Graph Mode, the graph is authoritative.\n"
                                            "This generates scripts from graph node data."));
    }
    if (m_syncScriptToGraphBtn) {
      m_syncScriptToGraphBtn->setVisible(false);
    }
    break;

  case PlaybackSourceMode::Mixed:
    // In Mixed Mode, both sources are used.
    // Both sync operations are available.
    if (m_syncGraphToScriptBtn) {
      m_syncGraphToScriptBtn->setVisible(true);
      m_syncGraphToScriptBtn->setEnabled(true);
      m_syncGraphToScriptBtn->setToolTip(tr("Export Story Graph content to NMScript files.\n"
                                            "In Mixed Mode, graph overrides take priority.\n"
                                            "Use this to backup graph changes to scripts."));
    }
    if (m_syncScriptToGraphBtn) {
      m_syncScriptToGraphBtn->setVisible(true);
      m_syncScriptToGraphBtn->setEnabled(true);
      m_syncScriptToGraphBtn->setToolTip(tr("Import NMScript content to Story Graph.\n"
                                            "In Mixed Mode, scripts provide base content.\n"
                                            "Graph changes will override script content."));
    }
    break;
  }

  qDebug() << "[StoryGraph] Updated sync button visibility for mode:" << static_cast<int>(mode);
}

// ============================================================================
// PERF-5: Incremental Graph Update Methods
// ============================================================================

void NMStoryGraphPanel::updateSingleNode(const QString& nodeIdString, const LayoutNode& nodeData) {
  if (!m_scene || nodeIdString.isEmpty()) {
    return;
  }

  // Find existing node
  NMGraphNodeItem* node = findNodeByIdString(nodeIdString);
  if (!node) {
    // Node doesn't exist, add it
    node = m_scene->addNode(nodeData.title, nodeData.type, nodeData.position, 0, nodeIdString);
    if (!node) {
      return;
    }
    node->setSceneId(nodeIdString);
  } else {
    // Update existing node in-place without full rebuild
    node->setTitle(nodeData.title);
    node->setNodeType(nodeData.type);
    node->setPos(nodeData.position);
  }

  // Update node properties
  node->setScriptPath(nodeData.scriptPath);
  node->setDialogueSpeaker(nodeData.speaker);
  node->setDialogueText(nodeData.dialogueText);
  node->setChoiceOptions(nodeData.choices);
  node->setHasEmbeddedDialogue(nodeData.hasEmbeddedDialogue);
  node->setDialogueCount(nodeData.dialogueCount);
  node->setThumbnailPath(nodeData.thumbnailPath);
  node->setConditionExpression(nodeData.conditionExpression);
  node->setConditionOutputs(nodeData.conditionOutputs);

  // Update layout cache
  m_layoutNodes.insert(nodeIdString, nodeData);
  m_nodeIdToString.insert(node->nodeId(), nodeIdString);

  // Force redraw of the node
  node->update();
}

void NMStoryGraphPanel::addSingleConnection(const QString& fromNodeIdString,
                                            const QString& toNodeIdString) {
  if (!m_scene || fromNodeIdString.isEmpty() || toNodeIdString.isEmpty()) {
    return;
  }

  NMGraphNodeItem* fromNode = findNodeByIdString(fromNodeIdString);
  NMGraphNodeItem* toNode = findNodeByIdString(toNodeIdString);

  if (!fromNode || !toNode) {
    qWarning() << "[StoryGraph] Cannot add connection: node not found" << fromNodeIdString << "->"
               << toNodeIdString;
    return;
  }

  // Check if connection already exists
  if (m_scene->hasConnection(fromNode->nodeId(), toNode->nodeId())) {
    return;
  }

  m_scene->addConnection(fromNode, toNode);
}

void NMStoryGraphPanel::removeSingleConnection(const QString& fromNodeIdString,
                                               const QString& toNodeIdString) {
  if (!m_scene || fromNodeIdString.isEmpty() || toNodeIdString.isEmpty()) {
    return;
  }

  NMGraphNodeItem* fromNode = findNodeByIdString(fromNodeIdString);
  NMGraphNodeItem* toNode = findNodeByIdString(toNodeIdString);

  if (!fromNode || !toNode) {
    return;
  }

  m_scene->removeConnection(fromNode->nodeId(), toNode->nodeId());
}

void NMStoryGraphPanel::updateNodePosition(const QString& nodeIdString, const QPointF& newPos) {
  if (!m_scene || nodeIdString.isEmpty()) {
    return;
  }

  NMGraphNodeItem* node = findNodeByIdString(nodeIdString);
  if (!node) {
    return;
  }

  // Update position without triggering full rebuild
  node->setPos(newPos);

  // Update layout cache
  auto it = m_layoutNodes.find(nodeIdString);
  if (it != m_layoutNodes.end()) {
    it->position = newPos;
  }

  // Update connections attached to this node
  for (auto* conn : m_scene->findConnectionsForNode(node)) {
    conn->updatePath();
  }
}

// ============================================================================
// Scene Auto-Sync Event Handlers (Issue #223)
// ============================================================================

void NMStoryGraphPanel::onSceneThumbnailUpdated(const QString& sceneId,
                                                const QString& thumbnailPath) {
  if (!m_scene || sceneId.isEmpty()) {
    return;
  }

  qDebug() << "[StoryGraph] Scene thumbnail updated:" << sceneId << "->" << thumbnailPath;

  // Find all scene nodes that reference this scene
  bool updated = false;
  for (auto* node : m_scene->nodes()) {
    if (node && node->isSceneNode() && node->sceneId() == sceneId) {
      node->setThumbnailPath(thumbnailPath);
      node->update(); // Force visual refresh
      updated = true;

      // Update layout cache
      auto it = m_layoutNodes.find(node->nodeIdString());
      if (it != m_layoutNodes.end()) {
        it->thumbnailPath = thumbnailPath;
      }
    }
  }

  if (updated) {
    detail::saveGraphLayout(m_layoutNodes, m_layoutEntryScene);
    qDebug() << "[StoryGraph] Updated thumbnail for scene nodes referencing" << sceneId;
  }
}

void NMStoryGraphPanel::onSceneRenamed(const QString& sceneId, const QString& newName) {
  if (!m_scene || sceneId.isEmpty()) {
    return;
  }

  qDebug() << "[StoryGraph] Scene renamed:" << sceneId << "to" << newName;

  // Find all scene nodes that reference this scene
  bool updated = false;
  for (auto* node : m_scene->nodes()) {
    if (node && node->isSceneNode() && node->sceneId() == sceneId) {
      // Update node title to reflect new scene name
      node->setTitle(newName);
      node->update(); // Force visual refresh
      updated = true;

      // Update layout cache
      auto it = m_layoutNodes.find(node->nodeIdString());
      if (it != m_layoutNodes.end()) {
        it->title = newName;
      }
    }
  }

  if (updated) {
    detail::saveGraphLayout(m_layoutNodes, m_layoutEntryScene);
    qDebug() << "[StoryGraph] Updated scene node titles for scene" << sceneId;
  }
}

void NMStoryGraphPanel::onSceneDeleted(const QString& sceneId) {
  if (!m_scene || sceneId.isEmpty()) {
    return;
  }

  qDebug() << "[StoryGraph] Scene deleted:" << sceneId;

  // Find all scene nodes that reference this deleted scene
  // Mark them with validation errors (orphaned references)
  bool hasOrphans = false;
  for (auto* node : m_scene->nodes()) {
    if (node && node->isSceneNode() && node->sceneId() == sceneId) {
      // Set validation error on orphaned scene nodes
      node->setSceneValidationError(true);
      node->setSceneValidationMessage(
          QString("Referenced scene '%1' has been deleted").arg(sceneId));
      node->update(); // Force visual refresh
      hasOrphans = true;

      qDebug() << "[StoryGraph] Marked node" << node->nodeIdString()
               << "as orphaned (references deleted scene" << sceneId << ")";
    }
  }

  if (hasOrphans) {
    qWarning() << "[StoryGraph] Found orphaned scene node references for"
               << "deleted scene" << sceneId << "- nodes marked with validation errors";
    // Update validation status in toolbar
    updateValidationStatus();
  }
}

void NMStoryGraphPanel::updateValidationStatus() {
  if (!m_scene || !m_validationStatusLabel) {
    return;
  }

  // Count nodes with validation errors or warnings
  int errorCount = 0;
  int warningCount = 0;

  for (auto* node : m_scene->nodes()) {
    if (node && node->isSceneNode()) {
      if (node->hasSceneValidationError()) {
        errorCount++;
      } else if (node->hasSceneValidationWarning()) {
        warningCount++;
      }
    }
  }

  // Update label text and style
  if (errorCount > 0) {
    QString text = tr("⚠️ %1 issue%2").arg(errorCount).arg(errorCount > 1 ? "s" : "");
    m_validationStatusLabel->setText(text);
    m_validationStatusLabel->setStyleSheet("QLabel { color: #dc3c3c; font-weight: bold; }");
    m_validationStatusLabel->setVisible(true);
    m_fixIssuesBtn->setVisible(true);
  } else if (warningCount > 0) {
    QString text = tr("⚠️ %1 warning%2").arg(warningCount).arg(warningCount > 1 ? "s" : "");
    m_validationStatusLabel->setText(text);
    m_validationStatusLabel->setStyleSheet("QLabel { color: #ffb43c; font-weight: bold; }");
    m_validationStatusLabel->setVisible(true);
    m_fixIssuesBtn->setVisible(true);
  } else {
    m_validationStatusLabel->setText(tr("✓ No issues"));
    m_validationStatusLabel->setStyleSheet("QLabel { color: #64c832; }");
    m_validationStatusLabel->setVisible(true);
    m_fixIssuesBtn->setVisible(false);
  }
}

void NMStoryGraphPanel::showValidationIssuesDialog() {
  if (!m_scene) {
    return;
  }

  // Collect all nodes with validation issues
  QStringList issues;
  QVector<NMGraphNodeItem*> problemNodes;

  for (auto* node : m_scene->nodes()) {
    if (node && node->isSceneNode()) {
      if (node->hasSceneValidationError() || node->hasSceneValidationWarning()) {
        const QString issueType = node->hasSceneValidationError() ? tr("Error") : tr("Warning");
        issues.append(QString("[%1] %2: %3")
                          .arg(issueType)
                          .arg(node->title())
                          .arg(node->sceneValidationMessage()));
        problemNodes.append(node);
      }
    }
  }

  if (issues.isEmpty()) {
    QMessageBox::information(this, tr("Scene Validation"), tr("No validation issues found."));
    return;
  }

  // Create dialog to show issues
  QDialog dialog(this);
  dialog.setWindowTitle(tr("Scene Validation Issues"));
  dialog.resize(600, 400);

  auto* layout = new QVBoxLayout(&dialog);

  // Info label
  auto* infoLabel = new QLabel(tr("The following scene nodes have validation issues:"), &dialog);
  layout->addWidget(infoLabel);

  // List widget to show issues
  auto* issueList = new QListWidget(&dialog);
  for (const QString& issue : issues) {
    issueList->addItem(issue);
  }
  layout->addWidget(issueList);

  // Connect double-click to navigate to node
  connect(issueList, &QListWidget::itemDoubleClicked,
          [this, problemNodes, issueList](QListWidgetItem* item) {
            int index = issueList->row(item);
            if (index >= 0 && index < problemNodes.size()) {
              auto* node = problemNodes[index];
              if (node && m_view) {
                // Navigate to the problem node
                m_view->centerOn(node);
                m_scene->clearSelection();
                node->setSelected(true);
              }
            }
          });

  // Help text
  auto* helpLabel = new QLabel(tr("Double-click an issue to navigate to the node.\n\n"
                                  "To fix orphaned scene references:\n"
                                  "• Right-click the node and select 'Rebind Scene'\n"
                                  "• Or delete the node if it's no longer needed"),
                               &dialog);
  helpLabel->setWordWrap(true);
  helpLabel->setStyleSheet("QLabel { color: #888; font-size: 10px; }");
  layout->addWidget(helpLabel);

  // Button box
  auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
  connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  layout->addWidget(buttonBox);

  dialog.exec();
}

} // namespace NovelMind::editor::qt
