#pragma once

/**
 * @file timeline_playback.hpp
 * @brief Timeline Evaluation/Playback Backend for NovelMind Editor
 *
 * Provides the playback engine for timeline preview:
 * - Global timeline clock
 * - Playback scheduling
 * - Event callbacks at specific times
 * - Multi-track synchronization
 * - Runtime preview binding
 *
 * This enables the Timeline GUI to play animations and preview scenes.
 */

#include "NovelMind/core/types.hpp"
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace NovelMind::editor {

// Forward declarations
class TimelinePlaybackEngine;

/**
 * @brief Playback state
 */
enum class PlaybackState : u8 {
  Stopped = 0,
  Playing,
  Paused,
  Scrubbing // User is dragging the playhead
};

/**
 * @brief Loop mode
 */
enum class LoopMode : u8 {
  None = 0, // Play once then stop
  Loop,     // Loop entire timeline
  PingPong, // Play forward then backward
  LoopRange // Loop selected range only
};

/**
 * @brief Playback direction
 */
enum class PlaybackDirection : i8 { Forward = 1, Backward = -1 };

/**
 * @brief Timeline event types
 */
enum class TimelineEventType : u8 {
  KeyframeReached,
  TrackStarted,
  TrackEnded,
  MarkerReached,
  LoopCompleted,
  PlaybackStarted,
  PlaybackStopped,
  PlaybackPaused,
  SeekCompleted
};

/**
 * @brief Timeline event data
 */
struct TimelineEvent {
  TimelineEventType type;
  f64 time = 0.0;
  std::string trackId;
  std::string markerId;
  u64 keyframeIndex = 0;
  std::string userData;
};

/**
 * @brief Event callback type
 */
using TimelineEventCallback = std::function<void(const TimelineEvent&)>;

/**
 * @brief Scheduled event for the playback engine
 */
struct ScheduledEvent {
  f64 time;
  std::string id;
  TimelineEventCallback callback;
  bool repeating = false;
  f64 repeatInterval = 0.0;
};

/**
 * @brief Track state for playback
 */
struct TrackPlaybackState {
  std::string trackId;
  bool enabled = true;
  bool solo = false;
  bool muted = false;
  f32 volume = 1.0f; // For audio tracks
  u64 currentKeyframeIndex = 0;
};

/**
 * @brief Playback configuration
 */
struct PlaybackConfig {
  f64 startTime = 0.0;
  f64 endTime = 0.0; // 0 = use timeline duration
  f64 speed = 1.0;   // Playback speed multiplier
  LoopMode loopMode = LoopMode::None;
  bool autoStop = true;      // Stop at end if not looping
  bool preRoll = false;      // Play from before start time
  f64 preRollDuration = 1.0; // Seconds of pre-roll
};

/**
 * @brief Listener interface for playback events
 */
class IPlaybackListener {
public:
  virtual ~IPlaybackListener() = default;

  virtual void onPlaybackStateChanged(PlaybackState /*state*/) {}
  virtual void onTimeChanged(f64 /*currentTime*/, f64 /*duration*/) {}
  virtual void onTrackStateChanged(const std::string& /*trackId*/) {}
  virtual void onLoopCompleted(u32 /*loopCount*/) {}
  virtual void onMarkerReached(const std::string& /*markerId*/, f64 /*time*/) {}
};

/**
 * @brief Timeline playback engine
 *
 * Responsibilities:
 * - Maintain global playback clock
 * - Schedule and dispatch timeline events
 * - Synchronize multiple tracks
 * - Handle play/pause/stop/seek operations
 * - Support preview in editor
 */
class TimelinePlaybackEngine {
public:
  TimelinePlaybackEngine();
  ~TimelinePlaybackEngine();

  // Prevent copying
  TimelinePlaybackEngine(const TimelinePlaybackEngine&) = delete;
  TimelinePlaybackEngine& operator=(const TimelinePlaybackEngine&) = delete;

  /**
   * @brief Get singleton instance
   */
  static TimelinePlaybackEngine& instance();

  // =========================================================================
  // Playback Control
  // =========================================================================

  /**
   * @brief Start playback
   */
  void play();

  /**
   * @brief Start playback with configuration
   */
  void play(const PlaybackConfig& config);

  /**
   * @brief Pause playback
   */
  void pause();

  /**
   * @brief Resume from pause
   */
  void resume();

  /**
   * @brief Stop playback and reset to start
   */
  void stop();

  /**
   * @brief Toggle play/pause
   */
  void togglePlayPause();

  /**
   * @brief Seek to a specific time
   */
  void seekTo(f64 time);

  /**
   * @brief Seek relative to current time
   */
  void seekRelative(f64 delta);

  /**
   * @brief Step forward by one frame
   */
  void stepForward();

  /**
   * @brief Step backward by one frame
   */
  void stepBackward();

  /**
   * @brief Jump to start
   */
  void jumpToStart();

  /**
   * @brief Jump to end
   */
  void jumpToEnd();

  /**
   * @brief Begin scrubbing (manual time control)
   */
  void beginScrubbing();

  /**
   * @brief Update scrub position
   */
  void scrubTo(f64 time);

  /**
   * @brief End scrubbing
   */
  void endScrubbing();

  // =========================================================================
  // State Queries
  // =========================================================================

  /**
   * @brief Get current playback state
   */
  [[nodiscard]] PlaybackState getState() const;

  /**
   * @brief Check if playing
   */
  [[nodiscard]] bool isPlaying() const;

  /**
   * @brief Check if paused
   */
  [[nodiscard]] bool isPaused() const;

  /**
   * @brief Check if stopped
   */
  [[nodiscard]] bool isStopped() const;

  /**
   * @brief Get current playback time
   */
  [[nodiscard]] f64 getCurrentTime() const;

  /**
   * @brief Get timeline duration
   */
  [[nodiscard]] f64 getDuration() const;

  /**
   * @brief Get playback speed
   */
  [[nodiscard]] f64 getSpeed() const;

  /**
   * @brief Get current loop count
   */
  [[nodiscard]] u32 getLoopCount() const;

  /**
   * @brief Get playback direction
   */
  [[nodiscard]] PlaybackDirection getDirection() const;

  /**
   * @brief Get normalized position (0-1)
   */
  [[nodiscard]] f64 getNormalizedPosition() const;

  // =========================================================================
  // Configuration
  // =========================================================================

  /**
   * @brief Set timeline duration
   */
  void setDuration(f64 duration);

  /**
   * @brief Set playback speed
   */
  void setSpeed(f64 speed);

  /**
   * @brief Set loop mode
   */
  void setLoopMode(LoopMode mode);

  /**
   * @brief Get loop mode
   */
  [[nodiscard]] LoopMode getLoopMode() const;

  /**
   * @brief Set loop range
   */
  void setLoopRange(f64 start, f64 end);

  /**
   * @brief Get loop range
   */
  [[nodiscard]] std::pair<f64, f64> getLoopRange() const;

  /**
   * @brief Set frame rate for stepping
   */
  void setFrameRate(f64 fps);

  /**
   * @brief Get frame rate
   */
  [[nodiscard]] f64 getFrameRate() const;

  // =========================================================================
  // Track Management
  // =========================================================================

  /**
   * @brief Register a track for playback
   */
  void registerTrack(const std::string& trackId);

  /**
   * @brief Unregister a track
   */
  void unregisterTrack(const std::string& trackId);

  /**
   * @brief Get track state
   */
  [[nodiscard]] std::optional<TrackPlaybackState> getTrackState(const std::string& trackId) const;

  /**
   * @brief Set track enabled state
   */
  void setTrackEnabled(const std::string& trackId, bool enabled);

  /**
   * @brief Set track solo state
   */
  void setTrackSolo(const std::string& trackId, bool solo);

  /**
   * @brief Set track mute state
   */
  void setTrackMuted(const std::string& trackId, bool muted);

  /**
   * @brief Clear all track solo states
   */
  void clearAllSolo();

  // =========================================================================
  // Event Scheduling
  // =========================================================================

  /**
   * @brief Schedule an event at a specific time
   * @return Event ID for removal
   */
  std::string scheduleEvent(f64 time, TimelineEventCallback callback);

  /**
   * @brief Schedule a repeating event
   */
  std::string scheduleRepeatingEvent(f64 startTime, f64 interval, TimelineEventCallback callback);

  /**
   * @brief Cancel a scheduled event
   */
  void cancelEvent(const std::string& eventId);

  /**
   * @brief Add a marker
   */
  void addMarker(const std::string& markerId, f64 time);

  /**
   * @brief Remove a marker
   */
  void removeMarker(const std::string& markerId);

  /**
   * @brief Get all markers
   */
  [[nodiscard]] std::vector<std::pair<std::string, f64>> getMarkers() const;

  /**
   * @brief Jump to marker
   */
  void jumpToMarker(const std::string& markerId);

  // =========================================================================
  // Update
  // =========================================================================

  /**
   * @brief Update the playback engine (call from main loop)
   * @param deltaTime Time since last update in seconds
   */
  void update(f64 deltaTime);

  /**
   * @brief Evaluate timeline at current time
   * This triggers callbacks for all tracks and keyframes at current time
   */
  void evaluate();

  // =========================================================================
  // Event Handling
  // =========================================================================

  /**
   * @brief Set global event callback
   */
  void setEventCallback(TimelineEventCallback callback);

  /**
   * @brief Add a playback listener
   */
  void addListener(IPlaybackListener* listener);

  /**
   * @brief Remove a playback listener
   */
  void removeListener(IPlaybackListener* listener);

  // =========================================================================
  // Snapshot/Restore
  // =========================================================================

  /**
   * @brief Get current playback snapshot for preview
   */
  struct PlaybackSnapshot {
    f64 time;
    PlaybackState state;
    f64 speed;
    LoopMode loopMode;
    std::unordered_map<std::string, TrackPlaybackState> trackStates;
  };

  [[nodiscard]] PlaybackSnapshot getSnapshot() const;

  /**
   * @brief Restore from snapshot
   */
  void restoreFromSnapshot(const PlaybackSnapshot& snapshot);

private:
  // Internal methods
  void processScheduledEvents(f64 fromTime, f64 toTime);
  void notifyStateChanged();
  void notifyTimeChanged();
  void notifyTrackStateChanged(const std::string& trackId);
  void handleLoopBoundary();
  f64 clampTime(f64 time) const;

  // State
  PlaybackState m_state = PlaybackState::Stopped;
  PlaybackDirection m_direction = PlaybackDirection::Forward;
  f64 m_currentTime = 0.0;
  f64 m_duration = 10.0; // Default 10 seconds
  f64 m_speed = 1.0;
  LoopMode m_loopMode = LoopMode::None;
  f64 m_loopStart = 0.0;
  f64 m_loopEnd = 0.0;
  u32 m_loopCount = 0;
  f64 m_frameRate = 60.0;

  // State before scrubbing (for restoration)
  PlaybackState m_stateBeforeScrub = PlaybackState::Stopped;

  // Tracks
  std::unordered_map<std::string, TrackPlaybackState> m_tracks;

  // Scheduled events
  std::vector<ScheduledEvent> m_scheduledEvents;
  u64 m_nextEventId = 1;

  // Markers
  std::unordered_map<std::string, f64> m_markers;

  // Callbacks and listeners
  TimelineEventCallback m_eventCallback;
  std::vector<IPlaybackListener*> m_listeners;

  // Thread safety
  mutable std::mutex m_mutex;

  // Singleton
  static std::unique_ptr<TimelinePlaybackEngine> s_instance;
};

} // namespace NovelMind::editor
