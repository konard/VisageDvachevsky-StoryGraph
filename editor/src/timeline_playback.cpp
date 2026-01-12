#include "NovelMind/editor/timeline_playback.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>

namespace NovelMind::editor {

// Static instance
std::unique_ptr<TimelinePlaybackEngine> TimelinePlaybackEngine::s_instance = nullptr;

TimelinePlaybackEngine::TimelinePlaybackEngine() = default;

TimelinePlaybackEngine::~TimelinePlaybackEngine() = default;

TimelinePlaybackEngine& TimelinePlaybackEngine::instance() {
  if (!s_instance) {
    s_instance = std::make_unique<TimelinePlaybackEngine>();
  }
  return *s_instance;
}

// ============================================================================
// Playback Control
// ============================================================================

void TimelinePlaybackEngine::play() {
  PlaybackConfig config;
  config.startTime = m_currentTime;
  config.endTime = m_duration;
  play(config);
}

void TimelinePlaybackEngine::play(const PlaybackConfig& config) {
  std::lock_guard<std::mutex> lock(m_mutex);

  m_speed = config.speed;
  m_loopMode = config.loopMode;

  if (config.preRoll) {
    m_currentTime = std::max(0.0, config.startTime - config.preRollDuration);
  }

  m_state = PlaybackState::Playing;
  m_direction = PlaybackDirection::Forward;
  m_loopCount = 0;

  notifyStateChanged();

  TimelineEvent event;
  event.type = TimelineEventType::PlaybackStarted;
  event.time = m_currentTime;
  if (m_eventCallback) {
    m_eventCallback(event);
  }
}

void TimelinePlaybackEngine::pause() {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_state != PlaybackState::Playing) {
    return;
  }

  m_state = PlaybackState::Paused;
  notifyStateChanged();

  TimelineEvent event;
  event.type = TimelineEventType::PlaybackPaused;
  event.time = m_currentTime;
  if (m_eventCallback) {
    m_eventCallback(event);
  }
}

void TimelinePlaybackEngine::resume() {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_state != PlaybackState::Paused) {
    return;
  }

  m_state = PlaybackState::Playing;
  notifyStateChanged();
}

void TimelinePlaybackEngine::stop() {
  std::lock_guard<std::mutex> lock(m_mutex);

  m_state = PlaybackState::Stopped;
  m_currentTime = 0.0;
  m_loopCount = 0;
  m_direction = PlaybackDirection::Forward;

  notifyStateChanged();
  notifyTimeChanged();

  TimelineEvent event;
  event.type = TimelineEventType::PlaybackStopped;
  event.time = m_currentTime;
  if (m_eventCallback) {
    m_eventCallback(event);
  }
}

void TimelinePlaybackEngine::togglePlayPause() {
  if (m_state == PlaybackState::Playing) {
    pause();
  } else if (m_state == PlaybackState::Paused) {
    resume();
  } else {
    play();
  }
}

void TimelinePlaybackEngine::seekTo(f64 time) {
  std::lock_guard<std::mutex> lock(m_mutex);

  [[maybe_unused]] f64 oldTime = m_currentTime;
  m_currentTime = clampTime(time);

  notifyTimeChanged();

  TimelineEvent event;
  event.type = TimelineEventType::SeekCompleted;
  event.time = m_currentTime;
  if (m_eventCallback) {
    m_eventCallback(event);
  }
}

void TimelinePlaybackEngine::seekRelative(f64 delta) {
  seekTo(m_currentTime + delta);
}

void TimelinePlaybackEngine::stepForward() {
  f64 frameTime = 1.0 / m_frameRate;
  seekTo(m_currentTime + frameTime);
}

void TimelinePlaybackEngine::stepBackward() {
  f64 frameTime = 1.0 / m_frameRate;
  seekTo(m_currentTime - frameTime);
}

void TimelinePlaybackEngine::jumpToStart() {
  seekTo(0.0);
}

void TimelinePlaybackEngine::jumpToEnd() {
  seekTo(m_duration);
}

void TimelinePlaybackEngine::beginScrubbing() {
  std::lock_guard<std::mutex> lock(m_mutex);

  m_stateBeforeScrub = m_state;
  m_state = PlaybackState::Scrubbing;
  notifyStateChanged();
}

void TimelinePlaybackEngine::scrubTo(f64 time) {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_state != PlaybackState::Scrubbing) {
    return;
  }

  m_currentTime = clampTime(time);
  notifyTimeChanged();
}

void TimelinePlaybackEngine::endScrubbing() {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_state != PlaybackState::Scrubbing) {
    return;
  }

  m_state = m_stateBeforeScrub;
  notifyStateChanged();
}

// ============================================================================
// State Queries
// ============================================================================

