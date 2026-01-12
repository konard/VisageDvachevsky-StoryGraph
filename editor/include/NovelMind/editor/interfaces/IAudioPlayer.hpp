#pragma once

/**
 * @file IAudioPlayer.hpp
 * @brief Audio player interface for decoupling from QMediaPlayer
 *
 * This interface provides an abstraction layer for audio playback,
 * allowing:
 * - Unit testing without actual audio hardware
 * - Easy swap of audio backends (Qt, FMOD, etc.)
 * - Mocking for CI/CD testing
 *
 * Part of issue #150: Add Missing Abstraction Interfaces
 */

#include "NovelMind/core/types.hpp"

#include <functional>
#include <memory>
#include <string>

namespace NovelMind::editor {

/**
 * @brief Playback state enumeration
 */
enum class AudioPlaybackState : u8 { Stopped = 0, Playing = 1, Paused = 2 };

/**
 * @brief Media loading status enumeration
 */
enum class AudioMediaStatus : u8 {
  NoMedia = 0,
  Loading = 1,
  Loaded = 2,
  Stalled = 3,
  Buffering = 4,
  Buffered = 5,
  EndOfMedia = 6,
  InvalidMedia = 7
};

/**
 * @brief Audio player interface
 *
 * Provides platform-independent audio playback capabilities.
 * Implementations should handle resource management internally.
 */
class IAudioPlayer {
public:
  virtual ~IAudioPlayer() = default;

  // =========================================================================
  // Media Control
  // =========================================================================

  /**
   * @brief Load an audio file for playback
   * @param filePath Path to the audio file
   * @return true if load was initiated successfully
   */
  virtual bool load(const std::string& filePath) = 0;

  /**
   * @brief Start playback
   * @return true if playback started successfully
   */
  virtual bool play() = 0;

  /**
   * @brief Pause playback
   * @return true if paused successfully
   */
  virtual bool pause() = 0;

  /**
   * @brief Stop playback and reset position
   * @return true if stopped successfully
   */
  virtual bool stop() = 0;

  /**
   * @brief Clear the current media source
   */
  virtual void clearSource() = 0;

  // =========================================================================
  // Position & Duration
  // =========================================================================

  /**
   * @brief Get total duration of current media in seconds
   * @return Duration in seconds, 0 if no media loaded
   */
  [[nodiscard]] virtual f32 getDuration() const = 0;

  /**
   * @brief Get total duration in milliseconds
   * @return Duration in milliseconds
   */
  [[nodiscard]] virtual i64 getDurationMs() const = 0;

  /**
   * @brief Get current playback position in seconds
   * @return Current position in seconds
   */
  [[nodiscard]] virtual f32 getCurrentPosition() const = 0;

  /**
   * @brief Get current playback position in milliseconds
   * @return Current position in milliseconds
   */
  [[nodiscard]] virtual i64 getPositionMs() const = 0;

  /**
   * @brief Set playback position
   * @param seconds Position in seconds
   * @return true if position was set successfully
   */
  virtual bool setPosition(f32 seconds) = 0;

  /**
   * @brief Set playback position in milliseconds
   * @param ms Position in milliseconds
   * @return true if position was set successfully
   */
  virtual bool setPositionMs(i64 ms) = 0;

  // =========================================================================
  // Volume Control
  // =========================================================================

  /**
   * @brief Get current volume level
   * @return Volume level from 0.0 (muted) to 1.0 (full)
   */
  [[nodiscard]] virtual f32 getVolume() const = 0;

  /**
   * @brief Set volume level
   * @param volume Volume from 0.0 (muted) to 1.0 (full)
   * @return true if volume was set successfully
   */
  virtual bool setVolume(f32 volume) = 0;

  // =========================================================================
  // State Queries
  // =========================================================================

  /**
   * @brief Check if currently playing
   * @return true if audio is playing
   */
  [[nodiscard]] virtual bool isPlaying() const = 0;

  /**
   * @brief Check if currently paused
   * @return true if audio is paused
   */
  [[nodiscard]] virtual bool isPaused() const = 0;

  /**
   * @brief Check if stopped
   * @return true if audio is stopped
   */
  [[nodiscard]] virtual bool isStopped() const = 0;

  /**
   * @brief Get current playback state
   * @return Current playback state
   */
  [[nodiscard]] virtual AudioPlaybackState getPlaybackState() const = 0;

  /**
   * @brief Get current media status
   * @return Current media status
   */
  [[nodiscard]] virtual AudioMediaStatus getMediaStatus() const = 0;

  /**
   * @brief Get the currently loaded file path
   * @return Path to current media, empty if none
   */
  [[nodiscard]] virtual std::string getCurrentFilePath() const = 0;

  /**
   * @brief Get the last error message
   * @return Error message, empty if no error
   */
  [[nodiscard]] virtual std::string getErrorString() const = 0;

  // =========================================================================
  // Callbacks
  // =========================================================================

  /**
   * @brief Set callback for when playback finishes naturally
   * @param callback Function to call when playback ends
   */
  virtual void setOnPlaybackFinished(std::function<void()> callback) = 0;

  /**
   * @brief Set callback for when an error occurs
   * @param callback Function to call with error message
   */
  virtual void setOnError(std::function<void(const std::string&)> callback) = 0;

  /**
   * @brief Set callback for when playback state changes
   * @param callback Function to call with new state
   */
  virtual void setOnPlaybackStateChanged(std::function<void(AudioPlaybackState)> callback) = 0;

  /**
   * @brief Set callback for when media status changes
   * @param callback Function to call with new status
   */
  virtual void setOnMediaStatusChanged(std::function<void(AudioMediaStatus)> callback) = 0;

  /**
   * @brief Set callback for when duration becomes known
   * @param callback Function to call with duration in ms
   */
  virtual void setOnDurationChanged(std::function<void(i64)> callback) = 0;

  /**
   * @brief Set callback for position updates during playback
   * @param callback Function to call with position in ms
   */
  virtual void setOnPositionChanged(std::function<void(i64)> callback) = 0;
};

/**
 * @brief Factory function type for creating audio players
 */
using AudioPlayerFactory = std::function<std::unique_ptr<IAudioPlayer>()>;

} // namespace NovelMind::editor
