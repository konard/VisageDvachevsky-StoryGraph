/**
 * @file nm_audio_mixer_panel.cpp
 * @brief Audio Mixer panel implementation
 *
 * Provides a comprehensive mixing console for audio preview:
 * - Transport controls (play/pause/stop/loop/seek)
 * - Master and per-channel volume controls
 * - Mute/Solo per channel
 * - Crossfade controls
 * - Auto-ducking configuration
 */

#include "NovelMind/editor/qt/panels/nm_audio_mixer_panel.hpp"
#include "NovelMind/editor/qt/nm_dialogs.hpp"
#include "NovelMind/core/logger.hpp"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

namespace NovelMind::editor::qt {

// ============================================================================
// Channel Colors (following UX spec)
// ============================================================================

namespace {
// Channel color mapping based on UX spec
const QColor CHANNEL_COLORS[] = {
    QColor(255, 215, 0),   // Master - Gold
    QColor(156, 39, 176),  // Music - Purple
    QColor(33, 150, 243),  // Sound - Blue
    QColor(76, 175, 80),   // Voice - Green
    QColor(0, 150, 136),   // Ambient - Teal
    QColor(255, 152, 0),   // UI - Orange
    QColor(158, 158, 158)  // Reserved - Gray
};

const char* CHANNEL_NAMES[] = {
    "MASTER",
    "Music",
    "Sound",
    "Voice",
    "Ambient",
    "UI",
    "Reserved"
};

// Default volumes per channel
const int DEFAULT_VOLUMES[] = {
    100,  // Master
    80,   // Music
    100,  // Sound
    100,  // Voice
    70,   // Ambient
    100,  // UI
    100   // Reserved
};

constexpr int NUM_CHANNELS = 6;  // Excluding Master
} // namespace

// ============================================================================
// NMAudioMixerPanel Implementation
// ============================================================================

NMAudioMixerPanel::NMAudioMixerPanel(QWidget *parent)
    : NMDockPanel("Audio Mixer", parent) {
  setupUI();
}

NMAudioMixerPanel::~NMAudioMixerPanel() {
  onShutdown();
}

void NMAudioMixerPanel::onInitialize() {
  NOVELMIND_LOG_INFO("Audio Mixer Panel initialized");

  // Initialize preview audio manager
  m_previewAudioManager = std::make_unique<audio::AudioManager>();
  auto result = m_previewAudioManager->initialize();
  if (result.isError()) {
    NOVELMIND_LOG_ERROR("Failed to initialize preview audio manager");
  }

  // Set up position update timer
  m_positionTimer = new QTimer(this);
  connect(m_positionTimer, &QTimer::timeout, this,
          &NMAudioMixerPanel::onUpdatePosition);
  m_positionTimer->start(100);  // Update every 100ms

  // Apply default channel volumes
  applyChannelVolumes();
}

void NMAudioMixerPanel::onShutdown() {
  if (m_positionTimer) {
    m_positionTimer->stop();
  }

  if (m_previewAudioManager) {
    m_previewAudioManager->stopAll();
    m_previewAudioManager->shutdown();
  }
}

void NMAudioMixerPanel::onUpdate(double) {
  // Position updates happen via timer
}

void NMAudioMixerPanel::setSelectedAudioAsset(const QString &assetPath) {
  m_currentAudioAsset = assetPath;
  if (m_currentTrackLabel) {
    QString displayName = assetPath;
    if (displayName.isEmpty()) {
      displayName = tr("No track selected");
    } else {
      // Extract filename from path
      qsizetype lastSlash = displayName.lastIndexOf('/');
      if (lastSlash >= 0) {
        displayName = displayName.mid(lastSlash + 1);
      }
    }
    m_currentTrackLabel->setText(QString::fromUtf8("🎵 ") + displayName);
  }
  resetPlaybackUI();
  emit audioAssetSelected(assetPath);
}

// ============================================================================
// Slot Implementations - Transport Controls
// ============================================================================

void NMAudioMixerPanel::onPlayClicked() {
  if (m_currentAudioAsset.isEmpty()) {
    setPlaybackError(tr("No audio file selected. Use Browse to select a file."));
    return;
  }

  if (m_isPaused && m_previewAudioManager) {
    // Resume playback
    m_previewAudioManager->resumeAll();
    m_isPaused = false;
    m_isPlaying = true;
  } else if (!m_isPlaying && m_previewAudioManager) {
    // Start new playback
    audio::MusicConfig config;
    config.volume = static_cast<f32>(m_masterVolumeSlider->value()) / 100.0f;
    config.loop = m_loopCheckBox->isChecked();
    config.fadeInDuration = 0.0f;

    m_currentMusicHandle =
        m_previewAudioManager->playMusic(m_currentAudioAsset.toStdString(), config);

    if (!m_currentMusicHandle.isValid()) {
      setPlaybackError(tr("Failed to play audio file."));
      return;
    }

    m_isPlaying = true;
    m_isPaused = false;

    // Duration tracking not available via AudioManager API, so use a placeholder
    // In practice, position updates will handle the seek bar
    m_currentDuration = 0.0f;
    if (m_durationLabel) {
      m_durationLabel->setText(formatTime(m_currentDuration));
    }
    if (m_seekSlider) {
      m_seekSlider->setEnabled(false);  // Seek not fully supported
    }
  }

  updatePlaybackState();
}

void NMAudioMixerPanel::onPauseClicked() {
  if (m_isPlaying && m_previewAudioManager) {
    m_previewAudioManager->pauseAll();
    m_isPaused = true;
    m_isPlaying = false;
  }
  updatePlaybackState();
}

void NMAudioMixerPanel::onStopClicked() {
  if (m_previewAudioManager) {
    m_previewAudioManager->stopAll();
  }
  m_isPlaying = false;
  m_isPaused = false;
  m_currentPosition = 0.0f;
  updatePlaybackState();
  updatePositionDisplay();
}

void NMAudioMixerPanel::onLoopToggled(bool checked) {
  // Loop is set when playing music via MusicConfig, not per-handle
  // This checkbox will be respected on next play
  (void)checked;
}

void NMAudioMixerPanel::onSeekSliderMoved(int value) {
  m_isSeeking = true;
  m_currentPosition = static_cast<f32>(value) / 1000.0f;
  if (m_positionLabel) {
    m_positionLabel->setText(formatTime(m_currentPosition));
  }
}

void NMAudioMixerPanel::onSeekSliderReleased() {
  if (m_previewAudioManager && m_currentMusicHandle.isValid()) {
    m_previewAudioManager->seekMusic(m_currentPosition);
  }
  m_isSeeking = false;
}

// ============================================================================
// Slot Implementations - Crossfade Controls
// ============================================================================

void NMAudioMixerPanel::onCrossfadeDurationChanged(double value) {
  m_crossfadeDuration = static_cast<f32>(value);
}

void NMAudioMixerPanel::onCrossfadeToClicked() {
  if (m_nextCrossfadeAsset.isEmpty()) {
    // Open file dialog to select next track
    QString filePath = NMFileDialog::getOpenFileName(
        this, tr("Select Next Track"), QString(),
        tr("Audio Files (*.wav *.ogg *.mp3 *.flac);;All Files (*)"));

    if (filePath.isEmpty()) {
      return;
    }
    m_nextCrossfadeAsset = filePath;
  }

  if (m_previewAudioManager && !m_nextCrossfadeAsset.isEmpty()) {
    audio::MusicConfig config;
    config.volume = static_cast<f32>(m_masterVolumeSlider->value()) / 100.0f;
    config.loop = m_loopCheckBox->isChecked();
    config.fadeInDuration = m_crossfadeDuration / 1000.0f;
    config.crossfadeDuration = m_crossfadeDuration / 1000.0f;

    // Use crossfadeMusic API which handles fading between tracks
    m_currentMusicHandle =
        m_previewAudioManager->crossfadeMusic(m_nextCrossfadeAsset.toStdString(),
                                              m_crossfadeDuration / 1000.0f, config);
    m_currentAudioAsset = m_nextCrossfadeAsset;
    m_nextCrossfadeAsset.clear();

    setSelectedAudioAsset(m_currentAudioAsset);
    m_isPlaying = true;
    m_isPaused = false;
    updatePlaybackState();
  }
}

// ============================================================================
// Slot Implementations - Ducking Controls
// ============================================================================

void NMAudioMixerPanel::onDuckingEnabledToggled(bool checked) {
  m_duckingEnabled = checked;
  if (m_previewAudioManager) {
    m_previewAudioManager->setAutoDuckingEnabled(checked);
  }
}

void NMAudioMixerPanel::onDuckAmountChanged(double value) {
  m_duckAmount = static_cast<f32>(value) / 100.0f;
  if (m_previewAudioManager) {
    m_previewAudioManager->setDuckingParams(m_duckAmount, m_duckFadeDuration);
  }
}

void NMAudioMixerPanel::onDuckAttackChanged(double value) {
  // AudioManager uses a single fadeDuration for both attack and release
  // Store attack value for UI but use it as the fade duration
  f32 attackSec = static_cast<f32>(value) / 1000.0f;
  m_duckFadeDuration = attackSec;
  if (m_previewAudioManager) {
    m_previewAudioManager->setDuckingParams(m_duckAmount, m_duckFadeDuration);
  }
}

void NMAudioMixerPanel::onDuckReleaseChanged(double value) {
  // AudioManager uses a single fadeDuration for both attack and release
  m_duckFadeDuration = static_cast<f32>(value) / 1000.0f;
  if (m_previewAudioManager) {
    m_previewAudioManager->setDuckingParams(m_duckAmount, m_duckFadeDuration);
  }
}

// ============================================================================
// Slot Implementations - Mixer Controls
// ============================================================================

void NMAudioMixerPanel::onMasterVolumeChanged(int value) {
  if (m_masterVolumeLabel) {
    m_masterVolumeLabel->setText(QString("%1%").arg(value));
  }
  if (m_previewAudioManager) {
    m_previewAudioManager->setMasterVolume(static_cast<f32>(value) / 100.0f);
  }
}

void NMAudioMixerPanel::onChannelVolumeChanged(int value) {
  // Find which slider sent this signal
  auto *slider = qobject_cast<QSlider *>(sender());
  if (!slider) return;

  for (size_t i = 0; i < m_channelControls.size(); ++i) {
    if (m_channelControls[i].volumeSlider == slider) {
      if (m_channelControls[i].volumeLabel) {
        m_channelControls[i].volumeLabel->setText(QString("%1%").arg(value));
      }
      if (m_previewAudioManager) {
        m_previewAudioManager->setChannelVolume(
            m_channelControls[i].channel, static_cast<f32>(value) / 100.0f);
      }
      break;
    }
  }
}

void NMAudioMixerPanel::onChannelMuteToggled(bool checked) {
  auto *button = qobject_cast<QPushButton *>(sender());
  if (!button) return;

  for (size_t i = 0; i < m_channelControls.size(); ++i) {
    if (m_channelControls[i].muteButton == button) {
      if (m_previewAudioManager) {
        m_previewAudioManager->setChannelMuted(m_channelControls[i].channel,
                                               checked);
      }
      // Update button style
      if (checked) {
        button->setStyleSheet(
            "QPushButton { background-color: #F44336; color: white; font-weight: bold; }");
      } else {
        button->setStyleSheet("");
      }
      break;
    }
  }
}

void NMAudioMixerPanel::onChannelSoloToggled(bool checked) {
  auto *button = qobject_cast<QPushButton *>(sender());
  if (!button) return;

  int clickedIndex = -1;
  for (size_t i = 0; i < m_channelControls.size(); ++i) {
    if (m_channelControls[i].soloButton == button) {
      clickedIndex = static_cast<int>(i);
      break;
    }
  }

  if (clickedIndex < 0) return;

  if (checked) {
    m_soloChannelIndex = clickedIndex;
  } else {
    m_soloChannelIndex = -1;
  }

  updateSoloState();
}

void NMAudioMixerPanel::onBrowseAudioClicked() {
  QString filePath = NMFileDialog::getOpenFileName(
      this, tr("Select Audio File"), QString(),
      tr("Audio Files (*.wav *.ogg *.mp3 *.flac);;All Files (*)"));

  if (!filePath.isEmpty()) {
    setSelectedAudioAsset(filePath);
    onAssetSelected(filePath);
  }
}

void NMAudioMixerPanel::onAssetSelected(const QString &assetPath) {
  setSelectedAudioAsset(assetPath);
}

void NMAudioMixerPanel::onUpdatePosition() {
  if (!m_isPlaying || m_isSeeking || !m_previewAudioManager) {
    return;
  }

  if (m_currentMusicHandle.isValid()) {
    m_currentPosition = m_previewAudioManager->getMusicPosition();

    // Check if playback finished using isPlaying
    if (!m_previewAudioManager->isPlaying(m_currentMusicHandle)) {
      m_isPlaying = false;
      m_isPaused = false;
      if (!m_loopCheckBox->isChecked()) {
        m_currentPosition = 0.0f;
      }
      updatePlaybackState();
    }
  }

  updatePositionDisplay();
}

// ============================================================================
// UI Setup
// ============================================================================

void NMAudioMixerPanel::setupUI() {
  QWidget *contentWidget = new QWidget(this);
  auto *mainLayout = new QVBoxLayout(contentWidget);
  mainLayout->setContentsMargins(8, 8, 8, 8);
  mainLayout->setSpacing(8);

  // Preview Section
  setupMusicPreviewControls(contentWidget);

  // Mixer Section
  setupMixerControls(contentWidget);

  // Crossfade Section (collapsible)
  setupCrossfadeControls(contentWidget);

  // Auto-Ducking Section (collapsible)
  setupDuckingControls(contentWidget);

  // Add stretch at bottom
  mainLayout->addStretch();

  setWidget(contentWidget);
}

void NMAudioMixerPanel::setupMusicPreviewControls(QWidget *parent) {
  m_previewGroup = new QGroupBox(tr("Now Playing"), parent);
  auto *layout = new QVBoxLayout(m_previewGroup);

  // Current track label
  m_currentTrackLabel = new QLabel(tr("No track selected"), m_previewGroup);
  m_currentTrackLabel->setStyleSheet("font-weight: bold; padding: 4px;");
  m_currentTrackLabel->setWordWrap(true);
  layout->addWidget(m_currentTrackLabel);

  // Transport controls row
  auto *transportLayout = new QHBoxLayout();

  m_playBtn = new QPushButton(tr("▶ Play"), m_previewGroup);
  m_playBtn->setToolTip(tr("Start playback (Space)"));
  connect(m_playBtn, &QPushButton::clicked, this,
          &NMAudioMixerPanel::onPlayClicked);
  transportLayout->addWidget(m_playBtn);

  m_pauseBtn = new QPushButton(tr("⏸ Pause"), m_previewGroup);
  m_pauseBtn->setToolTip(tr("Pause playback (Space)"));
  connect(m_pauseBtn, &QPushButton::clicked, this,
          &NMAudioMixerPanel::onPauseClicked);
  transportLayout->addWidget(m_pauseBtn);

  m_stopBtn = new QPushButton(tr("⏹ Stop"), m_previewGroup);
  m_stopBtn->setToolTip(tr("Stop playback (Escape)"));
  connect(m_stopBtn, &QPushButton::clicked, this,
          &NMAudioMixerPanel::onStopClicked);
  transportLayout->addWidget(m_stopBtn);

  transportLayout->addStretch();

  m_loopCheckBox = new QCheckBox(tr("🔁 Loop"), m_previewGroup);
  m_loopCheckBox->setToolTip(tr("Loop playback (L)"));
  connect(m_loopCheckBox, &QCheckBox::toggled, this,
          &NMAudioMixerPanel::onLoopToggled);
  transportLayout->addWidget(m_loopCheckBox);

  layout->addLayout(transportLayout);

  // Seek slider
  m_seekSlider = new QSlider(Qt::Horizontal, m_previewGroup);
  m_seekSlider->setRange(0, 1000);
  m_seekSlider->setValue(0);
  m_seekSlider->setEnabled(false);
  connect(m_seekSlider, &QSlider::sliderMoved, this,
          &NMAudioMixerPanel::onSeekSliderMoved);
  connect(m_seekSlider, &QSlider::sliderReleased, this,
          &NMAudioMixerPanel::onSeekSliderReleased);
  layout->addWidget(m_seekSlider);

  // Position/Duration row
  auto *posLayout = new QHBoxLayout();
  m_positionLabel = new QLabel("00:00.000", m_previewGroup);
  m_positionLabel->setStyleSheet("font-family: monospace;");
  posLayout->addWidget(m_positionLabel);

  posLayout->addStretch();

  m_durationLabel = new QLabel("00:00.000", m_previewGroup);
  m_durationLabel->setStyleSheet("font-family: monospace;");
  posLayout->addWidget(m_durationLabel);

  layout->addLayout(posLayout);

  // Browse button
  m_browseBtn = new QPushButton(tr("📂 Browse Audio..."), m_previewGroup);
  m_browseBtn->setToolTip(tr("Select an audio file to preview"));
  connect(m_browseBtn, &QPushButton::clicked, this,
          &NMAudioMixerPanel::onBrowseAudioClicked);
  layout->addWidget(m_browseBtn);

  if (auto *mainLayout = qobject_cast<QVBoxLayout *>(parent->layout())) {
    mainLayout->addWidget(m_previewGroup);
  }
}

void NMAudioMixerPanel::setupMixerControls(QWidget *parent) {
  m_mixerGroup = new QGroupBox(tr("Mixer"), parent);
  auto *layout = new QHBoxLayout(m_mixerGroup);
  layout->setSpacing(4);

  // Master channel
  auto *masterWidget = new QWidget(m_mixerGroup);
  auto *masterLayout = new QVBoxLayout(masterWidget);
  masterLayout->setSpacing(2);
  masterLayout->setContentsMargins(4, 4, 4, 4);

  auto *masterLabel = new QLabel("MASTER", masterWidget);
  masterLabel->setAlignment(Qt::AlignCenter);
  masterLabel->setStyleSheet(
      QString("font-weight: bold; color: %1;").arg(CHANNEL_COLORS[0].name()));
  masterLayout->addWidget(masterLabel);

  m_masterVolumeSlider = new QSlider(Qt::Vertical, masterWidget);
  m_masterVolumeSlider->setRange(0, 100);
  m_masterVolumeSlider->setValue(DEFAULT_VOLUMES[0]);
  m_masterVolumeSlider->setMinimumHeight(120);
  m_masterVolumeSlider->setToolTip(tr("Master volume"));
  connect(m_masterVolumeSlider, &QSlider::valueChanged, this,
          &NMAudioMixerPanel::onMasterVolumeChanged);
  masterLayout->addWidget(m_masterVolumeSlider, 1, Qt::AlignHCenter);

  m_masterVolumeLabel = new QLabel("100%", masterWidget);
  m_masterVolumeLabel->setAlignment(Qt::AlignCenter);
  masterLayout->addWidget(m_masterVolumeLabel);

  layout->addWidget(masterWidget);

  // Separator
  auto *separator = new QFrame(m_mixerGroup);
  separator->setFrameShape(QFrame::VLine);
  separator->setFrameShadow(QFrame::Sunken);
  layout->addWidget(separator);

  // Channel strips
  m_channelControls.clear();
  for (int i = 0; i < NUM_CHANNELS; ++i) {
    auto *channelWidget = new QWidget(m_mixerGroup);
    auto *channelLayout = new QVBoxLayout(channelWidget);
    channelLayout->setSpacing(2);
    channelLayout->setContentsMargins(4, 4, 4, 4);

    ChannelControl ctrl;
    ctrl.channel = static_cast<audio::AudioChannel>(i);

    // Channel label
    ctrl.nameLabel = new QLabel(CHANNEL_NAMES[i + 1], channelWidget);
    ctrl.nameLabel->setAlignment(Qt::AlignCenter);
    ctrl.nameLabel->setStyleSheet(
        QString("font-weight: bold; color: %1;").arg(CHANNEL_COLORS[i + 1].name()));
    channelLayout->addWidget(ctrl.nameLabel);

    // Volume slider
    ctrl.volumeSlider = new QSlider(Qt::Vertical, channelWidget);
    ctrl.volumeSlider->setRange(0, 100);
    ctrl.volumeSlider->setValue(DEFAULT_VOLUMES[i + 1]);
    ctrl.volumeSlider->setMinimumHeight(120);
    ctrl.volumeSlider->setToolTip(tr("%1 channel volume").arg(CHANNEL_NAMES[i + 1]));
    connect(ctrl.volumeSlider, &QSlider::valueChanged, this,
            &NMAudioMixerPanel::onChannelVolumeChanged);
    channelLayout->addWidget(ctrl.volumeSlider, 1, Qt::AlignHCenter);

    // Volume label
    ctrl.volumeLabel = new QLabel(QString("%1%").arg(DEFAULT_VOLUMES[i + 1]),
                                  channelWidget);
    ctrl.volumeLabel->setAlignment(Qt::AlignCenter);
    channelLayout->addWidget(ctrl.volumeLabel);

    // Mute/Solo buttons
    auto *btnLayout = new QHBoxLayout();

    ctrl.muteButton = new QPushButton("M", channelWidget);
    ctrl.muteButton->setCheckable(true);
    ctrl.muteButton->setFixedSize(24, 24);
    ctrl.muteButton->setToolTip(tr("Mute %1 channel").arg(CHANNEL_NAMES[i + 1]));
    connect(ctrl.muteButton, &QPushButton::toggled, this,
            &NMAudioMixerPanel::onChannelMuteToggled);
    btnLayout->addWidget(ctrl.muteButton);

    ctrl.soloButton = new QPushButton("S", channelWidget);
    ctrl.soloButton->setCheckable(true);
    ctrl.soloButton->setFixedSize(24, 24);
    ctrl.soloButton->setToolTip(tr("Solo %1 channel").arg(CHANNEL_NAMES[i + 1]));
    connect(ctrl.soloButton, &QPushButton::toggled, this,
            &NMAudioMixerPanel::onChannelSoloToggled);
    btnLayout->addWidget(ctrl.soloButton);

    channelLayout->addLayout(btnLayout);

    layout->addWidget(channelWidget);
    m_channelControls.push_back(ctrl);
  }

  if (auto *mainLayout = qobject_cast<QVBoxLayout *>(parent->layout())) {
    mainLayout->addWidget(m_mixerGroup);
  }
}

void NMAudioMixerPanel::setupCrossfadeControls(QWidget *parent) {
  m_crossfadeGroup = new QGroupBox(tr("▼ Crossfade"), parent);
  m_crossfadeGroup->setCheckable(true);
  m_crossfadeGroup->setChecked(false);
  auto *layout = new QHBoxLayout(m_crossfadeGroup);

  layout->addWidget(new QLabel(tr("Duration:"), m_crossfadeGroup));

  m_crossfadeDurationSpin = new QDoubleSpinBox(m_crossfadeGroup);
  m_crossfadeDurationSpin->setRange(0.0, 10000.0);
  m_crossfadeDurationSpin->setValue(1000.0);
  m_crossfadeDurationSpin->setSuffix(" ms");
  m_crossfadeDurationSpin->setToolTip(tr("Crossfade duration in milliseconds"));
  connect(m_crossfadeDurationSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &NMAudioMixerPanel::onCrossfadeDurationChanged);
  layout->addWidget(m_crossfadeDurationSpin);

  layout->addStretch();

  m_crossfadeBtn = new QPushButton(tr("⟳ Crossfade To..."), m_crossfadeGroup);
  m_crossfadeBtn->setToolTip(tr("Start crossfade to another track"));
  connect(m_crossfadeBtn, &QPushButton::clicked, this,
          &NMAudioMixerPanel::onCrossfadeToClicked);
  layout->addWidget(m_crossfadeBtn);

  if (auto *mainLayout = qobject_cast<QVBoxLayout *>(parent->layout())) {
    mainLayout->addWidget(m_crossfadeGroup);
  }
}

void NMAudioMixerPanel::setupDuckingControls(QWidget *parent) {
  m_duckingGroup = new QGroupBox(tr("▼ Auto-Ducking"), parent);
  m_duckingGroup->setCheckable(true);
  m_duckingGroup->setChecked(false);
  auto *layout = new QVBoxLayout(m_duckingGroup);

  // Enable checkbox
  m_duckingEnabledCheckBox = new QCheckBox(
      tr("Enable Auto-Ducking (music ducks when voice plays)"), m_duckingGroup);
  m_duckingEnabledCheckBox->setChecked(true);
  m_duckingEnabledCheckBox->setToolTip(
      tr("Automatically reduce music volume when voice plays"));
  connect(m_duckingEnabledCheckBox, &QCheckBox::toggled, this,
          &NMAudioMixerPanel::onDuckingEnabledToggled);
  layout->addWidget(m_duckingEnabledCheckBox);

  // Parameters row
  auto *paramsLayout = new QHBoxLayout();

  paramsLayout->addWidget(new QLabel(tr("Duck Amount:"), m_duckingGroup));
  m_duckAmountSpin = new QDoubleSpinBox(m_duckingGroup);
  m_duckAmountSpin->setRange(0.0, 100.0);
  m_duckAmountSpin->setValue(30.0);
  m_duckAmountSpin->setSuffix(" %");
  m_duckAmountSpin->setToolTip(tr("How much to reduce music volume when voice plays"));
  connect(m_duckAmountSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &NMAudioMixerPanel::onDuckAmountChanged);
  paramsLayout->addWidget(m_duckAmountSpin);

  paramsLayout->addWidget(new QLabel(tr("Attack:"), m_duckingGroup));
  m_duckAttackSpin = new QDoubleSpinBox(m_duckingGroup);
  m_duckAttackSpin->setRange(0.0, 1000.0);
  m_duckAttackSpin->setValue(200.0);
  m_duckAttackSpin->setSuffix(" ms");
  m_duckAttackSpin->setToolTip(tr("How quickly music fades down when voice starts"));
  connect(m_duckAttackSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &NMAudioMixerPanel::onDuckAttackChanged);
  paramsLayout->addWidget(m_duckAttackSpin);

  paramsLayout->addWidget(new QLabel(tr("Release:"), m_duckingGroup));
  m_duckReleaseSpin = new QDoubleSpinBox(m_duckingGroup);
  m_duckReleaseSpin->setRange(0.0, 2000.0);
  m_duckReleaseSpin->setValue(200.0);
  m_duckReleaseSpin->setSuffix(" ms");
  m_duckReleaseSpin->setToolTip(tr("How quickly music returns when voice stops"));
  connect(m_duckReleaseSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &NMAudioMixerPanel::onDuckReleaseChanged);
  paramsLayout->addWidget(m_duckReleaseSpin);

  layout->addLayout(paramsLayout);

  if (auto *mainLayout = qobject_cast<QVBoxLayout *>(parent->layout())) {
    mainLayout->addWidget(m_duckingGroup);
  }
}

void NMAudioMixerPanel::setupAssetBrowser(QWidget *) {
  // Asset browser integration is handled via onBrowseAudioClicked and signals
}

// ============================================================================
// Helper Methods
// ============================================================================

void NMAudioMixerPanel::updatePlaybackState() {
  if (m_playBtn) {
    m_playBtn->setEnabled(!m_isPlaying || m_isPaused);
  }
  if (m_pauseBtn) {
    m_pauseBtn->setEnabled(m_isPlaying && !m_isPaused);
  }
  if (m_stopBtn) {
    m_stopBtn->setEnabled(m_isPlaying || m_isPaused);
  }
}

void NMAudioMixerPanel::updatePositionDisplay() {
  if (m_positionLabel) {
    m_positionLabel->setText(formatTime(m_currentPosition));
  }
  if (m_seekSlider && !m_isSeeking) {
    m_seekSlider->setValue(static_cast<int>(m_currentPosition * 1000));
  }
}

void NMAudioMixerPanel::resetPlaybackUI() {
  m_isPlaying = false;
  m_isPaused = false;
  m_currentPosition = 0.0f;
  m_currentDuration = 0.0f;

  if (m_seekSlider) {
    m_seekSlider->setValue(0);
    m_seekSlider->setEnabled(false);
  }
  if (m_positionLabel) {
    m_positionLabel->setText("00:00.000");
  }
  if (m_durationLabel) {
    m_durationLabel->setText("00:00.000");
  }

  updatePlaybackState();
}

void NMAudioMixerPanel::setPlaybackError(const QString &message) {
  if (m_currentTrackLabel) {
    m_currentTrackLabel->setText(QString::fromUtf8("❌ ") + message);
  }
  emit playbackError(message);
}

QString NMAudioMixerPanel::formatTime(f32 seconds) const {
  int totalMs = static_cast<int>(seconds * 1000);
  int minutes = totalMs / 60000;
  int secs = (totalMs % 60000) / 1000;
  int ms = totalMs % 1000;
  return QString("%1:%2.%3")
      .arg(minutes, 2, 10, QChar('0'))
      .arg(secs, 2, 10, QChar('0'))
      .arg(ms, 3, 10, QChar('0'));
}

void NMAudioMixerPanel::applyChannelVolumes() {
  if (!m_previewAudioManager) return;

  // Apply master volume
  if (m_masterVolumeSlider) {
    m_previewAudioManager->setMasterVolume(
        static_cast<f32>(m_masterVolumeSlider->value()) / 100.0f);
  }

  // Apply channel volumes
  for (const auto &ctrl : m_channelControls) {
    if (ctrl.volumeSlider) {
      m_previewAudioManager->setChannelVolume(
          ctrl.channel, static_cast<f32>(ctrl.volumeSlider->value()) / 100.0f);
    }
  }
}

void NMAudioMixerPanel::updateSoloState() {
  if (!m_previewAudioManager) return;

  // If no solo is active, unmute all channels
  if (m_soloChannelIndex < 0) {
    for (size_t i = 0; i < m_channelControls.size(); ++i) {
      auto &ctrl = m_channelControls[i];
      // Only unmute if the mute button is not checked
      if (!ctrl.muteButton->isChecked()) {
        m_previewAudioManager->setChannelMuted(ctrl.channel, false);
      }
      ctrl.soloButton->setStyleSheet("");
    }
  } else {
    // Solo is active - mute all except the soloed channel
    for (size_t i = 0; i < m_channelControls.size(); ++i) {
      auto &ctrl = m_channelControls[i];
      bool isSoloed = (static_cast<int>(i) == m_soloChannelIndex);

      if (isSoloed) {
        m_previewAudioManager->setChannelMuted(ctrl.channel, false);
        ctrl.soloButton->setStyleSheet(
            "QPushButton { background-color: #FFC107; color: black; font-weight: bold; }");
      } else {
        m_previewAudioManager->setChannelMuted(ctrl.channel, true);
        ctrl.soloButton->setChecked(false);
        ctrl.soloButton->setStyleSheet("");
      }
    }
  }
}

} // namespace NovelMind::editor::qt