PlaybackState TimelinePlaybackEngine::getState() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_state;
}

bool TimelinePlaybackEngine::isPlaying() const {
  return getState() == PlaybackState::Playing;
}

bool TimelinePlaybackEngine::isPaused() const {
  return getState() == PlaybackState::Paused;
}

bool TimelinePlaybackEngine::isStopped() const {
  return getState() == PlaybackState::Stopped;
}

f64 TimelinePlaybackEngine::getCurrentTime() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_currentTime;
}

f64 TimelinePlaybackEngine::getDuration() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_duration;
}

f64 TimelinePlaybackEngine::getSpeed() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_speed;
}

u32 TimelinePlaybackEngine::getLoopCount() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_loopCount;
}

PlaybackDirection TimelinePlaybackEngine::getDirection() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_direction;
}

f64 TimelinePlaybackEngine::getNormalizedPosition() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_duration <= 0.0) {
    return 0.0;
  }
  return m_currentTime / m_duration;
}

// ============================================================================
// Configuration
// ============================================================================

void TimelinePlaybackEngine::setDuration(f64 duration) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_duration = std::max(0.0, duration);
  m_currentTime = clampTime(m_currentTime);
}

void TimelinePlaybackEngine::setSpeed(f64 speed) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_speed = std::max(0.01, std::min(10.0, speed));
}

void TimelinePlaybackEngine::setLoopMode(LoopMode mode) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_loopMode = mode;
}

LoopMode TimelinePlaybackEngine::getLoopMode() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_loopMode;
}

void TimelinePlaybackEngine::setLoopRange(f64 start, f64 end) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_loopStart = std::max(0.0, start);
  m_loopEnd = std::min(m_duration, end);
  if (m_loopStart > m_loopEnd) {
    std::swap(m_loopStart, m_loopEnd);
  }
}

std::pair<f64, f64> TimelinePlaybackEngine::getLoopRange() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return {m_loopStart, m_loopEnd};
}

void TimelinePlaybackEngine::setFrameRate(f64 fps) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_frameRate = std::max(1.0, fps);
}

f64 TimelinePlaybackEngine::getFrameRate() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_frameRate;
}

// ============================================================================
// Track Management
// ============================================================================

void TimelinePlaybackEngine::registerTrack(const std::string& trackId) {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_tracks.find(trackId) == m_tracks.end()) {
    TrackPlaybackState state;
    state.trackId = trackId;
    m_tracks[trackId] = state;
  }
}

void TimelinePlaybackEngine::unregisterTrack(const std::string& trackId) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_tracks.erase(trackId);
}

std::optional<TrackPlaybackState>
TimelinePlaybackEngine::getTrackState(const std::string& trackId) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  auto it = m_tracks.find(trackId);
  if (it != m_tracks.end()) {
    return it->second;
  }
  return std::nullopt;
}

void TimelinePlaybackEngine::setTrackEnabled(const std::string& trackId, bool enabled) {
  std::lock_guard<std::mutex> lock(m_mutex);

  auto it = m_tracks.find(trackId);
  if (it != m_tracks.end()) {
    it->second.enabled = enabled;
    notifyTrackStateChanged(trackId);
  }
}

void TimelinePlaybackEngine::setTrackSolo(const std::string& trackId, bool solo) {
  std::lock_guard<std::mutex> lock(m_mutex);

  auto it = m_tracks.find(trackId);
  if (it != m_tracks.end()) {
    it->second.solo = solo;
    notifyTrackStateChanged(trackId);
  }
}

void TimelinePlaybackEngine::setTrackMuted(const std::string& trackId, bool muted) {
  std::lock_guard<std::mutex> lock(m_mutex);

  auto it = m_tracks.find(trackId);
  if (it != m_tracks.end()) {
    it->second.muted = muted;
    notifyTrackStateChanged(trackId);
  }
}

void TimelinePlaybackEngine::clearAllSolo() {
  std::lock_guard<std::mutex> lock(m_mutex);

  for (auto& pair : m_tracks) {
    pair.second.solo = false;
  }
}

// ============================================================================
// Event Scheduling
// ============================================================================

std::string TimelinePlaybackEngine::scheduleEvent(f64 time, TimelineEventCallback callback) {
  std::lock_guard<std::mutex> lock(m_mutex);

  ScheduledEvent event;
  event.time = time;
  event.id = "event_" + std::to_string(m_nextEventId++);
  event.callback = std::move(callback);
  event.repeating = false;

  m_scheduledEvents.push_back(std::move(event));

  return m_scheduledEvents.back().id;
}

