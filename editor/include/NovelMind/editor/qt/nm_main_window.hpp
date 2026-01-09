#pragma once

/**
 * @file nm_main_window.hpp
 * @brief Main window for the NovelMind Editor
 *
 * The central main window that contains:
 * - Menu bar with all editor actions
 * - Toolbar with common actions
 * - Docking framework for all panels
 * - Status bar with editor state information
 */

#include <QMainWindow>
#include <QTimer>
#include <QToolBar>
#include <memory>

class QMenuBar;
class QToolBar;
class QStatusBar;
class QAction;
class QActionGroup;
class QLabel;

namespace NovelMind::editor {
class NMSettingsRegistry;
}

namespace NovelMind::editor::mediators {
class PanelMediatorManager;
}

namespace NovelMind::editor::qt {

// Forward declarations
class NMDockPanel;
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
class NMScriptInspectorPanel;
class NMIssuesPanel;
class NMDiagnosticsPanel;
class NMVoiceManagerPanel;
class NMLocalizationPanel;
class NMTimelinePanel;
class NMCurveEditorPanel;
class NMBuildSettingsPanel;
class NMVoiceStudioPanel;
class NMAudioMixerPanel;
class NMAnimationAdapter;
class NMProjectSettingsPanel;
class NMScriptRuntimeInspectorPanel;

/**
 * @brief Main application window for the NovelMind Editor
 */
class NMMainWindow : public QMainWindow {
  Q_OBJECT

public:
  /**
   * @brief Construct the main window
   * @param parent Parent widget (usually nullptr for main window)
   */
  explicit NMMainWindow(QWidget *parent = nullptr);

  /**
   * @brief Destructor
   */
  ~NMMainWindow() override;

  /**
   * @brief Initialize the main window and all panels
   * @return true if initialization succeeded
   */
  bool initialize();

  /**
   * @brief Shutdown and cleanup
   */
  void shutdown();

  // =========================================================================
  // Panel Access
  // =========================================================================

  [[nodiscard]] NMSceneViewPanel *sceneViewPanel() const {
    return m_sceneViewPanel;
  }
  [[nodiscard]] NMStoryGraphPanel *storyGraphPanel() const {
    return m_storyGraphPanel;
  }
  [[nodiscard]] NMInspectorPanel *inspectorPanel() const {
    return m_inspectorPanel;
  }
  [[nodiscard]] NMConsolePanel *consolePanel() const { return m_consolePanel; }
  [[nodiscard]] NMAssetBrowserPanel *assetBrowserPanel() const {
    return m_assetBrowserPanel;
  }
  [[nodiscard]] NMScenePalettePanel *scenePalettePanel() const {
    return m_scenePalettePanel;
  }
  [[nodiscard]] NMIssuesPanel *issuesPanel() const { return m_issuesPanel; }
  [[nodiscard]] NMDiagnosticsPanel *diagnosticsPanel() const {
    return m_diagnosticsPanel;
  }
  [[nodiscard]] NMHierarchyPanel *hierarchyPanel() const {
    return m_hierarchyPanel;
  }
  [[nodiscard]] NMScriptEditorPanel *scriptEditorPanel() const {
    return m_scriptEditorPanel;
  }
  [[nodiscard]] NMScriptDocPanel *scriptDocPanel() const {
    return m_scriptDocPanel;
  }
  [[nodiscard]] NMPlayToolbarPanel *playToolbarPanel() const {
    return m_playToolbarPanel;
  }
  [[nodiscard]] NMDebugOverlayPanel *debugOverlayPanel() const {
    return m_debugOverlayPanel;
  }
  [[nodiscard]] NMVoiceManagerPanel *voiceManagerPanel() const {
    return m_voiceManagerPanel;
  }
  [[nodiscard]] NMLocalizationPanel *localizationPanel() const {
    return m_localizationPanel;
  }
  [[nodiscard]] NMTimelinePanel *timelinePanel() const {
    return m_timelinePanel;
  }
  [[nodiscard]] NMCurveEditorPanel *curveEditorPanel() const {
    return m_curveEditorPanel;
  }
  [[nodiscard]] NMBuildSettingsPanel *buildSettingsPanel() const {
    return m_buildSettingsPanel;
  }
  [[nodiscard]] NMVoiceStudioPanel *voiceStudioPanel() const {
    return m_voiceStudioPanel;
  }
  [[nodiscard]] NMAudioMixerPanel *audioMixerPanel() const {
    return m_audioMixerPanel;
  }
  [[nodiscard]] NMProjectSettingsPanel *projectSettingsPanel() const {
    return m_projectSettingsPanel;
  }
  [[nodiscard]] NMScriptRuntimeInspectorPanel *scriptRuntimeInspectorPanel() const {
    return m_scriptRuntimeInspectorPanel;
  }

