#pragma once

/**
 * @file MockAudioPlayer.hpp
 * @brief Mock implementation of IAudioPlayer for testing
 *
 * Provides a mock audio player that can be used for:
 * - Unit testing without audio hardware
 * - CI/CD testing environments
 * - Verifying playback behavior
 *
 * Part of issue #150: Add Missing Abstraction Interfaces
 */

#include "NovelMind/editor/interfaces/IAudioPlayer.hpp"

#include <string>
#include <vector>

namespace NovelMind::editor {

/**
 * @brief Mock implementation of IAudioPlayer for testing
 *
 * This class provides a mock implementation that:
 * - Tracks all operations for verification
 * - Allows setting mock return values
 * - Runs without any audio hardware
 * - Executes ~100x faster than real audio playback
 */
class MockAudioPlayer : public IAudioPlayer {
public:
  MockAudioPlayer() = default;
  ~MockAudioPlayer() override = default;

  // =========================================================================
  // IAudioPlayer Implementation
  // =========================================================================

  bool load(const std::string& filePath) override {
    m_loadedFile = filePath;
    m_loadCount++;
    m_mediaStatus = AudioMediaStatus::Loaded;
    return m_mockLoadSuccess;
  }

  bool play() override {
    if (m_loadedFile.empty()) {
      return false;
    }
    m_playbackState = AudioPlaybackState::Playing;
    m_playCount++;
    if (m_onPlaybackStateChanged) {
      m_onPlaybackStateChanged(m_playbackState);
    }
    return true;
  }

  bool pause() override {
    if (m_playbackState != AudioPlaybackState::Playing) {
      return false;
    }
    m_playbackState = AudioPlaybackState::Paused;
    m_pauseCount++;
    if (m_onPlaybackStateChanged) {
      m_onPlaybackStateChanged(m_playbackState);
    }
    return true;
  }

  bool stop() override {
    m_playbackState = AudioPlaybackState::Stopped;
    m_currentPositionMs = 0;
    m_stopCount++;
    if (m_onPlaybackStateChanged) {
      m_onPlaybackStateChanged(m_playbackState);
    }
    return true;
  }

  void clearSource() override {
    m_loadedFile.clear();
    m_playbackState = AudioPlaybackState::Stopped;
    m_mediaStatus = AudioMediaStatus::NoMedia;
    m_currentPositionMs = 0;
  }

  [[nodiscard]] f32 getDuration() const override { return m_mockDuration; }

  [[nodiscard]] i64 getDurationMs() const override {
    return static_cast<i64>(m_mockDuration * 1000.0f);
  }

  [[nodiscard]] f32 getCurrentPosition() const override {
    return static_cast<f32>(m_currentPositionMs) / 1000.0f;
  }

  [[nodiscard]] i64 getPositionMs() const override { return m_currentPositionMs; }

  bool setPosition(f32 seconds) override {
    m_currentPositionMs = static_cast<i64>(seconds * 1000.0f);
    return true;
  }

  bool setPositionMs(i64 ms) override {
    m_currentPositionMs = ms;
    return true;
  }

  [[nodiscard]] f32 getVolume() const override { return m_volume; }

  bool setVolume(f32 volume) override {
    m_volume = volume;
    m_volumeChangeCount++;
    return true;
  }

  [[nodiscard]] bool isPlaying() const override {
    return m_playbackState == AudioPlaybackState::Playing;
  }

  [[nodiscard]] bool isPaused() const override {
    return m_playbackState == AudioPlaybackState::Paused;
  }

  [[nodiscard]] bool isStopped() const override {
    return m_playbackState == AudioPlaybackState::Stopped;
  }

  [[nodiscard]] AudioPlaybackState getPlaybackState() const override { return m_playbackState; }

  [[nodiscard]] AudioMediaStatus getMediaStatus() const override { return m_mediaStatus; }

  [[nodiscard]] std::string getCurrentFilePath() const override { return m_loadedFile; }

  [[nodiscard]] std::string getErrorString() const override { return m_mockErrorString; }

  void setOnPlaybackFinished(std::function<void()> callback) override {
    m_onPlaybackFinished = std::move(callback);
  }

  void setOnError(std::function<void(const std::string&)> callback) override {
    m_onError = std::move(callback);
  }

  void setOnPlaybackStateChanged(std::function<void(AudioPlaybackState)> callback) override {
    m_onPlaybackStateChanged = std::move(callback);
  }

  void setOnMediaStatusChanged(std::function<void(AudioMediaStatus)> callback) override {
    m_onMediaStatusChanged = std::move(callback);
  }

  void setOnDurationChanged(std::function<void(i64)> callback) override {
    m_onDurationChanged = std::move(callback);
  }

  void setOnPositionChanged(std::function<void(i64)> callback) override {
    m_onPositionChanged = std::move(callback);
  }

  // =========================================================================
  // Mock Configuration
  // =========================================================================

