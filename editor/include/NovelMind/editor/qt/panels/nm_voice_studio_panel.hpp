#pragma once

/**
 * @file nm_voice_studio_panel.hpp
 * @brief Voice Studio panel for recording, editing, and processing voice lines
 *
 * Provides a comprehensive voice-over authoring environment:
 * - Microphone selection and level monitoring
 * - Recording with waveform visualization
 * - Non-destructive editing (trim, fade in/out)
 * - Audio effects (normalize, high-pass, low-pass, EQ, noise gate)
 * - Preview playback with rendered effects
 * - Export to Voice Manager asset system
 * - Undo/Redo support for all editing operations
 *
 * This panel integrates with the existing Recording Studio for input and
 * Voice Manager for asset management.
 */

#include "NovelMind/editor/qt/nm_dock_panel.hpp"

#include <QPointer>
#include <QUndoStack>
#include <memory>
#include <vector>

// Forward declarations
class QMediaPlayer;
class QAudioOutput;
class QAudioSource;
class QAudioDevice;
class QComboBox;
class QPushButton;
class QLabel;
class QSlider;
class QDoubleSpinBox;
class QSpinBox;
class QCheckBox;
class QGroupBox;
class QScrollArea;
class QSplitter;
class QTimer;
class QToolBar;
class QProgressBar;
class QLineEdit;

namespace NovelMind::audio {
class AudioRecorder;
class VoiceManifest;
struct LevelMeter;
struct RecordingResult;
} // namespace NovelMind::audio

namespace NovelMind::editor::qt {

// ============================================================================
// Audio Data Structures
// ============================================================================

/**
 * @brief Audio format specification
 */
struct AudioFormat {
  uint32_t sampleRate = 48000;
  uint8_t channels = 1;
  uint8_t bitsPerSample = 16;
};

/**
 * @brief Non-destructive voice clip editing parameters
 *
 * All edits are stored as parameters rather than modifying the source file.
 * The final audio is rendered on-demand for preview and export.
 */
struct VoiceClipEdit {
  // Trim parameters (in samples)
  int64_t trimStartSamples = 0;
  int64_t trimEndSamples = 0; // 0 = no trim from end

  // Fade parameters (in milliseconds)
  float fadeInMs = 0.0f;
  float fadeOutMs = 0.0f;

  // Gain/Normalize
  float preGainDb = 0.0f;
  bool normalizeEnabled = false;
  float normalizeTargetDbFS = -1.0f;

  // Filters
  bool highPassEnabled = false;
  float highPassFreqHz = 80.0f; // Cutoff frequency

  bool lowPassEnabled = false;
  float lowPassFreqHz = 12000.0f; // Cutoff frequency

  // 3-band EQ
  bool eqEnabled = false;
  float eqLowGainDb = 0.0f;     // Low band gain
  float eqMidGainDb = 0.0f;     // Mid band gain
  float eqHighGainDb = 0.0f;    // High band gain
  float eqLowFreqHz = 300.0f;   // Low/mid crossover
  float eqHighFreqHz = 3000.0f; // Mid/high crossover

  // Noise gate
  bool noiseGateEnabled = false;
  float noiseGateThresholdDb = -40.0f;
  float noiseGateReductionDb = -80.0f;
  float noiseGateAttackMs = 1.0f;
  float noiseGateReleaseMs = 50.0f;

  // Reset all parameters to defaults
  void reset() { *this = VoiceClipEdit{}; }

  // Check if any edits have been made
  [[nodiscard]] bool hasEdits() const {
    return trimStartSamples != 0 || trimEndSamples != 0 || fadeInMs > 0 ||
           fadeOutMs > 0 || preGainDb != 0.0f || normalizeEnabled ||
           highPassEnabled || lowPassEnabled || eqEnabled || noiseGateEnabled;
  }
};

/**
 * @brief Represents a voice clip being edited
 */
struct VoiceClip {
  std::string sourcePath;     // Path to source audio file
  std::vector<float> samples; // Raw audio samples (mono, normalized -1 to 1)
  AudioFormat format;         // Audio format
  VoiceClipEdit edit;         // Non-destructive edit parameters

  // Cached peak data for waveform display
  std::vector<float> peakData;
  int peakBlockSize = 1024;

  // Duration calculations
  [[nodiscard]] double getDurationSeconds() const {
    if (format.sampleRate == 0)
      return 0.0;
    return static_cast<double>(samples.size()) / format.sampleRate;
  }

