#include "NovelMind/editor/mediators/panel_mediators.hpp"
#include "NovelMind/editor/project_integrity.hpp"
#include "NovelMind/editor/project_manager.hpp"
#include "NovelMind/editor/qt/nm_dialogs.hpp"
#include "NovelMind/editor/qt/nm_hotkeys_dialog.hpp"
#include "NovelMind/editor/qt/nm_main_window.hpp"
#include "NovelMind/editor/qt/nm_play_mode_controller.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"
#include "NovelMind/editor/qt/nm_undo_manager.hpp"
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
#include <QApplication>
#include <QDesktopServices>
#include <QDockWidget>
#include <QKeySequence>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QTextEdit>
#include <QUrl>
#include <cmath>

namespace NovelMind::editor::qt {

/**
 * @brief Setup all connections for the main window
 *
 * This function has been refactored from ~1,580 lines to ~80 lines
 * by using the EventBus pattern with mediators for panel communication.
 *
 * Previous approach (God Object pattern):
 * - 147 connect() calls for panel-to-panel communication
 * - Main window knew intimate details of every panel
 * - Adding new panel required modifying this function
 * - Circular dependencies between panels
 *
 * New approach (EventBus + Mediators):
 * - Panels publish/subscribe to events via EventBus
 * - Mediators coordinate complex multi-panel workflows
 * - Main window only handles menu/toolbar connections
 * - Adding new panel requires no changes here
 *
 * @see NovelMind::editor::mediators::PanelMediatorManager
 * @see NovelMind::editor::events::panel_events.hpp
 */
void NMMainWindow::setupConnections() {
  // =========================================================================
  // Menu Connections (kept in main window - UI framework responsibility)
  // =========================================================================

  // File menu
  connect(m_actionNewProject, &QAction::triggered, this,
          &NMMainWindow::newProjectRequested);
  connect(m_actionOpenProject, &QAction::triggered, this,
          &NMMainWindow::openProjectRequested);
  connect(m_actionSaveProject, &QAction::triggered, this,
          &NMMainWindow::saveProjectRequested);
  connect(m_actionExit, &QAction::triggered, this, &QMainWindow::close);

  // Project menu - validation
  connect(m_actionValidateProject, &QAction::triggered, this,
          &NMMainWindow::onValidateProject);

  // Edit menu - undo/redo
  connect(m_actionUndo, &QAction::triggered, &NMUndoManager::instance(),
          &NMUndoManager::undo);
  connect(m_actionRedo, &QAction::triggered, &NMUndoManager::instance(),
          &NMUndoManager::redo);
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
  m_actionUndo->setEnabled(NMUndoManager::instance().canUndo());
  m_actionRedo->setEnabled(NMUndoManager::instance().canRedo());

  // Edit menu - clipboard operations (delegated to focused widget)
  setupClipboardConnections();

  // Preferences
  connect(m_actionPreferences, &QAction::triggered, this,
          &NMMainWindow::showSettingsDialog);

  // View menu - panel toggles
  setupPanelToggleConnections();

  // Layout management
  setupLayoutConnections();

  // Play menu
  setupPlayConnections();

  // Help menu
  setupHelpConnections();

  // =========================================================================
  // Panel Mediators (replaces 1,400+ lines of panel-to-panel connections)
  // =========================================================================
  setupPanelMediators();
}

void NMMainWindow::setupClipboardConnections() {
  connect(m_actionCut, &QAction::triggered, this, []() {
    if (QWidget *focused = QApplication::focusWidget()) {
      if (auto *lineEdit = qobject_cast<QLineEdit *>(focused)) {
        lineEdit->cut();
      } else if (auto *textEdit = qobject_cast<QTextEdit *>(focused)) {
        textEdit->cut();
      } else if (auto *plainTextEdit = qobject_cast<QPlainTextEdit *>(focused)) {
        plainTextEdit->cut();
      }
    }
  });

  connect(m_actionCopy, &QAction::triggered, this, []() {
    if (QWidget *focused = QApplication::focusWidget()) {
      if (auto *lineEdit = qobject_cast<QLineEdit *>(focused)) {
        lineEdit->copy();
      } else if (auto *textEdit = qobject_cast<QTextEdit *>(focused)) {
        textEdit->copy();
      } else if (auto *plainTextEdit = qobject_cast<QPlainTextEdit *>(focused)) {
        plainTextEdit->copy();
      }
    }
  });

  connect(m_actionPaste, &QAction::triggered, this, []() {
    if (QWidget *focused = QApplication::focusWidget()) {
      if (auto *lineEdit = qobject_cast<QLineEdit *>(focused)) {
        lineEdit->paste();
      } else if (auto *textEdit = qobject_cast<QTextEdit *>(focused)) {
        textEdit->paste();
      } else if (auto *plainTextEdit = qobject_cast<QPlainTextEdit *>(focused)) {
        plainTextEdit->paste();
      }
    }
  });

  connect(m_actionDelete, &QAction::triggered, this, []() {
    if (QWidget *focused = QApplication::focusWidget()) {
      if (auto *lineEdit = qobject_cast<QLineEdit *>(focused)) {
        lineEdit->del();
      } else if (auto *textEdit = qobject_cast<QTextEdit *>(focused)) {
        textEdit->textCursor().removeSelectedText();
      } else if (auto *plainTextEdit = qobject_cast<QPlainTextEdit *>(focused)) {
        plainTextEdit->textCursor().removeSelectedText();
      }
    }
  });

  connect(m_actionSelectAll, &QAction::triggered, this, []() {
    if (QWidget *focused = QApplication::focusWidget()) {
      if (auto *lineEdit = qobject_cast<QLineEdit *>(focused)) {
        lineEdit->selectAll();
      } else if (auto *textEdit = qobject_cast<QTextEdit *>(focused)) {
        textEdit->selectAll();
      } else if (auto *plainTextEdit = qobject_cast<QPlainTextEdit *>(focused)) {
        plainTextEdit->selectAll();
      }
    }
  });
}

void NMMainWindow::setupPanelToggleConnections() {
  // Panel visibility toggles
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
  connect(m_actionToggleVoiceStudio, &QAction::toggled, this,
          [this](bool checked) { toggleVoiceStudioPanel(checked); });
  connect(m_actionToggleVoiceManager, &QAction::toggled, m_voiceManagerPanel,
          &QDockWidget::setVisible);
  connect(m_actionToggleAudioMixer, &QAction::toggled, this,
          [this](bool checked) { toggleAudioMixerPanel(checked); });
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

  // Visibility sync (panel → menu action)
  setupPanelVisibilitySync();
}

void NMMainWindow::setupPanelVisibilitySync() {
  auto syncVisibility = [](QDockWidget *panel, QAction *action) {
    QObject::connect(panel, &QDockWidget::visibilityChanged, action,
                     [action](bool visible) {
                       if (action->isChecked() != visible) {
                         QSignalBlocker blocker(action);
                         action->setChecked(visible);
                       }
                     });
  };

  syncVisibility(m_sceneViewPanel, m_actionToggleSceneView);
  syncVisibility(m_storyGraphPanel, m_actionToggleStoryGraph);
  syncVisibility(m_inspectorPanel, m_actionToggleInspector);
  syncVisibility(m_consolePanel, m_actionToggleConsole);
  syncVisibility(m_issuesPanel, m_actionToggleIssues);
  syncVisibility(m_diagnosticsPanel, m_actionToggleDiagnostics);
  syncVisibility(m_voiceStudioPanel, m_actionToggleVoiceStudio);
  syncVisibility(m_voiceManagerPanel, m_actionToggleVoiceManager);
  syncVisibility(m_audioMixerPanel, m_actionToggleAudioMixer);
  syncVisibility(m_localizationPanel, m_actionToggleLocalization);
  syncVisibility(m_timelinePanel, m_actionToggleTimeline);
  syncVisibility(m_curveEditorPanel, m_actionToggleCurveEditor);
  syncVisibility(m_buildSettingsPanel, m_actionToggleBuildSettings);
  syncVisibility(m_assetBrowserPanel, m_actionToggleAssetBrowser);
  syncVisibility(m_scenePalettePanel, m_actionToggleScenePalette);
  syncVisibility(m_hierarchyPanel, m_actionToggleHierarchy);
  syncVisibility(m_scriptEditorPanel, m_actionToggleScriptEditor);
  syncVisibility(m_scriptDocPanel, m_actionToggleScriptDocs);
  syncVisibility(m_debugOverlayPanel, m_actionToggleDebugOverlay);

  // Special: Clear story preview when leaving story graph
  connect(m_storyGraphPanel, &QDockWidget::visibilityChanged, this,
          [this](bool visible) {
            if (!visible && m_sceneViewPanel) {
              m_sceneViewPanel->clearStoryPreview();
            }
          });
}

void NMMainWindow::setupLayoutConnections() {
  connect(m_actionResetLayout, &QAction::triggered, this,
          &NMMainWindow::resetToDefaultLayout);

  // Workspace presets
  connect(m_actionLayoutDefault, &QAction::triggered, this,
          [this]() { applyWorkspacePreset(LayoutPreset::Default); });
  connect(m_actionLayoutStoryScript, &QAction::triggered, this,
          [this]() { applyWorkspacePreset(LayoutPreset::StoryScript); });
  connect(m_actionLayoutSceneAnimation, &QAction::triggered, this,
          [this]() { applyWorkspacePreset(LayoutPreset::SceneAnimation); });
  connect(m_actionLayoutAudioVoice, &QAction::triggered, this,
          [this]() { applyWorkspacePreset(LayoutPreset::AudioVoice); });

  // Legacy presets
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

  // UI scale
  auto &styleManager = NMStyleManager::instance();
  auto updateScaleActions = [this](double scale) {
    auto isNear = [](double value, double target) {
      return std::abs(value - target) < 0.01;
    };
    if (m_actionUiScaleCompact)
      m_actionUiScaleCompact->setChecked(isNear(scale, 0.9));
    if (m_actionUiScaleDefault)
      m_actionUiScaleDefault->setChecked(isNear(scale, 1.0));
    if (m_actionUiScaleComfort)
      m_actionUiScaleComfort->setChecked(isNear(scale, 1.1));
  };

  connect(m_actionUiScaleCompact, &QAction::triggered, this,
          []() { NMStyleManager::instance().setUiScale(0.9); });
  connect(m_actionUiScaleDefault, &QAction::triggered, this,
          []() { NMStyleManager::instance().setUiScale(1.0); });
  connect(m_actionUiScaleComfort, &QAction::triggered, this,
          []() { NMStyleManager::instance().setUiScale(1.1); });
  connect(m_actionUiScaleDown, &QAction::triggered, this, []() {
    auto &manager = NMStyleManager::instance();
    manager.setUiScale(manager.uiScale() - 0.1);
  });
  connect(m_actionUiScaleUp, &QAction::triggered, this, []() {
    auto &manager = NMStyleManager::instance();
    manager.setUiScale(manager.uiScale() + 0.1);
  });
  connect(m_actionUiScaleReset, &QAction::triggered, this,
          []() { NMStyleManager::instance().setUiScale(1.0); });
  connect(&styleManager, &NMStyleManager::scaleChanged, this, updateScaleActions);
  updateScaleActions(styleManager.uiScale());

  // Focus mode and dock options
  connect(m_actionFocusMode, &QAction::toggled, this,
          &NMMainWindow::toggleFocusMode);
  connect(m_actionFocusIncludeHierarchy, &QAction::toggled, this,
          [this](bool enabled) {
            m_focusIncludeHierarchy = enabled;
            if (m_focusModeEnabled)
              applyFocusModeLayout();
          });
  connect(m_actionLockLayout, &QAction::toggled, this,
          [this](bool locked) { applyDockLockState(locked); });
  connect(m_actionTabbedDockOnly, &QAction::toggled, this,
          [this](bool enabled) { applyTabbedDockMode(enabled); });
  connect(m_actionFloatAllowed, &QAction::toggled, this,
          [this](bool allowed) { applyFloatAllowed(allowed); });
}

void NMMainWindow::setupPlayConnections() {
  connect(m_actionPlay, &QAction::triggered, this,
          &NMMainWindow::playRequested);
  connect(m_actionStop, &QAction::triggered, this,
          &NMMainWindow::stopRequested);

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
          [this](NMPlayModeController::PlayMode) { updateStatusBarContext(); });
  connect(&playController, &NMPlayModeController::currentNodeChanged, this,
          [this](const QString &nodeId) {
            m_activeNodeId = nodeId;
            updateStatusBarContext();
          });
}

