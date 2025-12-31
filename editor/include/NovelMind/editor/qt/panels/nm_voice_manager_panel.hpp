#pragma once

/**
 * @file nm_voice_manager_panel.hpp
 * @brief Voice file management panel
 *
 * Provides comprehensive voice-over file management:
 * - Auto-detection and matching of voice files to dialogue lines
 * - Voice file preview/playback using Qt Multimedia
 * - Import/export of voice mapping tables
 * - Actor assignment and metadata management
 * - Missing voice detection
 * - Async duration probing with caching
 */

#include "NovelMind/audio/voice_manifest.hpp"
#include "NovelMind/editor/qt/nm_dock_panel.hpp"

#include <QHash>
#include <QPointer>
#include <QQueue>
#include <QToolBar>
#include <QWidget>

#include <memory>
#include <unordered_map>

// Forward declarations for Qt Multimedia
class QMediaPlayer;
class QAudioOutput;

class QListWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QToolBar;
class QPushButton;
class QLabel;
class QLineEdit;
class QComboBox;
class QSlider;
class QSplitter;
class QProgressBar;

namespace NovelMind::editor::qt {

/**
 * @brief Voice line entry representing a dialogue line and its voice file
 */
struct VoiceLineEntry {
  QString dialogueId;      // Unique ID of the dialogue line
  QString scriptPath;      // Script file containing the line
  int lineNumber = 0;      // Line number in script
  QString speaker;         // Character speaking
  QString dialogueText;    // The dialogue text
  QString voiceFilePath;   // Path to voice file (if assigned)
  QString actor;           // Voice actor name
  bool isMatched = false;  // Whether a voice file is assigned
  bool isVerified = false; // Whether the match has been verified
  double duration = 0.0;   // Voice file duration in seconds
};

/**
 * @brief Duration cache entry with modification time for invalidation
 */
struct DurationCacheEntry {
  double duration = 0.0;
  qint64 mtime = 0; // File modification time for cache invalidation
};

class NMVoiceManagerPanel : public NMDockPanel {
  Q_OBJECT

public:
  explicit NMVoiceManagerPanel(QWidget *parent = nullptr);
  ~NMVoiceManagerPanel() override;

  void onInitialize() override;
  void onShutdown() override;
  void onUpdate(double deltaTime) override;

  /**
   * @brief Scan project for dialogue lines and voice files
   */
  void scanProject();

  /**
   * @brief Auto-match voice files to dialogue lines
   */
  void autoMatchVoiceFiles();

  /**
   * @brief Get list of missing voice lines for current locale
   */
  [[nodiscard]] std::vector<const NovelMind::audio::VoiceManifestLine *>
  getMissingLines() const;

  /**
   * @brief Get list of unmatched lines (lines with no voice files)
   */
  [[nodiscard]] QStringList getUnmatchedLines() const;

  /**
   * @brief Export voice mapping to CSV
   */
  bool exportToCsv(const QString &filePath);

  /**
   * @brief Import voice mapping from CSV
   */
  bool importFromCsv(const QString &filePath);

signals:
  /**
   * @brief Emitted when a voice line is selected
   */
  void voiceLineSelected(const QString &dialogueId);

  /**
   * @brief Emitted when a voice file assignment changes
   */
  void voiceFileChanged(const QString &dialogueId,
                        const QString &voiceFilePath);

  /**
   * @brief Emitted when a playback error occurs
   * @param errorMessage Human-readable error description
   */
  void playbackError(const QString &errorMessage);

public slots:
  /**
   * @brief Handle recording completion from Voice Studio
   * @param lineId Voice line ID that was recorded
   * @param locale Locale of the recording
   */
  void onRecordingCompleted(const QString &lineId, const QString &locale);

