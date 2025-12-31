/**
 * @file nm_recording_studio_panel.cpp
 * @brief Recording Studio panel implementation
 *
 * Uses IAudioPlayer interface for take playback (issue #150)
 */

#include "NovelMind/editor/qt/panels/nm_recording_studio_panel.hpp"
#include "NovelMind/audio/audio_recorder.hpp"
#include "NovelMind/audio/voice_manifest.hpp"
#include "NovelMind/core/logger.hpp"
#include "NovelMind/editor/interfaces/IAudioPlayer.hpp"
#include "NovelMind/editor/interfaces/QtAudioPlayer.hpp"
#include "NovelMind/editor/interfaces/ServiceLocator.hpp"

#include <QApplication>
#include <QAudioOutput>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMediaPlayer>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSplitter>
#include <QTextEdit>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

namespace NovelMind::editor::qt {

// ============================================================================
// VUMeterWidget Implementation
// ============================================================================

VUMeterWidget::VUMeterWidget(QWidget *parent) : QWidget(parent) {
  setMinimumSize(200, 30);
  setMaximumHeight(40);
}

void VUMeterWidget::setLevel(float rmsDb, float peakDb, bool clipping) {
  m_rmsDb = rmsDb;
  m_peakDb = peakDb;
  m_clipping = clipping;
  update();
}

void VUMeterWidget::reset() {
  m_rmsDb = -60.0f;
  m_peakDb = -60.0f;
  m_clipping = false;
  update();
}

void VUMeterWidget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  const int w = width();
  const int h = height();
  const int margin = 2;
  const int barHeight = (h - margin * 3) / 2;
  const float widthAvailable = static_cast<float>(w - margin * 2);

  // Background
  painter.fillRect(rect(), QColor(30, 30, 30));

  // Convert dB to normalized value (-60dB to 0dB)
  auto dbToNorm = [](float db) {
    return std::clamp((db + 60.0f) / 60.0f, 0.0f, 1.0f);
  };

  float rmsNorm = dbToNorm(m_rmsDb);
  float peakNorm = dbToNorm(m_peakDb);

  // RMS bar (green/yellow/red gradient)
  int rmsWidth = static_cast<int>(rmsNorm * widthAvailable);

  QLinearGradient gradient(0, 0, w, 0);
  gradient.setColorAt(0.0, QColor(40, 180, 40));  // Green
  gradient.setColorAt(0.7, QColor(200, 200, 40)); // Yellow
  gradient.setColorAt(0.9, QColor(200, 100, 40)); // Orange
  gradient.setColorAt(1.0, QColor(200, 40, 40));  // Red

  painter.fillRect(margin, margin, rmsWidth, barHeight, gradient);

  // Peak indicator (thin line)
  int peakPos = margin + static_cast<int>(peakNorm * widthAvailable);
  painter.setPen(QPen(Qt::white, 2));
  painter.drawLine(peakPos, margin, peakPos, margin + barHeight);

  // Second bar for visual reference (background scale)
  int scaleY = margin * 2 + barHeight;
  painter.fillRect(margin, scaleY, w - margin * 2, barHeight,
                   QColor(50, 50, 50));

  // Draw scale markers
  painter.setPen(QColor(100, 100, 100));
  for (int db = -60; db <= 0; db += 6) {
    float norm = dbToNorm(static_cast<float>(db));
    int x = margin + static_cast<int>(norm * widthAvailable);
    painter.drawLine(x, scaleY, x, scaleY + barHeight);
  }

  // Clipping indicator
  if (m_clipping) {
    painter.fillRect(w - 20, margin, 18, barHeight, QColor(255, 0, 0));
  }

  // Border
  painter.setPen(QColor(80, 80, 80));
  painter.drawRect(margin, margin, w - margin * 2 - 1, barHeight - 1);
}

// ============================================================================
// NMRecordingStudioPanel Implementation
// ============================================================================

NMRecordingStudioPanel::NMRecordingStudioPanel(QWidget *parent,
                                               IAudioPlayer *audioPlayer)
    : NMDockPanel(tr("Recording Studio"), parent) {
  setPanelId("recording_studio");

  // Use injected audio player or create one
  if (audioPlayer) {
    m_audioPlayer = audioPlayer;
  } else {
    // Try ServiceLocator first, otherwise create QtAudioPlayer
    auto *locatorPlayer = ServiceLocator::getAudioPlayer();
    if (locatorPlayer) {
      m_audioPlayer = locatorPlayer;
    } else {
      // Create our own QtAudioPlayer
      m_ownedAudioPlayer = std::make_unique<QtAudioPlayer>(this);
      m_audioPlayer = m_ownedAudioPlayer.get();
    }
  }
}

NMRecordingStudioPanel::~NMRecordingStudioPanel() {
  if (m_recorder) {
    m_recorder->shutdown();
  }
}

