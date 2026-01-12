#include "NovelMind/editor/mediators/panel_mediators.hpp"
#include "NovelMind/editor/scene_registry.hpp"

#include <QDebug>

namespace NovelMind::editor::mediators {

PanelMediatorManager::PanelMediatorManager(QObject* parent) : QObject(parent) {}

PanelMediatorManager::~PanelMediatorManager() {
  shutdown();
}

void PanelMediatorManager::initialize(
    qt::NMSceneViewPanel* sceneView, qt::NMStoryGraphPanel* storyGraph,
    qt::NMSceneDialogueGraphPanel* dialogueGraph, qt::NMInspectorPanel* inspector,
    qt::NMHierarchyPanel* hierarchy, qt::NMScriptEditorPanel* scriptEditor,
    qt::NMScriptDocPanel* scriptDoc, qt::NMTimelinePanel* timeline,
    qt::NMCurveEditorPanel* curveEditor, qt::NMVoiceStudioPanel* voiceStudio,
    qt::NMVoiceManagerPanel* voiceManager, qt::NMDiagnosticsPanel* diagnostics,
    qt::NMIssuesPanel* issues, SceneRegistry* sceneRegistry) {
  if (m_initialized) {
    qWarning() << "[PanelMediatorManager] Already initialized, shutting down first";
    shutdown();
  }

  qDebug() << "[PanelMediatorManager] Initializing panel mediators...";

  // Create and initialize Selection Mediator
  m_selectionMediator =
      std::make_unique<SelectionMediator>(sceneView, hierarchy, inspector, storyGraph, this);
  m_selectionMediator->initialize();

  // Create and initialize Workflow Mediator
  m_workflowMediator = std::make_unique<WorkflowMediator>(
      sceneView, storyGraph, dialogueGraph, scriptEditor, timeline, voiceStudio, voiceManager,
      inspector, diagnostics, issues, this);
  m_workflowMediator->initialize();

  // Create and initialize Property Mediator
  m_propertyMediator = std::make_unique<PropertyMediator>(
      sceneView, inspector, storyGraph, curveEditor, scriptEditor, scriptDoc, this);
  m_propertyMediator->initialize();

  // Create and initialize Playback Mediator
  m_playbackMediator = std::make_unique<PlaybackMediator>(sceneView, timeline, this);
  m_playbackMediator->initialize();

  // Create and initialize Scene Registry Mediator (issue #213)
  if (sceneRegistry) {
    m_sceneRegistryMediator = std::make_unique<SceneRegistryMediator>(sceneRegistry, this);
    m_sceneRegistryMediator->initialize();
    qDebug() << "[PanelMediatorManager] Scene Registry Mediator initialized";
  } else {
    qDebug() << "[PanelMediatorManager] Scene Registry not provided, skipping "
                "Scene Registry Mediator";
  }

  m_initialized = true;
  qDebug() << "[PanelMediatorManager] All panel mediators initialized successfully";
}

void PanelMediatorManager::initializeSceneMediator(SceneRegistry* sceneRegistry,
                                                   qt::NMSceneViewPanel* sceneView,
                                                   qt::NMStoryGraphPanel* storyGraph) {
  if (!sceneRegistry) {
    qWarning() << "[PanelMediatorManager] Cannot initialize SceneMediator without SceneRegistry";
    return;
  }

  // Shutdown existing scene mediator if any
  if (m_sceneMediator) {
    m_sceneMediator->shutdown();
    m_sceneMediator.reset();
  }

  qDebug() << "[PanelMediatorManager] Initializing SceneMediator...";

  m_sceneMediator = std::make_unique<SceneMediator>(this);
  m_sceneMediator->initialize(sceneRegistry, sceneView, storyGraph);

  qDebug() << "[PanelMediatorManager] SceneMediator initialized successfully";
}

void PanelMediatorManager::shutdown() {
  if (!m_initialized) {
    return;
  }

  qDebug() << "[PanelMediatorManager] Shutting down panel mediators...";

  if (m_sceneRegistryMediator) {
    m_sceneRegistryMediator->shutdown();
    m_sceneRegistryMediator.reset();
  }

  if (m_playbackMediator) {
    m_playbackMediator->shutdown();
    m_playbackMediator.reset();
  }

  if (m_propertyMediator) {
    m_propertyMediator->shutdown();
    m_propertyMediator.reset();
  }

  if (m_workflowMediator) {
    m_workflowMediator->shutdown();
    m_workflowMediator.reset();
  }

  if (m_selectionMediator) {
    m_selectionMediator->shutdown();
    m_selectionMediator.reset();
  }

  m_initialized = false;
  qDebug() << "[PanelMediatorManager] All panel mediators shut down";
}

} // namespace NovelMind::editor::mediators
