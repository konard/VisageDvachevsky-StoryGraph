#include "NovelMind/editor/project_manager.hpp"
#include "NovelMind/editor/qt/nm_command_palette.hpp"
#include "NovelMind/editor/qt/nm_dialogs.hpp"
#include "NovelMind/editor/qt/nm_main_window.hpp"
#include "NovelMind/editor/qt/nm_play_mode_controller.hpp"
#include "NovelMind/editor/qt/panels/nm_asset_browser_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_console_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_hierarchy_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_inspector_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_scene_view_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_script_doc_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_script_editor_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_story_graph_panel.hpp"

#include <QAction>
#include <QCloseEvent>
#include <QDateTime>
#include <QDialog>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QMenuBar>
#include <QShortcut>
#include <QStatusBar>
#include <QStyle>
#include <QVBoxLayout>
#include <chrono>

namespace NovelMind::editor::qt {

void NMMainWindow::setupShortcuts() {
  // Shortcuts are already set on the actions in setupMenuBar()
  // This method can be used for additional context-specific shortcuts

  auto *nextDockShortcut = new QShortcut(QKeySequence("Ctrl+Tab"), this);
  connect(nextDockShortcut, &QShortcut::activated, this,
          [this]() { focusNextDock(false); });

  auto *prevDockShortcut = new QShortcut(QKeySequence("Ctrl+Shift+Tab"), this);
  connect(prevDockShortcut, &QShortcut::activated, this,
          [this]() { focusNextDock(true); });

  // Ctrl+P: Quick panel switcher (panels only)
  auto *panelSwitcherShortcut = new QShortcut(QKeySequence("Ctrl+P"), this);
  connect(panelSwitcherShortcut, &QShortcut::activated, this, [this]() {
    showCommandPalette(true); // panels only
  });

  // Ctrl+Shift+P: Full command palette (all commands)
  auto *commandPaletteShortcut = new QShortcut(QKeySequence("Ctrl+Shift+P"), this);
  connect(commandPaletteShortcut, &QShortcut::activated, this, [this]() {
    showCommandPalette(false); // all commands
  });

  auto *focusShortcut = new QShortcut(QKeySequence("Ctrl+Shift+F"), this);
  connect(focusShortcut, &QShortcut::activated, this, [this]() {
    if (m_actionFocusMode) {
      m_actionFocusMode->toggle();
    }
  });
}

void NMMainWindow::updateStatusBarContext() {
  auto &pm = ProjectManager::instance();
  const bool hasProject = pm.hasOpenProject();
  if (m_activeProjectName.isEmpty() && hasProject) {
    m_activeProjectName = QString::fromStdString(pm.getProjectName());
  }

  auto &playController = NMPlayModeController::instance();
  QString playText = "Stopped";
  if (playController.isPlaying()) {
    playText = "Playing";
  } else if (playController.isPaused()) {
    playText = "Paused";
  }
  if (m_statusPlay) {
    m_statusPlay->setText(QString("Play: %1").arg(playText));
    m_statusPlay->setProperty(
        "mode", playController.isPlaying()
                    ? "playing"
                    : (playController.isPaused() ? "paused" : "stopped"));
    m_statusPlay->style()->unpolish(m_statusPlay);
    m_statusPlay->style()->polish(m_statusPlay);
  }

  const QString nodeText = m_activeNodeId.isEmpty() ? "-" : m_activeNodeId;
  const QString sceneText = m_activeSceneId.isEmpty() ? "-" : m_activeSceneId;
  if (m_statusNode) {
    m_statusNode->setText(
        QString("Node: %1  Scene: %2").arg(nodeText, sceneText));
  }

  const QString selectionText =
      m_activeSelectionLabel.isEmpty() ? "-" : m_activeSelectionLabel;
  if (m_statusSelection) {
    m_statusSelection->setText(QString("Selected: %1").arg(selectionText));
  }

  QString assetText = m_activeAssetPath;
  if (assetText.isEmpty()) {
    assetText = "-";
  } else if (pm.hasOpenProject() &&
             pm.isPathInProject(assetText.toStdString())) {
    assetText =
        QString::fromStdString(pm.toRelativePath(assetText.toStdString()));
  }
  if (m_statusAsset) {
    m_statusAsset->setText(QString("Asset: %1").arg(assetText));
  }

  if (m_statusUnsaved) {
    const bool dirty = pm.hasUnsavedChanges();
    m_statusUnsaved->setText(dirty ? "Unsaved" : "Saved");
    m_statusUnsaved->setProperty("status", dirty ? "dirty" : "clean");
    m_statusUnsaved->style()->unpolish(m_statusUnsaved);
    m_statusUnsaved->style()->polish(m_statusUnsaved);
  }

  if (m_statusFps) {
    if (m_lastFps > 0.0) {
      m_statusFps->setText(
          QString("FPS: %1").arg(QString::number(m_lastFps, 'f', 1)));
    } else {
      m_statusFps->setText("FPS: --");
    }
  }
}

void NMMainWindow::onUpdateTick() {
  // Calculate delta time
  static auto lastTime = std::chrono::steady_clock::now();
  auto currentTime = std::chrono::steady_clock::now();
  double deltaTime =
      std::chrono::duration<double>(currentTime - lastTime).count();
  lastTime = currentTime;

  // FPS sampling
  m_fpsFrameCount++;
  const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
  if (m_fpsLastSample == 0) {
    m_fpsLastSample = nowMs;
  }
  const qint64 elapsedMs = nowMs - m_fpsLastSample;
  if (elapsedMs >= 1000) {
    m_lastFps = (elapsedMs > 0) ? (static_cast<double>(m_fpsFrameCount) *
                                   1000.0 / static_cast<double>(elapsedMs))
                                : 0.0;
    m_fpsFrameCount = 0;
    m_fpsLastSample = nowMs;
  }

  // Update all panels
  if (m_sceneViewPanel)
    m_sceneViewPanel->onUpdate(deltaTime);
  if (m_storyGraphPanel)
    m_storyGraphPanel->onUpdate(deltaTime);
  if (m_inspectorPanel)
    m_inspectorPanel->onUpdate(deltaTime);
  if (m_consolePanel)
    m_consolePanel->onUpdate(deltaTime);
  if (m_assetBrowserPanel)
    m_assetBrowserPanel->onUpdate(deltaTime);
  if (m_hierarchyPanel)
    m_hierarchyPanel->onUpdate(deltaTime);
  if (m_scriptEditorPanel)
    m_scriptEditorPanel->onUpdate(deltaTime);
  if (m_scriptDocPanel)
    m_scriptDocPanel->onUpdate(deltaTime);

  updateStatusBarContext();
}

void NMMainWindow::showAboutDialog() {
  NMMessageDialog::showInfo(
      this, tr("About NovelMind Editor"),
      tr("<h3>NovelMind Editor</h3>"
         "<p>Version 0.3.0</p>"
         "<p>A modern visual novel editor built with Qt 6.</p>"
         "<p>Copyright (c) 2024 NovelMind Contributors</p>"
         "<p>Licensed under MIT License</p>"));
}

void NMMainWindow::setStatusMessage(const QString &message, int timeout) {
  if (m_statusLabel) {
    m_statusLabel->setText(message);
  }
  if (timeout > 0) {
    statusBar()->showMessage(message, timeout);
  }
}

void NMMainWindow::updateWindowTitle(const QString &projectName) {
  if (projectName.isEmpty()) {
    setWindowTitle("NovelMind Editor");
    m_activeProjectName.clear();
  } else {
    setWindowTitle(QString("NovelMind Editor - %1").arg(projectName));
    m_activeProjectName = projectName;
  }
  updateStatusBarContext();
}

void NMMainWindow::showCommandPalette(bool panelsOnly) {
  QList<QAction *> actions;

  // Collect menu actions
  for (auto *menu : menuBar()->findChildren<QMenu *>()) {
    for (auto *action : menu->actions()) {
      if (action && !action->isSeparator()) {
        actions << action;
      }
    }
  }

  // Add panel toggle actions explicitly
  actions << m_actionToggleSceneView << m_actionToggleStoryGraph
          << m_actionToggleInspector << m_actionToggleConsole
          << m_actionToggleIssues << m_actionToggleDiagnostics
          << m_actionToggleVoiceManager << m_actionToggleLocalization
          << m_actionToggleTimeline << m_actionToggleCurveEditor
          << m_actionToggleBuildSettings << m_actionToggleAssetBrowser
          << m_actionToggleScenePalette << m_actionToggleHierarchy
          << m_actionToggleScriptEditor << m_actionToggleScriptDocs
          << m_actionToggleDebugOverlay << m_actionToggleVoiceStudio
          << m_actionToggleAudioMixer << m_actionToggleScriptRuntimeInspector;

  // Add layout/workspace actions
  actions << m_actionLayoutStory << m_actionLayoutScene << m_actionLayoutScript
          << m_actionLayoutDeveloper << m_actionLayoutCompact
          << m_actionLayoutDefault << m_actionLayoutStoryScript
          << m_actionLayoutSceneAnimation << m_actionLayoutAudioVoice
          << m_actionFocusMode << m_actionLockLayout << m_actionUiScaleDown
          << m_actionUiScaleUp << m_actionUiScaleReset;

  auto mode = panelsOnly ? NMCommandPalette::Mode::Panels : NMCommandPalette::Mode::All;
  auto *palette = new NMCommandPalette(this, actions, mode);
  palette->openCentered(this);
}

void NMMainWindow::closeEvent(QCloseEvent *event) {
  auto &projectManager = ProjectManager::instance();
  if (projectManager.hasOpenProject() && projectManager.hasUnsavedChanges()) {
    const auto choice = NMMessageDialog::showQuestion(
        this, tr("Unsaved Changes"),
        tr("You have unsaved project changes. Save before closing?"),
        {NMDialogButton::Save, NMDialogButton::Discard, NMDialogButton::Cancel},
        NMDialogButton::Save);
    if (choice == NMDialogButton::Cancel || choice == NMDialogButton::None) {
      event->ignore();
      return;
    }
    if (choice == NMDialogButton::Save) {
      auto result = projectManager.saveProject();
      if (result.isError()) {
        NMMessageDialog::showError(this, tr("Save Failed"),
                                   QString::fromStdString(result.error()));
        event->ignore();
        return;
      }
    }
  }

  saveLayout();
  event->accept();
}

} // namespace NovelMind::editor::qt
