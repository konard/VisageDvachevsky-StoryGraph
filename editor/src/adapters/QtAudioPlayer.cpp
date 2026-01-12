/**
 * @file QtAudioPlayer.cpp
 * @brief Qt-based implementation of IAudioPlayer interface
 *
 * Part of issue #150: Add Missing Abstraction Interfaces
 */

#include "NovelMind/editor/interfaces/QtAudioPlayer.hpp"

#include <QAudioOutput>
#include <QMediaPlayer>
#include <QUrl>

namespace NovelMind::editor {

QtAudioPlayer::QtAudioPlayer(QObject* parent) : QObject(parent) {
  // Create audio output with parent ownership
  m_audioOutput = new QAudioOutput(this);
  m_audioOutput->setVolume(1.0f);

  // Create media player with parent ownership
  m_player = new QMediaPlayer(this);
  m_player->setAudioOutput(m_audioOutput);

  // Connect Qt signals to our handlers
  connect(m_player, &QMediaPlayer::playbackStateChanged, this,
          &QtAudioPlayer::onQtPlaybackStateChanged);
  connect(m_player, &QMediaPlayer::mediaStatusChanged, this,
          &QtAudioPlayer::onQtMediaStatusChanged);
  connect(m_player, &QMediaPlayer::durationChanged, this, &QtAudioPlayer::onQtDurationChanged);
  connect(m_player, &QMediaPlayer::positionChanged, this, &QtAudioPlayer::onQtPositionChanged);
  connect(m_player, &QMediaPlayer::errorOccurred, this, &QtAudioPlayer::onQtErrorOccurred);
}

QtAudioPlayer::~QtAudioPlayer() {
  // QObject parent/child relationship handles cleanup
  // but we explicitly stop playback for clean shutdown
  if (m_player) {
    m_player->stop();
  }
}

bool QtAudioPlayer::load(const std::string& filePath) {
  if (!m_player) {
    return false;
  }

  m_currentFilePath = filePath;
  m_player->setSource(QUrl::fromLocalFile(QString::fromStdString(filePath)));
  return true;
}

bool QtAudioPlayer::play() {
  if (!m_player) {
    return false;
  }

  m_player->play();
  return true;
}

bool QtAudioPlayer::pause() {
  if (!m_player) {
    return false;
  }

  m_player->pause();
  return true;
}

bool QtAudioPlayer::stop() {
  if (!m_player) {
    return false;
  }

  m_player->stop();
  return true;
}

void QtAudioPlayer::clearSource() {
  if (m_player) {
    m_player->setSource(QUrl());
    m_currentFilePath.clear();
  }
}

f32 QtAudioPlayer::getDuration() const {
  if (!m_player) {
    return 0.0f;
  }
  return static_cast<f32>(m_player->duration()) / 1000.0f;
}

i64 QtAudioPlayer::getDurationMs() const {
  if (!m_player) {
    return 0;
  }
  return static_cast<i64>(m_player->duration());
}

f32 QtAudioPlayer::getCurrentPosition() const {
  if (!m_player) {
    return 0.0f;
  }
  return static_cast<f32>(m_player->position()) / 1000.0f;
}

i64 QtAudioPlayer::getPositionMs() const {
  if (!m_player) {
    return 0;
  }
  return static_cast<i64>(m_player->position());
}

bool QtAudioPlayer::setPosition(f32 seconds) {
  if (!m_player) {
    return false;
  }

  m_player->setPosition(static_cast<qint64>(seconds * 1000.0f));
  return true;
}

bool QtAudioPlayer::setPositionMs(i64 ms) {
  if (!m_player) {
    return false;
  }

  m_player->setPosition(static_cast<qint64>(ms));
  return true;
}

f32 QtAudioPlayer::getVolume() const {
  if (!m_audioOutput) {
    return 0.0f;
  }
  return m_audioOutput->volume();
}

bool QtAudioPlayer::setVolume(f32 volume) {
  if (!m_audioOutput) {
    return false;
  }

  m_audioOutput->setVolume(volume);
  return true;
}

bool QtAudioPlayer::isPlaying() const {
  if (!m_player) {
    return false;
  }
  return m_player->playbackState() == QMediaPlayer::PlayingState;
}

bool QtAudioPlayer::isPaused() const {
  if (!m_player) {
    return false;
  }
  return m_player->playbackState() == QMediaPlayer::PausedState;
}

bool QtAudioPlayer::isStopped() const {
  if (!m_player) {
    return true;
  }
  return m_player->playbackState() == QMediaPlayer::StoppedState;
}

AudioPlaybackState QtAudioPlayer::getPlaybackState() const {
  if (!m_player) {
    return AudioPlaybackState::Stopped;
  }

  switch (m_player->playbackState()) {
  case QMediaPlayer::PlayingState:
    return AudioPlaybackState::Playing;
  case QMediaPlayer::PausedState:
    return AudioPlaybackState::Paused;
  case QMediaPlayer::StoppedState:
  default:
    return AudioPlaybackState::Stopped;
  }
}

AudioMediaStatus QtAudioPlayer::getMediaStatus() const {
  if (!m_player) {
    return AudioMediaStatus::NoMedia;
  }

  switch (m_player->mediaStatus()) {
  case QMediaPlayer::NoMedia:
    return AudioMediaStatus::NoMedia;
  case QMediaPlayer::LoadingMedia:
    return AudioMediaStatus::Loading;
  case QMediaPlayer::LoadedMedia:
    return AudioMediaStatus::Loaded;
  case QMediaPlayer::StalledMedia:
    return AudioMediaStatus::Stalled;
  case QMediaPlayer::BufferingMedia:
    return AudioMediaStatus::Buffering;
  case QMediaPlayer::BufferedMedia:
    return AudioMediaStatus::Buffered;
  case QMediaPlayer::EndOfMedia:
    return AudioMediaStatus::EndOfMedia;
  case QMediaPlayer::InvalidMedia:
    return AudioMediaStatus::InvalidMedia;
  default:
    return AudioMediaStatus::NoMedia;
  }
}

std::string QtAudioPlayer::getCurrentFilePath() const {
  return m_currentFilePath;
}

std::string QtAudioPlayer::getErrorString() const {
  if (!m_player) {
    return "";
  }
  return m_player->errorString().toStdString();
}

void QtAudioPlayer::setOnPlaybackFinished(std::function<void()> callback) {
  m_onPlaybackFinished = std::move(callback);
}

void QtAudioPlayer::setOnError(std::function<void(const std::string&)> callback) {
  m_onError = std::move(callback);
}

void QtAudioPlayer::setOnPlaybackStateChanged(std::function<void(AudioPlaybackState)> callback) {
  m_onPlaybackStateChanged = std::move(callback);
}

void QtAudioPlayer::setOnMediaStatusChanged(std::function<void(AudioMediaStatus)> callback) {
  m_onMediaStatusChanged = std::move(callback);
}

void QtAudioPlayer::setOnDurationChanged(std::function<void(i64)> callback) {
  m_onDurationChanged = std::move(callback);
}

void QtAudioPlayer::setOnPositionChanged(std::function<void(i64)> callback) {
  m_onPositionChanged = std::move(callback);
}

void QtAudioPlayer::onQtPlaybackStateChanged() {
  if (!m_player) {
    return;
  }

  AudioPlaybackState state = getPlaybackState();

  if (m_onPlaybackStateChanged) {
    m_onPlaybackStateChanged(state);
  }
}

void QtAudioPlayer::onQtMediaStatusChanged() {
  if (!m_player) {
    return;
  }

  AudioMediaStatus status = getMediaStatus();

  if (m_onMediaStatusChanged) {
    m_onMediaStatusChanged(status);
  }

  // Check for end of media to trigger playback finished
  if (status == AudioMediaStatus::EndOfMedia && m_onPlaybackFinished) {
    m_onPlaybackFinished();
  }
}

void QtAudioPlayer::onQtDurationChanged(qint64 duration) {
  if (m_onDurationChanged) {
    m_onDurationChanged(static_cast<i64>(duration));
  }
}

void QtAudioPlayer::onQtPositionChanged(qint64 position) {
  if (m_onPositionChanged) {
    m_onPositionChanged(static_cast<i64>(position));
  }
}

void QtAudioPlayer::onQtErrorOccurred() {
  if (m_player && m_onError) {
    m_onError(m_player->errorString().toStdString());
  }
}

} // namespace NovelMind::editor
