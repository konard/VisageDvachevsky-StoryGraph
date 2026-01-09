#include "NovelMind/editor/qt/nm_icon_manager.hpp"
#include "NovelMind/editor/qt/nm_main_window.hpp"

#include <QAction>
#include <QActionGroup>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QToolBar>

namespace NovelMind::editor::qt {

void NMMainWindow::setupMenuBar() {
  QMenuBar *menuBar = this->menuBar();
  auto &iconMgr = NMIconManager::instance();

  // =========================================================================
  // File Menu
  // =========================================================================
  QMenu *fileMenu = menuBar->addMenu(tr("&File"));

  m_actionNewProject = fileMenu->addAction(iconMgr.getIcon("file-new", 16),
                                           tr("&New Project..."));
  m_actionNewProject->setShortcut(
      QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
  m_actionNewProject->setToolTip(tr("Create a new NovelMind project"));

  m_actionOpenProject = fileMenu->addAction(iconMgr.getIcon("file-open", 16),
                                            tr("&Open Project..."));
  m_actionOpenProject->setShortcut(QKeySequence::Open);
  m_actionOpenProject->setToolTip(tr("Open an existing project"));

  fileMenu->addSeparator();

  m_actionSaveProject = fileMenu->addAction(iconMgr.getIcon("file-save", 16),
                                            tr("&Save Project"));
  m_actionSaveProject->setShortcut(QKeySequence::Save);
  m_actionSaveProject->setToolTip(tr("Save the current project"));

  m_actionSaveProjectAs = fileMenu->addAction(iconMgr.getIcon("file-save", 16),
                                              tr("Save Project &As..."));
  m_actionSaveProjectAs->setShortcut(QKeySequence::SaveAs);
  m_actionSaveProjectAs->setToolTip(tr("Save the project with a new name"));

  fileMenu->addSeparator();

  m_actionCloseProject = fileMenu->addAction(iconMgr.getIcon("file-close", 16),
                                             tr("&Close Project"));
  m_actionCloseProject->setToolTip(tr("Close the current project"));

  fileMenu->addSeparator();

  m_actionExit = fileMenu->addAction(tr("E&xit"));
  m_actionExit->setShortcut(QKeySequence::Quit);
  m_actionExit->setToolTip(tr("Exit the editor"));

  // =========================================================================
  // Project Menu
  // =========================================================================
  QMenu *projectMenu = menuBar->addMenu(tr("&Project"));

  m_actionValidateProject = projectMenu->addAction(
      iconMgr.getIcon("panel-diagnostics", 16), tr("&Validate Project"));
  m_actionValidateProject->setShortcut(
      QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V));
  m_actionValidateProject->setToolTip(
      tr("Run full project integrity validation"));

  // =========================================================================
  // Edit Menu
  // =========================================================================
  QMenu *editMenu = menuBar->addMenu(tr("&Edit"));

  m_actionUndo =
      editMenu->addAction(iconMgr.getIcon("edit-undo", 16), tr("&Undo"));
  m_actionUndo->setShortcut(QKeySequence::Undo);
  m_actionUndo->setToolTip(tr("Undo the last action"));

  m_actionRedo =
      editMenu->addAction(iconMgr.getIcon("edit-redo", 16), tr("&Redo"));
  m_actionRedo->setShortcut(QKeySequence::Redo);
  m_actionRedo->setToolTip(tr("Redo the previously undone action"));

  editMenu->addSeparator();

  m_actionCut =
      editMenu->addAction(iconMgr.getIcon("edit-cut", 16), tr("Cu&t"));
  m_actionCut->setShortcut(QKeySequence::Cut);
  m_actionCut->setToolTip(tr("Cut selected items"));

  m_actionCopy =
      editMenu->addAction(iconMgr.getIcon("edit-copy", 16), tr("&Copy"));
  m_actionCopy->setShortcut(QKeySequence::Copy);
  m_actionCopy->setToolTip(tr("Copy selected items"));

  m_actionPaste =
      editMenu->addAction(iconMgr.getIcon("edit-paste", 16), tr("&Paste"));
  m_actionPaste->setShortcut(QKeySequence::Paste);
  m_actionPaste->setToolTip(tr("Paste from clipboard"));

  m_actionDelete =
      editMenu->addAction(iconMgr.getIcon("edit-delete", 16), tr("&Delete"));
  m_actionDelete->setShortcut(QKeySequence::Delete);
  m_actionDelete->setToolTip(tr("Delete selected items"));

  editMenu->addSeparator();

  m_actionSelectAll = editMenu->addAction(tr("Select &All"));
  m_actionSelectAll->setShortcut(QKeySequence::SelectAll);
  m_actionSelectAll->setToolTip(tr("Select all items"));

  editMenu->addSeparator();

  m_actionPreferences = editMenu->addAction(iconMgr.getIcon("settings", 16),
                                            tr("&Preferences..."));
  m_actionPreferences->setToolTip(tr("Open editor preferences"));

  // =========================================================================
  // View Menu
  // =========================================================================
  QMenu *viewMenu = menuBar->addMenu(tr("&View"));

  QMenu *panelsMenu = viewMenu->addMenu(tr("&Panels"));

  m_actionToggleSceneView = panelsMenu->addAction(
      iconMgr.getIcon("panel-scene", 16), tr("&Scene View"));
  m_actionToggleSceneView->setCheckable(true);
  m_actionToggleSceneView->setChecked(true);
  m_actionToggleSceneView->setToolTip(tr("Toggle Scene View panel"));

  m_actionToggleStoryGraph = panelsMenu->addAction(
      iconMgr.getIcon("panel-graph", 16), tr("Story &Graph"));
  m_actionToggleStoryGraph->setCheckable(true);
  m_actionToggleStoryGraph->setChecked(true);
  m_actionToggleStoryGraph->setToolTip(tr("Toggle Story Graph panel"));

  m_actionToggleScriptEditor = panelsMenu->addAction(
      iconMgr.getIcon("panel-console", 16), tr("Script &Editor"));
  m_actionToggleScriptEditor->setCheckable(true);
  m_actionToggleScriptEditor->setChecked(true);
  m_actionToggleScriptEditor->setToolTip(tr("Toggle Script Editor panel"));

  m_actionToggleScriptDocs =
      panelsMenu->addAction(iconMgr.getIcon("help", 16), tr("Script &Docs"));
  m_actionToggleScriptDocs->setCheckable(true);
  m_actionToggleScriptDocs->setChecked(true);
  m_actionToggleScriptDocs->setToolTip(tr("Toggle Script Docs panel"));

  m_actionToggleInspector = panelsMenu->addAction(
      iconMgr.getIcon("panel-inspector", 16), tr("&Inspector"));
  m_actionToggleInspector->setCheckable(true);
  m_actionToggleInspector->setChecked(true);
  m_actionToggleInspector->setToolTip(tr("Toggle Inspector panel"));

  m_actionToggleConsole = panelsMenu->addAction(
      iconMgr.getIcon("panel-console", 16), tr("&Console"));
  m_actionToggleConsole->setCheckable(true);
  m_actionToggleConsole->setChecked(true);
  m_actionToggleConsole->setToolTip(tr("Toggle Console panel"));

  m_actionToggleIssues = panelsMenu->addAction(
      iconMgr.getIcon("panel-diagnostics", 16), tr("&Issues"));
  m_actionToggleIssues->setCheckable(true);
  m_actionToggleIssues->setChecked(true);
  m_actionToggleIssues->setToolTip(tr("Toggle Issues panel"));

  m_actionToggleDiagnostics = panelsMenu->addAction(
      iconMgr.getIcon("panel-diagnostics", 16), tr("&Diagnostics"));
  m_actionToggleDiagnostics->setCheckable(true);
  m_actionToggleDiagnostics->setChecked(true);
  m_actionToggleDiagnostics->setToolTip(tr("Toggle Diagnostics panel"));

  // D6: Audio/Voice panels with improved discoverability
  m_actionToggleVoiceStudio = panelsMenu->addAction(
      iconMgr.getIcon("panel-voice", 16), tr("Voice &Studio"));
  m_actionToggleVoiceStudio->setCheckable(true);
  m_actionToggleVoiceStudio->setChecked(false);
  m_actionToggleVoiceStudio->setToolTip(
      tr("Voice Studio - Record and edit voice lines with waveform "
         "visualization (Ctrl+Shift+V)"));
  m_actionToggleVoiceStudio->setShortcut(QKeySequence("Ctrl+Shift+V"));

  m_actionToggleVoiceManager = panelsMenu->addAction(
      iconMgr.getIcon("panel-voice", 16), tr("Voice &Manager"));
  m_actionToggleVoiceManager->setCheckable(true);
  m_actionToggleVoiceManager->setChecked(true);
  m_actionToggleVoiceManager->setToolTip(
      tr("Voice Manager - Manage voice files and dialogue line mappings"));

  m_actionToggleAudioMixer = panelsMenu->addAction(
      iconMgr.getIcon("panel-voice", 16), tr("&Audio Mixer"));
  m_actionToggleAudioMixer->setCheckable(true);
  m_actionToggleAudioMixer->setChecked(true);
  m_actionToggleAudioMixer->setToolTip(
      tr("Audio Mixer - Control audio levels and mixing"));

  m_actionToggleLocalization = panelsMenu->addAction(
      iconMgr.getIcon("panel-localization", 16), tr("&Localization"));
  m_actionToggleLocalization->setCheckable(true);
  m_actionToggleLocalization->setChecked(true);
  m_actionToggleLocalization->setToolTip(tr("Toggle Localization panel"));

  m_actionToggleTimeline = panelsMenu->addAction(
      iconMgr.getIcon("panel-timeline", 16), tr("&Timeline"));
  m_actionToggleTimeline->setCheckable(true);
  m_actionToggleTimeline->setChecked(true);
  m_actionToggleTimeline->setToolTip(tr("Toggle Timeline panel"));

  m_actionToggleCurveEditor = panelsMenu->addAction(
      iconMgr.getIcon("panel-curve", 16), tr("&Curve Editor"));
  m_actionToggleCurveEditor->setCheckable(true);
  m_actionToggleCurveEditor->setChecked(true);
  m_actionToggleCurveEditor->setToolTip(tr("Toggle Curve Editor panel"));

  m_actionToggleBuildSettings = panelsMenu->addAction(
      iconMgr.getIcon("panel-build", 16), tr("&Build Settings"));
  m_actionToggleBuildSettings->setCheckable(true);
  m_actionToggleBuildSettings->setChecked(true);
  m_actionToggleBuildSettings->setToolTip(tr("Toggle Build Settings panel"));

  m_actionToggleAssetBrowser = panelsMenu->addAction(
      iconMgr.getIcon("panel-assets", 16), tr("&Asset Browser"));
  m_actionToggleAssetBrowser->setCheckable(true);
  m_actionToggleAssetBrowser->setChecked(true);
  m_actionToggleAssetBrowser->setToolTip(tr("Toggle Asset Browser panel"));

  m_actionToggleScenePalette = panelsMenu->addAction(
      iconMgr.getIcon("panel-scene", 16), tr("Scene &Palette"));
  m_actionToggleScenePalette->setCheckable(true);
  m_actionToggleScenePalette->setChecked(true);
  m_actionToggleScenePalette->setToolTip(tr("Toggle Scene Palette panel"));

  m_actionToggleHierarchy = panelsMenu->addAction(
      iconMgr.getIcon("panel-hierarchy", 16), tr("&Hierarchy"));
  m_actionToggleHierarchy->setCheckable(true);
  m_actionToggleHierarchy->setChecked(true);
  m_actionToggleHierarchy->setToolTip(tr("Toggle Hierarchy panel"));

  m_actionToggleDebugOverlay = panelsMenu->addAction(
      iconMgr.getIcon("panel-diagnostics", 16), tr("&Debug Overlay"));
  m_actionToggleDebugOverlay->setCheckable(true);
  m_actionToggleDebugOverlay->setChecked(true);
  m_actionToggleDebugOverlay->setToolTip(tr("Toggle Debug Overlay panel"));

  viewMenu->addSeparator();

  m_actionFocusMode = viewMenu->addAction(iconMgr.getIcon("panel-scene", 16),
                                          tr("&Focus Mode"));
  m_actionFocusMode->setCheckable(true);
  m_actionFocusMode->setToolTip(
      tr("Focus Scene View with Inspector and Assets"));
  m_actionFocusMode->setShortcut(QKeySequence("F9"));

  m_actionFocusIncludeHierarchy =
      viewMenu->addAction(tr("Focus Mode: Include &Hierarchy"));
  m_actionFocusIncludeHierarchy->setCheckable(true);
  m_actionFocusIncludeHierarchy->setChecked(true);

  // =========================================================================
  // D2: Workspace Presets Menu
  // =========================================================================
  QMenu *workspaceMenu = viewMenu->addMenu(tr("&Workspaces"));

  // D2: New primary workspace presets
  m_actionLayoutDefault = workspaceMenu->addAction(
      iconMgr.getIcon("panel-scene", 16), tr("&Default"));
  m_actionLayoutDefault->setToolTip(tr(
      "Balanced layout for general editing (Scene, Story, Inspector, Assets)"));
  m_actionLayoutDefault->setShortcut(QKeySequence("Ctrl+Alt+1"));

  m_actionLayoutStoryScript = workspaceMenu->addAction(
      iconMgr.getIcon("panel-graph", 16), tr("Story / &Script"));
  m_actionLayoutStoryScript->setToolTip(
      tr("Story graph and script editing (Story Graph, Script Editor, "
         "Inspector, Localization)"));
  m_actionLayoutStoryScript->setShortcut(QKeySequence("Ctrl+Alt+2"));

  m_actionLayoutSceneAnimation = workspaceMenu->addAction(
      iconMgr.getIcon("panel-timeline", 16), tr("Scene / &Animation"));
  m_actionLayoutSceneAnimation->setToolTip(
      tr("Scene and animation editing (Scene View, Timeline, Curve Editor, "
         "Hierarchy)"));
  m_actionLayoutSceneAnimation->setShortcut(QKeySequence("Ctrl+Alt+3"));

  m_actionLayoutAudioVoice = workspaceMenu->addAction(
      iconMgr.getIcon("panel-voice", 16), tr("&Audio / Voice"));
  m_actionLayoutAudioVoice->setToolTip(tr(
      "Audio and voice management (Voice Studio, Voice Manager, Audio Mixer)"));
  m_actionLayoutAudioVoice->setShortcut(QKeySequence("Ctrl+Alt+4"));

  workspaceMenu->addSeparator();
  workspaceMenu->addAction(tr("Legacy Workspaces"))->setEnabled(false);

  // Legacy workspace presets
  m_actionLayoutStory = workspaceMenu->addAction(
      iconMgr.getIcon("panel-graph", 16), tr("Stor&y (Legacy)"));
  m_actionLayoutStory->setToolTip(tr("Story Graph + Inspector + Play + Log"));
  m_actionLayoutStory->setShortcut(QKeySequence("Ctrl+1"));

  m_actionLayoutScene = workspaceMenu->addAction(
      iconMgr.getIcon("panel-scene", 16), tr("S&cene (Legacy)"));
  m_actionLayoutScene->setToolTip(
      tr("Scene View + Assets + Inspector + Hierarchy"));
  m_actionLayoutScene->setShortcut(QKeySequence("Ctrl+2"));

  m_actionLayoutScript = workspaceMenu->addAction(
      iconMgr.getIcon("panel-console", 16), tr("Scr&ipt (Legacy)"));
  m_actionLayoutScript->setToolTip(tr("Script Editor + Story Graph + Play"));
  m_actionLayoutScript->setShortcut(QKeySequence("Ctrl+3"));

  m_actionLayoutDeveloper = workspaceMenu->addAction(
      iconMgr.getIcon("panel-diagnostics", 16), tr("&Developer (Legacy)"));
  m_actionLayoutDeveloper->setToolTip(
      tr("Scene + Script + Console + Issues + Diagnostics + Debug"));
  m_actionLayoutDeveloper->setShortcut(QKeySequence("Ctrl+4"));

  m_actionLayoutCompact = workspaceMenu->addAction(
      iconMgr.getIcon("panel-asset", 16), tr("&Compact (Legacy)"));
  m_actionLayoutCompact->setToolTip(
      tr("Compact layout with more panels visible at once"));
  m_actionLayoutCompact->setShortcut(QKeySequence("Ctrl+5"));

  QMenu *layoutMenu = viewMenu->addMenu(tr("&Layouts"));
  m_actionSaveLayout = layoutMenu->addAction(iconMgr.getIcon("file-save", 16),
                                             tr("&Save Layout"));
  m_actionSaveLayout->setToolTip(tr("Save current layout"));
  m_actionLoadLayout = layoutMenu->addAction(iconMgr.getIcon("file-open", 16),
                                             tr("&Load Layout"));
  m_actionLoadLayout->setToolTip(tr("Load saved layout"));
  layoutMenu->addSeparator();
  m_actionResetLayout = layoutMenu->addAction(iconMgr.getIcon("refresh", 16),
                                              tr("&Reset Layout"));
  m_actionResetLayout->setToolTip(tr("Reset all panels to workspace defaults"));

  layoutMenu->addSeparator();
  m_actionLockLayout =
      layoutMenu->addAction(iconMgr.getIcon("locked", 16), tr("&Lock Layout"));
  m_actionLockLayout->setCheckable(true);
  m_actionLockLayout->setShortcut(QKeySequence("Ctrl+Shift+L"));
  m_actionLockLayout->setToolTip(tr("Prevent moving or floating panels"));

  m_actionTabbedDockOnly = layoutMenu->addAction(tr("Tabbed Dock Only"));
  m_actionTabbedDockOnly->setCheckable(true);
  m_actionTabbedDockOnly->setShortcut(QKeySequence("Ctrl+Shift+T"));
  m_actionTabbedDockOnly->setToolTip(tr("Keep panels in tabbed docks"));

  m_actionFloatAllowed = layoutMenu->addAction(tr("Float Allowed"));
  m_actionFloatAllowed->setCheckable(true);
  m_actionFloatAllowed->setChecked(true);
  m_actionFloatAllowed->setToolTip(tr("Allow panels to float"));

  QMenu *scaleMenu = viewMenu->addMenu(tr("UI &Scale"));
  QActionGroup *scaleGroup = new QActionGroup(scaleMenu);
  scaleGroup->setExclusive(true);

  m_actionUiScaleCompact = scaleMenu->addAction(tr("90% (Compact)"));
  m_actionUiScaleCompact->setCheckable(true);
  m_actionUiScaleCompact->setToolTip(tr("Set UI scale to 90%"));
  m_actionUiScaleDefault = scaleMenu->addAction(tr("100% (Default)"));
  m_actionUiScaleDefault->setCheckable(true);
  m_actionUiScaleDefault->setChecked(true);
  m_actionUiScaleDefault->setToolTip(tr("Set UI scale to 100%"));
  m_actionUiScaleComfort = scaleMenu->addAction(tr("110% (Comfort)"));
  m_actionUiScaleComfort->setCheckable(true);
  m_actionUiScaleComfort->setToolTip(tr("Set UI scale to 110%"));

  scaleGroup->addAction(m_actionUiScaleCompact);
  scaleGroup->addAction(m_actionUiScaleDefault);
  scaleGroup->addAction(m_actionUiScaleComfort);

  scaleMenu->addSeparator();
  m_actionUiScaleDown = scaleMenu->addAction(tr("Scale Down"));
  m_actionUiScaleDown->setShortcut(QKeySequence("Ctrl+Alt+-"));
  m_actionUiScaleDown->setToolTip(tr("Reduce UI scale by 10%"));
  m_actionUiScaleUp = scaleMenu->addAction(tr("Scale Up"));
  m_actionUiScaleUp->setShortcut(QKeySequence("Ctrl+Alt+="));
  m_actionUiScaleUp->setToolTip(tr("Increase UI scale by 10%"));
  m_actionUiScaleReset = scaleMenu->addAction(tr("Scale Reset"));
  m_actionUiScaleReset->setShortcut(QKeySequence("Ctrl+Alt+0"));
  m_actionUiScaleReset->setToolTip(tr("Reset UI scale to 100%"));

  viewMenu->addSeparator();

  // =========================================================================
  // Theme Menu
  // =========================================================================
  QMenu *themeMenu = viewMenu->addMenu(tr("&Theme"));
  QActionGroup *themeGroup = new QActionGroup(themeMenu);
  themeGroup->setExclusive(true);

  m_actionThemeDark = themeMenu->addAction(tr("&Dark Theme"));
  m_actionThemeDark->setCheckable(true);
  m_actionThemeDark->setChecked(true);
  m_actionThemeDark->setToolTip(tr("Use dark color theme (default)"));

  m_actionThemeLight = themeMenu->addAction(tr("&Light Theme"));
  m_actionThemeLight->setCheckable(true);
  m_actionThemeLight->setToolTip(tr("Use light color theme"));

  themeGroup->addAction(m_actionThemeDark);
  themeGroup->addAction(m_actionThemeLight);

  // =========================================================================
  // Play Menu
  // =========================================================================
  QMenu *playMenu = menuBar->addMenu(tr("&Play"));

  m_actionPlay = playMenu->addAction(iconMgr.getIcon("play", 16), tr("&Play"));
  m_actionPlay->setShortcut(Qt::Key_F5);
  m_actionPlay->setToolTip(tr("Start playback (F5)"));

  m_actionPause =
      playMenu->addAction(iconMgr.getIcon("pause", 16), tr("Pa&use"));
  m_actionPause->setShortcut(Qt::Key_F6);
  m_actionPause->setEnabled(false);
  m_actionPause->setToolTip(tr("Pause playback (F6)"));

  m_actionStop = playMenu->addAction(iconMgr.getIcon("stop", 16), tr("&Stop"));
  m_actionStop->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F5));
  m_actionStop->setEnabled(false);
  m_actionStop->setToolTip(tr("Stop playback (Shift+F5)"));

  playMenu->addSeparator();

  m_actionStepFrame = playMenu->addAction(iconMgr.getIcon("step-forward", 16),
                                          tr("Step &Frame"));
  m_actionStepFrame->setShortcut(Qt::Key_F10);
  m_actionStepFrame->setEnabled(false);
  m_actionStepFrame->setToolTip(tr("Step one frame forward (F10)"));

  playMenu->addSeparator();

  m_actionSaveState =
      playMenu->addAction(iconMgr.getIcon("file-save", 16), tr("&Save State"));
  m_actionSaveState->setToolTip(tr("Save runtime state to slot 0"));
  m_actionSaveState->setShortcut(
      QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F5));

  m_actionLoadState =
      playMenu->addAction(iconMgr.getIcon("file-open", 16), tr("&Load State"));
  m_actionLoadState->setToolTip(tr("Load runtime state from slot 0"));
  m_actionLoadState->setShortcut(
      QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F6));

  playMenu->addSeparator();

  m_actionAutoSaveState =
      playMenu->addAction(iconMgr.getIcon("file-save", 16), tr("Auto &Save"));
  m_actionAutoSaveState->setToolTip(tr("Save runtime state to auto-save"));

  m_actionAutoLoadState =
      playMenu->addAction(iconMgr.getIcon("file-open", 16), tr("Auto &Load"));
  m_actionAutoLoadState->setToolTip(tr("Load runtime state from auto-save"));

  // =========================================================================
  // Help Menu
  // =========================================================================
  QMenu *helpMenu = menuBar->addMenu(tr("&Help"));

  m_actionDocumentation =
      helpMenu->addAction(iconMgr.getIcon("help", 16), tr("&Documentation"));
  m_actionDocumentation->setShortcut(Qt::Key_F1);
  m_actionDocumentation->setToolTip(tr("Open documentation (F1)"));

  m_actionHotkeys =
      helpMenu->addAction(iconMgr.getIcon("help", 16), tr("&Hotkeys && Tips"));
  m_actionHotkeys->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_K));
  m_actionHotkeys->setToolTip(tr("Show hotkeys and tips (Ctrl+Shift+K)"));

  helpMenu->addSeparator();

  m_actionAbout = helpMenu->addAction(iconMgr.getIcon("info", 16),
                                      tr("&About NovelMind Editor..."));
  m_actionAbout->setToolTip(tr("About NovelMind Editor"));
}