  // =========================================================================
  // Layout Management
  // =========================================================================

  /**
   * @brief Save the current window layout to settings
   */
  void saveLayout();

  /**
   * @brief Restore the window layout from settings
   */
  void restoreLayout();

  /**
   * @brief Reset to the default layout
   */
  void resetToDefaultLayout();

signals:
  /**
   * @brief Emitted when a new project should be created
   */
  void newProjectRequested();

  /**
   * @brief Emitted when a project should be opened
   */
  void openProjectRequested();

  /**
   * @brief Emitted when the current project should be saved
   */
  void saveProjectRequested();

  /**
   * @brief Emitted when undo is requested
   */
  void undoRequested();

  /**
   * @brief Emitted when redo is requested
   */
  void redoRequested();

  /**
   * @brief Emitted when play mode should start
   */
  void playRequested();

  /**
   * @brief Emitted when play mode should stop
   */
  void stopRequested();

public slots:
  /**
   * @brief Update all panels (called by timer)
   */
  void onUpdateTick();

  /**
   * @brief Show the about dialog
   */
  void showAboutDialog();

  /**
   * @brief Show the settings dialog
   */
  void showSettingsDialog();

  /**
   * @brief Toggle panel visibility
   */
  void togglePanel(NMDockPanel *panel);

  /**
   * @brief Set status bar message
   */
  void setStatusMessage(const QString &message, int timeout = 0);

  /**
   * @brief Update the window title with project name
   */
  void updateWindowTitle(const QString &projectName = QString());

protected:
  void closeEvent(QCloseEvent *event) override;
  bool eventFilter(QObject *watched, QEvent *event) override;

public:
  /**
   * @brief D2: Layout presets for different workflows
   *
   * These presets are designed for common use cases in visual novel
   * development:
   * - Default: Balanced layout for general editing
   * - StoryScript: Story graph and script editing focused
   * - SceneAnimation: Scene view with timeline/curve editors
   * - AudioVoice: Voice studio with audio mixing
   * - Diagnostics: Development and debugging focused
   */
  enum class LayoutPreset {
    Default,        // D2: Default balanced workspace
    StoryScript,    // D2: Story/Script focused
    SceneAnimation, // D2: Scene/Animation focused
    AudioVoice,     // D2: Audio/Voice focused (D6)
    Story,          // Legacy: Story graph focused
    Scene,          // Legacy: Scene editing focused
    Script,         // Legacy: Script editing focused
    Developer,      // Legacy: Development tools visible
    Compact         // Legacy: Minimal UI
  };

  /**
   * @brief Apply a workspace preset
   * @param preset The preset to apply
   */
  void applyWorkspacePreset(LayoutPreset preset);

  /**
   * @brief Get the current workspace preset name
   * @return The name of the current preset
   */
  QString currentWorkspacePresetName() const;

  /**
   * @brief Save the current layout as a named preset
   * @param name The name for the preset
   */
  void saveWorkspacePreset(const QString &name);

  /**
   * @brief Load a named workspace preset
   * @param name The name of the preset to load
   * @return true if loaded successfully
   */
  bool loadWorkspacePreset(const QString &name);

  /**
   * @brief Get list of available workspace presets
   * @return List of preset names (including built-in and custom)
   */
  QStringList availableWorkspacePresets() const;

private:
  void setupMenuBar();
  void setupToolBar();
  void setupStatusBar();
  void setupPanels();
  void setupConnections();
  void setupShortcuts();
  void createDefaultLayout();
  void configureDocking();
  void applyLayoutPreset(LayoutPreset preset);
  void focusNextDock(bool reverse = false);
  void showCommandPalette();
  void addDockContextActions(QDockWidget *dock);
  void handleNavigationRequest(const QString &locationString);
  void toggleFocusMode(bool enabled);
  void applyFocusModeLayout();
  void updateStatusBarContext();
  void applyDockLockState(bool locked);
  void applyTabbedDockMode(bool enabled);
  void applyFloatAllowed(bool allowed);
  void saveCustomLayout();
  void loadCustomLayout();

