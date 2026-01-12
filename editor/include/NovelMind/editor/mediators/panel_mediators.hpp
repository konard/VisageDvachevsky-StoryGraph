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
 * - SceneMediator: Scene Registry integration (issue #211)
 * - SceneRegistryMediator: Scene Registry auto-sync events (issue #213)
 */

#include "NovelMind/editor/mediators/playback_mediator.hpp"
#include "NovelMind/editor/mediators/property_mediator.hpp"
#include "NovelMind/editor/mediators/scene_mediator.hpp"
#include "NovelMind/editor/mediators/scene_registry_mediator.hpp"
#include "NovelMind/editor/mediators/selection_mediator.hpp"
#include "NovelMind/editor/mediators/workflow_mediator.hpp"

#include <QObject>
#include <memory>

namespace NovelMind::editor {
class SceneRegistry; // Forward declaration
}

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

namespace NovelMind::editor {
class SceneRegistry;
} // namespace NovelMind::editor

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
  explicit PanelMediatorManager(QObject* parent = nullptr);
  ~PanelMediatorManager() override;

  /**
   * @brief Initialize all mediators with the given panels
   *
   * This replaces the ~1,500 lines of direct connect() calls
   * with mediator-based coordination.
   *
   * @param sceneRegistry Optional SceneRegistry for auto-sync events (issue #213)
   */
  void initialize(qt::NMSceneViewPanel* sceneView, qt::NMStoryGraphPanel* storyGraph,
                  qt::NMSceneDialogueGraphPanel* dialogueGraph, qt::NMInspectorPanel* inspector,
                  qt::NMHierarchyPanel* hierarchy, qt::NMScriptEditorPanel* scriptEditor,
                  qt::NMScriptDocPanel* scriptDoc, qt::NMTimelinePanel* timeline,
                  qt::NMCurveEditorPanel* curveEditor, qt::NMVoiceStudioPanel* voiceStudio,
                  qt::NMVoiceManagerPanel* voiceManager, qt::NMDiagnosticsPanel* diagnostics,
                  qt::NMIssuesPanel* issues, SceneRegistry* sceneRegistry = nullptr);

  /**
   * @brief Initialize the Scene Mediator with a SceneRegistry
   *
   * This can be called after initialize() when a SceneRegistry becomes
   * available (e.g., when a project is loaded).
   *
   * @param sceneRegistry The scene registry to use
   * @param sceneView The scene view panel
   * @param storyGraph The story graph panel
   */
  void initializeSceneMediator(SceneRegistry* sceneRegistry, qt::NMSceneViewPanel* sceneView,
                               qt::NMStoryGraphPanel* storyGraph);

  /**
   * @brief Shutdown all mediators
   */
  void shutdown();

  /**
   * @brief Check if mediators are initialized
   */
  [[nodiscard]] bool isInitialized() const { return m_initialized; }

  // Accessor methods for individual mediators if needed
  [[nodiscard]] SelectionMediator* selectionMediator() const { return m_selectionMediator.get(); }
  [[nodiscard]] WorkflowMediator* workflowMediator() const { return m_workflowMediator.get(); }
  [[nodiscard]] PropertyMediator* propertyMediator() const { return m_propertyMediator.get(); }
  [[nodiscard]] PlaybackMediator* playbackMediator() const { return m_playbackMediator.get(); }
  [[nodiscard]] SceneRegistryMediator* sceneRegistryMediator() const {
    return m_sceneRegistryMediator.get();
  }
  [[nodiscard]] SceneMediator* sceneMediator() const { return m_sceneMediator.get(); }

private:
  std::unique_ptr<SelectionMediator> m_selectionMediator;
  std::unique_ptr<WorkflowMediator> m_workflowMediator;
  std::unique_ptr<PropertyMediator> m_propertyMediator;
  std::unique_ptr<PlaybackMediator> m_playbackMediator;
  std::unique_ptr<SceneMediator> m_sceneMediator;
  std::unique_ptr<SceneRegistryMediator> m_sceneRegistryMediator;

  bool m_initialized = false;
};

} // namespace NovelMind::editor::mediators
