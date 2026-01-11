#include "NovelMind/editor/mediators/selection_mediator.hpp"
#include "NovelMind/editor/qt/panels/nm_hierarchy_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_inspector_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_scene_view_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_story_graph_panel.hpp"

#include <QDebug>
#include <QFileInfo>

namespace NovelMind::editor::mediators {

SelectionMediator::SelectionMediator(qt::NMSceneViewPanel *sceneView,
                                     qt::NMHierarchyPanel *hierarchy,
                                     qt::NMInspectorPanel *inspector,
                                     qt::NMStoryGraphPanel *storyGraph,
                                     QObject *parent)
    : QObject(parent), m_sceneView(sceneView), m_hierarchy(hierarchy),
      m_inspector(inspector), m_storyGraph(storyGraph) {}

SelectionMediator::~SelectionMediator() { shutdown(); }

void SelectionMediator::initialize() {
  auto &bus = EventBus::instance();

  // Subscribe to scene object selection events
  m_subscriptions.push_back(
      bus.subscribe<events::SceneObjectSelectedEvent>(
          [this](const events::SceneObjectSelectedEvent &event) {
            onSceneObjectSelected(event);
          }));

  // Subscribe to story graph node selection events
  m_subscriptions.push_back(
      bus.subscribe<events::StoryGraphNodeSelectedEvent>(
          [this](const events::StoryGraphNodeSelectedEvent &event) {
            onStoryGraphNodeSelected(event);
          }));

  // Subscribe to asset selection events
  m_subscriptions.push_back(bus.subscribe<events::AssetSelectedEvent>(
      [this](const events::AssetSelectedEvent &event) {
        onAssetSelected(event);
      }));

  // Subscribe to hierarchy double-click events
  m_subscriptions.push_back(
      bus.subscribe<events::HierarchyObjectDoubleClickedEvent>(
          [this](const events::HierarchyObjectDoubleClickedEvent &event) {
            onHierarchyObjectDoubleClicked(event);
          }));

  // =========================================================================
  // Issue #203: Connect Qt signals to EventBus for panel integration
  // This bridges the gap between Qt's signal/slot mechanism and the EventBus
  // pattern used by mediators.
  // =========================================================================

  // Connect Story Graph Panel's nodeSelected signal to publish EventBus event
  // Issue #470: Direct connection without debouncing here - debouncing is handled
  // in onStoryGraphNodeSelected() to allow immediate status updates while
  // throttling expensive operations like scene loading
  if (m_storyGraph) {
    connect(m_storyGraph, &qt::NMStoryGraphPanel::nodeSelected, this,
            [this](const QString &nodeIdString) {
              qDebug() << "[SelectionMediator] Publishing "
                          "StoryGraphNodeSelectedEvent for node:"
                       << nodeIdString;

              events::StoryGraphNodeSelectedEvent event;
              event.nodeIdString = nodeIdString;

              // Populate additional event data from the node if available
              if (auto *node = m_storyGraph->findNodeByIdString(nodeIdString)) {
                event.nodeType = node->nodeType();
                event.dialogueSpeaker = node->dialogueSpeaker();
                event.dialogueText = node->dialogueText();
                event.choiceOptions = node->choiceOptions();
              }

              EventBus::instance().publish(event);
            });

    connect(m_storyGraph, &qt::NMStoryGraphPanel::scriptNodeRequested, this,
            [](const QString &scriptPath) {
              qDebug() << "[SelectionMediator] Publishing ScriptNodeRequestedEvent for script:"
                       << scriptPath;
              events::ScriptNodeRequestedEvent event;
              event.scriptPath = scriptPath;
              EventBus::instance().publish(event);
            });

    connect(m_storyGraph, &qt::NMStoryGraphPanel::editDialogueFlowRequested, this,
            [](const QString &sceneId) {
              qDebug() << "[SelectionMediator] Publishing EditDialogueFlowRequestedEvent for scene:"
                       << sceneId;
              events::EditDialogueFlowRequestedEvent event;
              event.sceneId = sceneId;
              EventBus::instance().publish(event);
            });

    connect(m_storyGraph, &qt::NMStoryGraphPanel::openSceneScriptRequested, this,
            [](const QString &sceneId, const QString &scriptPath) {
              qDebug() << "[SelectionMediator] Publishing OpenSceneScriptRequestedEvent for scene:"
                       << sceneId << "script:" << scriptPath;
              events::OpenSceneScriptRequestedEvent event;
              event.sceneId = sceneId;
              event.scriptPath = scriptPath;
              EventBus::instance().publish(event);
            });

    // Issue #239: Connect navigateToScriptDefinitionRequested signal
    connect(m_storyGraph, &qt::NMStoryGraphPanel::navigateToScriptDefinitionRequested,
            this, [](const QString &sceneId, const QString &scriptPath) {
              qDebug() << "[SelectionMediator] Publishing "
                          "NavigateToScriptDefinitionEvent for scene:"
                       << sceneId << "script:" << scriptPath;

              events::NavigateToScriptDefinitionEvent event;
              event.sceneId = sceneId;
              event.scriptPath = scriptPath;
              EventBus::instance().publish(event);
            });

    // Issue #344: Connect sceneNodeDoubleClicked signal to navigate to Scene View
    connect(m_storyGraph, &qt::NMStoryGraphPanel::sceneNodeDoubleClicked,
            this, [](const QString &sceneId) {
              qDebug() << "[SelectionMediator] Publishing "
                          "SceneNodeDoubleClickedEvent for scene:"
                       << sceneId;

              events::SceneNodeDoubleClickedEvent event;
              event.sceneId = sceneId;
              EventBus::instance().publish(event);
            });
  }

  qDebug() << "[SelectionMediator] Initialized with"
           << m_subscriptions.size() << "subscriptions";
}

void SelectionMediator::shutdown() {
  // Issue #470: Cancel any pending debounced operations before shutdown
  m_selectionDebouncer.cancel();
  m_sceneLoadDebouncer.cancel();

  // Issue #463: Disconnect all Qt signal connections to prevent memory leaks
  // and stale connections. This is critical for proper cleanup when panels
  // are destroyed or mediator is reinitialized.
  if (m_storyGraph) {
    disconnect(m_storyGraph, nullptr, this, nullptr);
  }

  auto &bus = EventBus::instance();
  for (const auto &sub : m_subscriptions) {
    bus.unsubscribe(sub);
  }
  m_subscriptions.clear();
  qDebug() << "[SelectionMediator] Shutdown complete";
}

void SelectionMediator::onSceneObjectSelected(
    const events::SceneObjectSelectedEvent &event) {
  if (m_processingSelection) {
    return;
  }
  m_processingSelection = true;

  qDebug() << "[SelectionMediator] Scene object selected:" << event.objectId
           << "from:" << event.sourcePanel;

  // Update inspector if it's not the source
  if (m_inspector && event.sourcePanel != "Inspector") {
    if (event.objectId.isEmpty()) {
      m_inspector->showNoSelection();
    } else if (m_sceneView) {
      if (auto *obj = m_sceneView->findObjectById(event.objectId)) {
        m_inspector->inspectSceneObject(obj, event.editable);
      }
    }
  }

  // Update hierarchy if it's not the source
  if (m_hierarchy && event.sourcePanel != "Hierarchy") {
    m_hierarchy->selectObject(event.objectId);
  }

  // Update scene view if it's not the source
  if (m_sceneView && event.sourcePanel != "SceneView") {
    m_sceneView->selectObjectById(event.objectId);
  }

  // Publish status context update
  events::StatusContextChangedEvent statusEvent;
  if (!event.objectId.isEmpty()) {
    statusEvent.selectionLabel =
        QObject::tr("Object: %1").arg(event.objectId);
  }
  EventBus::instance().publish(statusEvent);

  m_processingSelection = false;
}

void SelectionMediator::onStoryGraphNodeSelected(
    const events::StoryGraphNodeSelectedEvent &event) {
  if (m_processingSelection) {
    return;
  }
  m_processingSelection = true;

  qDebug() << "[SelectionMediator] Story graph node selected:"
           << event.nodeIdString;

  // Issue #470: Store pending node ID for debounced operations
  m_pendingNodeId = event.nodeIdString;

  // Immediate lightweight updates (no debouncing needed)
  // Publish status context update immediately for responsive feedback
  events::StatusContextChangedEvent statusEvent;
  if (!event.nodeIdString.isEmpty()) {
    statusEvent.selectionLabel =
        QObject::tr("Node: %1").arg(event.nodeIdString);
    statusEvent.nodeId = event.nodeIdString;
  }
  EventBus::instance().publish(statusEvent);

  // Issue #470: Debounce inspector and scene view updates
  // This prevents UI freeze during rapid selection changes (marquee selection)
  m_selectionDebouncer.trigger([this, event]() {
    if (m_inspector) {
      if (event.nodeIdString.isEmpty()) {
        m_inspector->showNoSelection();
        if (m_sceneView) {
          m_sceneView->clearStoryPreview();
        }
      } else if (m_storyGraph) {
        if (auto *node =
                m_storyGraph->findNodeByIdString(event.nodeIdString)) {
          m_inspector->inspectStoryGraphNode(node, true);
          m_inspector->show();
          m_inspector->raise();

          // Update scene view with story preview
          if (m_sceneView) {
            m_sceneView->setStoryPreview(event.dialogueSpeaker,
                                         event.dialogueText,
                                         event.choiceOptions);
          }
        }
      }
    }

    // Issue #470: Debounce expensive scene loading operation separately
    // Uses longer delay to avoid loading multiple scenes during rapid navigation
    m_sceneLoadDebouncer.trigger([this, event]() {
      if (m_sceneView && m_storyGraph) {
        // Load scene document if not in play mode (handled by PlayModeController)
        if (event.nodeType.compare("Entry", Qt::CaseInsensitive) != 0) {
          // Scene loading is handled via LoadSceneDocumentRequestedEvent
          events::LoadSceneDocumentRequestedEvent loadEvent;
          loadEvent.sceneId = event.nodeIdString;
          EventBus::instance().publish(loadEvent);
          qDebug() << "[SelectionMediator] Debounced scene load for node:"
                   << event.nodeIdString;
        }
      }
    });
  });

  m_processingSelection = false;
}

void SelectionMediator::onAssetSelected(
    const events::AssetSelectedEvent &event) {
  qDebug() << "[SelectionMediator] Asset selected:" << event.path;

  // Publish status context update
  events::StatusContextChangedEvent statusEvent;
  if (!event.path.isEmpty()) {
    QFileInfo info(event.path);
    statusEvent.selectionLabel =
        QObject::tr("Asset: %1").arg(info.fileName());
    statusEvent.assetPath = event.path;
  }
  EventBus::instance().publish(statusEvent);
}

void SelectionMediator::onHierarchyObjectDoubleClicked(
    const events::HierarchyObjectDoubleClickedEvent &event) {
  if (event.objectId.isEmpty()) {
    return;
  }

  qDebug() << "[SelectionMediator] Hierarchy object double-clicked:"
           << event.objectId;

  // Show and raise inspector when hierarchy item is double-clicked
  if (m_inspector) {
    if (m_inspector->isFloating()) {
      m_inspector->setFloating(false);
    }
    m_inspector->show();
    m_inspector->raise();
    m_inspector->setFocus();
  }
}

} // namespace NovelMind::editor::mediators