  [[nodiscard]] double getTrimmedDurationSeconds() const {
    if (format.sampleRate == 0)
      return 0.0;
    int64_t totalSamples = static_cast<int64_t>(samples.size());
    int64_t trimmedSamples =
        totalSamples - edit.trimStartSamples - edit.trimEndSamples;
    return static_cast<double>(std::max<int64_t>(0, trimmedSamples)) /
           format.sampleRate;
  }
};

// ============================================================================
// Waveform Widget
// ============================================================================

/**
 * @brief Widget for displaying and interacting with audio waveforms
 *
 * Features:
 * - Peak waveform visualization
 * - Selection range for trimming
 * - Playhead position indicator
 * - Zoom and scroll
 * - Click-to-seek support
 */
class WaveformWidget : public QWidget {
  Q_OBJECT

public:
  explicit WaveformWidget(QWidget *parent = nullptr);

  // Set the clip to display
  void setClip(const VoiceClip *clip);

  // Selection (for trimming)
  void setSelection(double startSec, double endSec);
  [[nodiscard]] double getSelectionStart() const { return m_selectionStart; }
  [[nodiscard]] double getSelectionEnd() const { return m_selectionEnd; }
  void clearSelection();

  // Playhead
  void setPlayheadPosition(double seconds);
  [[nodiscard]] double getPlayheadPosition() const { return m_playheadPos; }

  // Zoom (samples per pixel)
  void setZoom(double samplesPerPixel);
  void zoomIn();
  void zoomOut();
  void zoomToFit();

  // Scroll position
  void setScrollPosition(double seconds);

signals:
  void selectionChanged(double startSec, double endSec);
  void playheadClicked(double seconds);
  void zoomChanged(double samplesPerPixel);

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  double timeToX(double seconds) const;
  double xToTime(double x) const;
  void updatePeakCache();

  const VoiceClip *m_clip = nullptr;
  std::vector<float> m_displayPeaks;

  double m_selectionStart = 0.0;
  double m_selectionEnd = 0.0;
  double m_playheadPos = 0.0;
  double m_scrollPos = 0.0;
  double m_samplesPerPixel = 1000.0;

  bool m_isDragging = false;
  bool m_isSelecting = false;
  double m_dragStartTime = 0.0;
};

// ============================================================================
// VU Meter Widget (reused from Recording Studio)
// ============================================================================

/**
 * @brief VU meter visualization widget
 */
class StudioVUMeterWidget : public QWidget {
  Q_OBJECT

public:
  explicit StudioVUMeterWidget(QWidget *parent = nullptr);

  void setLevel(float rmsDb, float peakDb, bool clipping);
  void reset();

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  float m_rmsDb = -60.0f;
  float m_peakDb = -60.0f;
  bool m_clipping = false;
};

// ============================================================================
// Audio Processor
// ============================================================================

/**
 * @brief Processes audio samples with the given edit parameters
 *
 * All processing is done in-memory and returns a new buffer.
 * Used for both preview playback and final export.
 */
class AudioProcessor {
public:
  /**
   * @brief Apply all edit parameters to the source samples
   * @param source Source samples
   * @param edit Edit parameters
   * @param format Audio format
   * @return Processed samples
   */
  static std::vector<float> process(const std::vector<float> &source,
                                    const VoiceClipEdit &edit,
                                    const AudioFormat &format);

  /**
   * @brief Apply trim only
   */
  static std::vector<float> applyTrim(const std::vector<float> &samples,
                                      int64_t trimStart, int64_t trimEnd);

  /**
   * @brief Apply fade in/out
   */
  static void applyFades(std::vector<float> &samples, float fadeInMs,
                         float fadeOutMs, uint32_t sampleRate);

  /**
   * @brief Apply pre-gain
   */
  static void applyGain(std::vector<float> &samples, float gainDb);

  /**
   * @brief Apply normalization
   */
  static void applyNormalize(std::vector<float> &samples, float targetDbFS);

  /**
   * @brief Apply high-pass filter
   */
  static void applyHighPass(std::vector<float> &samples, float cutoffHz,
                            uint32_t sampleRate);

  /**
   * @brief Apply low-pass filter
   */
  static void applyLowPass(std::vector<float> &samples, float cutoffHz,
                           uint32_t sampleRate);