void NMRecordingStudioPanel::onInitialize() {
  setupUI();

  // Initialize audio recorder
  m_recorder = std::make_unique<audio::AudioRecorder>();
  auto result = m_recorder->initialize();
  if (result.isError()) {
    // Show error in UI
    if (m_lineIdLabel) {
      m_lineIdLabel->setText(
          tr("Error: %1").arg(QString::fromStdString(result.error())));
    }
    return;
  }

  // Set up callbacks
  m_recorder->setOnLevelUpdate([this](const audio::LevelMeter &level) {
    // Use Qt::QueuedConnection for thread safety
    QMetaObject::invokeMethod(
        this, [this, level]() { onLevelUpdate(level); }, Qt::QueuedConnection);
  });

  m_recorder->setOnRecordingStateChanged([this](audio::RecordingState state) {
    QMetaObject::invokeMethod(
        this,
        [this, state]() { onRecordingStateChanged(static_cast<int>(state)); },
        Qt::QueuedConnection);
  });

  m_recorder->setOnRecordingComplete(
      [this](const audio::RecordingResult &rec_result) {
        QMetaObject::invokeMethod(
            this, [this, rec_result]() { onRecordingComplete(rec_result); },
            Qt::QueuedConnection);
      });

  m_recorder->setOnRecordingError([this](const std::string &error) {
    QMetaObject::invokeMethod(
        this,
        [this, error]() { onRecordingError(QString::fromStdString(error)); },
        Qt::QueuedConnection);
  });

  // Set up audio player callbacks (issue #150 - using IAudioPlayer interface)
  if (m_audioPlayer) {
    m_audioPlayer->setVolume(1.0f);
    m_audioPlayer->setOnPlaybackStateChanged(
        [this](AudioPlaybackState state) {
          m_isPlayingTake = (state == AudioPlaybackState::Playing);
          if (m_playTakeBtn) {
            m_playTakeBtn->setText(m_isPlayingTake ? tr("Stop") : tr("Play"));
          }
        });
  }

  // Refresh device list
  refreshDeviceList();

  // Start level metering
  m_recorder->startMetering();

  // Set up update timer
  m_updateTimer = new QTimer(this);
  connect(m_updateTimer, &QTimer::timeout, this, [this]() {
    if (m_isRecording && m_recorder) {
      float duration = m_recorder->getRecordingDuration();
      int minutes = static_cast<int>(duration) / 60;
      int seconds = static_cast<int>(duration) % 60;
      int tenths =
          static_cast<int>((duration - static_cast<int>(duration)) * 10);
      m_recordingTimeLabel->setText(QString("%1:%2.%3")
                                        .arg(minutes)
                                        .arg(seconds, 2, 10, QChar('0'))
                                        .arg(tenths));
    }
  });
  m_updateTimer->start(100);
}

void NMRecordingStudioPanel::onShutdown() {
  if (m_updateTimer) {
    m_updateTimer->stop();
  }

  if (m_recorder) {
    m_recorder->stopMetering();
    m_recorder->shutdown();
  }
}

void NMRecordingStudioPanel::onUpdate([[maybe_unused]] double deltaTime) {
  // Level meter updates happen via callbacks
}

void NMRecordingStudioPanel::setManifest(audio::VoiceManifest *manifest) {
  m_manifest = manifest;
  updateLineInfo();
  updateTakeList();
}

void NMRecordingStudioPanel::setCurrentLine(const std::string &lineId) {
  m_currentLineId = lineId;
  updateLineInfo();
  updateTakeList();
  generateOutputPath();
}

void NMRecordingStudioPanel::setupUI() {
  m_contentWidget = new QWidget(this);
  setContentWidget(m_contentWidget);

  auto *mainLayout = new QVBoxLayout(m_contentWidget);
  mainLayout->setContentsMargins(8, 8, 8, 8);
  mainLayout->setSpacing(8);

  // Create sections
  setupDeviceSection();
  setupFormatSection();
  setupLevelMeterSection();
  setupLineInfoSection();
  setupRecordingControls();
  setupTakeManagement();
  setupNavigationSection();
}

void NMRecordingStudioPanel::setupDeviceSection() {
  auto *group = new QGroupBox(tr("Input Device"), m_contentWidget);
  auto *layout = new QHBoxLayout(group);

  m_inputDeviceCombo = new QComboBox(group);
  m_inputDeviceCombo->setMinimumWidth(200);
  connect(m_inputDeviceCombo,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &NMRecordingStudioPanel::onInputDeviceChanged);
  layout->addWidget(m_inputDeviceCombo, 1);

  layout->addWidget(new QLabel(tr("Gain:"), group));

  m_inputVolumeSlider = new QSlider(Qt::Horizontal, group);
  m_inputVolumeSlider->setRange(0, 100);
  m_inputVolumeSlider->setValue(100);
  m_inputVolumeSlider->setMaximumWidth(80);
  connect(m_inputVolumeSlider, &QSlider::valueChanged, this,
          &NMRecordingStudioPanel::onInputVolumeChanged);
  layout->addWidget(m_inputVolumeSlider);

  m_inputVolumeLabel = new QLabel("100%", group);
  m_inputVolumeLabel->setMinimumWidth(40);
  layout->addWidget(m_inputVolumeLabel);

  if (auto *mainLayout =
          qobject_cast<QVBoxLayout *>(m_contentWidget->layout())) {
    mainLayout->addWidget(group);
  }
}

void NMRecordingStudioPanel::setupFormatSection() {
  auto *group = new QGroupBox(tr("Recording Format"), m_contentWidget);
  auto *layout = new QHBoxLayout(group);

  // Sample rate selector
  layout->addWidget(new QLabel(tr("Sample Rate:"), group));
  m_sampleRateCombo = new QComboBox(group);
  m_sampleRateCombo->addItem(tr("44.1 kHz (CD Quality)"), 44100);
  m_sampleRateCombo->addItem(tr("48 kHz (Professional)"), 48000);
  m_sampleRateCombo->addItem(tr("96 kHz (Hi-Res)"), 96000);
  m_sampleRateCombo->setCurrentIndex(1);  // Default: 48 kHz
  connect(m_sampleRateCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int) { onFormatChanged(); });
  layout->addWidget(m_sampleRateCombo);

  layout->addSpacing(12);

  // Bit depth selector
  layout->addWidget(new QLabel(tr("Bit Depth:"), group));
  m_bitDepthCombo = new QComboBox(group);
  m_bitDepthCombo->addItem(tr("16-bit (Standard)"), 16);
  m_bitDepthCombo->addItem(tr("24-bit (Professional)"), 24);
  m_bitDepthCombo->setCurrentIndex(0);  // Default: 16-bit
  connect(m_bitDepthCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int) { onFormatChanged(); });
  layout->addWidget(m_bitDepthCombo);

  layout->addSpacing(12);

  // Channels selector
  layout->addWidget(new QLabel(tr("Channels:"), group));
  m_channelsCombo = new QComboBox(group);
  m_channelsCombo->addItem(tr("Mono"), 1);
  m_channelsCombo->addItem(tr("Stereo"), 2);
  m_channelsCombo->setCurrentIndex(0);  // Default: Mono (typical for voice)
  connect(m_channelsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int) { onFormatChanged(); });
  layout->addWidget(m_channelsCombo);

  layout->addStretch();

  if (auto *mainLayout =
          qobject_cast<QVBoxLayout *>(m_contentWidget->layout())) {
    mainLayout->addWidget(group);
  }
}

