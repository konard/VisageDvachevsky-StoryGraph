#include "NovelMind/editor/qt/nm_dock_panel.hpp"
#include "NovelMind/editor/qt/nm_icon_manager.hpp"
#include "NovelMind/editor/qt/nm_main_window.hpp"
#include "NovelMind/editor/qt/panels/nm_asset_browser_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_audio_mixer_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_build_settings_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_console_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_curve_editor_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_debug_overlay_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_diagnostics_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_hierarchy_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_inspector_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_issues_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_localization_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_play_toolbar_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_project_settings_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_scene_dialogue_graph_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_scene_palette_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_scene_view_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_script_doc_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_script_editor_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_story_graph_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_timeline_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_voice_manager_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_voice_studio_panel.hpp"

#include <QAction>
#include <QDockWidget>
#include <QTabWidget>

namespace NovelMind::editor::qt {

void NMMainWindow::setupPanels() {
  auto &iconMgr = NMIconManager::instance();

  // Create all panels with their respective icons
  m_sceneViewPanel = new NMSceneViewPanel(this);
  m_sceneViewPanel->setObjectName("SceneViewPanel");
  m_sceneViewPanel->setWindowIcon(iconMgr.getIcon("panel-scene", 16));

  m_storyGraphPanel = new NMStoryGraphPanel(this);
  m_storyGraphPanel->setObjectName("StoryGraphPanel");
  m_storyGraphPanel->setWindowIcon(iconMgr.getIcon("panel-graph", 16));

  m_sceneDialogueGraphPanel = new NMSceneDialogueGraphPanel(this);
  m_sceneDialogueGraphPanel->setObjectName("SceneDialogueGraphPanel");
  m_sceneDialogueGraphPanel->setWindowIcon(
      iconMgr.getIcon("node-dialogue", 16));

  m_inspectorPanel = new NMInspectorPanel(this);
  m_inspectorPanel->setObjectName("InspectorPanel");
  m_inspectorPanel->setWindowIcon(iconMgr.getIcon("panel-inspector", 16));

  m_consolePanel = new NMConsolePanel(this);
  m_consolePanel->setObjectName("ConsolePanel");
  m_consolePanel->setWindowIcon(iconMgr.getIcon("panel-console", 16));

  m_assetBrowserPanel = new NMAssetBrowserPanel(this);
  m_assetBrowserPanel->setObjectName("AssetBrowserPanel");
  m_assetBrowserPanel->setWindowIcon(iconMgr.getIcon("panel-assets", 16));

  m_voiceManagerPanel = new NMVoiceManagerPanel(this);
  m_voiceManagerPanel->setObjectName("VoiceManagerPanel");
  m_voiceManagerPanel->setWindowIcon(iconMgr.getIcon("panel-voice", 16));

  m_voiceStudioPanel = new NMVoiceStudioPanel(this);
  m_voiceStudioPanel->setObjectName("VoiceStudioPanel");
  m_voiceStudioPanel->setWindowIcon(iconMgr.getIcon("panel-voice", 16));

  m_audioMixerPanel = new NMAudioMixerPanel(this);
  m_audioMixerPanel->setObjectName("AudioMixerPanel");
  m_audioMixerPanel->setWindowIcon(iconMgr.getIcon("panel-voice", 16));

  m_localizationPanel = new NMLocalizationPanel(this);
  m_localizationPanel->setObjectName("LocalizationPanel");
  m_localizationPanel->setWindowIcon(iconMgr.getIcon("panel-localization", 16));

  m_timelinePanel = new NMTimelinePanel(this);
  m_timelinePanel->setObjectName("TimelinePanel");
  m_timelinePanel->setWindowIcon(iconMgr.getIcon("panel-timeline", 16));

  m_curveEditorPanel = new NMCurveEditorPanel(this);
  m_curveEditorPanel->setObjectName("CurveEditorPanel");
  m_curveEditorPanel->setWindowIcon(iconMgr.getIcon("panel-curve", 16));

  m_buildSettingsPanel = new NMBuildSettingsPanel(this);
  m_buildSettingsPanel->setObjectName("BuildSettingsPanel");
  m_buildSettingsPanel->setWindowIcon(iconMgr.getIcon("panel-build", 16));

  m_scenePalettePanel = new NMScenePalettePanel(this);
  m_scenePalettePanel->setObjectName("ScenePalettePanel");
  m_scenePalettePanel->setWindowIcon(iconMgr.getIcon("panel-scene", 16));

  m_hierarchyPanel = new NMHierarchyPanel(this);
  m_hierarchyPanel->setObjectName("HierarchyPanel");
  m_hierarchyPanel->setWindowIcon(iconMgr.getIcon("panel-hierarchy", 16));

  m_scriptEditorPanel = new NMScriptEditorPanel(this);
  m_scriptEditorPanel->setObjectName("ScriptEditorPanel");
  m_scriptEditorPanel->setWindowIcon(iconMgr.getIcon("panel-console", 16));

  m_scriptDocPanel = new NMScriptDocPanel(this);
  m_scriptDocPanel->setObjectName("ScriptDocPanel");
  m_scriptDocPanel->setWindowIcon(iconMgr.getIcon("help", 16));

  m_issuesPanel = new NMIssuesPanel(this);
  m_issuesPanel->setObjectName("IssuesPanel");
  m_issuesPanel->setWindowIcon(iconMgr.getIcon("panel-diagnostics", 16));
  m_scriptEditorPanel->setIssuesPanel(m_issuesPanel);

  m_diagnosticsPanel = new NMDiagnosticsPanel(this);
  m_diagnosticsPanel->setObjectName("DiagnosticsPanel");
  m_diagnosticsPanel->setWindowIcon(iconMgr.getIcon("panel-diagnostics", 16));

  if (m_sceneViewPanel && m_hierarchyPanel) {
    m_hierarchyPanel->setScene(m_sceneViewPanel->graphicsScene());
    m_hierarchyPanel->setSceneViewPanel(m_sceneViewPanel);
  }

  // Phase 5 - Play-In-Editor panels
  m_playToolbarPanel = new NMPlayToolbarPanel(this);
  m_playToolbarPanel->setObjectName("PlayToolbarPanel");
  m_playToolbarPanel->setWindowIcon(iconMgr.getIcon("play", 16));

  m_debugOverlayPanel = new NMDebugOverlayPanel(this);
  m_debugOverlayPanel->setObjectName("DebugOverlayPanel");
  m_debugOverlayPanel->setWindowIcon(iconMgr.getIcon("panel-diagnostics", 16));

  // Project Settings panel (issue #125)
  m_projectSettingsPanel = new NMProjectSettingsPanel(this);
  m_projectSettingsPanel->setObjectName("ProjectSettingsPanel");
  m_projectSettingsPanel->setWindowIcon(iconMgr.getIcon("settings", 16));

  // Connect Project Settings <-> Play Toolbar bidirectional sync (issue #125)
  // When Project Settings changes workflow mode, update Play Toolbar
  connect(m_projectSettingsPanel, &NMProjectSettingsPanel::workflowModeChanged,
          m_playToolbarPanel, &NMPlayToolbarPanel::setSourceMode);
  // When Play Toolbar changes workflow mode, update Project Settings
  connect(m_playToolbarPanel, &NMPlayToolbarPanel::playbackSourceModeChanged,
          m_projectSettingsPanel, &NMProjectSettingsPanel::setWorkflowMode);

  // Add panels to the main window
  addDockWidget(Qt::LeftDockWidgetArea, m_scenePalettePanel);
  addDockWidget(Qt::LeftDockWidgetArea, m_hierarchyPanel);
  addDockWidget(Qt::RightDockWidgetArea, m_inspectorPanel);
  addDockWidget(Qt::RightDockWidgetArea, m_debugOverlayPanel); // Phase 5
  addDockWidget(Qt::RightDockWidgetArea, m_voiceManagerPanel);
  addDockWidget(Qt::RightDockWidgetArea, m_voiceStudioPanel);
  addDockWidget(Qt::RightDockWidgetArea, m_audioMixerPanel);
  addDockWidget(Qt::RightDockWidgetArea, m_localizationPanel);
  addDockWidget(Qt::BottomDockWidgetArea, m_timelinePanel);
  addDockWidget(Qt::BottomDockWidgetArea, m_curveEditorPanel);
  addDockWidget(Qt::BottomDockWidgetArea, m_buildSettingsPanel);
  addDockWidget(Qt::BottomDockWidgetArea, m_consolePanel);
  addDockWidget(Qt::BottomDockWidgetArea, m_assetBrowserPanel);
  addDockWidget(Qt::BottomDockWidgetArea, m_scriptEditorPanel);
  addDockWidget(Qt::BottomDockWidgetArea, m_issuesPanel);
  addDockWidget(Qt::BottomDockWidgetArea, m_diagnosticsPanel);
  addDockWidget(Qt::RightDockWidgetArea, m_scriptDocPanel);
  addDockWidget(Qt::RightDockWidgetArea, m_projectSettingsPanel); // Issue #125
  addDockWidget(Qt::TopDockWidgetArea, m_playToolbarPanel); // Phase 5

  // Central area: Scene View and Story Graph as tabs
  setCentralWidget(nullptr);
  addDockWidget(Qt::TopDockWidgetArea, m_sceneViewPanel);
  addDockWidget(Qt::TopDockWidgetArea, m_storyGraphPanel);
  addDockWidget(Qt::TopDockWidgetArea, m_sceneDialogueGraphPanel);
  tabifyDockWidget(m_sceneViewPanel, m_storyGraphPanel);
  tabifyDockWidget(m_storyGraphPanel, m_sceneDialogueGraphPanel);
  m_sceneViewPanel->raise(); // Make Scene View the active tab

  // Initially hide the dialogue graph panel (shown on demand)
  m_sceneDialogueGraphPanel->hide();

  // Tab the left panels
  tabifyDockWidget(m_scenePalettePanel, m_hierarchyPanel);
  m_scenePalettePanel->raise();

  // Tab the right panels
  tabifyDockWidget(m_inspectorPanel, m_debugOverlayPanel);
  tabifyDockWidget(m_inspectorPanel, m_voiceManagerPanel);
  tabifyDockWidget(m_inspectorPanel, m_voiceStudioPanel);
  tabifyDockWidget(m_inspectorPanel, m_audioMixerPanel);
  tabifyDockWidget(m_inspectorPanel, m_localizationPanel);
  tabifyDockWidget(m_inspectorPanel, m_scriptDocPanel);
  tabifyDockWidget(m_inspectorPanel, m_projectSettingsPanel); // Issue #125
  m_inspectorPanel->raise();

  // Tab the bottom panels
  tabifyDockWidget(m_consolePanel, m_assetBrowserPanel);
  tabifyDockWidget(m_consolePanel, m_scriptEditorPanel);
  tabifyDockWidget(m_consolePanel, m_issuesPanel);
  tabifyDockWidget(m_consolePanel, m_diagnosticsPanel);
  tabifyDockWidget(m_consolePanel, m_timelinePanel);
  tabifyDockWidget(m_consolePanel, m_curveEditorPanel);
  tabifyDockWidget(m_consolePanel, m_buildSettingsPanel);
  m_consolePanel->raise();
}

void NMMainWindow::configureDocking() {
  setDockNestingEnabled(true);
  setDockOptions(QMainWindow::AllowTabbedDocks | QMainWindow::AllowNestedDocks |
                 QMainWindow::GroupedDragging | QMainWindow::AnimatedDocks);
  setTabPosition(Qt::AllDockWidgetAreas, QTabWidget::North);

  const auto configureDock = [this](QDockWidget *dock) {
    if (!dock) {
      return;
    }
    dock->setAllowedAreas(Qt::AllDockWidgetAreas);
    const auto features = QDockWidget::DockWidgetClosable |
                          QDockWidget::DockWidgetMovable |
                          QDockWidget::DockWidgetFloatable;
    dock->setFeatures(features);
    dock->installEventFilter(this);
    addDockContextActions(dock);
  };

  configureDock(m_sceneViewPanel);
  configureDock(m_storyGraphPanel);
  configureDock(m_inspectorPanel);
  configureDock(m_consolePanel);
  configureDock(m_assetBrowserPanel);
  configureDock(m_voiceManagerPanel);
  configureDock(m_voiceStudioPanel);
  configureDock(m_audioMixerPanel);
  configureDock(m_localizationPanel);
  configureDock(m_timelinePanel);
  configureDock(m_curveEditorPanel);
  configureDock(m_buildSettingsPanel);
  configureDock(m_scenePalettePanel);
  configureDock(m_hierarchyPanel);
  configureDock(m_scriptEditorPanel);
  configureDock(m_scriptDocPanel);
  configureDock(m_playToolbarPanel);
  configureDock(m_debugOverlayPanel);
  configureDock(m_issuesPanel);
  configureDock(m_diagnosticsPanel);
  configureDock(m_sceneDialogueGraphPanel);
  configureDock(m_projectSettingsPanel); // Issue #125

  applyDockLockState(m_layoutLocked);
  applyTabbedDockMode(m_tabbedDockOnly);
}

void NMMainWindow::addDockContextActions(QDockWidget *dock) {
  if (!dock) {
    return;
  }
  dock->setContextMenuPolicy(Qt::ActionsContextMenu);

  auto addMove = [this, dock](const QString &label, Qt::DockWidgetArea area) {
    QAction *action = new QAction(label, dock);
    connect(action, &QAction::triggered, this, [this, dock, area]() {
      addDockWidget(area, dock);
      dock->show();
      dock->raise();
      m_lastFocusedDock = dock;
    });
    dock->addAction(action);
  };

  addMove(tr("Move to Left"), Qt::LeftDockWidgetArea);
  addMove(tr("Move to Right"), Qt::RightDockWidgetArea);
  addMove(tr("Move to Top"), Qt::TopDockWidgetArea);
  addMove(tr("Move to Bottom"), Qt::BottomDockWidgetArea);

  QAction *floatAction = new QAction(tr("Toggle Floating"), dock);
  connect(floatAction, &QAction::triggered, dock, [dock]() {
    dock->setFloating(!dock->isFloating());
    dock->raise();
  });
  dock->addAction(floatAction);
}

void NMMainWindow::togglePanel(NMDockPanel *panel) {
  if (panel) {
    panel->setVisible(!panel->isVisible());
  }
}

} // namespace NovelMind::editor::qt
