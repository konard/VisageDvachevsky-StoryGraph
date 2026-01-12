#include <NovelMind/editor/qt/nm_icon_manager.hpp>
#include <NovelMind/editor/qt/nm_scrollable_toolbar.hpp>
#include <NovelMind/editor/qt/panels/nm_play_toolbar_panel.hpp>
#include <QSignalBlocker>
#include <QToolBar>
#include <QVBoxLayout>

namespace NovelMind::editor::qt {

NMPlayToolbarPanel::NMPlayToolbarPanel(QWidget* parent) : NMDockPanel("Play Controls", parent) {
  setupUI();
}

void NMPlayToolbarPanel::onInitialize() {
  NMDockPanel::onInitialize();

  auto& controller = NMPlayModeController::instance();

  // Connect to controller signals
  connect(&controller, &NMPlayModeController::playModeChanged, this,
          &NMPlayToolbarPanel::onPlayModeChanged);
  connect(&controller, &NMPlayModeController::currentNodeChanged, this,
          &NMPlayToolbarPanel::onCurrentNodeChanged);
  connect(&controller, &NMPlayModeController::breakpointHit, this,
          &NMPlayToolbarPanel::onBreakpointHit);

  m_statusTimer.setSingleShot(true);
  connect(&m_statusTimer, &QTimer::timeout, this, &NMPlayToolbarPanel::updateStatusLabel);

  // Initialize playback source mode from project settings
  // Use QSignalBlocker to prevent signal loops during programmatic
  // initialization
  auto& pm = ProjectManager::instance();
  if (pm.hasOpenProject()) {
    const auto& meta = pm.getMetadata();
    int modeIndex = static_cast<int>(meta.playbackSourceMode);
    if (m_sourceCombo && modeIndex >= 0 && modeIndex < m_sourceCombo->count()) {
      // Block signals to prevent onSourceModeChanged from triggering during
      // initialization
      QSignalBlocker blocker(m_sourceCombo);
      m_sourceCombo->setCurrentIndex(modeIndex);
    }
  }
  // Trigger initial indicator update (direct call, not via signal)
  if (m_sourceCombo) {
    // Update indicator without emitting signal since we're just initializing
    updateSourceIndicator(m_sourceCombo->currentIndex());
  }

  updateButtonStates();
}

void NMPlayToolbarPanel::onShutdown() {
  NMDockPanel::onShutdown();

  // Stop playback on shutdown
  if (m_currentMode != NMPlayModeController::Stopped) {
    NMPlayModeController::instance().stop();
  }
}

void NMPlayToolbarPanel::onUpdate(double deltaTime) {
  NMDockPanel::onUpdate(deltaTime);
  // Real-time updates if needed
}

void NMPlayToolbarPanel::setupUI() {
  // Create content widget
  auto* contentWidget = new QWidget;
  auto* layout = new QVBoxLayout(contentWidget);
  layout->setContentsMargins(4, 4, 4, 4);
  layout->setSpacing(4);

  // Wrap the toolbar in a scrollable container for adaptive layout
  // This ensures all play controls remain accessible when panel is small
  m_scrollableToolBar = new NMScrollableToolBar(this);
  auto* toolbar = m_scrollableToolBar->toolbar();
  m_scrollableToolBar->setToolBarObjectName("PlayControlBar");
  m_scrollableToolBar->setIconSize(QSize(24, 24));
  toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

  auto& iconMgr = NMIconManager::instance();

  // Play button
  m_playButton = new QPushButton;
  m_playButton->setObjectName("PlayButton");
  m_playButton->setIcon(iconMgr.getIcon("play", 24));
  m_playButton->setText("Play");
  m_playButton->setToolTip("Start or resume playback (F5)");
  m_playButton->setShortcut(Qt::Key_F5);
  m_playButton->setFlat(true);
  connect(m_playButton, &QPushButton::clicked, []() { NMPlayModeController::instance().play(); });

  // Pause button
  m_pauseButton = new QPushButton;
  m_pauseButton->setObjectName("PauseButton");
  m_pauseButton->setIcon(iconMgr.getIcon("pause", 24));
  m_pauseButton->setText("Pause");
  m_pauseButton->setToolTip("Pause playback");
  m_pauseButton->setFlat(true);
  connect(m_pauseButton, &QPushButton::clicked, []() { NMPlayModeController::instance().pause(); });

  // Stop button
  m_stopButton = new QPushButton;
  m_stopButton->setObjectName("StopButton");
  m_stopButton->setIcon(iconMgr.getIcon("stop", 24));
  m_stopButton->setText("Stop");
  m_stopButton->setToolTip("Stop playback and reset (Shift+F5)");
  m_stopButton->setShortcut(Qt::SHIFT | Qt::Key_F5);
  m_stopButton->setFlat(true);
  connect(m_stopButton, &QPushButton::clicked, []() { NMPlayModeController::instance().stop(); });

  // Step Forward button
  m_stepButton = new QPushButton;
  m_stepButton->setObjectName("StepButton");
  m_stepButton->setIcon(iconMgr.getIcon("step-forward", 24));
  m_stepButton->setText("Step");
  m_stepButton->setToolTip("Execute one instruction (F10)");
  m_stepButton->setShortcut(Qt::Key_F10);
  m_stepButton->setFlat(true);
  connect(m_stepButton, &QPushButton::clicked,
          []() { NMPlayModeController::instance().stepForward(); });

  // Skip Mode Toggle button
  m_skipButton = new QPushButton;
  m_skipButton->setObjectName("SkipButton");
  m_skipButton->setIcon(iconMgr.getIcon("fast-forward", 24));
  m_skipButton->setText("Skip");
  m_skipButton->setToolTip("Toggle skip mode (fast forward through dialogue)");
  m_skipButton->setCheckable(true);
  m_skipButton->setChecked(false);
  m_skipButton->setFlat(true);
  connect(m_skipButton, &QPushButton::toggled, [this](bool checked) {
    auto& controller = NMPlayModeController::instance();
    if (controller.isRuntimeLoaded()) {
      auto* runtime = controller.getScriptRuntime();
      if (runtime) {
        runtime->setSkipMode(checked);
      }
    }
    if (checked) {
      showTransientStatus("Skip mode enabled", "#2196f3");
    } else {
      showTransientStatus("Skip mode disabled", "#a0a0a0");
    }
  });

  // Add buttons to toolbar
  toolbar->addWidget(m_playButton);
  toolbar->addWidget(m_pauseButton);
  toolbar->addWidget(m_stopButton);
  toolbar->addSeparator();
  toolbar->addWidget(m_stepButton);
  toolbar->addWidget(m_skipButton);
  toolbar->addSeparator();

  QLabel* slotLabel = new QLabel("Slot");
  slotLabel->setObjectName("SaveSlotLabel");
  toolbar->addWidget(slotLabel);

  m_slotSpin = new QSpinBox;
  m_slotSpin->setObjectName("SaveSlotSpin");
  m_slotSpin->setRange(0, 99);
  m_slotSpin->setValue(0);
  m_slotSpin->setToolTip("Save slot index");
  toolbar->addWidget(m_slotSpin);

  m_saveButton = new QPushButton;
  m_saveButton->setObjectName("SaveButton");
  m_saveButton->setIcon(iconMgr.getIcon("file-save", 20));
  m_saveButton->setText("Save");
  m_saveButton->setToolTip("Save runtime state to slot");
  m_saveButton->setFlat(true);
  connect(m_saveButton, &QPushButton::clicked, [this]() {
    const int slot = m_slotSpin ? m_slotSpin->value() : 0;
    if (NMPlayModeController::instance().saveSlot(slot)) {
      showTransientStatus(QString("Saved slot %1").arg(slot), "#4caf50");
    } else {
      showTransientStatus(QString("Save failed (%1)").arg(slot), "#f44336");
    }
  });

  m_loadButton = new QPushButton;
  m_loadButton->setObjectName("LoadButton");
  m_loadButton->setIcon(iconMgr.getIcon("file-open", 20));
  m_loadButton->setText("Load");
  m_loadButton->setToolTip("Load runtime state from slot");
  m_loadButton->setFlat(true);
  connect(m_loadButton, &QPushButton::clicked, [this]() {
    const int slot = m_slotSpin ? m_slotSpin->value() : 0;
    if (NMPlayModeController::instance().loadSlot(slot)) {
      showTransientStatus(QString("Loaded slot %1").arg(slot), "#4caf50");
    } else {
      showTransientStatus(QString("Load failed (%1)").arg(slot), "#f44336");
    }
  });

  m_autoSaveButton = new QPushButton;
  m_autoSaveButton->setObjectName("AutoSaveButton");
  m_autoSaveButton->setIcon(iconMgr.getIcon("file-save", 18));
  m_autoSaveButton->setText("Auto Save");
  m_autoSaveButton->setToolTip("Save runtime state to auto-save");
  m_autoSaveButton->setFlat(true);
  connect(m_autoSaveButton, &QPushButton::clicked, [this]() {
    if (NMPlayModeController::instance().saveAuto()) {
      showTransientStatus("Auto-saved", "#4caf50");
      updateButtonStates();
    } else {
      showTransientStatus("Auto-save failed", "#f44336");
    }
  });

  m_autoLoadButton = new QPushButton;
  m_autoLoadButton->setObjectName("AutoLoadButton");
  m_autoLoadButton->setIcon(iconMgr.getIcon("file-open", 18));
  m_autoLoadButton->setText("Auto Load");
  m_autoLoadButton->setToolTip("Load runtime state from auto-save");
  m_autoLoadButton->setFlat(true);
  connect(m_autoLoadButton, &QPushButton::clicked, [this]() {
    if (NMPlayModeController::instance().loadAuto()) {
      showTransientStatus("Auto-loaded", "#4caf50");
    } else {
      showTransientStatus("Auto-load failed", "#f44336");
    }
  });

  toolbar->addWidget(m_saveButton);
  toolbar->addWidget(m_loadButton);
  toolbar->addSeparator();
  toolbar->addWidget(m_autoSaveButton);
  toolbar->addWidget(m_autoLoadButton);
  toolbar->addSeparator();

  // Playback source selector (issue #82)
  QLabel* sourceLabel = new QLabel(tr("Source:"));
  sourceLabel->setObjectName("PlaybackSourceLabel");
  sourceLabel->setToolTip(tr("Playback source determines where story content comes from"));
  toolbar->addWidget(sourceLabel);

  m_sourceCombo = new QComboBox;
  m_sourceCombo->setObjectName("PlaybackSourceCombo");
  m_sourceCombo->addItem(tr("Script"), static_cast<int>(PlaybackSourceMode::Script));
  m_sourceCombo->addItem(tr("Graph"), static_cast<int>(PlaybackSourceMode::Graph));
  m_sourceCombo->addItem(tr("Mixed"), static_cast<int>(PlaybackSourceMode::Mixed));
  m_sourceCombo->setToolTip(tr("Script: NMScript files are authoritative\n"
                               "Graph: Story Graph visual data is authoritative\n"
                               "Mixed: Script + Graph overrides (Graph wins on conflicts)"));
  m_sourceCombo->setCurrentIndex(0);
  connect(m_sourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &NMPlayToolbarPanel::onSourceModeChanged);
  toolbar->addWidget(m_sourceCombo);

  // Source indicator showing current active source
  m_sourceIndicator = new QLabel;
  m_sourceIndicator->setObjectName("PlaybackSourceIndicator");
  m_sourceIndicator->setToolTip(tr("Shows which data source is currently being used for playback"));
  toolbar->addWidget(m_sourceIndicator);
  toolbar->addSeparator();

  // Status label
  m_statusLabel = new QLabel("Stopped");
  m_statusLabel->setObjectName("PlayStatusLabel");
  toolbar->addWidget(m_statusLabel);

  layout->addWidget(m_scrollableToolBar);
  layout->addStretch();

  // Use setContentWidget instead of setLayout
  setContentWidget(contentWidget);

  updateStatusLabel();
}

void NMPlayToolbarPanel::updateButtonStates() {
  const bool isPlaying = (m_currentMode == NMPlayModeController::Playing);
  const bool isPaused = (m_currentMode == NMPlayModeController::Paused);
  const bool isStopped = (m_currentMode == NMPlayModeController::Stopped);
  auto& controller = NMPlayModeController::instance();
  const bool runtimeReady = controller.isRuntimeLoaded();
  const bool hasAutoSave = controller.hasAutoSave();

  m_playButton->setEnabled(!isPlaying);
  m_pauseButton->setEnabled(isPlaying);
  m_stopButton->setEnabled(!isStopped);
  m_stepButton->setEnabled(isStopped || isPaused);
  if (m_saveButton) {
    m_saveButton->setEnabled(runtimeReady);
  }
  if (m_loadButton) {
    m_loadButton->setEnabled(runtimeReady);
  }
  if (m_autoSaveButton) {
    m_autoSaveButton->setEnabled(runtimeReady);
  }
  if (m_autoLoadButton) {
    m_autoLoadButton->setEnabled(runtimeReady && hasAutoSave);
  }
}

void NMPlayToolbarPanel::updateStatusLabel() {
  QString status;
  QString color;
  const QString bg = "#1d232b";
  const QString border = "#3a424e";

  switch (m_currentMode) {
  case NMPlayModeController::Playing:
    status = "Playing";
    color = "#4caf50"; // Green
    if (!m_currentNodeId.isEmpty()) {
      status += QString(" - %1").arg(m_currentNodeId);
    }
    break;
  case NMPlayModeController::Paused:
    status = "Paused";
    color = "#ff9800"; // Orange
    if (!m_currentNodeId.isEmpty()) {
      status += QString(" at %1").arg(m_currentNodeId);
    }
    break;
  case NMPlayModeController::Stopped:
  default:
    status = "Stopped";
    color = "#a0a0a0"; // Gray
    break;
  }

  m_statusLabel->setText(status);
  m_statusLabel->setStyleSheet(
      QString("QLabel { color: %1; background-color: %2; border: 1px solid %3; "
              "border-radius: 10px; padding: 3px 8px; font-weight: 600; }")
          .arg(color, bg, border));
}

void NMPlayToolbarPanel::onPlayModeChanged(NMPlayModeController::PlayMode mode) {
  m_currentMode = mode;
  updateButtonStates();
  updateStatusLabel();
}

void NMPlayToolbarPanel::onCurrentNodeChanged(const QString& nodeId) {
  m_currentNodeId = nodeId;
  updateStatusLabel();
}

void NMPlayToolbarPanel::onBreakpointHit(const QString& nodeId) {
  // Visual feedback for breakpoint hit
  m_statusLabel->setText(QString("Breakpoint at %1").arg(nodeId));
  m_statusLabel->setStyleSheet(
      "QLabel { color: #f44336; background-color: #2b1d1d; border: 1px solid "
      "#5a2b2b; border-radius: 10px; padding: 3px 8px; font-weight: 600; }");
}

void NMPlayToolbarPanel::showTransientStatus(const QString& text, const QString& color) {
  if (!m_statusLabel) {
    return;
  }
  const QString bg = "#1d232b";
  const QString border = "#3a424e";
  m_statusLabel->setText(text);
  m_statusLabel->setStyleSheet(
      QString("QLabel { color: %1; background-color: %2; border: 1px solid %3; "
              "border-radius: 10px; padding: 3px 8px; font-weight: 600; }")
          .arg(color, bg, border));
  m_statusTimer.start(2000);
}

void NMPlayToolbarPanel::updateSourceIndicator(int index) {
  if (!m_sourceCombo || !m_sourceIndicator) {
    return;
  }

  auto mode = static_cast<PlaybackSourceMode>(m_sourceCombo->itemData(index).toInt());

  // Update indicator style based on selected mode
  QString indicatorText;
  QString indicatorColor;
  QString indicatorBg;

  switch (mode) {
  case PlaybackSourceMode::Script:
    indicatorText = "NMS";
    indicatorColor = "#2196f3"; // Blue
    indicatorBg = "#1d2b32";
    break;
  case PlaybackSourceMode::Graph:
    indicatorText = "GRAPH";
    indicatorColor = "#4caf50"; // Green
    indicatorBg = "#1d2b1d";
    break;
  case PlaybackSourceMode::Mixed:
    indicatorText = "MIXED";
    indicatorColor = "#ff9800"; // Orange
    indicatorBg = "#2b251d";
    break;
  }

  m_sourceIndicator->setText(indicatorText);
  m_sourceIndicator->setStyleSheet(
      QString("QLabel { color: %1; background-color: %2; border: 1px solid %1; "
              "border-radius: 8px; padding: 2px 6px; font-weight: bold; "
              "font-size: 10px; }")
          .arg(indicatorColor, indicatorBg));
}

void NMPlayToolbarPanel::onSourceModeChanged(int index) {
  // Update the visual indicator first
  updateSourceIndicator(index);

  if (!m_sourceCombo) {
    return;
  }

  auto mode = static_cast<PlaybackSourceMode>(m_sourceCombo->itemData(index).toInt());

  // Update project settings
  auto& pm = ProjectManager::instance();
  if (pm.hasOpenProject()) {
    auto meta = pm.getMetadata();
    meta.playbackSourceMode = mode;
    pm.setMetadata(meta);
  }

  // Emit signal for other panels to react
  emit playbackSourceModeChanged(mode);

  // Show transient status
  QString indicatorColor;
  QString modeStr;
  switch (mode) {
  case PlaybackSourceMode::Script:
    modeStr = tr("Script (NMScript files)");
    indicatorColor = "#2196f3";
    break;
  case PlaybackSourceMode::Graph:
    modeStr = tr("Graph (Story Graph visual data)");
    indicatorColor = "#4caf50";
    break;
  case PlaybackSourceMode::Mixed:
    modeStr = tr("Mixed (Script + Graph overrides)");
    indicatorColor = "#ff9800";
    break;
  }
  showTransientStatus(tr("Playback source: %1").arg(modeStr), indicatorColor);
}

void NMPlayToolbarPanel::setSourceMode(PlaybackSourceMode mode) {
  // Update the combo box to reflect the new mode from external source
  // (e.g., from Project Settings panel - issue #125)
  if (!m_sourceCombo) {
    return;
  }

  // Find the index for this mode
  int targetIndex = -1;
  for (int i = 0; i < m_sourceCombo->count(); ++i) {
    if (m_sourceCombo->itemData(i).toInt() == static_cast<int>(mode)) {
      targetIndex = i;
      break;
    }
  }

  if (targetIndex >= 0 && targetIndex != m_sourceCombo->currentIndex()) {
    // Block signals to prevent feedback loop
    QSignalBlocker blocker(m_sourceCombo);
    m_sourceCombo->setCurrentIndex(targetIndex);

    // Update the indicator directly (since we blocked the signal)
    // This mirrors the logic from onSourceModeChanged
    QString indicatorText;
    QString indicatorColor;
    QString indicatorBg;

    switch (mode) {
    case PlaybackSourceMode::Script:
      indicatorText = "NMS";
      indicatorColor = "#2196f3"; // Blue
      indicatorBg = "#1d2b32";
      break;
    case PlaybackSourceMode::Graph:
      indicatorText = "GRAPH";
      indicatorColor = "#4caf50"; // Green
      indicatorBg = "#1d2b1d";
      break;
    case PlaybackSourceMode::Mixed:
      indicatorText = "MIXED";
      indicatorColor = "#ff9800"; // Orange
      indicatorBg = "#2b251d";
      break;
    }

    if (m_sourceIndicator) {
      m_sourceIndicator->setText(indicatorText);
      m_sourceIndicator->setStyleSheet(
          QString("QLabel { color: %1; background-color: %2; border: 1px solid "
                  "%1; "
                  "border-radius: 8px; padding: 2px 6px; font-weight: bold; "
                  "font-size: 10px; }")
              .arg(indicatorColor, indicatorBg));
    }
  }
}

} // namespace NovelMind::editor::qt