void NMRecordingStudioPanel::onFormatChanged() {
  if (!m_recorder) {
    return;
  }

  audio::RecordingFormat format = m_recorder->getRecordingFormat();

  if (m_sampleRateCombo) {
    format.sampleRate = static_cast<u32>(m_sampleRateCombo->currentData().toInt());
  }
  if (m_bitDepthCombo) {
    format.bitsPerSample = static_cast<u8>(m_bitDepthCombo->currentData().toInt());
  }
  if (m_channelsCombo) {
    format.channels = static_cast<u8>(m_channelsCombo->currentData().toInt());
  }

  m_recorder->setRecordingFormat(format);

  NOVELMIND_LOG_INFO(std::string("[RecordingStudio] Recording format: ") +
                     std::to_string(format.sampleRate) + " Hz, " +
                     std::to_string(format.bitsPerSample) + "-bit, " +
                     std::to_string(format.channels) + " ch");
}

void NMRecordingStudioPanel::setupLevelMeterSection() {
  auto *group = new QGroupBox(tr("Level Meter"), m_contentWidget);
  auto *layout = new QVBoxLayout(group);

  // VU meter widget
  m_vuMeter = new VUMeterWidget(group);
  layout->addWidget(m_vuMeter);

  // Level info row
  auto *infoLayout = new QHBoxLayout();

  m_levelDbLabel = new QLabel(tr("Level: -60 dB"), group);
  infoLayout->addWidget(m_levelDbLabel);

  // Level status label (Good level / Too quiet / Clipping)
  m_levelStatusLabel = new QLabel(tr("Ready"), group);
  m_levelStatusLabel->setMinimumWidth(150);
  infoLayout->addWidget(m_levelStatusLabel);

  infoLayout->addStretch();

  m_clippingWarning = new QLabel(tr("CLIPPING!"), group);
  m_clippingWarning->setStyleSheet("color: #ff4444; font-weight: bold;");
  m_clippingWarning->setVisible(false);
  infoLayout->addWidget(m_clippingWarning);

  layout->addLayout(infoLayout);

  if (auto *mainLayout =
          qobject_cast<QVBoxLayout *>(m_contentWidget->layout())) {
    mainLayout->addWidget(group);
  }
}

void NMRecordingStudioPanel::setupLineInfoSection() {
  auto *group = new QGroupBox(tr("Current Line"), m_contentWidget);
  auto *layout = new QGridLayout(group);

  layout->addWidget(new QLabel(tr("ID:"), group), 0, 0);
  m_lineIdLabel = new QLabel("-", group);
  m_lineIdLabel->setStyleSheet("font-weight: bold;");
  layout->addWidget(m_lineIdLabel, 0, 1);

  layout->addWidget(new QLabel(tr("Speaker:"), group), 1, 0);
  m_speakerLabel = new QLabel("-", group);
  layout->addWidget(m_speakerLabel, 1, 1);

  layout->addWidget(new QLabel(tr("Dialogue:"), group), 2, 0, Qt::AlignTop);
  m_dialogueText = new QTextEdit(group);
  m_dialogueText->setReadOnly(true);
  m_dialogueText->setMaximumHeight(80);
  m_dialogueText->setPlaceholderText(tr("Select a line to record..."));
  layout->addWidget(m_dialogueText, 2, 1);

  layout->addWidget(new QLabel(tr("Notes:"), group), 3, 0);
  m_notesLabel = new QLabel("-", group);
  m_notesLabel->setWordWrap(true);
  layout->addWidget(m_notesLabel, 3, 1);

  layout->setColumnStretch(1, 1);

  if (auto *mainLayout =
          qobject_cast<QVBoxLayout *>(m_contentWidget->layout())) {
    mainLayout->addWidget(group);
  }
}

void NMRecordingStudioPanel::setupRecordingControls() {
  auto *group = new QGroupBox(tr("Recording"), m_contentWidget);
  auto *layout = new QVBoxLayout(group);

  // Button row
  auto *btnLayout = new QHBoxLayout();

  m_recordBtn = new QPushButton(tr("Record"), group);
  m_recordBtn->setStyleSheet(
      "QPushButton { background-color: #c44; color: white; font-weight: bold; "
      "padding: 8px 16px; }"
      "QPushButton:hover { background-color: #d66; }"
      "QPushButton:disabled { background-color: #666; }");
  connect(m_recordBtn, &QPushButton::clicked, this,
          &NMRecordingStudioPanel::onRecordClicked);
  btnLayout->addWidget(m_recordBtn);

  m_stopBtn = new QPushButton(tr("Stop"), group);
  m_stopBtn->setEnabled(false);
  connect(m_stopBtn, &QPushButton::clicked, this,
          &NMRecordingStudioPanel::onStopClicked);
  btnLayout->addWidget(m_stopBtn);

  m_cancelBtn = new QPushButton(tr("Cancel"), group);
  m_cancelBtn->setEnabled(false);
  connect(m_cancelBtn, &QPushButton::clicked, this,
          &NMRecordingStudioPanel::onCancelClicked);
  btnLayout->addWidget(m_cancelBtn);

  btnLayout->addStretch();

  m_recordingTimeLabel = new QLabel("0:00.0", group);
  m_recordingTimeLabel->setStyleSheet(
      "font-size: 16px; font-family: monospace;");
  btnLayout->addWidget(m_recordingTimeLabel);

  layout->addLayout(btnLayout);

  // Progress bar (for visual feedback during recording)
  m_recordingProgress = new QProgressBar(group);
  m_recordingProgress->setRange(0, 0); // Indeterminate
  m_recordingProgress->setVisible(false);
  layout->addWidget(m_recordingProgress);

  if (auto *mainLayout =
          qobject_cast<QVBoxLayout *>(m_contentWidget->layout())) {
    mainLayout->addWidget(group);
  }
}