void NMMainWindow::setupHelpConnections() {
  connect(m_actionAbout, &QAction::triggered, this,
          &NMMainWindow::showAboutDialog);
  connect(m_actionDocumentation, &QAction::triggered, []() {
    QDesktopServices::openUrl(
        QUrl("https://github.com/VisageDvachevsky/StoryGraph"));
  });
  connect(m_actionHotkeys, &QAction::triggered, this,
          &NMMainWindow::showHotkeysDialog);
}

void NMMainWindow::setupPanelMediators() {
  // Create and initialize the mediator manager
  // This replaces ~1,400 lines of direct panel-to-panel connections
  m_mediatorManager = std::make_unique<mediators::PanelMediatorManager>(this);
  m_mediatorManager->initialize(
      m_sceneViewPanel,
      m_storyGraphPanel,
      m_sceneDialogueGraphPanel,
      m_inspectorPanel,
      m_hierarchyPanel,
      m_scriptEditorPanel,
      m_scriptDocPanel,
      m_timelinePanel,
      m_curveEditorPanel,
      m_voiceStudioPanel,
      m_voiceManagerPanel,
      m_diagnosticsPanel,
      m_issuesPanel);

  qDebug() << "[NMMainWindow] Panel mediators initialized - replaces ~1,400 lines"
           << "of direct connections with EventBus pattern";
}