void NMMainWindow::setupToolBar() {
  m_mainToolBar = addToolBar(tr("Main Toolbar"));
  m_mainToolBar->setObjectName("MainToolBar");
  m_mainToolBar->setMovable(false);

  // File operations
  m_mainToolBar->addAction(m_actionNewProject);
  m_mainToolBar->addAction(m_actionOpenProject);
  m_mainToolBar->addAction(m_actionSaveProject);

  m_mainToolBar->addSeparator();

  // Edit operations
  m_mainToolBar->addAction(m_actionUndo);
  m_mainToolBar->addAction(m_actionRedo);

  m_mainToolBar->addSeparator();

  // Play controls
  m_mainToolBar->addAction(m_actionPlay);
  m_mainToolBar->addAction(m_actionPause);
  m_mainToolBar->addAction(m_actionStop);
}

void NMMainWindow::setupStatusBar() {
  QStatusBar *status = statusBar();

  m_statusLabel = new QLabel(tr("Ready"));
  m_statusLabel->setObjectName("StatusMessage");
  status->addWidget(m_statusLabel, 1);

  m_statusPlay = new QLabel(tr("Play: Stopped"));
  m_statusPlay->setObjectName("StatusPlay");
  status->addPermanentWidget(m_statusPlay);

  m_statusNode = new QLabel(tr("Node: -"));
  m_statusNode->setObjectName("StatusNode");
  status->addPermanentWidget(m_statusNode);

  m_statusSelection = new QLabel(tr("Selected: -"));
  m_statusSelection->setObjectName("StatusSelection");
  status->addPermanentWidget(m_statusSelection);

  m_statusAsset = new QLabel(tr("Asset: -"));
  m_statusAsset->setObjectName("StatusAsset");
  status->addPermanentWidget(m_statusAsset, 1);

  m_statusUnsaved = new QLabel(tr("Saved"));
  m_statusUnsaved->setObjectName("StatusUnsaved");
  status->addPermanentWidget(m_statusUnsaved);

  m_statusFps = new QLabel(tr("FPS: --"));
  m_statusFps->setObjectName("StatusFps");
  status->addPermanentWidget(m_statusFps);

  m_statusCache = new QLabel(tr("Cache: --"));
  m_statusCache->setObjectName("StatusCache");
  status->addPermanentWidget(m_statusCache);
}

} // namespace NovelMind::editor::qt