  // Connection setup helpers (refactored from monolithic setupConnections)
  void setupClipboardConnections();
  void setupPanelToggleConnections();
  void setupPanelVisibilitySync();
  void setupLayoutConnections();
  void setupPlayConnections();
  void setupHelpConnections();
  void setupPanelMediators();

  // Panel toggle helpers
  void toggleVoiceStudioPanel(bool checked);
  void toggleAudioMixerPanel(bool checked);

  // Dialog helpers
  void showHotkeysDialog();
  void onValidateProject();

  // =========================================================================
  // Menu Actions
  // =========================================================================

  // File menu
  QAction *m_actionNewProject = nullptr;
  QAction *m_actionOpenProject = nullptr;
  QAction *m_actionSaveProject = nullptr;
  QAction *m_actionSaveProjectAs = nullptr;
  QAction *m_actionCloseProject = nullptr;
  QAction *m_actionExit = nullptr;

  // Project menu
  QAction *m_actionValidateProject = nullptr;

  // Edit menu
  QAction *m_actionUndo = nullptr;
  QAction *m_actionRedo = nullptr;
  QAction *m_actionCut = nullptr;
  QAction *m_actionCopy = nullptr;
  QAction *m_actionPaste = nullptr;
  QAction *m_actionDelete = nullptr;
  QAction *m_actionSelectAll = nullptr;
  QAction *m_actionPreferences = nullptr;

  // View menu
  QAction *m_actionToggleSceneView = nullptr;
  QAction *m_actionToggleStoryGraph = nullptr;
  QAction *m_actionToggleInspector = nullptr;
  QAction *m_actionToggleConsole = nullptr;
  QAction *m_actionToggleAssetBrowser = nullptr;
  QAction *m_actionToggleScenePalette = nullptr;
  QAction *m_actionToggleHierarchy = nullptr;
  QAction *m_actionToggleScriptEditor = nullptr;
  QAction *m_actionToggleScriptDocs = nullptr;
  QAction *m_actionToggleIssues = nullptr;
  QAction *m_actionToggleDiagnostics = nullptr;
  QAction *m_actionToggleDebugOverlay = nullptr;
  QAction *m_actionToggleScriptRuntimeInspector = nullptr;
  QAction *m_actionToggleVoiceManager = nullptr;
  QAction *m_actionToggleVoiceStudio = nullptr;
  QAction *m_actionToggleAudioMixer = nullptr;
  QAction *m_actionToggleLocalization = nullptr;
  QAction *m_actionToggleTimeline = nullptr;
  QAction *m_actionToggleCurveEditor = nullptr;
  QAction *m_actionToggleBuildSettings = nullptr;
  QAction *m_actionLayoutStory = nullptr;
  QAction *m_actionLayoutScene = nullptr;
  QAction *m_actionLayoutScript = nullptr;
  QAction *m_actionLayoutDeveloper = nullptr;
  QAction *m_actionLayoutCompact = nullptr;

  // D2: New workspace presets
  QAction *m_actionLayoutDefault = nullptr;
  QAction *m_actionLayoutStoryScript = nullptr;
  QAction *m_actionLayoutSceneAnimation = nullptr;
  QAction *m_actionLayoutAudioVoice = nullptr;
  QAction *m_actionResetLayout = nullptr;
  QAction *m_actionSaveLayout = nullptr;
  QAction *m_actionLoadLayout = nullptr;
  QAction *m_actionFocusMode = nullptr;
  QAction *m_actionFocusIncludeHierarchy = nullptr;
  QAction *m_actionLockLayout = nullptr;
  QAction *m_actionTabbedDockOnly = nullptr;
  QAction *m_actionFloatAllowed = nullptr;
  QAction *m_actionUiScaleDown = nullptr;
  QAction *m_actionUiScaleUp = nullptr;
  QAction *m_actionUiScaleReset = nullptr;
  QAction *m_actionUiScaleCompact = nullptr;
  QAction *m_actionUiScaleDefault = nullptr;
  QAction *m_actionUiScaleComfort = nullptr;
  QAction *m_actionThemeDark = nullptr;
  QAction *m_actionThemeLight = nullptr;