  /**
   * @brief Apply 3-band EQ
   */
  static void applyEQ(std::vector<float> &samples, float lowGainDb,
                      float midGainDb, float highGainDb, float lowFreq,
                      float highFreq, uint32_t sampleRate);

  /**
   * @brief Apply noise gate
   */
  static void applyNoiseGate(std::vector<float> &samples, float thresholdDb,
                             float reductionDb, float attackMs, float releaseMs,
                             uint32_t sampleRate);

  /**
   * @brief Calculate peak level in dB
   */
  static float calculatePeakDb(const std::vector<float> &samples);

  /**
   * @brief Calculate RMS level in dB
   */
  static float calculateRmsDb(const std::vector<float> &samples);
};

// ============================================================================
// Voice Studio Panel
// ============================================================================

/**
 * @brief Voice Studio panel for recording and editing voice lines
 */
class NMVoiceStudioPanel : public NMDockPanel {
  Q_OBJECT

public:
  explicit NMVoiceStudioPanel(QWidget *parent = nullptr);
  ~NMVoiceStudioPanel() override;

  void onInitialize() override;
  void onShutdown() override;
  void onUpdate(double deltaTime) override;

  /**
   * @brief Set the voice manifest for integration
   */
  void setManifest(audio::VoiceManifest *manifest);

  /**
   * @brief Load a voice file for editing
   */
  bool loadFile(const QString &filePath);

  /**
   * @brief Load from a voice line ID
   */
  bool loadFromLineId(const QString &lineId, const QString &locale = "en");

  /**
   * @brief Get current clip being edited
   */
  [[nodiscard]] const VoiceClip *getCurrentClip() const { return m_clip.get(); }

  /**
   * @brief Check if there are unsaved changes
   */
  [[nodiscard]] bool hasUnsavedChanges() const;

signals:
  /**
   * @brief Emitted when a file is saved
   */
  void fileSaved(const QString &filePath);

  /**
   * @brief Emitted when editing is complete and asset is updated
   */
  void assetUpdated(const QString &lineId, const QString &filePath);

  /**
   * @brief Emitted when recording completes
   */
  void recordingCompleted(const QString &filePath);

  /**
   * @brief Emitted on playback error
   */
  void playbackError(const QString &message);

private slots:
  // Device/Recording slots
  void onInputDeviceChanged(int index);
  void onRecordClicked();
  void onStopRecordClicked();
  void onCancelRecordClicked();

  // Transport slots
  void onPlayClicked();
  void onStopClicked();
  void onLoopClicked(bool checked);

  // Edit slots
  void onTrimToSelection();
  void onResetTrim();
  void onFadeInChanged(double value);
  void onFadeOutChanged(double value);
  void onPreGainChanged(double value);
  void onNormalizeToggled(bool checked);
  void onNormalizeTargetChanged(double value);

  // Filter slots
  void onHighPassToggled(bool checked);
  void onHighPassFreqChanged(double value);
  void onLowPassToggled(bool checked);
  void onLowPassFreqChanged(double value);
  void onEQToggled(bool checked);
  void onEQLowChanged(double value);
  void onEQMidChanged(double value);
  void onEQHighChanged(double value);
  void onNoiseGateToggled(bool checked);
  void onNoiseGateThresholdChanged(double value);

  // Preset slots
  void onPresetSelected(int index);
  void onSavePresetClicked();

  // File slots
  void onSaveClicked();
  void onSaveAsClicked();
  void onExportClicked();
  void onOpenClicked();

  // Undo/Redo slots
  void onUndoClicked();
  void onRedoClicked();

  // Waveform slots
  void onWaveformSelectionChanged(double start, double end);
  void onWaveformPlayheadClicked(double seconds);

  // Playback slots
  void onPlaybackPositionChanged(qint64 position);
  void onPlaybackStateChanged();
  void onPlaybackMediaStatusChanged();

  // Recording callbacks
  void onLevelUpdate(const audio::LevelMeter &level);
  void onRecordingStateChanged(int state);
  void onRecordingComplete(const audio::RecordingResult &result);
  void onRecordingError(const QString &error);

  // Timer slots
  void onUpdateTimer();

private:
  void setupUI();
  void setupToolbar();
  void setupDeviceSection();
  void setupTransportSection();
  void setupWaveformSection();
  void setupEditSection();
  void setupFilterSection();
  void setupPresetSection();
  void setupStatusBar();
  void setupMediaPlayer();
  void setupRecorder();

