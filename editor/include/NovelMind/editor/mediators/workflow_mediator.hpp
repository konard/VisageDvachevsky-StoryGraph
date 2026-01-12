#pragma once

/**
 * @file workflow_mediator.hpp
 * @brief Mediator for coordinating workflow navigation across panels
 *
 * The WorkflowMediator handles:
 * - Story graph navigation (node activation, scene loading)
 * - Dialogue graph navigation
 * - Script/asset navigation
 * - Voice recording workflow coordination
 */

#include "NovelMind/editor/event_bus.hpp"
#include "NovelMind/editor/events/panel_events.hpp"
#include <QObject>
#include <memory>
#include <vector>

namespace NovelMind::editor::qt {
class NMSceneViewPanel;
class NMStoryGraphPanel;
class NMSceneDialogueGraphPanel;
class NMScriptEditorPanel;
class NMTimelinePanel;
class NMVoiceStudioPanel;
class NMVoiceManagerPanel;
class NMInspectorPanel;
class NMDiagnosticsPanel;
class NMIssuesPanel;
} // namespace NovelMind::editor::qt

namespace NovelMind::editor::mediators {

/**
 * @brief Mediator for workflow navigation coordination
 *
 * This class handles complex multi-panel workflows such as:
 * - Opening scenes from story graph
 * - Navigating to scripts
 * - Voice recording workflows
 * - Dialogue graph editing
 */
class WorkflowMediator : public QObject {
  Q_OBJECT

public:
  /**
   * @brief Construct the workflow mediator
   */
  WorkflowMediator(qt::NMSceneViewPanel* sceneView, qt::NMStoryGraphPanel* storyGraph,
                   qt::NMSceneDialogueGraphPanel* dialogueGraph,
                   qt::NMScriptEditorPanel* scriptEditor, qt::NMTimelinePanel* timeline,
                   qt::NMVoiceStudioPanel* voiceStudio, qt::NMVoiceManagerPanel* voiceManager,
                   qt::NMInspectorPanel* inspector, qt::NMDiagnosticsPanel* diagnostics,
                   qt::NMIssuesPanel* issues, QObject* parent = nullptr);

  ~WorkflowMediator() override;

  /**
   * @brief Initialize event subscriptions
   */
  void initialize();

  /**
   * @brief Shutdown and cleanup subscriptions
   */
  void shutdown();

private:
  // Story graph navigation
  void onStoryGraphNodeActivated(const events::StoryGraphNodeActivatedEvent& event);
  void onSceneNodeDoubleClicked(const events::SceneNodeDoubleClickedEvent& event);
  void onScriptNodeRequested(const events::ScriptNodeRequestedEvent& event);

  // Dialogue graph navigation
  void onEditDialogueFlowRequested(const events::EditDialogueFlowRequestedEvent& event);
  void onReturnToStoryGraphRequested(const events::ReturnToStoryGraphRequestedEvent& event);
  void onDialogueCountChanged(const events::DialogueCountChangedEvent& event);

  // Script navigation
  void onOpenSceneScriptRequested(const events::OpenSceneScriptRequestedEvent& event);
  void onGoToScriptLocation(const events::GoToScriptLocationEvent& event);
  void onNavigateToScriptDefinition(const events::NavigateToScriptDefinitionEvent& event);

  // Voice recording workflow
  void onVoiceClipAssignRequested(const events::VoiceClipAssignRequestedEvent& event);
  void onVoiceAutoDetectRequested(const events::VoiceAutoDetectRequestedEvent& event);
  void onVoiceClipPreviewRequested(const events::VoiceClipPreviewRequestedEvent& event);
  void onVoiceRecordingRequested(const events::VoiceRecordingRequestedEvent& event);

  // Issue/diagnostic navigation
  void onIssueActivated(const events::IssueActivatedEvent& event);
  void onDiagnosticActivated(const events::DiagnosticActivatedEvent& event);
  void onNavigationRequested(const events::NavigationRequestedEvent& event);

  // Asset workflow
  void onAssetDoubleClicked(const events::AssetDoubleClickedEvent& event);
  void onAssetsDropped(const events::AssetsDroppedEvent& event);

  // Scene document loading
  void onLoadSceneDocumentRequested(const events::LoadSceneDocumentRequestedEvent& event);

  // Scene auto-sync events (issue #213)
  void onSceneDocumentModified(const events::SceneDocumentModifiedEvent& event);
  void onSceneThumbnailUpdated(const events::SceneThumbnailUpdatedEvent& event);
  void onSceneRenamed(const events::SceneRenamedEvent& event);
  void onSceneDeleted(const events::SceneDeletedEvent& event);

  qt::NMSceneViewPanel* m_sceneView = nullptr;
  qt::NMStoryGraphPanel* m_storyGraph = nullptr;
  qt::NMSceneDialogueGraphPanel* m_dialogueGraph = nullptr;
  qt::NMScriptEditorPanel* m_scriptEditor = nullptr;
  qt::NMTimelinePanel* m_timeline = nullptr;
  qt::NMVoiceStudioPanel* m_voiceStudio = nullptr;
  qt::NMVoiceManagerPanel* m_voiceManager = nullptr;
  qt::NMInspectorPanel* m_inspector = nullptr;
  qt::NMDiagnosticsPanel* m_diagnostics = nullptr;
  qt::NMIssuesPanel* m_issues = nullptr;

  std::vector<EventSubscription> m_subscriptions;
};

} // namespace NovelMind::editor::mediators