  /**
   * @brief Set mock duration for getDuration() calls
   * @param duration Duration in seconds
   */
  void setMockDuration(f32 duration) { m_mockDuration = duration; }

  /**
   * @brief Set whether load() should succeed
   * @param success true if load should succeed
   */
  void setMockLoadSuccess(bool success) { m_mockLoadSuccess = success; }

  /**
   * @brief Set mock error string
   * @param error Error message to return
   */
  void setMockErrorString(const std::string& error) { m_mockErrorString = error; }

  /**
   * @brief Set mock media status
   * @param status Status to return
   */
  void setMockMediaStatus(AudioMediaStatus status) {
    m_mediaStatus = status;
    if (m_onMediaStatusChanged) {
      m_onMediaStatusChanged(status);
    }
  }

  // =========================================================================
  // Test Helpers - Verification
  // =========================================================================

  /**
   * @brief Get the file path that was loaded
   * @return Loaded file path
   */
  [[nodiscard]] const std::string& getLoadedFile() const { return m_loadedFile; }

  /**
   * @brief Get number of times load() was called
   * @return Load call count
   */
  [[nodiscard]] int getLoadCount() const { return m_loadCount; }

  /**
   * @brief Get number of times play() was called
   * @return Play call count
   */
  [[nodiscard]] int getPlayCount() const { return m_playCount; }

  /**
   * @brief Get number of times pause() was called
   * @return Pause call count
   */
  [[nodiscard]] int getPauseCount() const { return m_pauseCount; }

  /**
   * @brief Get number of times stop() was called
   * @return Stop call count
   */
  [[nodiscard]] int getStopCount() const { return m_stopCount; }

  /**
   * @brief Get number of times setVolume() was called
   * @return Volume change count
   */
  [[nodiscard]] int getVolumeChangeCount() const { return m_volumeChangeCount; }

  /**
   * @brief Reset all counters and state
   */
  void reset() {
    m_loadedFile.clear();
    m_playbackState = AudioPlaybackState::Stopped;
    m_mediaStatus = AudioMediaStatus::NoMedia;
    m_currentPositionMs = 0;
    m_volume = 1.0f;
    m_loadCount = 0;
    m_playCount = 0;
    m_pauseCount = 0;
    m_stopCount = 0;
    m_volumeChangeCount = 0;
    m_mockDuration = 3.0f;
    m_mockLoadSuccess = true;
    m_mockErrorString.clear();
  }

  // =========================================================================
  // Test Helpers - Simulate Events
  // =========================================================================

  /**
   * @brief Simulate playback finishing (e.g., end of file)
   */
  void simulatePlaybackFinished() {
    m_playbackState = AudioPlaybackState::Stopped;
    m_mediaStatus = AudioMediaStatus::EndOfMedia;
    if (m_onPlaybackFinished) {
      m_onPlaybackFinished();
    }
    if (m_onMediaStatusChanged) {
      m_onMediaStatusChanged(m_mediaStatus);
    }
    if (m_onPlaybackStateChanged) {
      m_onPlaybackStateChanged(m_playbackState);
    }
  }

  /**
   * @brief Simulate an error occurring
   * @param error Error message
   */
  void simulateError(const std::string& error) {
    m_mockErrorString = error;
    if (m_onError) {
      m_onError(error);
    }
  }

  /**
   * @brief Simulate duration becoming known after load
   * @param durationMs Duration in milliseconds
   */
  void simulateDurationChanged(i64 durationMs) {
    m_mockDuration = static_cast<f32>(durationMs) / 1000.0f;
    if (m_onDurationChanged) {
      m_onDurationChanged(durationMs);
    }
  }

  /**
   * @brief Simulate position update during playback
   * @param positionMs Position in milliseconds
   */
  void simulatePositionChanged(i64 positionMs) {
    m_currentPositionMs = positionMs;
    if (m_onPositionChanged) {
      m_onPositionChanged(positionMs);
    }
  }

private:
  // State
  std::string m_loadedFile;
  AudioPlaybackState m_playbackState = AudioPlaybackState::Stopped;
  AudioMediaStatus m_mediaStatus = AudioMediaStatus::NoMedia;
  i64 m_currentPositionMs = 0;
  f32 m_volume = 1.0f;

  // Mock configuration
  f32 m_mockDuration = 3.0f;
  bool m_mockLoadSuccess = true;
  std::string m_mockErrorString;

  // Call counters for verification
  int m_loadCount = 0;
  int m_playCount = 0;
  int m_pauseCount = 0;
  int m_stopCount = 0;
  int m_volumeChangeCount = 0;

  // Callbacks
  std::function<void()> m_onPlaybackFinished;
  std::function<void(const std::string&)> m_onError;
  std::function<void(AudioPlaybackState)> m_onPlaybackStateChanged;
  std::function<void(AudioMediaStatus)> m_onMediaStatusChanged;
  std::function<void(i64)> m_onDurationChanged;
  std::function<void(i64)> m_onPositionChanged;
};

} // namespace NovelMind::editor
