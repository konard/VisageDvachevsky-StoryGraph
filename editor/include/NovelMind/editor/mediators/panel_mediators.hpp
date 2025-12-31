#pragma once

/**
 * @file panel_mediators.hpp
 * @brief Main include file for all panel mediators
 *
 * This file provides a unified interface for managing all panel mediators
 * used to decouple panel communication via the EventBus pattern.
 *
 * Instead of 1,500+ lines of direct connect() calls in NMMainWindow,
 * mediators handle specific coordination concerns:
 * - SelectionMediator: Object/node selection synchronization
 * - WorkflowMediator: Multi-panel workflow navigation
 * - PropertyMediator: Property change propagation
 * - PlaybackMediator: Timeline/animation coordination
 */

#include "NovelMind/editor/mediators/playback_mediator.hpp"
#include "NovelMind/editor/mediators/property_mediator.hpp"
#include "NovelMind/editor/mediators/selection_mediator.hpp"
#include "NovelMind/editor/mediators/workflow_mediator.hpp"

#include <QObject>
#include <memory>

namespace NovelMind::editor::qt {

// Forward declarations for all panels
class NMSceneViewPanel;
class NMStoryGraphPanel;
class NMSceneDialogueGraphPanel;
class NMInspectorPanel;
class NMConsolePanel;
class NMAssetBrowserPanel;
class NMScenePalettePanel;
class NMHierarchyPanel;
class NMScriptEditorPanel;
class NMScriptDocPanel;
class NMPlayToolbarPanel;
class NMDebugOverlayPanel;
class NMIssuesPanel;
class NMDiagnosticsPanel;
class NMVoiceManagerPanel;
class NMVoiceStudioPanel;
class NMAudioMixerPanel;
class NMLocalizationPanel;
class NMTimelinePanel;
class NMCurveEditorPanel;
class NMBuildSettingsPanel;

} // namespace NovelMind::editor::qt

namespace NovelMind::editor::mediators {

/**
 * @brief Manager class for all panel mediators
 *
 * This class owns and manages the lifecycle of all mediators,
 * providing a single point of initialization and shutdown.
 */
class PanelMediatorManager : public QObject {
  Q_OBJECT

public:
  explicit PanelMediatorManager(QObject *parent = nullptr);
  ~PanelMediatorManager() override;

  /**
   * @brief Initialize all mediators with the given panels
   *
   * This replaces the ~1,500 lines of direct connect() calls
   * with mediator-based coordination.
   */
  void initialize(qt::NMSceneViewPanel *sceneView,
                  qt::NMStoryGraphPanel *storyGraph,
                  qt::NMSceneDialogueGraphPanel *dialogueGraph,
                  qt::NMInspectorPanel *inspector,
                  qt::NMHierarchyPanel *hierarchy,
                  qt::NMScriptEditorPanel *scriptEditor,
                  qt::NMScriptDocPanel *scriptDoc,
                  qt::NMTimelinePanel *timeline,
                  qt::NMCurveEditorPanel *curveEditor,
                  qt::NMVoiceStudioPanel *voiceStudio,
                  qt::NMVoiceManagerPanel *voiceManager,
                  qt::NMDiagnosticsPanel *diagnostics,
                  qt::NMIssuesPanel *issues);

  /**
   * @brief Shutdown all mediators
   */
  void shutdown();

  /**
   * @brief Check if mediators are initialized
   */
  [[nodiscard]] bool isInitialized() const { return m_initialized; }

  // Accessor methods for individual mediators if needed
  [[nodiscard]] SelectionMediator *selectionMediator() const {
    return m_selectionMediator.get();
  }
  [[nodiscard]] WorkflowMediator *workflowMediator() const {
    return m_workflowMediator.get();
  }
  [[nodiscard]] PropertyMediator *propertyMediator() const {
    return m_propertyMediator.get();
  }
  [[nodiscard]] PlaybackMediator *playbackMediator() const {
    return m_playbackMediator.get();
  }

private:
  std::unique_ptr<SelectionMediator> m_selectionMediator;
  std::unique_ptr<WorkflowMediator> m_workflowMediator;
  std::unique_ptr<PropertyMediator> m_propertyMediator;
  std::unique_ptr<PlaybackMediator> m_playbackMediator;

  bool m_initialized = false;
};

} // namespace NovelMind::editor::mediators
