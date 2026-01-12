#pragma once

/**
 * @file QtAudioPlayer.hpp
 * @brief Qt-based implementation of IAudioPlayer interface
 *
 * Implements the IAudioPlayer interface using Qt Multimedia's
 * QMediaPlayer and QAudioOutput classes.
 *
 * Part of issue #150: Add Missing Abstraction Interfaces
 */

#include "NovelMind/editor/interfaces/IAudioPlayer.hpp"

#include <QObject>
#include <QPointer>
#include <QString>

class QMediaPlayer;
class QAudioOutput;

namespace NovelMind::editor {

/**
 * @brief Qt Multimedia implementation of IAudioPlayer
 *
 * This class wraps QMediaPlayer and QAudioOutput to provide
 * platform-independent audio playback through the IAudioPlayer interface.
 *
 * Memory management:
 * - Inherits from QObject for proper Qt memory management
 * - Uses QPointer for safe reference handling
 * - Automatically cleaned up when parent is destroyed
 */
class QtAudioPlayer : public QObject, public IAudioPlayer {
  Q_OBJECT

public:
  /**
   * @brief Construct a new QtAudioPlayer
   * @param parent Optional parent QObject for memory management
   */
  explicit QtAudioPlayer(QObject* parent = nullptr);

  /**
   * @brief Destructor - cleans up Qt multimedia resources
   */
  ~QtAudioPlayer() override;

  // Prevent copying
  QtAudioPlayer(const QtAudioPlayer&) = delete;
  QtAudioPlayer& operator=(const QtAudioPlayer&) = delete;

  // =========================================================================
  // IAudioPlayer Implementation
  // =========================================================================

  bool load(const std::string& filePath) override;
  bool play() override;
  bool pause() override;
  bool stop() override;
  void clearSource() override;

  [[nodiscard]] f32 getDuration() const override;
  [[nodiscard]] i64 getDurationMs() const override;
  [[nodiscard]] f32 getCurrentPosition() const override;
  [[nodiscard]] i64 getPositionMs() const override;
  bool setPosition(f32 seconds) override;
  bool setPositionMs(i64 ms) override;

  [[nodiscard]] f32 getVolume() const override;
  bool setVolume(f32 volume) override;

  [[nodiscard]] bool isPlaying() const override;
  [[nodiscard]] bool isPaused() const override;
  [[nodiscard]] bool isStopped() const override;
  [[nodiscard]] AudioPlaybackState getPlaybackState() const override;
  [[nodiscard]] AudioMediaStatus getMediaStatus() const override;
  [[nodiscard]] std::string getCurrentFilePath() const override;
  [[nodiscard]] std::string getErrorString() const override;

  void setOnPlaybackFinished(std::function<void()> callback) override;
  void setOnError(std::function<void(const std::string&)> callback) override;
  void setOnPlaybackStateChanged(std::function<void(AudioPlaybackState)> callback) override;
  void setOnMediaStatusChanged(std::function<void(AudioMediaStatus)> callback) override;
  void setOnDurationChanged(std::function<void(i64)> callback) override;
  void setOnPositionChanged(std::function<void(i64)> callback) override;

private slots:
  void onQtPlaybackStateChanged();
  void onQtMediaStatusChanged();
  void onQtDurationChanged(qint64 duration);
  void onQtPositionChanged(qint64 position);
  void onQtErrorOccurred();

private:
  QPointer<QMediaPlayer> m_player;
  QPointer<QAudioOutput> m_audioOutput;
  std::string m_currentFilePath;

  // Callbacks
  std::function<void()> m_onPlaybackFinished;
  std::function<void(const std::string&)> m_onError;
  std::function<void(AudioPlaybackState)> m_onPlaybackStateChanged;
  std::function<void(AudioMediaStatus)> m_onMediaStatusChanged;
  std::function<void(i64)> m_onDurationChanged;
  std::function<void(i64)> m_onPositionChanged;
};

} // namespace NovelMind::editor