std::string TimelinePlaybackEngine::scheduleRepeatingEvent(f64 startTime, f64 interval,
                                                           TimelineEventCallback callback) {
  std::lock_guard<std::mutex> lock(m_mutex);

  ScheduledEvent event;
  event.time = startTime;
  event.id = "event_" + std::to_string(m_nextEventId++);
  event.callback = std::move(callback);
  event.repeating = true;
  event.repeatInterval = interval;

  m_scheduledEvents.push_back(std::move(event));

  return m_scheduledEvents.back().id;
}

void TimelinePlaybackEngine::cancelEvent(const std::string& eventId) {
  std::lock_guard<std::mutex> lock(m_mutex);

  m_scheduledEvents.erase(
      std::remove_if(m_scheduledEvents.begin(), m_scheduledEvents.end(),
                     [&eventId](const ScheduledEvent& e) { return e.id == eventId; }),
      m_scheduledEvents.end());
}

void TimelinePlaybackEngine::addMarker(const std::string& markerId, f64 time) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_markers[markerId] = clampTime(time);
}

void TimelinePlaybackEngine::removeMarker(const std::string& markerId) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_markers.erase(markerId);
}

std::vector<std::pair<std::string, f64>> TimelinePlaybackEngine::getMarkers() const {
  std::lock_guard<std::mutex> lock(m_mutex);

  std::vector<std::pair<std::string, f64>> result;
  result.reserve(m_markers.size());

  for (const auto& pair : m_markers) {
    result.emplace_back(pair.first, pair.second);
  }

  // Sort by time
  std::sort(result.begin(), result.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });

  return result;
}

void TimelinePlaybackEngine::jumpToMarker(const std::string& markerId) {
  std::lock_guard<std::mutex> lock(m_mutex);

  auto it = m_markers.find(markerId);
  if (it != m_markers.end()) {
    m_currentTime = it->second;
    notifyTimeChanged();
  }
}

// ============================================================================
// Update
// ============================================================================

void TimelinePlaybackEngine::update(f64 deltaTime) {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_state != PlaybackState::Playing) {
    return;
  }

  f64 oldTime = m_currentTime;
  f64 timeStep = deltaTime * m_speed * static_cast<f64>(m_direction);
  m_currentTime += timeStep;

  // Handle loop boundaries
  handleLoopBoundary();

  // Process scheduled events
  processScheduledEvents(oldTime, m_currentTime);

  // Check for markers
  for (const auto& marker : m_markers) {
    bool crossed = false;
    if (m_direction == PlaybackDirection::Forward) {
      crossed = oldTime < marker.second && m_currentTime >= marker.second;
    } else {
      crossed = oldTime > marker.second && m_currentTime <= marker.second;
    }

    if (crossed) {
      TimelineEvent event;
      event.type = TimelineEventType::MarkerReached;
      event.time = marker.second;
      event.markerId = marker.first;

      if (m_eventCallback) {
        m_eventCallback(event);
      }

      for (auto* listener : m_listeners) {
        listener->onMarkerReached(marker.first, marker.second);
      }
    }
  }

  notifyTimeChanged();
}

void TimelinePlaybackEngine::evaluate() {
  std::lock_guard<std::mutex> lock(m_mutex);

  // Evaluate all tracks at current time
  for (const auto& trackPair : m_tracks) {
    const auto& track = trackPair.second;

    // Skip disabled, muted tracks (unless solo is active elsewhere)
    if (!track.enabled || track.muted) {
      continue;
    }

    // Check for solo
    bool hasSolo = false;
    for (const auto& t : m_tracks) {
      if (t.second.solo) {
        hasSolo = true;
        break;
      }
    }

    if (hasSolo && !track.solo) {
      continue;
    }

    // Track evaluation would happen here
    // This would trigger keyframe interpolation and property updates
  }
}

// ============================================================================
// Event Handling
// ============================================================================

void TimelinePlaybackEngine::setEventCallback(TimelineEventCallback callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_eventCallback = std::move(callback);
}

void TimelinePlaybackEngine::addListener(IPlaybackListener* listener) {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (listener &&
      std::find(m_listeners.begin(), m_listeners.end(), listener) == m_listeners.end()) {
    m_listeners.push_back(listener);
  }
}

void TimelinePlaybackEngine::removeListener(IPlaybackListener* listener) {
  std::lock_guard<std::mutex> lock(m_mutex);

  m_listeners.erase(std::remove(m_listeners.begin(), m_listeners.end(), listener),
                    m_listeners.end());
}

// ============================================================================
// Snapshot/Restore
// ============================================================================

TimelinePlaybackEngine::PlaybackSnapshot TimelinePlaybackEngine::getSnapshot() const {
  std::lock_guard<std::mutex> lock(m_mutex);

  PlaybackSnapshot snapshot;
  snapshot.time = m_currentTime;
  snapshot.state = m_state;
  snapshot.speed = m_speed;
  snapshot.loopMode = m_loopMode;
  snapshot.trackStates = m_tracks;

  return snapshot;
}