  // Play menu
  QAction *m_actionPlay = nullptr;
  QAction *m_actionPause = nullptr;
  QAction *m_actionStop = nullptr;
  QAction *m_actionStepFrame = nullptr;
  QAction *m_actionSaveState = nullptr;
  QAction *m_actionLoadState = nullptr;
  QAction *m_actionAutoSaveState = nullptr;
  QAction *m_actionAutoLoadState = nullptr;

  // Help menu
  QAction *m_actionAbout = nullptr;
  QAction *m_actionDocumentation = nullptr;
  QAction *m_actionHotkeys = nullptr;

  // Status bar segments
  QLabel *m_statusLabel = nullptr;
  QLabel *m_statusPlay = nullptr;
  QLabel *m_statusNode = nullptr;
  QLabel *m_statusSelection = nullptr;
  QLabel *m_statusAsset = nullptr;
  QLabel *m_statusUnsaved = nullptr;
  QLabel *m_statusFps = nullptr;
  QLabel *m_statusCache = nullptr;

  // =========================================================================
  // UI Components
  // =========================================================================

  QToolBar *m_mainToolBar = nullptr;
  bool m_focusModeEnabled = false;
  bool m_focusIncludeHierarchy = true;
  QByteArray m_focusState;
  QByteArray m_focusGeometry;

  bool m_layoutLocked = false;
  bool m_tabbedDockOnly = false;
  bool m_floatAllowed = true;

  // D2: Current workspace preset tracking
  LayoutPreset m_currentPreset = LayoutPreset::Default;

  QString m_activeProjectName;
  QString m_activeGraphLabel = "StoryGraph";
  QString m_activeNodeId;
  QString m_activeSceneId;
  QString m_activeSelectionLabel;
  QString m_activeAssetPath;
  int m_fpsFrameCount = 0;
  qint64 m_fpsLastSample = 0;
  double m_lastFps = 0.0;
  QDockWidget *m_lastFocusedDock = nullptr;

  // =========================================================================
  // Panels
  // =========================================================================

  NMSceneViewPanel *m_sceneViewPanel = nullptr;
  NMStoryGraphPanel *m_storyGraphPanel = nullptr;
  NMSceneDialogueGraphPanel *m_sceneDialogueGraphPanel = nullptr;
  NMInspectorPanel *m_inspectorPanel = nullptr;
  NMConsolePanel *m_consolePanel = nullptr;
  NMAssetBrowserPanel *m_assetBrowserPanel = nullptr;
  NMScenePalettePanel *m_scenePalettePanel = nullptr;
  NMHierarchyPanel *m_hierarchyPanel = nullptr;
  NMScriptEditorPanel *m_scriptEditorPanel = nullptr;
  NMScriptDocPanel *m_scriptDocPanel = nullptr;
  NMPlayToolbarPanel *m_playToolbarPanel = nullptr;
  NMDebugOverlayPanel *m_debugOverlayPanel = nullptr;
  NMScriptInspectorPanel *m_scriptInspectorPanel = nullptr;
  NMIssuesPanel *m_issuesPanel = nullptr;
  NMDiagnosticsPanel *m_diagnosticsPanel = nullptr;
  NMVoiceManagerPanel *m_voiceManagerPanel = nullptr;
  NMLocalizationPanel *m_localizationPanel = nullptr;
  NMTimelinePanel *m_timelinePanel = nullptr;
  NMCurveEditorPanel *m_curveEditorPanel = nullptr;
  NMBuildSettingsPanel *m_buildSettingsPanel = nullptr;
  NMVoiceStudioPanel *m_voiceStudioPanel = nullptr;
  NMAudioMixerPanel *m_audioMixerPanel = nullptr;
  NMAnimationAdapter *m_animationAdapter = nullptr;
  NMProjectSettingsPanel *m_projectSettingsPanel = nullptr;
  NMScriptRuntimeInspectorPanel *m_scriptRuntimeInspectorPanel = nullptr;

  // =========================================================================
  // State
  // =========================================================================

  QTimer *m_updateTimer = nullptr;
  bool m_initialized = false;
  static constexpr int UPDATE_INTERVAL_MS = 16; // ~60 FPS

  // Settings system
  std::unique_ptr<editor::NMSettingsRegistry> m_settingsRegistry;

  // Panel mediator manager (replaces 1,400+ lines of direct connections)
  std::unique_ptr<mediators::PanelMediatorManager> m_mediatorManager;
};

} // namespace NovelMind::editor::qt
