#include "NovelMind/editor/qt/panels/nm_story_graph_panel.hpp"
#include "NovelMind/editor/project_manager.hpp"
#include "NovelMind/editor/qt/nm_play_mode_controller.hpp"
#include "NovelMind/editor/qt/nm_scrollable_toolbar.hpp"
#include "NovelMind/editor/qt/panels/nm_story_graph_minimap.hpp"

#include <QAction>
#include <QComboBox>
#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QList>
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

NMStoryGraphPanel::NMStoryGraphPanel(QWidget *parent)
    : NMDockPanel(tr("Story Graph"), parent) {
  setPanelId("StoryGraph");

  // Story Graph needs enough space to display nodes with their labels,
  // the toolbar, node palette, and minimap without cramping
  setMinimumPanelSize(400, 300);

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

  // Use Unicode-aware patterns to support Cyrillic, Greek, CJK, and other
  // non-ASCII identifiers in scene names, goto targets, and choice targets.
  // \p{L} matches any Unicode letter, \p{N} matches any Unicode digit.
  const QRegularExpression sceneRe(
      "\\bscene\\s+([\\p{L}_][\\p{L}\\p{N}_]*)",
      QRegularExpression::UseUnicodePropertiesOption);
  const QRegularExpression gotoRe(
      "\\bgoto\\s+([\\p{L}_][\\p{L}\\p{N}_]*)",
      QRegularExpression::UseUnicodePropertiesOption);
  const QRegularExpression choiceRe(
      "\"([^\"]+)\"\\s*->\\s*([\\p{L}_][\\p{L}\\p{N}_]*)",
      QRegularExpression::UseUnicodePropertiesOption);

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
        ProjectManager::instance().toRelativePath(entry.path().string()));

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
    const QPointF pos = (layoutIt != m_layoutNodes.constEnd())
                            ? layoutIt->position
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
      // Always set sceneId from the parameter to ensure it's never empty
      node->setSceneId(sceneId);

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
  // Wrap the toolbar in a scrollable container for adaptive layout
  // This ensures all toolbar controls remain accessible when panel is small
  m_scrollableToolBar = new NMScrollableToolBar(this);
  m_toolBar = m_scrollableToolBar->toolbar();
  m_scrollableToolBar->setToolBarObjectName("StoryGraphToolBar");
  m_scrollableToolBar->setIconSize(QSize(16, 16));

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
  actionAutoLayout->setToolTip(
      tr("Automatically arrange nodes (hierarchical layout)"));
  connect(actionAutoLayout, &QAction::triggered, this,
          &NMStoryGraphPanel::onAutoLayout);

  // Localization controls section
  m_toolBar->addSeparator();

  // Locale preview selector
  QLabel *localeLabel = new QLabel(tr("Preview:"), m_toolBar);
  m_toolBar->addWidget(localeLabel);

  m_localePreviewSelector = new QComboBox(m_toolBar);
  m_localePreviewSelector->setMinimumWidth(80);
  m_localePreviewSelector->addItem(tr("Source"), "");
  m_localePreviewSelector->setToolTip(
      tr("Preview dialogue text in selected locale"));
  connect(m_localePreviewSelector,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &NMStoryGraphPanel::onLocalePreviewChanged);
  m_toolBar->addWidget(m_localePreviewSelector);

  // Generate localization keys button
  m_generateKeysBtn = new QPushButton(tr("Gen Keys"), m_toolBar);
  m_generateKeysBtn->setToolTip(
      tr("Generate localization keys for all dialogue nodes"));
  connect(m_generateKeysBtn, &QPushButton::clicked, this,
          &NMStoryGraphPanel::onGenerateLocalizationKeysClicked);
  m_toolBar->addWidget(m_generateKeysBtn);

  // Export dialogue button
  m_exportDialogueBtn = new QPushButton(tr("Export"), m_toolBar);
  m_exportDialogueBtn->setToolTip(
      tr("Export all dialogue to localization files"));
  connect(m_exportDialogueBtn, &QPushButton::clicked, this,
          &NMStoryGraphPanel::onExportDialogueClicked);
  m_toolBar->addWidget(m_exportDialogueBtn);

  // Sync section (issue #82)
  m_toolBar->addSeparator();

  m_syncGraphToScriptBtn = new QPushButton(tr("Sync to Script"), m_toolBar);
  m_syncGraphToScriptBtn->setToolTip(
      tr("Synchronize Story Graph changes to NMScript files.\n"
         "This writes graph node data (speaker, dialogue) to .nms files."));
  connect(m_syncGraphToScriptBtn, &QPushButton::clicked, this,
          &NMStoryGraphPanel::onSyncGraphToScript);
  m_toolBar->addWidget(m_syncGraphToScriptBtn);

  if (auto *layout = qobject_cast<QVBoxLayout *>(m_contentWidget->layout())) {
    layout->insertWidget(0, m_scrollableToolBar);
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

void NMStoryGraphPanel::setReadOnly(bool readOnly, const QString &reason) {
  if (m_readOnly == readOnly) {
    return;
  }

  m_readOnly = readOnly;

  // Create or update the read-only banner
  if (readOnly) {
    if (!m_readOnlyBanner) {
      m_readOnlyBanner = new QFrame(m_contentWidget);
      m_readOnlyBanner->setObjectName("WorkflowReadOnlyBanner");
      m_readOnlyBanner->setStyleSheet(
          "QFrame#WorkflowReadOnlyBanner {"
          "  background-color: #3d5a80;"
          "  border: 1px solid #98c1d9;"
          "  border-radius: 4px;"
          "  padding: 6px 12px;"
          "  margin: 4px 8px;"
          "}");

      auto *bannerLayout = new QHBoxLayout(m_readOnlyBanner);
      bannerLayout->setContentsMargins(8, 4, 8, 4);
      bannerLayout->setSpacing(8);

      // Info icon (using text for now)
      auto *iconLabel = new QLabel(QString::fromUtf8("\xE2\x84\xB9"), // ℹ
                                   m_readOnlyBanner);
      iconLabel->setStyleSheet("font-size: 14px; color: #e0fbfc;");
      bannerLayout->addWidget(iconLabel);

      m_readOnlyLabel = new QLabel(m_readOnlyBanner);
      m_readOnlyLabel->setStyleSheet("color: #e0fbfc; font-weight: bold;");
      bannerLayout->addWidget(m_readOnlyLabel);

      bannerLayout->addStretch();

      // Insert banner at the top of the content widget
      if (auto *layout =
              qobject_cast<QVBoxLayout *>(m_contentWidget->layout())) {
        // Insert after toolbar (index 1)
        layout->insertWidget(1, m_readOnlyBanner);
      }
    }

    // Update banner text
    QString bannerText = tr("Read-only mode");
    if (!reason.isEmpty()) {
      bannerText += QString(" (%1)").arg(reason);
    }
    bannerText +=
        tr(" - Graph editing is disabled. Use 'Sync to Script' to update.");
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

  // Sync button is always enabled - it's how you sync changes
  if (m_syncGraphToScriptBtn) {
    m_syncGraphToScriptBtn->setEnabled(true);
    if (readOnly) {
      m_syncGraphToScriptBtn->setToolTip(
          tr("Story Graph is read-only in Script Mode.\n"
             "Script files are authoritative."));
    } else {
      m_syncGraphToScriptBtn->setToolTip(
          tr("Synchronize Story Graph changes to NMScript files.\n"
             "This writes graph node data (speaker, dialogue) to .nms files."));
    }
  }

  qDebug() << "[StoryGraph] Read-only mode:" << readOnly
           << "reason:" << reason;
}

} // namespace NovelMind::editor::qt