  /**
   * @brief Handle file status change from manifest
   * @param lineId Voice line ID that changed
   * @param locale Locale that changed
   */
  void onFileStatusChanged(const QString &lineId, const QString &locale);

private slots:
  void onScanClicked();
  void onAutoMatchClicked();
  void onImportClicked();
  void onExportClicked();
  void onExportTemplateClicked();
  void onValidateManifestClicked();
  void onPlayClicked();
  void onStopClicked();
  void onLineSelected(QTreeWidgetItem *item, int column);
  void onFilterChanged(const QString &text);
  void onCharacterFilterChanged(int index);
  void onLocaleFilterChanged(int index);
  void onStatusFilterChanged(int index);
  void onShowOnlyUnmatched(bool checked);
  void onVolumeChanged(int value);
  void onAssignVoiceFile();
  void onClearVoiceFile();
  void onOpenVoiceFolder();
  void onEditLineMetadata();
  void onAddTake();
  void onSetActiveTake();
  void onSetLineStatus();

  // Qt Multimedia slots
  void onPlaybackStateChanged();
  void onMediaStatusChanged();
  void onDurationChanged(qint64 duration);
  void onPositionChanged(qint64 position);
  void onMediaErrorOccurred();

  // Async duration probing slots
  void onProbeDurationFinished();
  void processNextDurationProbe();

private:
  void setupUI();
  void setupToolBar();
  void setupFilterBar();
  void setupVoiceList();
  void setupPreviewBar();
  void setupMediaPlayer();
  void updateVoiceList();
  void updateStatistics();
  void scanScriptsForDialogue();
  void scanVoiceFolder();
  void matchVoiceToDialogue(const QString &voiceFile);
  QString generateDialogueId(const QString &scriptPath, int lineNumber) const;
  bool playVoiceFile(const QString &filePath);
  void stopPlayback();
  void resetPlaybackUI();
  void setPlaybackError(const QString &message);
  QString formatDuration(qint64 ms) const;

  // Async duration probing
  void startDurationProbing();
  void probeDurationAsync(const QString &filePath);
  double getCachedDuration(const QString &filePath) const;
  void cacheDuration(const QString &filePath, double duration);
  void updateDurationsInList();

  // Selection and row management (VM-4)
  int findRowByLineId(const QString &lineId) const;
  void updateRowStatus(int row, const QString &lineId, const QString &locale);
  void flashRow(int row);
  void setupManifestCallbacks();

  // UI Elements
  QSplitter *m_splitter = nullptr;
  QTreeWidget *m_voiceTree = nullptr;
  QToolBar *m_toolbar = nullptr;
  QLineEdit *m_filterEdit = nullptr;
  QComboBox *m_characterFilter = nullptr;
  QComboBox *m_localeFilter = nullptr;
  QComboBox *m_statusFilter = nullptr;
  QPushButton *m_showUnmatchedBtn = nullptr;
  QPushButton *m_playBtn = nullptr;
  QPushButton *m_stopBtn = nullptr;
  QSlider *m_volumeSlider = nullptr;
  QLabel *m_durationLabel = nullptr;
  QProgressBar *m_playbackProgress = nullptr;
  QLabel *m_statsLabel = nullptr;

  // Qt Multimedia - using QPointer for safe references
  QPointer<QMediaPlayer> m_mediaPlayer;
  QPointer<QAudioOutput> m_audioOutput;

  // Duration probing player (separate from playback)
  QPointer<QMediaPlayer> m_probePlayer;
  QQueue<QString> m_probeQueue;
  QString m_currentProbeFile;
  bool m_isProbing = false;
  static constexpr int MAX_CONCURRENT_PROBES = 1; // One at a time for stability

  // Duration cache: path -> {duration, mtime}
  std::unordered_map<std::string, DurationCacheEntry> m_durationCache;

  // Data - VoiceManifest is the single source of truth
  std::unique_ptr<NovelMind::audio::VoiceManifest> m_manifest;
  QString m_currentLocale;
  QStringList m_voiceFiles;
  QString m_currentlyPlayingFile;
  bool m_isPlaying = false;
  qint64 m_currentDuration = 0;
  double m_volume = 1.0;

  // Verbose logging flag (can be toggled for debugging)
  static constexpr bool VERBOSE_LOGGING = false;
};

} // namespace NovelMind::editor::qt