void NMMainWindow::toggleVoiceStudioPanel(bool checked) {
  if (checked) {
    if (!m_voiceStudioPanel->isVisible() || m_voiceStudioPanel->isFloating()) {
      QList<QDockWidget *> docks = this->findChildren<QDockWidget *>();
      if (!docks.contains(m_voiceStudioPanel) ||
          m_voiceStudioPanel->parent() != this) {
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

  // Issue #117: Workflow Mode Enforcement - Connect PlayToolbar's source mode
  // change to update panel read-only states
  connect(m_playToolbarPanel, &NMPlayToolbarPanel::playbackSourceModeChanged,
          this, [this](PlaybackSourceMode mode) {
            qDebug() << "[WorkflowMode] Playback source mode changed to:"
                     << static_cast<int>(mode);

            switch (mode) {
            case PlaybackSourceMode::Script:
              // Script Mode: Story Graph is read-only, Scripts are editable
              if (m_storyGraphPanel) {
                m_storyGraphPanel->setReadOnly(true, tr("Script Mode"));
              }
              if (m_scriptEditorPanel) {
                m_scriptEditorPanel->setReadOnly(false);
              }
              setStatusMessage(
                  tr("Script Mode: NMScript files are authoritative"), 3000);
              break;

            case PlaybackSourceMode::Graph:
              // Graph Mode: Scripts are read-only, Story Graph is editable
              if (m_storyGraphPanel) {
                m_storyGraphPanel->setReadOnly(false);
              }
              if (m_scriptEditorPanel) {
                m_scriptEditorPanel->setReadOnly(true, tr("Graph Mode"));
              }
              setStatusMessage(tr("Graph Mode: Story Graph is authoritative"),
                               3000);
              break;

            case PlaybackSourceMode::Mixed:
              // Mixed Mode: Both panels are editable (conflicts resolved at
              // runtime)
              if (m_storyGraphPanel) {
                m_storyGraphPanel->setReadOnly(false);
              }
              if (m_scriptEditorPanel) {
                m_scriptEditorPanel->setReadOnly(false);
              }
              setStatusMessage(
                  tr("Mixed Mode: Both sources are editable, Graph wins on "
                     "conflicts"),
                  3000);
              break;
            }
          });

  // Issue #117: Connect Script Editor sync to Story Graph
  connect(m_scriptEditorPanel, &NMScriptEditorPanel::syncToGraphRequested, this,
          [this](const QString &sceneName, const QString &speaker,
                 const QString &dialogueText, const QStringList &choices) {
            if (!m_storyGraphPanel) {
              return;
            }

            // Find the node corresponding to this scene
            auto *node = m_storyGraphPanel->findNodeByIdString(sceneName);
            if (!node) {
              qDebug() << "[WorkflowMode] No graph node found for scene:"
                       << sceneName;
              return;
            }

            // Update node properties from script
            m_storyGraphPanel->applyNodePropertyChange(sceneName, "speaker",
                                                       speaker);
            m_storyGraphPanel->applyNodePropertyChange(sceneName, "text",
                                                       dialogueText);
            if (!choices.isEmpty()) {
              m_storyGraphPanel->applyNodePropertyChange(sceneName, "choices",
                                                         choices.join("\n"));
            }

            qDebug() << "[WorkflowMode] Synced script scene to graph:"
                     << sceneName;
          });

  // Timeline Panel ↔ Scene View Panel synchronization
  if (m_timelinePanel && m_sceneViewPanel) {
    // When Timeline frame changes, update Scene View preview
    connect(m_timelinePanel, &NMTimelinePanel::frameChanged, this,
            [this]([[maybe_unused]] int frame) {
              if (!m_sceneViewPanel ||
                  !m_sceneViewPanel->isAnimationPreviewMode()) {
                return;
              }
              // Note: Animation adapter would apply interpolated values to
              // scene objects For now, we just trigger a viewport update to
              // redraw with current state
              if (auto *view = m_sceneViewPanel->graphicsView()) {
                view->viewport()->update();
              }
            });

    // Connect asset updated signal for when Voice Studio updates manifest
    connect(m_voiceStudioPanel, &NMVoiceStudioPanel::assetUpdated, this,
            [this](const QString &lineId,
                   [[maybe_unused]] const QString &filePath) {
              // Notify Voice Manager panel of the file status change
              m_voiceManagerPanel->onFileStatusChanged(lineId, "en");
            });
  }
}

void NMMainWindow::toggleAudioMixerPanel(bool checked) {
  if (checked) {
    if (!m_audioMixerPanel->isVisible() || m_audioMixerPanel->isFloating()) {
      QList<QDockWidget *> docks = this->findChildren<QDockWidget *>();
      if (!docks.contains(m_audioMixerPanel) ||
          m_audioMixerPanel->parent() != this) {
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
}

void NMMainWindow::onValidateProject() {
  auto &pm = ProjectManager::instance();
  if (!pm.hasOpenProject()) {
    setStatusMessage(tr("No project is open"), 3000);
    return;
  }

  setStatusMessage(tr("Running project validation..."));
  m_diagnosticsPanel->clearDiagnostics();

  ProjectIntegrityChecker checker;
  checker.setProjectPath(pm.getProjectPath());

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

  m_diagnosticsPanel->show();
  m_diagnosticsPanel->raise();

  if (report.passed) {
    setStatusMessage(tr("Validation passed - no critical issues found"), 5000);
  } else {
    setStatusMessage(
        tr("Validation found %1 error(s) and %2 warning(s)")
            .arg(report.summary.errorCount + report.summary.criticalCount)
            .arg(report.summary.warningCount),
        5000);
  }
}

void NMMainWindow::showHotkeysDialog() {
  auto shortcutText = [](QAction *action) {
    if (!action)
      return QString();
    return action->shortcut().toString(QKeySequence::NativeText);
  };

  QList<NMHotkeyEntry> entries;
  auto addActionEntry = [&entries, &shortcutText](
                            const QString &section, const QString &actionName,
                            QAction *action, const QString &notes = QString()) {
    const QString shortcut = shortcutText(action);
    NMHotkeyEntry entry;
    entry.id = action ? action->objectName() : actionName;
    if (entry.id.isEmpty())
      entry.id = actionName;
    entry.section = section;
    entry.action = actionName;
    entry.shortcut = shortcut;
    entry.defaultShortcut = shortcut;
    entry.notes = notes;
    entries.push_back(entry);
  };

  // File menu
  addActionEntry(tr("File"), tr("New Project"), m_actionNewProject);
  addActionEntry(tr("File"), tr("Open Project"), m_actionOpenProject);
  addActionEntry(tr("File"), tr("Save Project"), m_actionSaveProject);
  addActionEntry(tr("File"), tr("Save Project As"), m_actionSaveProjectAs);
  addActionEntry(tr("File"), tr("Close Project"), m_actionCloseProject);
  addActionEntry(tr("File"), tr("Quit"), m_actionExit);

  // Edit menu
  addActionEntry(tr("Edit"), tr("Undo"), m_actionUndo);
  addActionEntry(tr("Edit"), tr("Redo"), m_actionRedo);
  addActionEntry(tr("Edit"), tr("Cut"), m_actionCut);
  addActionEntry(tr("Edit"), tr("Copy"), m_actionCopy);
  addActionEntry(tr("Edit"), tr("Paste"), m_actionPaste);
  addActionEntry(tr("Edit"), tr("Delete"), m_actionDelete);
  addActionEntry(tr("Edit"), tr("Select All"), m_actionSelectAll);

  // Play menu
  addActionEntry(tr("Play"), tr("Play"), m_actionPlay);
  addActionEntry(tr("Play"), tr("Pause"), m_actionPause);
  addActionEntry(tr("Play"), tr("Stop"), m_actionStop);
  addActionEntry(tr("Play"), tr("Step Frame"), m_actionStepFrame);
  addActionEntry(tr("Play"), tr("Save State"), m_actionSaveState);
  addActionEntry(tr("Play"), tr("Load State"), m_actionLoadState);
  addActionEntry(tr("Play"), tr("Auto Save"), m_actionAutoSaveState);
  addActionEntry(tr("Play"), tr("Auto Load"), m_actionAutoLoadState);

  // Audio/Voice
  addActionEntry(tr("Audio / Voice"), tr("Voice Studio"), m_actionToggleVoiceStudio,
                 tr("Record and edit voice lines with waveform visualization"));

  // Workspaces
  addActionEntry(tr("Workspaces"), tr("Default"), m_actionLayoutDefault);
  addActionEntry(tr("Workspaces"), tr("Story / Script"), m_actionLayoutStoryScript);
  addActionEntry(tr("Workspaces"), tr("Scene / Animation"), m_actionLayoutSceneAnimation);
  addActionEntry(tr("Workspaces"), tr("Audio / Voice"), m_actionLayoutAudioVoice);

  // Legacy workspaces
  addActionEntry(tr("Workspaces"), tr("Story (Legacy)"), m_actionLayoutStory);
  addActionEntry(tr("Workspaces"), tr("Scene (Legacy)"), m_actionLayoutScene);
  addActionEntry(tr("Workspaces"), tr("Script (Legacy)"), m_actionLayoutScript);
  addActionEntry(tr("Workspaces"), tr("Developer (Legacy)"), m_actionLayoutDeveloper);
  addActionEntry(tr("Workspaces"), tr("Compact (Legacy)"), m_actionLayoutCompact);

  // Layout
  addActionEntry(tr("Layout"), tr("Focus Mode"), m_actionFocusMode);
  addActionEntry(tr("Layout"), tr("Lock Layout"), m_actionLockLayout);
  addActionEntry(tr("Layout"), tr("Tabbed Dock Only"), m_actionTabbedDockOnly);

  // UI Scale
  addActionEntry(tr("UI Scale"), tr("Scale Down"), m_actionUiScaleDown);
  addActionEntry(tr("UI Scale"), tr("Scale Up"), m_actionUiScaleUp);
  addActionEntry(tr("UI Scale"), tr("Scale Reset"), m_actionUiScaleReset);

  // Static entries for context-sensitive shortcuts
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
  addStaticEntry(tr("Script Editor"), tr("Command Palette"), tr("Ctrl+Shift+P"),
                 tr("Open command palette for quick actions"));
  addStaticEntry(tr("Script Editor"), tr("Save Script"), tr("Ctrl+S"),
                 tr("Save current script tab"));
  addStaticEntry(tr("Script Editor"), tr("Save All Scripts"), tr("Ctrl+Shift+S"),
                 tr("Save all open script tabs"));
  addStaticEntry(tr("Script Editor"), tr("Insert Snippet"), tr("Ctrl+J"),
                 tr("Insert code snippet"));
  addStaticEntry(tr("Script Editor"), tr("Go to Symbol"), tr("Ctrl+Shift+O"),
                 tr("Navigate to symbols in current script"));
  addStaticEntry(tr("Script Editor"), tr("Format Document"), tr("Ctrl+Shift+F"),
                 tr("Auto-format current script"));
  addStaticEntry(tr("Script Editor"), tr("Find"), tr("Ctrl+F"),
                 tr("Find text in current script"));
  addStaticEntry(tr("Script Editor"), tr("Replace"), tr("Ctrl+H"),
                 tr("Find and replace text"));
  addStaticEntry(tr("Script Editor"), tr("Toggle Comment"), tr("Ctrl+/"),
                 tr("Comment/uncomment selected lines"));
  addStaticEntry(tr("Script Editor"), tr("Go to Definition"), tr("F12"),
                 tr("Jump to symbol definition"));
  addStaticEntry(tr("Script Editor"), tr("Find References"), tr("Shift+F12"),
                 tr("Find all references to symbol"));
  addStaticEntry(tr("Script Editor"), tr("Navigate to Graph"), tr("Ctrl+Shift+G"),
                 tr("Navigate to corresponding graph node"));
  addStaticEntry(tr("Script Editor"), tr("Go to Line"), tr("Ctrl+G"),
                 tr("Jump to specific line number"));
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
}

void NMMainWindow::handleNavigationRequest(const QString &locationString) {
  // Delegate to workflow mediator via EventBus
  events::NavigationRequestedEvent event;
  event.locationString = locationString;
  EventBus::instance().publish(event);
}

} // namespace NovelMind::editor::qt