void NMRecordingStudioPanel::setupTakeManagement() {
  auto *group = new QGroupBox(tr("Takes"), m_contentWidget);
  auto *mainGroupLayout = new QVBoxLayout(group);

  // Takes header with count
  m_takesHeaderLabel = new QLabel(tr("Recorded Takes (0 total)"), group);
  m_takesHeaderLabel->setStyleSheet("font-weight: bold;");
  mainGroupLayout->addWidget(m_takesHeaderLabel);

  auto *contentLayout = new QHBoxLayout();

  // Takes list
  m_takesList = new QListWidget(group);
  m_takesList->setMaximumHeight(150);
  m_takesList->setMinimumHeight(100);
  m_takesList->setSelectionMode(QAbstractItemView::SingleSelection);
  m_takesList->setContextMenuPolicy(Qt::CustomContextMenu);

  connect(m_takesList, &QListWidget::currentRowChanged, this,
          &NMRecordingStudioPanel::onTakeSelected);
  connect(m_takesList, &QListWidget::itemDoubleClicked, this,
          &NMRecordingStudioPanel::onTakeDoubleClicked);
  connect(m_takesList, &QListWidget::customContextMenuRequested, this,
          &NMRecordingStudioPanel::onTakesContextMenu);
  contentLayout->addWidget(m_takesList, 1);

  // Take controls
  auto *controlsLayout = new QVBoxLayout();

  m_playTakeBtn = new QPushButton(tr("Play"), group);
  m_playTakeBtn->setEnabled(false);
  connect(m_playTakeBtn, &QPushButton::clicked, this,
          &NMRecordingStudioPanel::onPlayClicked);
  controlsLayout->addWidget(m_playTakeBtn);

  m_setActiveBtn = new QPushButton(tr("Set Active"), group);
  m_setActiveBtn->setEnabled(false);
  connect(m_setActiveBtn, &QPushButton::clicked, this,
          &NMRecordingStudioPanel::onSetActiveTakeClicked);
  controlsLayout->addWidget(m_setActiveBtn);

  m_deleteTakeBtn = new QPushButton(tr("Delete"), group);
  m_deleteTakeBtn->setEnabled(false);
  connect(m_deleteTakeBtn, &QPushButton::clicked, this,
          &NMRecordingStudioPanel::onDeleteTakeClicked);
  controlsLayout->addWidget(m_deleteTakeBtn);

  controlsLayout->addStretch();

  contentLayout->addLayout(controlsLayout);
  mainGroupLayout->addLayout(contentLayout);

  if (auto *mainLayout =
          qobject_cast<QVBoxLayout *>(m_contentWidget->layout())) {
    mainLayout->addWidget(group);
  }
}

void NMRecordingStudioPanel::setupNavigationSection() {
  auto *layout = new QHBoxLayout();

  m_prevLineBtn = new QPushButton(tr("<< Previous"), m_contentWidget);
  connect(m_prevLineBtn, &QPushButton::clicked, this,
          &NMRecordingStudioPanel::onPrevLineClicked);
  layout->addWidget(m_prevLineBtn);

  layout->addStretch();

  m_progressLabel = new QLabel(tr("Line 0 of 0"), m_contentWidget);
  layout->addWidget(m_progressLabel);

  layout->addStretch();

  m_nextLineBtn = new QPushButton(tr("Next >>"), m_contentWidget);
  connect(m_nextLineBtn, &QPushButton::clicked, this,
          &NMRecordingStudioPanel::onNextLineClicked);
  layout->addWidget(m_nextLineBtn);

  if (auto *mainLayout =
          qobject_cast<QVBoxLayout *>(m_contentWidget->layout())) {
    mainLayout->addLayout(layout);
    mainLayout->addStretch();
  }
}

void NMRecordingStudioPanel::refreshDeviceList() {
  if (!m_recorder || !m_inputDeviceCombo) {
    return;
  }

  // Block signals to prevent onInputDeviceChanged from triggering during
  // programmatic updates, which could cause feedback loops
  QSignalBlocker blocker(m_inputDeviceCombo);

  m_inputDeviceCombo->clear();

  auto devices = m_recorder->getInputDevices();

  if (devices.empty()) {
    // No devices found - show warning
    m_inputDeviceCombo->addItem(tr("No microphone detected"));
    m_inputDeviceCombo->setEnabled(false);
    if (m_recordBtn) {
      m_recordBtn->setEnabled(false);
    }

    // Show warning in level status
    if (m_levelStatusLabel) {
      m_levelStatusLabel->setText(tr("No recording devices found. Please connect a microphone."));
      m_levelStatusLabel->setStyleSheet("color: #ff6666;");
    }
    return;
  }

  // Add default device option first
  m_inputDeviceCombo->addItem(tr("(System Default)"), QString());
  m_inputDeviceCombo->setEnabled(true);
  if (m_recordBtn) {
    m_recordBtn->setEnabled(!m_currentLineId.empty());
  }

  // Add all detected devices
  int defaultIndex = 0;
  for (size_t i = 0; i < devices.size(); ++i) {
    const auto &device = devices[i];
    QString name = QString::fromStdString(device.name);

    // Add sample rate and channel info if available
    if (!device.supportedSampleRates.empty()) {
      u32 maxRate = *std::max_element(device.supportedSampleRates.begin(),
                                       device.supportedSampleRates.end());
      name += QString(" (%1 Hz, %2 ch)")
                  .arg(maxRate)
                  .arg(device.maxInputChannels > 0 ? device.maxInputChannels : 2);
    }

    // Mark default device with ★
    if (device.isDefault) {
      name = QString::fromUtf8("\u2605 ") + name + tr(" (Default)");
      // Remember index for default selection (offset by 1 for "(System Default)" entry)
      defaultIndex = static_cast<int>(i) + 1;
    }

    m_inputDeviceCombo->addItem(name, QString::fromStdString(device.id));
  }

  // Select default device
  m_inputDeviceCombo->setCurrentIndex(defaultIndex);
}

