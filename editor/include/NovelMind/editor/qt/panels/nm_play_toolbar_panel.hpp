#ifndef NOVELMIND_EDITOR_NM_PLAY_TOOLBAR_PANEL_HPP
#define NOVELMIND_EDITOR_NM_PLAY_TOOLBAR_PANEL_HPP

#include <NovelMind/editor/project_manager.hpp>
#include <NovelMind/editor/qt/nm_dock_panel.hpp>
#include <NovelMind/editor/qt/nm_play_mode_controller.hpp>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>

namespace NovelMind::editor::qt {

/**
 * @brief Toolbar panel for Play-In-Editor controls
 *
 * Provides Play/Pause/Stop/Step buttons and status display.
 * Typically docked in the main toolbar or as a floating panel.
 *
 * Signal Flow:
 * - Outgoing: playbackSourceModeChanged(PlaybackSourceMode) - emitted when user
 *   changes the playback source mode via combo box selection
 * - Uses QSignalBlocker during initialization to prevent feedback loops with
 *   NMProjectSettingsPanel when loading settings from project metadata
 */
class NMPlayToolbarPanel : public NMDockPanel {
  Q_OBJECT

public:
  explicit NMPlayToolbarPanel(QWidget *parent = nullptr);
  ~NMPlayToolbarPanel() override = default;

  void onInitialize() override;
  void onShutdown() override;
  void onUpdate(double deltaTime) override;

signals:
  /// Emitted when playback source mode is changed by the user
  void playbackSourceModeChanged(PlaybackSourceMode mode);

private slots:
  void onPlayModeChanged(NMPlayModeController::PlayMode mode);
  void onCurrentNodeChanged(const QString &nodeId);
  void onBreakpointHit(const QString &nodeId);
  void onSourceModeChanged(int index);

private:
  void setupUI();
  void updateButtonStates();
  void updateStatusLabel();
  void showTransientStatus(const QString &text, const QString &color);
  /// Updates source indicator UI without emitting signals or modifying project
  /// settings
  void updateSourceIndicator(int index);

  // UI Elements
  class NMScrollableToolBar *m_scrollableToolBar = nullptr;
  QPushButton *m_playButton = nullptr;
  QPushButton *m_pauseButton = nullptr;
  QPushButton *m_stopButton = nullptr;
  QPushButton *m_stepButton = nullptr;
  QPushButton *m_skipButton = nullptr;
  QPushButton *m_saveButton = nullptr;
  QPushButton *m_loadButton = nullptr;
  QPushButton *m_autoSaveButton = nullptr;
  QPushButton *m_autoLoadButton = nullptr;
  QSpinBox *m_slotSpin = nullptr;
  QLabel *m_statusLabel = nullptr;
  QTimer m_statusTimer;

  // Playback source controls (issue #82)
  QComboBox *m_sourceCombo = nullptr;
  QLabel *m_sourceIndicator = nullptr;

  // State
  NMPlayModeController::PlayMode m_currentMode = NMPlayModeController::Stopped;
  QString m_currentNodeId;
};

} // namespace NovelMind::editor::qt

#endif // NOVELMIND_EDITOR_NM_PLAY_TOOLBAR_PANEL_HPP
