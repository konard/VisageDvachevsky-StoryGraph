#include "NovelMind/editor/qt/nm_main_window.hpp"
#include "NovelMind/editor/project_integrity.hpp"
#include "NovelMind/editor/project_manager.hpp"
#include "NovelMind/editor/qt/nm_dialogs.hpp"
#include "NovelMind/editor/qt/nm_hotkeys_dialog.hpp"
#include "NovelMind/editor/qt/nm_play_mode_controller.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"
#include "NovelMind/editor/qt/nm_undo_manager.hpp"
#include "NovelMind/editor/qt/panels/nm_asset_browser_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_build_settings_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_console_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_curve_editor_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_debug_overlay_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_diagnostics_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_hierarchy_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_inspector_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_issues_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_localization_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_scene_palette_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_scene_view_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_script_doc_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_script_editor_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_story_graph_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_timeline_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_voice_manager_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_voice_studio_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_audio_mixer_panel.hpp"

#include <QAction>
#include <QDateTime>
#include <QDesktopServices>
#include <QDockWidget>
#include <QFileInfo>
#include <QKeySequence>
#include <QSignalBlocker>
#include <QUrl>
#include <cmath>

namespace NovelMind::editor::qt {

void NMMainWindow::setupConnections() {
  // File menu
  connect(m_actionNewProject, &QAction::triggered, this,
          &NMMainWindow::newProjectRequested);
  connect(m_actionOpenProject, &QAction::triggered, this,
          &NMMainWindow::openProjectRequested);
  connect(m_actionSaveProject, &QAction::triggered, this,
          &NMMainWindow::saveProjectRequested);
  connect(m_actionExit, &QAction::triggered, this, &QMainWindow::close);

  // Project menu
  connect(m_actionValidateProject, &QAction::triggered, this, [this]() {
    auto &pm = ProjectManager::instance();
    if (!pm.hasOpenProject()) {
      setStatusMessage(tr("No project is open"), 3000);
      return;
    }

    setStatusMessage(tr("Running project validation..."));
    m_diagnosticsPanel->clearDiagnostics();

    // Run validation using ProjectIntegrityChecker
    ProjectIntegrityChecker checker;
    checker.setProjectPath(pm.getProjectPath());

    // Configure for full validation
    IntegrityCheckConfig config;
    config.checkScenes = true;
    config.checkAssets = true;
    config.checkVoiceLines = true;
    config.checkLocalization = true;
    config.checkStoryGraph = true;
    config.checkScripts = true;
    config.checkResources = true;
    config.checkConfiguration = true;
    config.reportUnreferencedAssets = true;
    config.reportUnreachableNodes = true;
    config.reportCycles = true;
    config.reportMissingTranslations = true;
    checker.setConfig(config);

    IntegrityReport report = checker.runFullCheck();

    // Display results in Diagnostics panel
    for (const auto &issue : report.issues) {
      QString type;
      switch (issue.severity) {
      case IssueSeverity::Critical:
      case IssueSeverity::Error:
        type = "Error";
        break;
      case IssueSeverity::Warning:
        type = "Warning";
        break;
      case IssueSeverity::Info:
        type = "Info";
        break;
      }

      QString message = QString::fromStdString(issue.message);
      if (!issue.context.empty()) {
        message += " - " + QString::fromStdString(issue.context);
      }

      // Build location string for navigation
      QString location;
      if (issue.category == IssueCategory::Script ||
          issue.category == IssueCategory::Scene) {
        location = "Script:" + QString::fromStdString(issue.filePath);
        if (issue.lineNumber > 0) {
          location += ":" + QString::number(issue.lineNumber);
        }
      } else if (issue.category == IssueCategory::Asset) {
        location = "Asset:" + QString::fromStdString(issue.filePath);
      } else if (!issue.filePath.empty()) {
        location = "File:" + QString::fromStdString(issue.filePath);
      }

      m_diagnosticsPanel->addDiagnosticWithLocation(type, message, location);
    }

    // Show diagnostics panel
    m_diagnosticsPanel->show();
    m_diagnosticsPanel->raise();

    // Update status
    if (report.passed) {
      setStatusMessage(tr("Validation passed - no critical issues found"),
                       5000);
    } else {
      setStatusMessage(
          tr("Validation found %1 error(s) and %2 warning(s)")
              .arg(report.summary.errorCount + report.summary.criticalCount)
              .arg(report.summary.warningCount),
          5000);
    }
  });

  // Edit menu - connect to undo manager
  connect(m_actionUndo, &QAction::triggered, &NMUndoManager::instance(),
          &NMUndoManager::undo);
  connect(m_actionRedo, &QAction::triggered, &NMUndoManager::instance(),
          &NMUndoManager::redo);

  // Update undo/redo action states based on undo manager
  connect(&NMUndoManager::instance(), &NMUndoManager::canUndoChanged,
          m_actionUndo, &QAction::setEnabled);
  connect(&NMUndoManager::instance(), &NMUndoManager::canRedoChanged,
          m_actionRedo, &QAction::setEnabled);
  connect(&NMUndoManager::instance(), &NMUndoManager::undoTextChanged,
          [this](const QString &text) {
            m_actionUndo->setText(text.isEmpty() ? tr("&Undo")
                                                 : tr("&Undo %1").arg(text));
          });
  connect(&NMUndoManager::instance(), &NMUndoManager::redoTextChanged,
          [this](const QString &text) {
            m_actionRedo->setText(text.isEmpty() ? tr("&Redo")
                                                 : tr("&Redo %1").arg(text));
          });

  // Initialize undo/redo states
  m_actionUndo->setEnabled(NMUndoManager::instance().canUndo());
  m_actionRedo->setEnabled(NMUndoManager::instance().canRedo());

  // Preferences action
  connect(m_actionPreferences, &QAction::triggered, this,
          &NMMainWindow::showSettingsDialog);

  // View menu - panel toggles
  connect(m_actionToggleSceneView, &QAction::toggled, m_sceneViewPanel,
          &QDockWidget::setVisible);
  connect(m_actionToggleStoryGraph, &QAction::toggled, m_storyGraphPanel,
          &QDockWidget::setVisible);
  connect(m_actionToggleInspector, &QAction::toggled, m_inspectorPanel,
          &QDockWidget::setVisible);
  connect(m_actionToggleConsole, &QAction::toggled, m_consolePanel,
          &QDockWidget::setVisible);
  connect(m_actionToggleIssues, &QAction::toggled, m_issuesPanel,
          &QDockWidget::setVisible);
  connect(m_actionToggleDiagnostics, &QAction::toggled, m_diagnosticsPanel,
          &QDockWidget::setVisible);
  // D6: Audio/Voice panel connections
  connect(m_actionToggleVoiceStudio, &QAction::toggled, this,
          [this](bool checked) {
            if (checked) {
              // Re-dock if not currently docked
              if (!m_voiceStudioPanel->isVisible() || m_voiceStudioPanel->isFloating()) {
                QList<QDockWidget*> docks = this->findChildren<QDockWidget*>();
                if (!docks.contains(m_voiceStudioPanel) || m_voiceStudioPanel->parent() != this) {
                  addDockWidget(Qt::RightDockWidgetArea, m_voiceStudioPanel);
                  if (m_inspectorPanel && m_inspectorPanel->parent() == this) {
                    tabifyDockWidget(m_inspectorPanel, m_voiceStudioPanel);
                  }
                }
              }
              m_voiceStudioPanel->show();
              m_voiceStudioPanel->raise();
            } else {
              m_voiceStudioPanel->hide();
            }
          });
  connect(m_actionToggleVoiceManager, &QAction::toggled, m_voiceManagerPanel,
          &QDockWidget::setVisible);
  connect(m_actionToggleAudioMixer, &QAction::toggled, this,
          [this](bool checked) {
            if (checked) {
              // Re-dock if not currently docked
              if (!m_audioMixerPanel->isVisible() || m_audioMixerPanel->isFloating()) {
                QList<QDockWidget*> docks = this->findChildren<QDockWidget*>();
                if (!docks.contains(m_audioMixerPanel) || m_audioMixerPanel->parent() != this) {
                  addDockWidget(Qt::RightDockWidgetArea, m_audioMixerPanel);
                  if (m_inspectorPanel && m_inspectorPanel->parent() == this) {
                    tabifyDockWidget(m_inspectorPanel, m_audioMixerPanel);
                  }
                }
              }
              m_audioMixerPanel->show();
              m_audioMixerPanel->raise();
            } else {
              m_audioMixerPanel->hide();
            }
          });
  connect(m_actionToggleLocalization, &QAction::toggled, m_localizationPanel,
          &QDockWidget::setVisible);
  connect(m_actionToggleTimeline, &QAction::toggled, m_timelinePanel,
          &QDockWidget::setVisible);
  connect(m_actionToggleCurveEditor, &QAction::toggled, m_curveEditorPanel,
          &QDockWidget::setVisible);
  connect(m_actionToggleBuildSettings, &QAction::toggled, m_buildSettingsPanel,
          &QDockWidget::setVisible);
  connect(m_actionToggleAssetBrowser, &QAction::toggled, m_assetBrowserPanel,
          &QDockWidget::setVisible);
  connect(m_actionToggleScenePalette, &QAction::toggled, m_scenePalettePanel,
          &QDockWidget::setVisible);
  connect(m_actionToggleHierarchy, &QAction::toggled, m_hierarchyPanel,
          &QDockWidget::setVisible);
  connect(m_actionToggleScriptEditor, &QAction::toggled, m_scriptEditorPanel,
          &QDockWidget::setVisible);
  connect(m_actionToggleScriptDocs, &QAction::toggled, m_scriptDocPanel,
          &QDockWidget::setVisible);
  connect(m_actionToggleDebugOverlay, &QAction::toggled, m_debugOverlayPanel,
          &QDockWidget::setVisible);
  connect(m_issuesPanel, &QDockWidget::visibilityChanged, this,
          [this](bool visible) {
            Q_UNUSED(visible);
          });

  // Sync panel visibility with menu actions - use smart sync to prevent feedback loops with tabbed docks
  connect(m_sceneViewPanel, &QDockWidget::visibilityChanged, this,
          [this](bool visible) {
            if (m_actionToggleSceneView->isChecked() != visible) {
              QSignalBlocker blocker(m_actionToggleSceneView);
              m_actionToggleSceneView->setChecked(visible);
            }
          });
  connect(m_storyGraphPanel, &QDockWidget::visibilityChanged, this,
          [this](bool visible) {
            if (m_actionToggleStoryGraph->isChecked() != visible) {
              QSignalBlocker blocker(m_actionToggleStoryGraph);
              m_actionToggleStoryGraph->setChecked(visible);
            }
          });
  connect(m_inspectorPanel, &QDockWidget::visibilityChanged, this,
          [this](bool visible) {
            if (m_actionToggleInspector->isChecked() != visible) {
              QSignalBlocker blocker(m_actionToggleInspector);
              m_actionToggleInspector->setChecked(visible);
            }
          });
  connect(m_consolePanel, &QDockWidget::visibilityChanged, this,
          [this](bool visible) {
            if (m_actionToggleConsole->isChecked() != visible) {
              QSignalBlocker blocker(m_actionToggleConsole);
              m_actionToggleConsole->setChecked(visible);
            }
          });
  connect(m_issuesPanel, &QDockWidget::visibilityChanged, this,
          [this](bool visible) {
            if (m_actionToggleIssues->isChecked() != visible) {
              QSignalBlocker blocker(m_actionToggleIssues);
              m_actionToggleIssues->setChecked(visible);
            }
          });
  connect(m_diagnosticsPanel, &QDockWidget::visibilityChanged, this,
          [this](bool visible) {
            if (m_actionToggleDiagnostics->isChecked() != visible) {
              QSignalBlocker blocker(m_actionToggleDiagnostics);
              m_actionToggleDiagnostics->setChecked(visible);
            }
          });
  // D6: Audio/Voice visibility sync - use smarter sync that doesn't break tabbed docks
  connect(m_voiceStudioPanel, &QDockWidget::visibilityChanged, this,
          [this](bool visible) {
            if (m_actionToggleVoiceStudio->isChecked() != visible) {
              QSignalBlocker blocker(m_actionToggleVoiceStudio);
              m_actionToggleVoiceStudio->setChecked(visible);
            }
          });
  connect(m_voiceManagerPanel, &QDockWidget::visibilityChanged, this,
          [this](bool visible) {
            if (m_actionToggleVoiceManager->isChecked() != visible) {
              QSignalBlocker blocker(m_actionToggleVoiceManager);
              m_actionToggleVoiceManager->setChecked(visible);
            }
          });
  connect(m_audioMixerPanel, &QDockWidget::visibilityChanged, this,
          [this](bool visible) {
            if (m_actionToggleAudioMixer->isChecked() != visible) {
              QSignalBlocker blocker(m_actionToggleAudioMixer);
              m_actionToggleAudioMixer->setChecked(visible);
            }
          });
  connect(m_localizationPanel, &QDockWidget::visibilityChanged, this,
          [this](bool visible) {
            if (m_actionToggleLocalization->isChecked() != visible) {
              QSignalBlocker blocker(m_actionToggleLocalization);
              m_actionToggleLocalization->setChecked(visible);
            }
          });
  connect(m_timelinePanel, &QDockWidget::visibilityChanged, this,
          [this](bool visible) {
            if (m_actionToggleTimeline->isChecked() != visible) {
              QSignalBlocker blocker(m_actionToggleTimeline);
              m_actionToggleTimeline->setChecked(visible);
            }
          });
  connect(m_curveEditorPanel, &QDockWidget::visibilityChanged, this,
          [this](bool visible) {
            if (m_actionToggleCurveEditor->isChecked() != visible) {
              QSignalBlocker blocker(m_actionToggleCurveEditor);
              m_actionToggleCurveEditor->setChecked(visible);
            }
          });
  connect(m_buildSettingsPanel, &QDockWidget::visibilityChanged, this,
          [this](bool visible) {
            if (m_actionToggleBuildSettings->isChecked() != visible) {
              QSignalBlocker blocker(m_actionToggleBuildSettings);
              m_actionToggleBuildSettings->setChecked(visible);
            }
          });
  connect(m_assetBrowserPanel, &QDockWidget::visibilityChanged, this,
          [this](bool visible) {
            if (m_actionToggleAssetBrowser->isChecked() != visible) {
              QSignalBlocker blocker(m_actionToggleAssetBrowser);
              m_actionToggleAssetBrowser->setChecked(visible);
            }
          });
  connect(m_scenePalettePanel, &QDockWidget::visibilityChanged, this,
          [this](bool visible) {
            if (m_actionToggleScenePalette->isChecked() != visible) {
              QSignalBlocker blocker(m_actionToggleScenePalette);
              m_actionToggleScenePalette->setChecked(visible);
            }
          });
  connect(m_hierarchyPanel, &QDockWidget::visibilityChanged, this,
          [this](bool visible) {
            if (m_actionToggleHierarchy->isChecked() != visible) {
              QSignalBlocker blocker(m_actionToggleHierarchy);
              m_actionToggleHierarchy->setChecked(visible);
            }
          });
  connect(m_scriptEditorPanel, &QDockWidget::visibilityChanged, this,
          [this](bool visible) {
            if (m_actionToggleScriptEditor->isChecked() != visible) {
              QSignalBlocker blocker(m_actionToggleScriptEditor);
              m_actionToggleScriptEditor->setChecked(visible);
            }
          });
  connect(m_scriptDocPanel, &QDockWidget::visibilityChanged, this,
          [this](bool visible) {
            if (m_actionToggleScriptDocs->isChecked() != visible) {
              QSignalBlocker blocker(m_actionToggleScriptDocs);
              m_actionToggleScriptDocs->setChecked(visible);
            }
          });
  connect(m_debugOverlayPanel, &QDockWidget::visibilityChanged, this,
          [this](bool visible) {
            if (m_actionToggleDebugOverlay->isChecked() != visible) {
              QSignalBlocker blocker(m_actionToggleDebugOverlay);
              m_actionToggleDebugOverlay->setChecked(visible);
            }
          });

  connect(m_actionResetLayout, &QAction::triggered, this,
          &NMMainWindow::resetToDefaultLayout);

  // D2: New workspace preset connections
  connect(m_actionLayoutDefault, &QAction::triggered, this,
          [this]() { applyWorkspacePreset(LayoutPreset::Default); });
  connect(m_actionLayoutStoryScript, &QAction::triggered, this,
          [this]() { applyWorkspacePreset(LayoutPreset::StoryScript); });
  connect(m_actionLayoutSceneAnimation, &QAction::triggered, this,
          [this]() { applyWorkspacePreset(LayoutPreset::SceneAnimation); });
  connect(m_actionLayoutAudioVoice, &QAction::triggered, this,
          [this]() { applyWorkspacePreset(LayoutPreset::AudioVoice); });

  // Legacy workspace presets
  connect(m_actionLayoutStory, &QAction::triggered, this,
          [this]() { applyLayoutPreset(LayoutPreset::Story); });
  connect(m_actionLayoutScene, &QAction::triggered, this,
          [this]() { applyLayoutPreset(LayoutPreset::Scene); });
  connect(m_actionLayoutScript, &QAction::triggered, this,
          [this]() { applyLayoutPreset(LayoutPreset::Script); });
  connect(m_actionLayoutDeveloper, &QAction::triggered, this,
          [this]() { applyLayoutPreset(LayoutPreset::Developer); });
  connect(m_actionLayoutCompact, &QAction::triggered, this,
          [this]() { applyLayoutPreset(LayoutPreset::Compact); });
  connect(m_actionSaveLayout, &QAction::triggered, this,
          &NMMainWindow::saveCustomLayout);
  connect(m_actionLoadLayout, &QAction::triggered, this,
          &NMMainWindow::loadCustomLayout);

  auto &styleManager = NMStyleManager::instance();
  auto updateScaleActions = [this](double scale) {
    auto isNear = [](double value, double target) {
      return std::abs(value - target) < 0.01;
    };
    if (m_actionUiScaleCompact) {
      m_actionUiScaleCompact->setChecked(isNear(scale, 0.9));
    }
    if (m_actionUiScaleDefault) {
      m_actionUiScaleDefault->setChecked(isNear(scale, 1.0));
    }
    if (m_actionUiScaleComfort) {
      m_actionUiScaleComfort->setChecked(isNear(scale, 1.1));
    }
  };

  connect(m_actionUiScaleCompact, &QAction::triggered, this, []() {
    NMStyleManager::instance().setUiScale(0.9);
  });
  connect(m_actionUiScaleDefault, &QAction::triggered, this, []() {
    NMStyleManager::instance().setUiScale(1.0);
  });
  connect(m_actionUiScaleComfort, &QAction::triggered, this, []() {
    NMStyleManager::instance().setUiScale(1.1);
  });
  connect(m_actionUiScaleDown, &QAction::triggered, this, []() {
    auto &manager = NMStyleManager::instance();
    manager.setUiScale(manager.uiScale() - 0.1);
  });
  connect(m_actionUiScaleUp, &QAction::triggered, this, []() {
    auto &manager = NMStyleManager::instance();
    manager.setUiScale(manager.uiScale() + 0.1);
  });
  connect(m_actionUiScaleReset, &QAction::triggered, this, []() {
    NMStyleManager::instance().setUiScale(1.0);
  });
  connect(&styleManager, &NMStyleManager::scaleChanged, this,
          [updateScaleActions](double scale) { updateScaleActions(scale); });
  updateScaleActions(styleManager.uiScale());

  connect(m_actionFocusMode, &QAction::toggled, this,
          &NMMainWindow::toggleFocusMode);
  connect(m_actionFocusIncludeHierarchy, &QAction::toggled, this,
          [this](bool enabled) {
            m_focusIncludeHierarchy = enabled;
            if (m_focusModeEnabled) {
              applyFocusModeLayout();
            }
          });
  connect(m_actionLockLayout, &QAction::toggled, this,
          [this](bool locked) { applyDockLockState(locked); });
  connect(m_actionTabbedDockOnly, &QAction::toggled, this,
          [this](bool enabled) { applyTabbedDockMode(enabled); });
  connect(m_actionFloatAllowed, &QAction::toggled, this,
          [this](bool allowed) { applyFloatAllowed(allowed); });

  // Play menu
  connect(m_actionPlay, &QAction::triggered, this,
          &NMMainWindow::playRequested);
  connect(m_actionStop, &QAction::triggered, this,
          &NMMainWindow::stopRequested);

  // Play menu -> PlayModeController
  auto &playController = NMPlayModeController::instance();
  connect(m_actionPlay, &QAction::triggered, &playController,
          &NMPlayModeController::play);
  connect(m_actionPause, &QAction::triggered, &playController,
          &NMPlayModeController::pause);
  connect(m_actionStop, &QAction::triggered, &playController,
          &NMPlayModeController::stop);
  connect(m_actionStepFrame, &QAction::triggered, &playController,
          &NMPlayModeController::stepForward);
  connect(m_actionSaveState, &QAction::triggered, this, [this, &playController]() {
    if (!playController.saveSlot(0)) {
      NMMessageDialog::showError(this, tr("Save Failed"),
                                 tr("Failed to save runtime state."));
    }
  });
  connect(m_actionLoadState, &QAction::triggered, this, [this, &playController]() {
    if (!playController.loadSlot(0)) {
      NMMessageDialog::showError(this, tr("Load Failed"),
                                 tr("Failed to load runtime state."));
    }
  });
  connect(m_actionAutoSaveState, &QAction::triggered, this,
          [this, &playController]() {
            if (!playController.saveAuto()) {
              NMMessageDialog::showError(this, tr("Auto-Save Failed"),
                                         tr("Failed to auto-save runtime state."));
            }
          });
  connect(m_actionAutoLoadState, &QAction::triggered, this,
          [this, &playController]() {
            if (!playController.loadAuto()) {
              NMMessageDialog::showError(this, tr("Auto-Load Failed"),
                                         tr("Failed to auto-load runtime state."));
            }
          });

  auto updatePlayActions = [this](NMPlayModeController::PlayMode mode) {
    const bool isPlaying = (mode == NMPlayModeController::Playing);
    const bool isPaused = (mode == NMPlayModeController::Paused);

    m_actionPlay->setEnabled(!isPlaying);
    m_actionPause->setEnabled(isPlaying);
    m_actionStop->setEnabled(isPlaying || isPaused);
    m_actionStepFrame->setEnabled(!isPlaying);
    const bool runtimeReady = NMPlayModeController::instance().isRuntimeLoaded();
    const bool hasAutoSave = NMPlayModeController::instance().hasAutoSave();
    m_actionSaveState->setEnabled(runtimeReady);
    m_actionLoadState->setEnabled(runtimeReady);
    m_actionAutoSaveState->setEnabled(runtimeReady);
    m_actionAutoLoadState->setEnabled(runtimeReady && hasAutoSave);
  };
  connect(&playController, &NMPlayModeController::playModeChanged, this,
          updatePlayActions);
  updatePlayActions(playController.playMode());
  connect(&playController, &NMPlayModeController::playModeChanged, this,
          [this](NMPlayModeController::PlayMode mode) {
            Q_UNUSED(mode);
            updateStatusBarContext();
          });
  connect(&playController, &NMPlayModeController::currentNodeChanged, this,
          [this](const QString &nodeId) {
            m_activeNodeId = nodeId;
            updateStatusBarContext();
          });

  // Help menu
  connect(m_actionAbout, &QAction::triggered, this,
          &NMMainWindow::showAboutDialog);
  connect(m_actionDocumentation, &QAction::triggered, []() {
    QDesktopServices::openUrl(QUrl("https://github.com/VisageDvachevsky/NM-"));
  });
  connect(m_actionHotkeys, &QAction::triggered, this, [this]() {
    auto shortcutText = [](QAction *action) {
      if (!action) {
        return QString();
      }
      return action->shortcut().toString(QKeySequence::NativeText);
    };

    QList<NMHotkeyEntry> entries;
    auto addActionEntry = [&entries, &shortcutText](const QString &section,
                                                    const QString &actionName,
                                                    QAction *action,
                                                    const QString &notes = QString()) {
      const QString shortcut = shortcutText(action);
      NMHotkeyEntry entry;
      entry.id = action ? action->objectName() : actionName;
      if (entry.id.isEmpty()) {
        entry.id = actionName;
      }
      entry.section = section;
      entry.action = actionName;
      entry.shortcut = shortcut;
      entry.defaultShortcut = shortcut;
      entry.notes = notes;
      entries.push_back(entry);
    };

    addActionEntry(tr("File"), tr("New Project"), m_actionNewProject);
    addActionEntry(tr("File"), tr("Open Project"), m_actionOpenProject);
    addActionEntry(tr("File"), tr("Save Project"), m_actionSaveProject);
    addActionEntry(tr("File"), tr("Save Project As"), m_actionSaveProjectAs);
    addActionEntry(tr("File"), tr("Close Project"), m_actionCloseProject);
    addActionEntry(tr("File"), tr("Quit"), m_actionExit);

    addActionEntry(tr("Edit"), tr("Undo"), m_actionUndo);
    addActionEntry(tr("Edit"), tr("Redo"), m_actionRedo);
    addActionEntry(tr("Edit"), tr("Cut"), m_actionCut);
    addActionEntry(tr("Edit"), tr("Copy"), m_actionCopy);
    addActionEntry(tr("Edit"), tr("Paste"), m_actionPaste);
    addActionEntry(tr("Edit"), tr("Delete"), m_actionDelete);
    addActionEntry(tr("Edit"), tr("Select All"), m_actionSelectAll);

    addActionEntry(tr("Play"), tr("Play"), m_actionPlay);
    addActionEntry(tr("Play"), tr("Pause"), m_actionPause);
    addActionEntry(tr("Play"), tr("Stop"), m_actionStop);
    addActionEntry(tr("Play"), tr("Step Frame"), m_actionStepFrame);
    addActionEntry(tr("Play"), tr("Save State"), m_actionSaveState);
    addActionEntry(tr("Play"), tr("Load State"), m_actionLoadState);
    addActionEntry(tr("Play"), tr("Auto Save"), m_actionAutoSaveState);
    addActionEntry(tr("Play"), tr("Auto Load"), m_actionAutoLoadState);

    // D6: Audio/Voice actions
    addActionEntry(tr("Audio / Voice"), tr("Voice Studio"), m_actionToggleVoiceStudio,
                   tr("Record and edit voice lines with waveform visualization"));

    // D2: New workspace presets
    addActionEntry(tr("Workspaces"), tr("Default"), m_actionLayoutDefault);
    addActionEntry(tr("Workspaces"), tr("Story / Script"), m_actionLayoutStoryScript);
    addActionEntry(tr("Workspaces"), tr("Scene / Animation"), m_actionLayoutSceneAnimation);
    addActionEntry(tr("Workspaces"), tr("Audio / Voice"), m_actionLayoutAudioVoice);

    // Legacy workspaces
    addActionEntry(tr("Workspaces"), tr("Story (Legacy)"), m_actionLayoutStory);
    addActionEntry(tr("Workspaces"), tr("Scene (Legacy)"), m_actionLayoutScene);
    addActionEntry(tr("Workspaces"), tr("Script (Legacy)"), m_actionLayoutScript);
    addActionEntry(tr("Workspaces"), tr("Developer (Legacy)"),
                   m_actionLayoutDeveloper);
    addActionEntry(tr("Workspaces"), tr("Compact (Legacy)"),
                   m_actionLayoutCompact);
    addActionEntry(tr("Layout"), tr("Focus Mode"), m_actionFocusMode);
    addActionEntry(tr("Layout"), tr("Lock Layout"), m_actionLockLayout);
    addActionEntry(tr("Layout"), tr("Tabbed Dock Only"), m_actionTabbedDockOnly);
    addActionEntry(tr("UI Scale"), tr("Scale Down"), m_actionUiScaleDown);
    addActionEntry(tr("UI Scale"), tr("Scale Up"), m_actionUiScaleUp);
    addActionEntry(tr("UI Scale"), tr("Scale Reset"), m_actionUiScaleReset);

    auto addStaticEntry = [&entries](const QString &section, const QString &action,
                                     const QString &shortcut, const QString &notes) {
      NMHotkeyEntry entry;
      entry.id = section + "." + action;
      entry.section = section;
      entry.action = action;
      entry.shortcut = shortcut;
      entry.defaultShortcut = shortcut;
      entry.notes = notes;
      entries.push_back(entry);
    };

    addStaticEntry(tr("Script Editor"), tr("Completion"), tr("Ctrl+Space"),
                   tr("Trigger code suggestions"));
    addStaticEntry(tr("Script Editor"), tr("Save Script"), tr("Ctrl+S"),
                   tr("Save current script tab"));

    addStaticEntry(tr("Story Graph"), tr("Connect Nodes"), tr("Ctrl+Drag"),
                   tr("Drag from output port to input"));
    addStaticEntry(tr("Story Graph"), tr("Pan View"), tr("Middle Mouse"),
                   tr("Hold and drag to pan"));
    addStaticEntry(tr("Story Graph"), tr("Zoom"), tr("Mouse Wheel"),
                   tr("Scroll to zoom in/out"));

    addStaticEntry(tr("Scene View"), tr("Pan View"), tr("Middle Mouse"),
                   tr("Hold and drag to pan"));
    addStaticEntry(tr("Scene View"), tr("Zoom"), tr("Mouse Wheel"),
                   tr("Scroll to zoom in/out"));
    addStaticEntry(tr("Scene View"), tr("Frame Selected"), tr("F"),
                   tr("Focus camera on selected object"));
    addStaticEntry(tr("Scene View"), tr("Frame All"), tr("A"),
                   tr("Frame everything in view"));
    addStaticEntry(tr("Scene View"), tr("Toggle Grid"), tr("G"),
                   tr("Show/hide grid"));
    addStaticEntry(tr("Scene View"), tr("Copy Object"), tr("Ctrl+C"),
                   tr("Copy selected object"));
    addStaticEntry(tr("Scene View"), tr("Paste Object"), tr("Ctrl+V"),
                   tr("Paste copied object"));
    addStaticEntry(tr("Scene View"), tr("Duplicate Object"), tr("Ctrl+D"),
                   tr("Duplicate selected object"));
    addStaticEntry(tr("Scene View"), tr("Rename Object"), tr("F2"),
                   tr("Rename selected object"));
    addStaticEntry(tr("Scene View"), tr("Delete Object"), tr("Del"),
                   tr("Delete selected object"));

    addStaticEntry(tr("Docking"), tr("Move Panel"), QString(),
                   tr("Drag panel tabs to dock anywhere"));
    addStaticEntry(tr("Docking"), tr("Tab Panels"), QString(),
                   tr("Drop a panel on another to create tabs"));

    NMHotkeysDialog dialog(entries, this);
    dialog.exec();
  });

  // Panel inter-connections
  // Connect scene view selection to inspector
  connect(m_sceneViewPanel, &NMSceneViewPanel::objectSelected, this,
          [this](const QString &objectId) {
            if (!m_inspectorPanel) {
              return;
            }
            if (objectId.isEmpty()) {
              m_inspectorPanel->showNoSelection();
              return;
            }
            if (auto *obj = m_sceneViewPanel->findObjectById(objectId)) {
              const bool editable =
                  !objectId.startsWith("runtime_");
              m_inspectorPanel->inspectSceneObject(obj, editable);
            }
            if (!objectId.isEmpty()) {
              m_activeSelectionLabel = tr("Object: %1").arg(objectId);
            } else {
              m_activeSelectionLabel.clear();
            }
            updateStatusBarContext();
          });

  connect(m_sceneViewPanel, &NMSceneViewPanel::sceneObjectsChanged,
          m_hierarchyPanel, &NMHierarchyPanel::refresh);
  connect(m_sceneViewPanel, &NMSceneViewPanel::objectSelected, m_hierarchyPanel,
          &NMHierarchyPanel::selectObject);

  // Connect hierarchy selection to scene view and inspector
  connect(m_hierarchyPanel, &NMHierarchyPanel::objectSelected, m_sceneViewPanel,
          [this](const QString &objectId) {
            if (m_sceneViewPanel) {
              m_sceneViewPanel->selectObjectById(objectId);
            }
          });
  connect(m_hierarchyPanel, &NMHierarchyPanel::objectSelected, this,
          [this](const QString &objectId) {
            if (!m_inspectorPanel) {
              return;
            }
            if (objectId.isEmpty()) {
              m_inspectorPanel->showNoSelection();
              return;
            }
            if (auto *obj = m_sceneViewPanel->findObjectById(objectId)) {
              const bool editable =
                  !objectId.startsWith("runtime_");
              m_inspectorPanel->inspectSceneObject(obj, editable);
            }
            if (!objectId.isEmpty()) {
              m_activeSelectionLabel = tr("Object: %1").arg(objectId);
            } else {
              m_activeSelectionLabel.clear();
            }
            updateStatusBarContext();
          });

  connect(m_storyGraphPanel, &NMStoryGraphPanel::nodeSelected, this,
          [this](const QString &nodeIdString) {
            if (!m_inspectorPanel) {
              return;
            }
            if (nodeIdString.isEmpty()) {
              m_inspectorPanel->showNoSelection();
              if (m_sceneViewPanel) {
                m_sceneViewPanel->clearStoryPreview();
              }
              return;
            }
            if (auto *node =
                    m_storyGraphPanel->findNodeByIdString(nodeIdString)) {
              m_inspectorPanel->inspectStoryGraphNode(node, true);
              m_inspectorPanel->show();
              m_inspectorPanel->raise();
              if (m_sceneViewPanel) {
                m_sceneViewPanel->setStoryPreview(node->dialogueSpeaker(),
                                                  node->dialogueText(),
                                                  node->choiceOptions());
              }
              if (m_sceneViewPanel &&
                  node->nodeType().compare("Entry", Qt::CaseInsensitive) !=
                      0) {
                auto &playControllerRef = NMPlayModeController::instance();
                if (!playControllerRef.isPlaying() &&
                    !playControllerRef.isPaused()) {
                  m_sceneViewPanel->loadSceneDocument(node->nodeIdString());
                }
              }
            }
            if (!NMPlayModeController::instance().isPlaying() &&
                !NMPlayModeController::instance().isPaused()) {
              m_activeNodeId = nodeIdString;
            }
            if (!nodeIdString.isEmpty()) {
              m_activeSelectionLabel = tr("Node: %1").arg(nodeIdString);
            } else {
              m_activeSelectionLabel.clear();
            }
            updateStatusBarContext();
          });

  connect(m_storyGraphPanel, &NMStoryGraphPanel::nodeActivated, this,
          [this](const QString &nodeIdString) {
            if (!m_sceneViewPanel) {
              return;
            }
            if (nodeIdString.isEmpty()) {
              return;
            }
            auto &playControllerRef = NMPlayModeController::instance();
            if (!playControllerRef.isPlaying() &&
                !playControllerRef.isPaused()) {
              m_sceneViewPanel->loadSceneDocument(nodeIdString);
            }
            m_sceneViewPanel->show();
            m_sceneViewPanel->raise();
            m_sceneViewPanel->setFocus();
          });

  connect(m_storyGraphPanel, &NMStoryGraphPanel::scriptNodeRequested, this,
          [this](const QString &scriptPath) {
            if (!m_scriptEditorPanel) {
              return;
            }
            m_scriptEditorPanel->openScript(scriptPath);
            m_scriptEditorPanel->show();
            m_scriptEditorPanel->raise();
            m_scriptEditorPanel->setFocus();
          });

  // Scene Node → Timeline Panel integration
  connect(m_storyGraphPanel, &NMStoryGraphPanel::sceneNodeDoubleClicked, this,
          [this](const QString &sceneId) {
            if (!m_sceneViewPanel || !m_timelinePanel) {
              return;
            }
            qDebug() << "[MainWindow] Scene node double-clicked, opening Scene View and Timeline for:" << sceneId;

            // Load scene in Scene View
            auto &playControllerRef = NMPlayModeController::instance();
            if (!playControllerRef.isPlaying() && !playControllerRef.isPaused()) {
              m_sceneViewPanel->loadSceneDocument(sceneId);
            }

            // Show Scene View panel
            m_sceneViewPanel->show();
            m_sceneViewPanel->raise();

            // Show Timeline panel
            m_timelinePanel->show();
            m_timelinePanel->raise();

            // Enable animation preview mode in Scene View
            m_sceneViewPanel->setAnimationPreviewMode(true);

            setStatusMessage(tr("Editing scene: %1 (Timeline and Scene View)").arg(sceneId), 3000);
          });

  connect(m_scriptEditorPanel, &NMScriptEditorPanel::docHtmlChanged,
          m_scriptDocPanel, &NMScriptDocPanel::setDocHtml);
  connect(m_assetBrowserPanel, &NMAssetBrowserPanel::assetSelected, this,
          [this](const QString &path) {
            m_activeAssetPath = path;
            if (!path.isEmpty()) {
              m_activeSelectionLabel =
                  tr("Asset: %1").arg(QFileInfo(path).fileName());
            } else {
              m_activeSelectionLabel.clear();
            }
            updateStatusBarContext();
          });
  connect(m_issuesPanel, &NMIssuesPanel::issueActivated, this,
          [this](const QString &file, int line) {
            if (m_scriptEditorPanel) {
              m_scriptEditorPanel->goToLocation(file, line);
            }
          });

  // Diagnostics panel navigation
  connect(m_diagnosticsPanel, &NMDiagnosticsPanel::diagnosticActivated, this,
          [this](const QString &location) {
            handleNavigationRequest(location);
          });

  connect(m_assetBrowserPanel, &NMAssetBrowserPanel::assetDoubleClicked, this,
          [this](const QString &path) {
            if (path.endsWith(".nms")) {
              m_scriptEditorPanel->openScript(path);
              return;
            }

            QFileInfo info(path);
            const QString ext = info.suffix().toLower();
            const bool isImage = (ext == "png" || ext == "jpg" ||
                                  ext == "jpeg" || ext == "bmp" ||
                                  ext == "gif");
            if (!isImage || !m_sceneViewPanel) {
              return;
            }

            if (auto *scene = m_sceneViewPanel->graphicsScene()) {
              if (auto *selected = scene->selectedObject()) {
                m_sceneViewPanel->setObjectAsset(selected->id(), path);
                m_sceneViewPanel->selectObjectById(selected->id());
                return;
              }
            }

            m_sceneViewPanel->addObjectFromAsset(path, QPointF(0, 0));
          });

  auto createSnapshot =
      [this](NMSceneObjectType type, const QPointF &pos) -> SceneObjectSnapshot {
    SceneObjectSnapshot snapshot;
    const qint64 stamp = QDateTime::currentMSecsSinceEpoch();
    QString prefix;
    QString label;
    switch (type) {
    case NMSceneObjectType::Background:
      prefix = "background";
      label = "New Background";
      break;
    case NMSceneObjectType::Character:
      prefix = "character";
      label = "New Character";
      break;
    case NMSceneObjectType::Effect:
      prefix = "effect";
      label = "New Effect";
      break;
    default:
      prefix = "ui";
      label = "New UI Element";
      break;
    }
    snapshot.id = QString("%1_%2").arg(prefix).arg(stamp);
    snapshot.name = label;
    snapshot.type = type;
    snapshot.position = pos;
    snapshot.scaleX = 1.0;
    snapshot.scaleY = 1.0;
    snapshot.rotation = 0.0;
    snapshot.opacity = 1.0;
    snapshot.visible = true;
    snapshot.zValue = 0.0;
    return snapshot;
  };

  connect(m_scenePalettePanel, &NMScenePalettePanel::createObjectRequested,
          this, [this, createSnapshot](NMSceneObjectType type) {
            if (!m_sceneViewPanel) {
              return;
            }
            SceneObjectSnapshot snapshot =
                createSnapshot(type, QPointF(0, 0));
            NMUndoManager::instance().pushCommand(
                new AddObjectCommand(m_sceneViewPanel, snapshot));
          });

  connect(m_scenePalettePanel, &NMScenePalettePanel::assetsDropped, this,
          [this](const QStringList &paths, int typeHint) {
            if (!m_sceneViewPanel || paths.isEmpty()) {
              return;
            }
            QPointF basePos(0, 0);
            if (auto *view = m_sceneViewPanel->graphicsView()) {
              basePos = view->mapToScene(view->viewport()->rect().center());
            }
            QPointF pos = basePos;
            const QPointF offset(32.0, 32.0);
            for (const QString &path : paths) {
              if (typeHint < 0) {
                m_sceneViewPanel->addObjectFromAsset(path, pos);
              } else {
                m_sceneViewPanel->addObjectFromAsset(
                    path, pos, static_cast<NMSceneObjectType>(typeHint));
              }
              pos += offset;
            }
          });

  connect(m_inspectorPanel, &NMInspectorPanel::propertyChanged, this,
          [this](const QString &objectId, const QString &propertyName,
                 const QString &newValue) {
            if (objectId.isEmpty()) {
              return;
            }

            auto applyProperty = [this](const QString &targetId,
                                        const QString &key,
                                        const QString &value) {
              if (auto *obj = m_sceneViewPanel->findObjectById(targetId)) {
                if (key == "name") {
                  m_sceneViewPanel->renameObject(targetId, value);
                  m_sceneViewPanel->selectObjectById(targetId);
                } else if (key == "asset") {
                  m_sceneViewPanel->setObjectAsset(targetId, value);
                  m_sceneViewPanel->selectObjectById(targetId);
                } else if (key == "position_x" || key == "position_y") {
                  QPointF pos = obj->pos();
                  if (key == "position_x") {
                    pos.setX(value.toDouble());
                  } else {
                    pos.setY(value.toDouble());
                  }
                  m_sceneViewPanel->moveObject(targetId, pos);
                } else if (key == "rotation") {
                  m_sceneViewPanel->rotateObject(targetId, value.toDouble());
                } else if (key == "scale_x" || key == "scale_y") {
                  QPointF scale =
                      m_sceneViewPanel->graphicsScene()->getObjectScale(
                          targetId);
                  if (key == "scale_x") {
                    scale.setX(value.toDouble());
                  } else {
                    scale.setY(value.toDouble());
                  }
                  m_sceneViewPanel->scaleObject(targetId, scale.x(), scale.y());
                } else if (key == "visible") {
                  const bool newVisible =
                      (value.toLower() == "true" || value == "1");
                  const bool oldVisible = obj->isVisible();
                  if (oldVisible != newVisible) {
                    auto *cmd = new ToggleObjectVisibilityCommand(
                        m_sceneViewPanel, targetId, oldVisible, newVisible);
                    NMUndoManager::instance().pushCommand(cmd);
                  }
                } else if (key == "alpha") {
                  m_sceneViewPanel->setObjectOpacity(targetId,
                                                     value.toDouble());
                } else if (key == "z") {
                  m_sceneViewPanel->setObjectZOrder(targetId,
                                                    value.toDouble());
                } else if (key == "locked") {
                  const bool newLocked =
                      (value.toLower() == "true" || value == "1");
                  const bool oldLocked = obj->isLocked();
                  if (oldLocked != newLocked) {
                    auto *cmd = new ToggleObjectLockedCommand(
                        m_sceneViewPanel, targetId, oldLocked, newLocked);
                    NMUndoManager::instance().pushCommand(cmd);
                  }
                }
              } else if (m_storyGraphPanel->findNodeByIdString(targetId) !=
                         nullptr) {
                m_storyGraphPanel->applyNodePropertyChange(targetId, key,
                                                           value);
                if (key == "scriptPath" && m_scriptEditorPanel) {
                  m_scriptEditorPanel->openScript(value);
                }
                if (m_sceneViewPanel) {
                  if (auto *node =
                          m_storyGraphPanel->findNodeByIdString(targetId)) {
                    m_sceneViewPanel->setStoryPreview(
                        node->dialogueSpeaker(), node->dialogueText(),
                        node->choiceOptions());
                  }
                }
              }

              if (m_inspectorPanel &&
                  m_inspectorPanel->currentObjectId() == targetId) {
                m_inspectorPanel->updatePropertyValue(key, value);
              }
            };

            QString oldValue;
            bool found = false;
            if (auto *obj = m_sceneViewPanel->findObjectById(objectId)) {
              if (propertyName == "name") {
                oldValue = obj->name();
                found = true;
              } else if (propertyName == "position_x") {
                oldValue = QString::number(obj->pos().x());
                found = true;
              } else if (propertyName == "position_y") {
                oldValue = QString::number(obj->pos().y());
                found = true;
              } else if (propertyName == "rotation") {
                oldValue = QString::number(obj->rotation());
                found = true;
              } else if (propertyName == "scale_x") {
                oldValue = QString::number(obj->scaleX());
                found = true;
              } else if (propertyName == "scale_y") {
                oldValue = QString::number(obj->scaleY());
                found = true;
              } else if (propertyName == "visible") {
                oldValue = obj->isVisible() ? "true" : "false";
                found = true;
              } else if (propertyName == "asset") {
                oldValue = obj->assetPath();
                found = true;
              } else if (propertyName == "alpha") {
                oldValue = QString::number(obj->opacity());
                found = true;
              } else if (propertyName == "z") {
                oldValue = QString::number(obj->zValue());
                found = true;
              } else if (propertyName == "locked") {
                oldValue = obj->isLocked() ? "true" : "false";
                found = true;
              }
            } else if (auto *node =
                           m_storyGraphPanel->findNodeByIdString(objectId)) {
              if (propertyName == "title") {
                oldValue = node->title();
                found = true;
              } else if (propertyName == "type") {
                oldValue = node->nodeType();
                found = true;
              } else if (propertyName == "scriptPath") {
                oldValue = node->scriptPath();
                found = true;
              } else if (propertyName == "speaker") {
                oldValue = node->dialogueSpeaker();
                found = true;
              } else if (propertyName == "text") {
                oldValue = node->dialogueText();
                found = true;
              } else if (propertyName == "choices") {
                oldValue = node->choiceOptions().join("\n");
                found = true;
              }
            } else {
              return;
            }

            if (!found) {
              return;
            }

            if (oldValue == newValue) {
              return;
            }

            PropertyValue oldVariant = oldValue.toStdString();
            PropertyValue newVariant = newValue.toStdString();

            auto apply = [applyProperty, objectId,
                          propertyName](const PropertyValue &value,
                                        bool /*isUndo*/) {
              const auto *stringValue = std::get_if<std::string>(&value);
              const QString qValue =
                  stringValue ? QString::fromStdString(*stringValue) : QString();
              applyProperty(objectId, propertyName, qValue);
            };

            NMUndoManager::instance().pushCommand(new PropertyChangeCommand(
                objectId, propertyName, oldVariant, newVariant, apply));
          });

  connect(m_sceneViewPanel, &NMSceneViewPanel::objectPositionChanged, this,
          [this](const QString &objectId, const QPointF &pos) {
            if (!m_inspectorPanel) {
              return;
            }
            if (m_inspectorPanel->currentObjectId() != objectId) {
              return;
            }
            m_inspectorPanel->updatePropertyValue("position_x",
                                                  QString::number(pos.x()));
            m_inspectorPanel->updatePropertyValue("position_y",
                                                  QString::number(pos.y()));
          });

  connect(m_sceneViewPanel, &NMSceneViewPanel::objectTransformFinished, this,
          [this](const QString &objectId, const QPointF & /*oldPos*/,
                 const QPointF &newPos, qreal /*oldRotation*/,
                 qreal newRotation, qreal /*oldScaleX*/, qreal newScaleX,
                 qreal /*oldScaleY*/, qreal newScaleY) {
            if (!m_inspectorPanel) {
              return;
            }
            if (m_inspectorPanel->currentObjectId() != objectId) {
              return;
            }
            m_inspectorPanel->updatePropertyValue(
                "position_x", QString::number(newPos.x()));
            m_inspectorPanel->updatePropertyValue(
                "position_y", QString::number(newPos.y()));
            m_inspectorPanel->updatePropertyValue(
                "rotation", QString::number(newRotation));
            m_inspectorPanel->updatePropertyValue(
                "scale_x", QString::number(newScaleX));
            m_inspectorPanel->updatePropertyValue(
                "scale_y", QString::number(newScaleY));
          });

  // Connect Inspector Curve property to Curve Editor panel
  // When user clicks "Edit Curve..." button in Inspector, open Curve Editor
  connect(m_inspectorPanel, &NMInspectorPanel::propertyChanged, this,
          [this](const QString &objectId, const QString &propertyName,
                 const QString &newValue) {
            Q_UNUSED(objectId);
            // Check if this is a curve editor open request
            if (propertyName.endsWith(":openCurveEditor")) {
              // Extract the actual property name
              QString actualPropertyName =
                  propertyName.left(propertyName.length() -
                                    QString(":openCurveEditor").length());
              Q_UNUSED(actualPropertyName);

              // Show and raise the curve editor panel
              if (m_curveEditorPanel) {
                m_curveEditorPanel->setCurve(newValue);
                m_curveEditorPanel->show();
                m_curveEditorPanel->raise();
                m_curveEditorPanel->setFocus();
              }
            }
          });

  // Connect Curve Editor changes back to notify when curve data changes
  if (m_curveEditorPanel) {
    connect(m_curveEditorPanel, &NMCurveEditorPanel::curveChanged, this,
            [](const QString &curveId) {
              Q_UNUSED(curveId);
              // Could update timeline or other panels that use curves
              // Curve editing triggers document modification tracking
              // through the undo system when connected to property changes
            });
  }

  // Timeline Panel ↔ Scene View Panel synchronization
  if (m_timelinePanel && m_sceneViewPanel) {
    // When Timeline frame changes, update Scene View preview
    connect(m_timelinePanel, &NMTimelinePanel::frameChanged, this,
            [this](int frame) {
              if (!m_sceneViewPanel || !m_sceneViewPanel->isAnimationPreviewMode()) {
                return;
              }
              // Note: Animation adapter would apply interpolated values to scene objects
              // For now, we just trigger a viewport update to redraw with current state
              if (auto *view = m_sceneViewPanel->graphicsView()) {
                view->viewport()->update();
              }
            });

    // When Timeline playback state changes, update UI
    connect(m_timelinePanel, &NMTimelinePanel::playbackStateChanged, this,
            [this](bool playing) {
              if (playing) {
                setStatusMessage(tr("Animation preview playing..."));
              } else {
                setStatusMessage(tr("Animation preview stopped"), 2000);
              }
            });
  }
}

void NMMainWindow::handleNavigationRequest(const QString &locationString) {
  if (locationString.isEmpty()) {
    qWarning() << "[Navigation] Empty location string";
    return;
  }

  // Split location string to determine type
  QStringList parts = locationString.split(':');
  if (parts.size() < 2) {
    qWarning() << "[Navigation] Invalid location format:" << locationString;
    return;
  }

  QString typeStr = parts[0].trimmed();

  // Navigate to StoryGraph node
  if (typeStr.compare("StoryGraph", Qt::CaseInsensitive) == 0) {
    if (!m_storyGraphPanel) {
      qWarning() << "[Navigation] StoryGraph panel not available";
      return;
    }

    QString nodeId = parts[1].trimmed();
    if (nodeId.isEmpty()) {
      qWarning() << "[Navigation] Empty node ID";
      return;
    }

    qDebug() << "[Navigation] Navigating to StoryGraph node:" << nodeId;
    if (m_storyGraphPanel->navigateToNode(nodeId)) {
      qDebug() << "[Navigation] Successfully navigated to node:" << nodeId;
    } else {
      qWarning() << "[Navigation] Failed to navigate to node:" << nodeId;
      m_diagnosticsPanel->addDiagnosticWithLocation(
          "Warning", QString("Could not find node '%1'").arg(nodeId),
          locationString);
    }
    return;
  }

  // Navigate to Script file
  if (typeStr.compare("Script", Qt::CaseInsensitive) == 0) {
    if (!m_scriptEditorPanel) {
      qWarning() << "[Navigation] Script editor panel not available";
      return;
    }

    if (parts.size() < 2) {
      qWarning() << "[Navigation] Missing file path in Script location";
      return;
    }

    QString filePath = parts[1].trimmed();
    if (filePath.isEmpty()) {
      qWarning() << "[Navigation] Empty file path";
      return;
    }

    int lineNumber = -1;
    if (parts.size() >= 3) {
      bool ok = false;
      lineNumber = parts[2].trimmed().toInt(&ok);
      if (!ok || lineNumber < 1) {
        qWarning() << "[Navigation] Invalid line number:" << parts[2];
        lineNumber = -1;
      }
    }

    qDebug() << "[Navigation] Navigating to Script:" << filePath
             << "line:" << lineNumber;
    m_scriptEditorPanel->goToLocation(filePath, lineNumber);
    return;
  }

  qWarning() << "[Navigation] Unknown location type:" << typeStr;
}

} // namespace NovelMind::editor::qt