void NMRecordingStudioPanel::updateLineInfo() {
  if (!m_manifest || m_currentLineId.empty()) {
    m_lineIdLabel->setText("-");
    m_speakerLabel->setText("-");
    m_dialogueText->clear();
    m_notesLabel->setText("-");
    return;
  }

  auto *line = m_manifest->getLine(m_currentLineId);
  if (!line) {
    m_lineIdLabel->setText(QString::fromStdString(m_currentLineId) +
                           tr(" (not found)"));
    return;
  }

  m_lineIdLabel->setText(QString::fromStdString(line->id));
  m_speakerLabel->setText(QString::fromStdString(line->speaker));
  m_dialogueText->setText(QString::fromStdString(line->textKey));
  m_notesLabel->setText(
      line->notes.empty() ? "-" : QString::fromStdString(line->notes));

  // Update progress
  auto stats = m_manifest->getCoverageStats(m_currentLocale);
  m_progressLabel->setText(tr("Line %1 of %2").arg(1).arg(stats.totalLines));
}

void NMRecordingStudioPanel::updateTakeList() {
  if (!m_takesList) {
    return;
  }

  // Block signals to prevent onTakeSelected from triggering during
  // programmatic updates, which could cause feedback loops
  QSignalBlocker blocker(m_takesList);

  m_takesList->clear();

  if (!m_manifest || m_currentLineId.empty()) {
    updateTakesHeader(0, 0);
    return;
  }

  auto takes = m_manifest->getTakes(m_currentLineId, m_currentLocale);

  if (takes.empty()) {
    auto *emptyItem = new QListWidgetItem(tr("No takes recorded yet"));
    emptyItem->setFlags(Qt::NoItemFlags); // Not selectable
    emptyItem->setForeground(QColor("#808080"));
    m_takesList->addItem(emptyItem);
    updateTakesHeader(0, 0);
    return;
  }

  int activeTakeNum = 0;
  for (size_t i = 0; i < takes.size(); ++i) {
    const auto &take = takes[i];

    // Build display text with star indicator for active take
    QString displayText;
    if (take.isActive) {
      displayText = QString::fromUtf8("\u2605 Take %1").arg(take.takeNumber);
      activeTakeNum = static_cast<int>(take.takeNumber);
    } else {
      displayText = QString("   Take %1").arg(take.takeNumber);
    }

    // Add duration
    if (take.duration > 0.0f) {
      displayText += QString(" - %1s").arg(take.duration, 0, 'f', 2);
    }

    // Add timestamp
    if (take.recordedTimestamp > 0) {
      QDateTime timestamp = QDateTime::fromSecsSinceEpoch(
          static_cast<qint64>(take.recordedTimestamp));
      displayText += QString(" - %1").arg(timestamp.toString("MMM dd, hh:mm"));
    }

    // Add file size
    QString filePath = QString::fromStdString(take.filePath);
    if (!filePath.isEmpty()) {
      QString fullPath =
          QString::fromStdString(m_manifest->getBasePath()) + "/" + filePath;
      QFileInfo fileInfo(fullPath);
      if (fileInfo.exists()) {
        qint64 sizeKB = fileInfo.size() / 1024;
        displayText += QString(" - %1 KB").arg(sizeKB);
      }
    }

    auto *item = new QListWidgetItem(displayText, m_takesList);
    item->setData(Qt::UserRole, static_cast<int>(i)); // Store index
    item->setData(Qt::UserRole + 1, take.takeNumber); // Store take number

    if (take.isActive) {
      item->setForeground(QColor("#ffd700")); // Gold for active take
    }

    // Build tooltip with full details
    QString tooltip =
        QString("Take %1\n"
                "File: %2\n"
                "Duration: %3 seconds\n"
                "Recorded: %4\n"
                "Active: %5")
            .arg(take.takeNumber)
            .arg(QString::fromStdString(take.filePath))
            .arg(take.duration, 0, 'f', 3)
            .arg(take.recordedTimestamp > 0
                     ? QDateTime::fromSecsSinceEpoch(
                           static_cast<qint64>(take.recordedTimestamp))
                           .toString()
                     : "-")
            .arg(take.isActive ? "Yes" : "No");
    item->setToolTip(tooltip);
  }

  updateTakesHeader(static_cast<int>(takes.size()), activeTakeNum);

  // Select active take by default
  for (int i = 0; i < m_takesList->count(); ++i) {
    auto *item = m_takesList->item(i);
    if (item && takes[static_cast<size_t>(i)].isActive) {
      m_takesList->setCurrentRow(i);
      break;
    }
  }
}