void TimelinePlaybackEngine::restoreFromSnapshot(const PlaybackSnapshot& snapshot) {
  std::lock_guard<std::mutex> lock(m_mutex);

  m_currentTime = snapshot.time;
  m_state = snapshot.state;
  m_speed = snapshot.speed;
  m_loopMode = snapshot.loopMode;
  m_tracks = snapshot.trackStates;

  notifyStateChanged();
  notifyTimeChanged();
}

// ============================================================================
// Private Methods
// ============================================================================

void TimelinePlaybackEngine::processScheduledEvents(f64 fromTime, f64 toTime) {
  std::vector<ScheduledEvent*> toTrigger;

  // Find events in time range
  for (auto& event : m_scheduledEvents) {
    bool inRange = false;

    if (m_direction == PlaybackDirection::Forward) {
      inRange = event.time > fromTime && event.time <= toTime;
    } else {
      inRange = event.time < fromTime && event.time >= toTime;
    }

    if (inRange) {
      toTrigger.push_back(&event);
    }
  }

  // Trigger events
  for (auto* event : toTrigger) {
    if (event->callback) {
      TimelineEvent te;
      te.type = TimelineEventType::KeyframeReached;
      te.time = event->time;
      event->callback(te);
    }

    // Update repeating events
    if (event->repeating) {
      if (m_direction == PlaybackDirection::Forward) {
        event->time += event->repeatInterval;
      } else {
        event->time -= event->repeatInterval;
      }
    }
  }

  // Remove non-repeating events that have triggered
  m_scheduledEvents.erase(std::remove_if(m_scheduledEvents.begin(), m_scheduledEvents.end(),
                                         [fromTime, toTime, this](const ScheduledEvent& e) {
                                           if (e.repeating) {
                                             return false;
                                           }
                                           if (m_direction == PlaybackDirection::Forward) {
                                             return e.time > fromTime && e.time <= toTime;
                                           }
                                           return e.time < fromTime && e.time >= toTime;
                                         }),
                          m_scheduledEvents.end());
}

void TimelinePlaybackEngine::notifyStateChanged() {
  for (auto* listener : m_listeners) {
    listener->onPlaybackStateChanged(m_state);
  }
}

void TimelinePlaybackEngine::notifyTimeChanged() {
  for (auto* listener : m_listeners) {
    listener->onTimeChanged(m_currentTime, m_duration);
  }
}

void TimelinePlaybackEngine::notifyTrackStateChanged(const std::string& trackId) {
  for (auto* listener : m_listeners) {
    listener->onTrackStateChanged(trackId);
  }
}

void TimelinePlaybackEngine::handleLoopBoundary() {
  f64 loopStart = m_loopMode == LoopMode::LoopRange ? m_loopStart : 0.0;
  f64 loopEnd = m_loopMode == LoopMode::LoopRange ? m_loopEnd : m_duration;

  bool loopOccurred = false;

  if (m_direction == PlaybackDirection::Forward && m_currentTime >= loopEnd) {
    switch (m_loopMode) {
    case LoopMode::None:
      m_currentTime = loopEnd;
      stop();
      return;

    case LoopMode::Loop:
    case LoopMode::LoopRange:
      m_currentTime = loopStart + std::fmod(m_currentTime - loopEnd, loopEnd - loopStart);
      loopOccurred = true;
      break;

    case LoopMode::PingPong:
      m_currentTime = loopEnd - (m_currentTime - loopEnd);
      m_direction = PlaybackDirection::Backward;
      loopOccurred = true;
      break;
    }
  } else if (m_direction == PlaybackDirection::Backward && m_currentTime <= loopStart) {
    switch (m_loopMode) {
    case LoopMode::None:
      m_currentTime = loopStart;
      stop();
      return;

    case LoopMode::Loop:
    case LoopMode::LoopRange:
      m_currentTime = loopEnd - std::fmod(loopStart - m_currentTime, loopEnd - loopStart);
      loopOccurred = true;
      break;

    case LoopMode::PingPong:
      m_currentTime = loopStart + (loopStart - m_currentTime);
      m_direction = PlaybackDirection::Forward;
      loopOccurred = true;
      break;
    }
  }

  if (loopOccurred) {
    m_loopCount++;

    TimelineEvent event;
    event.type = TimelineEventType::LoopCompleted;
    event.time = m_currentTime;

    if (m_eventCallback) {
      m_eventCallback(event);
    }

    for (auto* listener : m_listeners) {
      listener->onLoopCompleted(m_loopCount);
    }
  }
}

f64 TimelinePlaybackEngine::clampTime(f64 time) const {
  return std::max(0.0, std::min(m_duration, time));
}

} // namespace NovelMind::editor
