#pragma once

/**
 * @file nm_recording_studio_panel.hpp
 * @brief Recording Studio panel for voice line recording
 *
 * Provides an integrated recording studio with:
 * - Device selection (input/output)
 * - VU meter level monitoring
 * - Recording controls (record, stop, cancel)
 * - Take management (record multiple takes, select active)
 * - Integration with Voice Manifest
 *
 * Signal Flow:
 * - Outgoing: recordingCompleted(QString, QString) - emitted when recording
 * finishes
 * - Outgoing: activeTakeChanged(QString, int) - emitted when active take
 * changes
 * - Outgoing: requestNextLine() / requestPrevLine() - for navigation between
 * lines
 * - Uses QSignalBlocker in refreshDeviceList() and updateTakeList() to prevent
 *   feedback loops during programmatic combo box and list widget updates
 */

#include "NovelMind/editor/qt/nm_dock_panel.hpp"

#include <QPointer>
#include <memory>

class QComboBox;
class QPushButton;
class QLabel;
class QProgressBar;
class QListWidget;
class QListWidgetItem;
class QSlider;
class QTextEdit;
class QLineEdit;
class QTimer;
class QStackedWidget;
class QGroupBox;

namespace NovelMind::audio {
class VoiceManifest;
class AudioRecorder;
struct LevelMeter;
struct RecordingResult;
struct VoiceManifestLine;
} // namespace NovelMind::audio

namespace NovelMind::editor {
class IAudioPlayer;
} // namespace NovelMind::editor

namespace NovelMind::editor::qt {

/**
 * @brief VU meter visualization widget
 */
class VUMeterWidget : public QWidget {
  Q_OBJECT

public:
  explicit VUMeterWidget(QWidget *parent = nullptr);

  void setLevel(float rmsDb, float peakDb, bool clipping);
  void reset();

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  float m_rmsDb = -60.0f;
  float m_peakDb = -60.0f;
  bool m_clipping = false;
};

/**
 * @brief Recording Studio panel
 *
 * Uses IAudioPlayer interface for take playback, enabling:
 * - Unit testing without audio hardware
 * - Mocking for CI/CD environments
 * - Easy swap of audio backends
 */
class NMRecordingStudioPanel : public NMDockPanel {
  Q_OBJECT

public:
  /**
   * @brief Construct panel with optional audio player injection
   * @param parent Parent widget
   * @param audioPlayer Optional audio player for dependency injection
   *                    If nullptr, uses ServiceLocator or creates QtAudioPlayer
   */
  explicit NMRecordingStudioPanel(QWidget *parent = nullptr,
                                  IAudioPlayer *audioPlayer = nullptr);
  ~NMRecordingStudioPanel() override;

  void onInitialize() override;
  void onShutdown() override;
  void onUpdate(double deltaTime) override;

  /**
   * @brief Set the voice manifest to work with
   */
  void setManifest(audio::VoiceManifest *manifest);

  /**
   * @brief Set the current voice line to record
   */
  void setCurrentLine(const std::string &lineId);

  /**
   * @brief Get the current voice line ID
   */
  [[nodiscard]] const std::string &getCurrentLineId() const {
    return m_currentLineId;
  }

signals:
  /**
   * @brief Emitted when a recording is completed
   */
  void recordingCompleted(const QString &lineId, const QString &filePath);

  /**
   * @brief Emitted when the active take changes
   */
  void activeTakeChanged(const QString &lineId, int takeIndex);

  /**
   * @brief Emitted to request moving to the next line
   */
  void requestNextLine();

  /**
   * @brief Emitted to request moving to the previous line
   */
  void requestPrevLine();

private slots:
  void onInputDeviceChanged(int index);
  void onRecordClicked();
  void onStopClicked();
  void onCancelClicked();
  void onPlayClicked();
  void onPlayStopClicked();
  void onNextLineClicked();
  void onPrevLineClicked();
  void onTakeSelected(int index);
  void onDeleteTakeClicked();
  void onSetActiveTakeClicked();
  void onTakeDoubleClicked(QListWidgetItem *item);
  void onTakesContextMenu(const QPoint &pos);
  void onInputVolumeChanged(int value);

  // Recorder callbacks
  void onLevelUpdate(const audio::LevelMeter &level);
  void onRecordingStateChanged(int state);
  void onRecordingComplete(const audio::RecordingResult &result);
  void onRecordingError(const QString &error);

private:
  void setupUI();
  void setupDeviceSection();
  void setupLevelMeterSection();
  void setupRecordingControls();
  void setupLineInfoSection();
  void setupTakeManagement();
  void setupNavigationSection();

  void refreshDeviceList();
  void updateLineInfo();
  void updateTakeList();
  void updateTakesHeader(int totalTakes, int activeTakeNum);
  void updateRecordingState();
  void generateOutputPath();

  // UI elements
  QWidget *m_contentWidget = nullptr;

  // Device selection
  QComboBox *m_inputDeviceCombo = nullptr;
  QSlider *m_inputVolumeSlider = nullptr;
  QLabel *m_inputVolumeLabel = nullptr;

  // Level meter
  VUMeterWidget *m_vuMeter = nullptr;
  QLabel *m_levelDbLabel = nullptr;
  QLabel *m_clippingWarning = nullptr;

  // Recording controls
  QPushButton *m_recordBtn = nullptr;
  QPushButton *m_stopBtn = nullptr;
  QPushButton *m_cancelBtn = nullptr;
  QLabel *m_recordingTimeLabel = nullptr;
  QProgressBar *m_recordingProgress = nullptr;

  // Line info
  QLabel *m_lineIdLabel = nullptr;
  QLabel *m_speakerLabel = nullptr;
  QTextEdit *m_dialogueText = nullptr;
  QLabel *m_notesLabel = nullptr;

  // Take management
  QLabel *m_takesHeaderLabel = nullptr;
  QListWidget *m_takesList = nullptr;
  QPushButton *m_playTakeBtn = nullptr;
  QPushButton *m_deleteTakeBtn = nullptr;
  QPushButton *m_setActiveBtn = nullptr;
  QLineEdit *m_takeNotesEdit = nullptr;

  // Navigation
  QPushButton *m_prevLineBtn = nullptr;
  QPushButton *m_nextLineBtn = nullptr;
  QLabel *m_progressLabel = nullptr;

  // Timer for recording time update
  QPointer<QTimer> m_updateTimer;

  // State
  std::unique_ptr<audio::AudioRecorder> m_recorder;
  audio::VoiceManifest *m_manifest = nullptr;
  std::string m_currentLineId;
  std::string m_currentLocale = "en";
  std::string m_outputPath;
  bool m_isRecording = false;
  float m_recordingStartTime = 0.0f;

  // Playback - using IAudioPlayer interface (issue #150)
  std::unique_ptr<IAudioPlayer> m_ownedAudioPlayer; // If we created it
  IAudioPlayer *m_audioPlayer = nullptr;            // Interface pointer
  bool m_isPlayingTake = false;
};

} // namespace NovelMind::editor::qt