void NMRecordingStudioPanel::updateTakesHeader(int totalTakes,
                                               int activeTakeNum) {
  if (!m_takesHeaderLabel) {
    return;
  }

  QString headerText;
  if (totalTakes == 0) {
    headerText = tr("Recorded Takes (0 total)");
  } else if (activeTakeNum > 0) {
    headerText = tr("Recorded Takes (%1 total, active: #%2)")
                     .arg(totalTakes)
                     .arg(activeTakeNum);
  } else {
    headerText = tr("Recorded Takes (%1 total)").arg(totalTakes);
  }

  m_takesHeaderLabel->setText(headerText);
}

void NMRecordingStudioPanel::updateRecordingState() {
  bool isRecording = m_isRecording;

  m_recordBtn->setEnabled(!isRecording && !m_currentLineId.empty());
  m_stopBtn->setEnabled(isRecording);
  m_cancelBtn->setEnabled(isRecording);
  m_recordingProgress->setVisible(isRecording);

  m_inputDeviceCombo->setEnabled(!isRecording);
  m_prevLineBtn->setEnabled(!isRecording);
  m_nextLineBtn->setEnabled(!isRecording);

  // Disable format controls during recording
  if (m_sampleRateCombo) {
    m_sampleRateCombo->setEnabled(!isRecording);
  }
  if (m_bitDepthCombo) {
    m_bitDepthCombo->setEnabled(!isRecording);
  }
  if (m_channelsCombo) {
    m_channelsCombo->setEnabled(!isRecording);
  }
}

void NMRecordingStudioPanel::generateOutputPath() {
  if (!m_manifest || m_currentLineId.empty()) {
    m_outputPath.clear();
    return;
  }

  auto *line = m_manifest->getLine(m_currentLineId);
  if (!line) {
    m_outputPath.clear();
    return;
  }

  // Get the next take number
  auto takes = m_manifest->getTakes(m_currentLineId, m_currentLocale);
  u32 takeNum = static_cast<u32>(takes.size()) + 1;

  // Generate path using naming convention
  const auto &convention = m_manifest->getNamingConvention();
  m_outputPath = m_manifest->getBasePath() + "/" +
                 convention.generatePath(m_currentLocale, m_currentLineId,
                                         line->scene, line->speaker, takeNum);
}

void NMRecordingStudioPanel::onInputDeviceChanged(int index) {
  if (!m_recorder || !m_inputDeviceCombo) {
    return;
  }

  QString deviceId = m_inputDeviceCombo->itemData(index).toString();
  m_recorder->setInputDevice(deviceId.toStdString());
}

void NMRecordingStudioPanel::onRecordClicked() {
  if (!m_recorder || m_currentLineId.empty()) {
    return;
  }

  generateOutputPath();
  if (m_outputPath.empty()) {
    onRecordingError(tr("Cannot generate output path"));
    return;
  }

  auto result = m_recorder->startRecording(m_outputPath);
  if (result.isError()) {
    onRecordingError(QString::fromStdString(result.error()));
    return;
  }

  m_isRecording = true;
  updateRecordingState();
}

void NMRecordingStudioPanel::onStopClicked() {
  if (!m_recorder || !m_isRecording) {
    return;
  }

  m_recorder->stopRecording();
}

void NMRecordingStudioPanel::onCancelClicked() {
  if (!m_recorder || !m_isRecording) {
    return;
  }

  m_recorder->cancelRecording();
  m_isRecording = false;
  updateRecordingState();
  m_recordingTimeLabel->setText("0:00.0");
}

void NMRecordingStudioPanel::onPlayClicked() {
  if (!m_audioPlayer || !m_manifest || m_currentLineId.empty()) {
    return;
  }

  // If already playing, stop
  if (m_isPlayingTake) {
    m_audioPlayer->stop();
    return;
  }

  // Get selected take
  int selectedIndex = m_takesList ? m_takesList->currentRow() : -1;
  if (selectedIndex < 0) {
    NOVELMIND_LOG_WARN("[RecordingStudio] No take selected for playback");
    return;
  }

  // Get takes for current line
  auto takes = m_manifest->getTakes(m_currentLineId, m_currentLocale);
  if (selectedIndex >= static_cast<int>(takes.size())) {
    NOVELMIND_LOG_WARN("[RecordingStudio] Take index out of range");
    return;
  }

  const auto &take = takes[static_cast<size_t>(selectedIndex)];
  QString filePath = QString::fromStdString(take.filePath);

  if (!QFile::exists(filePath)) {
    NOVELMIND_LOG_WARN(std::string("[RecordingStudio] Take file not found: ") +
                       take.filePath);
    QMessageBox::warning(this, tr("Playback Error"),
                         tr("Audio file not found: %1").arg(filePath));
    return;
  }

  // Play the file using IAudioPlayer interface (issue #150)
  m_audioPlayer->load(take.filePath);
  m_audioPlayer->play();
  NOVELMIND_LOG_INFO(std::string("[RecordingStudio] Playing take: ") +
                     take.filePath);
}

void NMRecordingStudioPanel::onPlayStopClicked() {
  if (m_audioPlayer && m_isPlayingTake) {
    m_audioPlayer->stop();
  }
}

void NMRecordingStudioPanel::onNextLineClicked() { emit requestNextLine(); }

void NMRecordingStudioPanel::onPrevLineClicked() { emit requestPrevLine(); }

void NMRecordingStudioPanel::onTakeSelected(int index) {
  bool hasSelection = index >= 0;

  m_playTakeBtn->setEnabled(hasSelection);
  m_deleteTakeBtn->setEnabled(hasSelection);

  if (!hasSelection || !m_manifest || m_currentLineId.empty()) {
    m_setActiveBtn->setEnabled(false);
    m_setActiveBtn->setText(tr("Set Active"));
    return;
  }

  // Get takes to check if selected take is already active
  auto takes = m_manifest->getTakes(m_currentLineId, m_currentLocale);
  if (index < static_cast<int>(takes.size())) {
    bool isActive = takes[static_cast<size_t>(index)].isActive;
    if (isActive) {
      m_setActiveBtn->setText(tr("Active \u2605"));
      m_setActiveBtn->setEnabled(false);
    } else {
      m_setActiveBtn->setText(tr("Set Active"));
      m_setActiveBtn->setEnabled(true);
    }
  }
}