  void refreshDeviceList();
  void updateUI();
  void updateEditControls();
  void updatePlaybackState();
  void updateStatusBar();

  bool loadWavFile(const QString &filePath);
  bool saveWavFile(const QString &filePath);
  std::vector<float> renderProcessedAudio();

  void applyPreset(const QString &presetName);
  void pushUndoCommand(const QString &description);

  QString formatTime(double seconds) const;
  QString formatTimeMs(double seconds) const;

  // UI Elements
  QWidget *m_contentWidget = nullptr;
  QToolBar *m_toolbar = nullptr;
  QSplitter *m_mainSplitter = nullptr;

  // Device section
  QGroupBox *m_deviceGroup = nullptr;
  QComboBox *m_inputDeviceCombo = nullptr;
  QSlider *m_inputGainSlider = nullptr;
  QLabel *m_inputGainLabel = nullptr;
  StudioVUMeterWidget *m_vuMeter = nullptr;
  QLabel *m_levelLabel = nullptr;

  // Recording controls
  QPushButton *m_recordBtn = nullptr;
  QPushButton *m_stopRecordBtn = nullptr;
  QPushButton *m_cancelRecordBtn = nullptr;
  QLabel *m_recordingTimeLabel = nullptr;

  // Transport section
  QGroupBox *m_transportGroup = nullptr;
  QPushButton *m_playBtn = nullptr;
  QPushButton *m_stopBtn = nullptr;
  QPushButton *m_loopBtn = nullptr;
  QLabel *m_positionLabel = nullptr;
  QLabel *m_durationLabel = nullptr;

  // Waveform section
  WaveformWidget *m_waveformWidget = nullptr;
  QScrollArea *m_waveformScroll = nullptr;
  QSlider *m_zoomSlider = nullptr;

  // Edit section
  QGroupBox *m_editGroup = nullptr;
  QPushButton *m_trimToSelectionBtn = nullptr;
  QPushButton *m_resetTrimBtn = nullptr;
  QDoubleSpinBox *m_fadeInSpin = nullptr;
  QDoubleSpinBox *m_fadeOutSpin = nullptr;
  QDoubleSpinBox *m_preGainSpin = nullptr;
  QCheckBox *m_normalizeCheck = nullptr;
  QDoubleSpinBox *m_normalizeTargetSpin = nullptr;

  // Filter section
  QGroupBox *m_filterGroup = nullptr;
  QCheckBox *m_highPassCheck = nullptr;
  QDoubleSpinBox *m_highPassFreqSpin = nullptr;
  QCheckBox *m_lowPassCheck = nullptr;
  QDoubleSpinBox *m_lowPassFreqSpin = nullptr;
  QCheckBox *m_eqCheck = nullptr;
  QDoubleSpinBox *m_eqLowSpin = nullptr;
  QDoubleSpinBox *m_eqMidSpin = nullptr;
  QDoubleSpinBox *m_eqHighSpin = nullptr;
  QCheckBox *m_noiseGateCheck = nullptr;
  QDoubleSpinBox *m_noiseGateThresholdSpin = nullptr;

  // Preset section
  QComboBox *m_presetCombo = nullptr;
  QPushButton *m_savePresetBtn = nullptr;

  // Status bar
  QLabel *m_statusLabel = nullptr;
  QLabel *m_fileInfoLabel = nullptr;
  QProgressBar *m_progressBar = nullptr;

  // Playback
  QPointer<QMediaPlayer> m_mediaPlayer;
  QPointer<QAudioOutput> m_audioOutput;
  bool m_isPlaying = false;
  bool m_isLooping = false;

  // Recording
  std::unique_ptr<audio::AudioRecorder> m_recorder;
  bool m_isRecording = false;
  QString m_tempRecordingPath;

  // Timer
  QPointer<QTimer> m_updateTimer;

  // Data
  std::unique_ptr<VoiceClip> m_clip;
  QString m_currentFilePath;
  audio::VoiceManifest *m_manifest = nullptr;
  QString m_currentLineId;
  QString m_currentLocale = "en";

  // Undo/Redo
  std::unique_ptr<QUndoStack> m_undoStack;
  VoiceClipEdit m_lastSavedEdit;

  // Presets
  struct Preset {
    QString name;
    VoiceClipEdit edit;
  };
  std::vector<Preset> m_presets;
};

} // namespace NovelMind::editor::qt