void NMRecordingStudioPanel::onDeleteTakeClicked() {
  if (!m_manifest || m_currentLineId.empty()) {
    return;
  }

  // Get selected take index
  int selectedIndex = m_takesList ? m_takesList->currentRow() : -1;
  if (selectedIndex < 0) {
    NOVELMIND_LOG_WARN("[RecordingStudio] No take selected for deletion");
    return;
  }

  // Get takes for current line
  auto takes = m_manifest->getTakes(m_currentLineId, m_currentLocale);
  if (selectedIndex >= static_cast<int>(takes.size())) {
    NOVELMIND_LOG_WARN(
        "[RecordingStudio] Take index out of range for deletion");
    return;
  }

  const auto &take = takes[static_cast<size_t>(selectedIndex)];

  // Confirm deletion
  QMessageBox::StandardButton reply = QMessageBox::question(
      this, tr("Delete Take"),
      tr("Are you sure you want to delete take #%1?\n\nFile: %2")
          .arg(take.takeNumber)
          .arg(QString::fromStdString(take.filePath)),
      QMessageBox::Yes | QMessageBox::No);

  if (reply != QMessageBox::Yes) {
    return;
  }

  // Stop playback if this take is playing
  if (m_audioPlayer && m_isPlayingTake) {
    m_audioPlayer->stop();
  }

  // Delete the file
  QString filePath = QString::fromStdString(take.filePath);
  if (QFile::exists(filePath)) {
    if (!QFile::remove(filePath)) {
      NOVELMIND_LOG_WARN(
          std::string("[RecordingStudio] Failed to delete file: ") +
          take.filePath);
      QMessageBox::warning(this, tr("Delete Error"),
                           tr("Failed to delete file: %1").arg(filePath));
      return;
    }
    NOVELMIND_LOG_INFO(std::string("[RecordingStudio] Deleted take file: ") +
                       take.filePath);
  }

  // Remove from manifest
  m_manifest->removeTake(m_currentLineId, m_currentLocale, take.takeNumber);

  // Refresh the take list
  updateTakeList();

  NOVELMIND_LOG_INFO(std::string("[RecordingStudio] Deleted take #") +
                     std::to_string(take.takeNumber) + " for line " +
                     m_currentLineId);
}

void NMRecordingStudioPanel::onSetActiveTakeClicked() {
  if (!m_manifest || m_currentLineId.empty()) {
    return;
  }

  int selectedIndex = m_takesList ? m_takesList->currentRow() : -1;
  if (selectedIndex < 0) {
    return;
  }

  auto takes = m_manifest->getTakes(m_currentLineId, m_currentLocale);
  if (selectedIndex >= static_cast<int>(takes.size())) {
    return;
  }

  // Don't set active if already active
  if (takes[static_cast<size_t>(selectedIndex)].isActive) {
    return;
  }

  // Set the active take via manifest
  auto result = m_manifest->setActiveTake(m_currentLineId, m_currentLocale,
                                          static_cast<u32>(selectedIndex));
  if (result.isError()) {
    NOVELMIND_LOG_WARN("[RecordingStudio] Failed to set active take: {}",
                       result.error());
    return;
  }

  // Refresh UI
  updateTakeList();

  // Emit signal for voice manager
  emit activeTakeChanged(QString::fromStdString(m_currentLineId),
                         selectedIndex);

  NOVELMIND_LOG_INFO("[RecordingStudio] Set take {} as active for line {}",
                     takes[static_cast<size_t>(selectedIndex)].takeNumber,
                     m_currentLineId);
}

void NMRecordingStudioPanel::onTakeDoubleClicked(QListWidgetItem *item) {
  if (!item) {
    return;
  }

  // Double-click sets take as active (convenience shortcut)
  int takeIndex = item->data(Qt::UserRole).toInt();

  auto takes = m_manifest->getTakes(m_currentLineId, m_currentLocale);
  if (takeIndex >= static_cast<int>(takes.size())) {
    return;
  }

  // Only set if not already active
  if (!takes[static_cast<size_t>(takeIndex)].isActive) {
    m_takesList->setCurrentRow(takeIndex);
    onSetActiveTakeClicked();
  }
}

void NMRecordingStudioPanel::onTakesContextMenu(const QPoint &pos) {
  QListWidgetItem *item = m_takesList->itemAt(pos);
  if (!item) {
    return;
  }

  if (!m_manifest || m_currentLineId.empty()) {
    return;
  }

  int takeIndex = item->data(Qt::UserRole).toInt();
  auto takes = m_manifest->getTakes(m_currentLineId, m_currentLocale);
  if (takeIndex >= static_cast<int>(takes.size())) {
    return;
  }

  const auto &take = takes[static_cast<size_t>(takeIndex)];
  bool isActive = take.isActive;

  // Build context menu
  QMenu menu(this);

  // Play action
  QAction *playAction = menu.addAction(tr("Play Take"));
  connect(playAction, &QAction::triggered, this,
          &NMRecordingStudioPanel::onPlayClicked);

  menu.addSeparator();

  // Set active action
  if (!isActive) {
    QAction *setActiveAction = menu.addAction(tr("Set as Active"));
    connect(setActiveAction, &QAction::triggered, this,
            &NMRecordingStudioPanel::onSetActiveTakeClicked);
  } else {
    QAction *activeAction = menu.addAction(tr("\u2605 Active Take"));
    activeAction->setEnabled(false);
  }

  menu.addSeparator();

  // Show in file manager action
  QAction *showFileAction = menu.addAction(tr("Show in File Manager"));
  connect(showFileAction, &QAction::triggered, this, [this, take]() {
    QString filePath = QString::fromStdString(take.filePath);
    if (!filePath.isEmpty()) {
      QString fullPath =
          QString::fromStdString(m_manifest->getBasePath()) + "/" + filePath;
      QFileInfo fileInfo(fullPath);
      if (fileInfo.exists()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(fileInfo.absolutePath()));
      } else {
        QMessageBox::warning(this, tr("File Not Found"),
                             tr("The file does not exist:\n%1").arg(fullPath));
      }
    }
  });

  menu.addSeparator();

  // Delete action
  QAction *deleteAction = menu.addAction(tr("Delete Take"));
  connect(deleteAction, &QAction::triggered, this,
          &NMRecordingStudioPanel::onDeleteTakeClicked);

  // Disable delete if only one take
  if (takes.size() == 1) {
    deleteAction->setEnabled(false);
    deleteAction->setToolTip(tr("Cannot delete the only take"));
  }

  menu.exec(m_takesList->mapToGlobal(pos));
}

void NMRecordingStudioPanel::onInputVolumeChanged(int value) {
  if (m_inputVolumeLabel) {
    m_inputVolumeLabel->setText(QString("%1%").arg(value));
  }
  // Note: Input gain is typically controlled at the OS level
  // This could be used for a software gain if implemented
}

void NMRecordingStudioPanel::onLevelUpdate(const audio::LevelMeter &level) {
  if (m_vuMeter) {
    m_vuMeter->setLevel(level.rmsLevelDb, level.peakLevelDb, level.clipping);
  }

  if (m_levelDbLabel) {
    m_levelDbLabel->setText(
        tr("Level: %1 dB").arg(level.rmsLevelDb, 0, 'f', 1));
  }

  if (m_clippingWarning) {
    m_clippingWarning->setVisible(level.clipping);
  }

  // Update level status message
  if (m_levelStatusLabel) {
    if (level.clipping) {
      // Clipping - signal too loud
      m_levelStatusLabel->setText(tr("Reduce input gain!"));
      m_levelStatusLabel->setStyleSheet("color: #ff4444; font-weight: bold;");

      // Play warning beep on first clip detection
      if (!m_clippingWarningShown) {
        QApplication::beep();
        m_clippingWarningShown = true;
      }
    } else if (level.rmsLevelDb > -6.0f) {
      // Very good signal level (close to optimal)
      m_levelStatusLabel->setText(tr("\u2713 Good level"));
      m_levelStatusLabel->setStyleSheet("color: #44ff44;");
      m_clippingWarningShown = false;
    } else if (level.rmsLevelDb > -12.0f) {
      // Good signal level
      m_levelStatusLabel->setText(tr("\u2713 Good level"));
      m_levelStatusLabel->setStyleSheet("color: #66cc66;");
      m_clippingWarningShown = false;
    } else if (level.rmsLevelDb > -20.0f) {
      // Acceptable level
      m_levelStatusLabel->setText(tr("Level OK"));
      m_levelStatusLabel->setStyleSheet("color: #ffaa44;");
      m_clippingWarningShown = false;
    } else if (level.rmsLevelDb > -40.0f) {
      // Too quiet
      m_levelStatusLabel->setText(tr("Too quiet - speak louder"));
      m_levelStatusLabel->setStyleSheet("color: #aaaaaa;");
      m_clippingWarningShown = false;
    } else {
      // Silence / very quiet
      m_levelStatusLabel->setText(tr("Ready"));
      m_levelStatusLabel->setStyleSheet("color: #888888;");
      m_clippingWarningShown = false;
    }
  }
}

void NMRecordingStudioPanel::onRecordingStateChanged(int state) {
  auto recordingState = static_cast<audio::RecordingState>(state);

  switch (recordingState) {
  case audio::RecordingState::Idle:
    m_isRecording = false;
    break;
  case audio::RecordingState::Preparing:
  case audio::RecordingState::Recording:
    m_isRecording = true;
    break;
  case audio::RecordingState::Stopping:
  case audio::RecordingState::Processing:
    // Still recording (finishing up)
    break;
  case audio::RecordingState::Error:
    m_isRecording = false;
    break;
  }

  updateRecordingState();
}

void NMRecordingStudioPanel::onRecordingComplete(
    const audio::RecordingResult &result) {
  m_isRecording = false;
  updateRecordingState();

  if (!m_manifest || m_currentLineId.empty()) {
    return;
  }

  // Add take to manifest
  audio::VoiceTake take;
  take.takeNumber =
      static_cast<uint32_t>(
          m_manifest->getTakes(m_currentLineId, m_currentLocale).size()) +
      1;
  take.filePath = result.filePath;
  take.duration = result.duration;
  take.recordedTimestamp = static_cast<uint64_t>(std::time(nullptr));
  take.isActive = true;

  m_manifest->addTake(m_currentLineId, m_currentLocale, take);

  // Update the file status
  m_manifest->markAsRecorded(m_currentLineId, m_currentLocale, result.filePath);

  // Refresh UI
  updateTakeList();

  emit recordingCompleted(QString::fromStdString(m_currentLineId),
                          QString::fromStdString(result.filePath));
}

void NMRecordingStudioPanel::onRecordingError(const QString &error) {
  m_isRecording = false;
  updateRecordingState();

  // Show error - could use a message box or status label
  if (m_lineIdLabel) {
    m_lineIdLabel->setText(tr("Error: %1").arg(error));
  }
}

} // namespace NovelMind::editor::qt
