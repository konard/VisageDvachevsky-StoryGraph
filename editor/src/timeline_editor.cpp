/**
 * @file timeline_editor.cpp
 * @brief Implementation of Timeline Editor for NovelMind
 */

#include "NovelMind/editor/timeline_editor.hpp"
#include "NovelMind/core/logger.hpp"
#include <algorithm>
#include <cmath>

namespace NovelMind::editor {

// =============================================================================
// TimelineClip Implementation
// =============================================================================

TimelineClip::TimelineClip(const std::string& id, const std::string& name)
    : m_id(id), m_name(name) {}

void TimelineClip::addPropertyTrack(const PropertyTrack& track) {
  m_propertyTracks.push_back(track);
}

PropertyTrack* TimelineClip::getPropertyTrack(const std::string& propertyName) {
  for (auto& track : m_propertyTracks) {
    if (track.propertyName == propertyName) {
      return &track;
    }
  }
  return nullptr;
}

Result<void> TimelineClip::addKeyframe(const std::string& propertyName, const Keyframe& keyframe) {
  auto* track = getPropertyTrack(propertyName);
  if (!track) {
    return Result<void>::error("Property track not found: " + propertyName);
  }

  // Insert keyframe in sorted order
  auto it = std::lower_bound(track->keyframes.begin(), track->keyframes.end(), keyframe,
                             [](const Keyframe& a, const Keyframe& b) { return a.time < b.time; });

  track->keyframes.insert(it, keyframe);
  return Result<void>::ok();
}

Result<void> TimelineClip::removeKeyframe(const std::string& propertyName, f64 time) {
  auto* track = getPropertyTrack(propertyName);
  if (!track) {
    return Result<void>::error("Property track not found: " + propertyName);
  }

  auto it = std::find_if(track->keyframes.begin(), track->keyframes.end(),
                         [time](const Keyframe& k) { return std::abs(k.time - time) < 0.0001; });

  if (it != track->keyframes.end()) {
    track->keyframes.erase(it);
    return Result<void>::ok();
  }

  return Result<void>::error("Keyframe not found at time: " + std::to_string(time));
}

Result<void> TimelineClip::moveKeyframe(const std::string& propertyName, f64 oldTime, f64 newTime) {
  auto* track = getPropertyTrack(propertyName);
  if (!track) {
    return Result<void>::error("Property track not found: " + propertyName);
  }

  auto it =
      std::find_if(track->keyframes.begin(), track->keyframes.end(),
                   [oldTime](const Keyframe& k) { return std::abs(k.time - oldTime) < 0.0001; });

  if (it == track->keyframes.end()) {
    return Result<void>::error("Keyframe not found at time: " + std::to_string(oldTime));
  }

  Keyframe keyframe = *it;
  track->keyframes.erase(it);

  keyframe.time = newTime;

  // Re-insert in sorted order
  auto insertIt =
      std::lower_bound(track->keyframes.begin(), track->keyframes.end(), keyframe,
                       [](const Keyframe& a, const Keyframe& b) { return a.time < b.time; });

  track->keyframes.insert(insertIt, keyframe);
  return Result<void>::ok();
}

// =============================================================================
// CharacterClip Implementation
// =============================================================================

CharacterClip::CharacterClip(const std::string& id, const std::string& characterId)
    : TimelineClip(id, characterId), m_characterId(characterId) {
  // Setup default property tracks
  PropertyTrack posXTrack;
  posXTrack.propertyName = "position.x";
  posXTrack.displayName = "Position X";
  posXTrack.minValue = 0.0f;
  posXTrack.maxValue = 1920.0f;
  addPropertyTrack(posXTrack);

  PropertyTrack posYTrack;
  posYTrack.propertyName = "position.y";
  posYTrack.displayName = "Position Y";
  posYTrack.minValue = 0.0f;
  posYTrack.maxValue = 1080.0f;
  addPropertyTrack(posYTrack);

  PropertyTrack opacityTrack;
  opacityTrack.propertyName = "opacity";
  opacityTrack.displayName = "Opacity";
  opacityTrack.minValue = 0.0f;
  opacityTrack.maxValue = 1.0f;
  addPropertyTrack(opacityTrack);

  PropertyTrack scaleXTrack;
  scaleXTrack.propertyName = "scale.x";
  scaleXTrack.displayName = "Scale X";
  scaleXTrack.minValue = 0.0f;
  scaleXTrack.maxValue = 3.0f;
  addPropertyTrack(scaleXTrack);

  PropertyTrack scaleYTrack;
  scaleYTrack.propertyName = "scale.y";
  scaleYTrack.displayName = "Scale Y";
  scaleYTrack.minValue = 0.0f;
  scaleYTrack.maxValue = 3.0f;
  addPropertyTrack(scaleYTrack);
}

void CharacterClip::setExpression(const std::string& expression, f64 time) {
  // Expression changes are stored as string keyframes
  Keyframe kf;
  kf.time = time;
  kf.value = expression;
  kf.interpolation = KeyframeInterpolation::Constant;

  auto* track = getPropertyTrack("expression");
  if (!track) {
    PropertyTrack exprTrack;
    exprTrack.propertyName = "expression";
    exprTrack.displayName = "Expression";
    addPropertyTrack(exprTrack);
    track = getPropertyTrack("expression");
  }

  if (track) {
    (void)addKeyframe("expression", kf);
  }
}

void CharacterClip::addPositionKeyframe(f64 time, f32 x, f32 y) {
  Keyframe kfX;
  kfX.time = time;
  kfX.value = x;
  (void)addKeyframe("position.x", kfX);

  Keyframe kfY;
  kfY.time = time;
  kfY.value = y;
  (void)addKeyframe("position.y", kfY);
}

void CharacterClip::addOpacityKeyframe(f64 time, f32 opacity) {
  Keyframe kf;
  kf.time = time;
  kf.value = opacity;
  (void)addKeyframe("opacity", kf);
}

void CharacterClip::addScaleKeyframe(f64 time, f32 scaleX, f32 scaleY) {
  Keyframe kfX;
  kfX.time = time;
  kfX.value = scaleX;
  (void)addKeyframe("scale.x", kfX);

  Keyframe kfY;
  kfY.time = time;
  kfY.value = scaleY;
  (void)addKeyframe("scale.y", kfY);
}

// =============================================================================
// DialogueClip Implementation
// =============================================================================

DialogueClip::DialogueClip(const std::string& id) : TimelineClip(id, "Dialogue") {}

// =============================================================================
// AudioClip Implementation
// =============================================================================

AudioClip::AudioClip(const std::string& id, AudioType type)
    : TimelineClip(id, "Audio"), m_audioType(type) {}

// =============================================================================
// CameraClip Implementation
// =============================================================================

CameraClip::CameraClip(const std::string& id) : TimelineClip(id, "Camera") {
  // Setup camera property tracks
  PropertyTrack posXTrack;
  posXTrack.propertyName = "position.x";
  posXTrack.displayName = "Camera X";
  addPropertyTrack(posXTrack);

  PropertyTrack posYTrack;
  posYTrack.propertyName = "position.y";
  posYTrack.displayName = "Camera Y";
  addPropertyTrack(posYTrack);

  PropertyTrack zoomTrack;
  zoomTrack.propertyName = "zoom";
  zoomTrack.displayName = "Zoom";
  zoomTrack.minValue = 0.1f;
  zoomTrack.maxValue = 5.0f;
  addPropertyTrack(zoomTrack);

  PropertyTrack rotationTrack;
  rotationTrack.propertyName = "rotation";
  rotationTrack.displayName = "Rotation";
  rotationTrack.minValue = -360.0f;
  rotationTrack.maxValue = 360.0f;
  addPropertyTrack(rotationTrack);
}

void CameraClip::addPositionKeyframe(f64 time, f32 x, f32 y) {
  Keyframe kfX;
  kfX.time = time;
  kfX.value = x;
  (void)addKeyframe("position.x", kfX);

  Keyframe kfY;
  kfY.time = time;
  kfY.value = y;
  (void)addKeyframe("position.y", kfY);
}

void CameraClip::addZoomKeyframe(f64 time, f32 zoom) {
  Keyframe kf;
  kf.time = time;
  kf.value = zoom;
  (void)addKeyframe("zoom", kf);
}

void CameraClip::addRotationKeyframe(f64 time, f32 angle) {
  Keyframe kf;
  kf.time = time;
  kf.value = angle;
  (void)addKeyframe("rotation", kf);
}

void CameraClip::setShake(f64 startTime, f64 duration, f32 intensity) {
  PropertyTrack* track = getPropertyTrack("shake.intensity");
  if (!track) {
    PropertyTrack shakeTrack;
    shakeTrack.propertyName = "shake.intensity";
    shakeTrack.displayName = "Shake Intensity";
    shakeTrack.minValue = 0.0f;
    shakeTrack.maxValue = 1.0f;
    addPropertyTrack(shakeTrack);
    track = getPropertyTrack("shake.intensity");
  }

  if (!track) {
    return;
  }

  Keyframe start;
  start.time = startTime;
  start.value = intensity;
  start.interpolation = KeyframeInterpolation::Linear;
  (void)addKeyframe(track->propertyName, start);

  Keyframe end;
  end.time = startTime + duration;
  end.value = 0.0f;
  end.interpolation = KeyframeInterpolation::Linear;
  (void)addKeyframe(track->propertyName, end);
}

// =============================================================================
// TimelineTrack Implementation
// =============================================================================

TimelineTrack::TimelineTrack(const std::string& id, const std::string& name, TrackType type)
    : m_id(id), m_name(name), m_type(type) {}

void TimelineTrack::addClip(std::unique_ptr<TimelineClip> clip) {
  m_clips.push_back(std::move(clip));
}

void TimelineTrack::removeClip(const std::string& clipId) {
  m_clips.erase(std::remove_if(m_clips.begin(), m_clips.end(),
                               [&clipId](const std::unique_ptr<TimelineClip>& clip) {
                                 return clip->getId() == clipId;
                               }),
                m_clips.end());
}

TimelineClip* TimelineTrack::getClip(const std::string& clipId) {
  for (auto& clip : m_clips) {
    if (clip->getId() == clipId) {
      return clip.get();
    }
  }
  return nullptr;
}

TimelineClip* TimelineTrack::getClipAtTime(f64 time) {
  for (auto& clip : m_clips) {
    if (time >= clip->getStartTime() && time < clip->getEndTime()) {
      return clip.get();
    }
  }
  return nullptr;
}

std::vector<TimelineClip*> TimelineTrack::getClipsInRange(f64 startTime, f64 endTime) {
  std::vector<TimelineClip*> result;
  for (auto& clip : m_clips) {
    if (clip->getEndTime() > startTime && clip->getStartTime() < endTime) {
      result.push_back(clip.get());
    }
  }
  return result;
}

void TimelineTrack::addChildTrack(std::unique_ptr<TimelineTrack> track) {
  m_childTracks.push_back(std::move(track));
}

// =============================================================================
// Timeline Implementation
// =============================================================================

Timeline::Timeline(const std::string& name) : m_name(name) {}

void Timeline::addTrack(std::unique_ptr<TimelineTrack> track) {
  m_tracks.push_back(std::move(track));
}

void Timeline::removeTrack(const std::string& trackId) {
  m_tracks.erase(std::remove_if(m_tracks.begin(), m_tracks.end(),
                                [&trackId](const std::unique_ptr<TimelineTrack>& track) {
                                  return track->getId() == trackId;
                                }),
                 m_tracks.end());
}

void Timeline::moveTrack(const std::string& trackId, size_t newIndex) {
  auto it = std::find_if(m_tracks.begin(), m_tracks.end(),
                         [&trackId](const std::unique_ptr<TimelineTrack>& track) {
                           return track->getId() == trackId;
                         });

  if (it != m_tracks.end() && newIndex < m_tracks.size()) {
    auto track = std::move(*it);
    m_tracks.erase(it);

    auto insertPos = m_tracks.begin() + static_cast<std::ptrdiff_t>(newIndex);
    m_tracks.insert(insertPos, std::move(track));
  }
}

TimelineTrack* Timeline::getTrack(const std::string& trackId) {
  for (auto& track : m_tracks) {
    if (track->getId() == trackId) {
      return track.get();
    }
  }
  return nullptr;
}

void Timeline::addMarker(const Marker& marker) {
  auto it = std::lower_bound(m_markers.begin(), m_markers.end(), marker,
                             [](const Marker& a, const Marker& b) { return a.time < b.time; });
  m_markers.insert(it, marker);
}

void Timeline::removeMarker(f64 time) {
  m_markers.erase(
      std::remove_if(m_markers.begin(), m_markers.end(),
                     [time](const Marker& m) { return std::abs(m.time - time) < 0.0001; }),
      m_markers.end());
}

Result<void> Timeline::save(const std::string& path) {
  // Serialization implementation would go here
  (void)path;
  return Result<void>::ok();
}

Result<std::unique_ptr<Timeline>> Timeline::load(const std::string& path) {
  // Deserialization implementation would go here
  (void)path;
  return Result<std::unique_ptr<Timeline>>::ok(std::make_unique<Timeline>("Loaded"));
}

// =============================================================================
// TimelinePlayback Implementation
// =============================================================================

TimelinePlayback::TimelinePlayback() {}

void TimelinePlayback::setTimeline(Timeline* timeline) {
  m_timeline = timeline;
  stop();
}

void TimelinePlayback::play() {
  if (m_state == State::Playing)
    return;

  m_state = State::Playing;
  if (m_onStateChanged) {
    m_onStateChanged(m_state);
  }
}

void TimelinePlayback::pause() {
  if (m_state != State::Playing)
    return;

  m_state = State::Paused;
  if (m_onStateChanged) {
    m_onStateChanged(m_state);
  }
}

void TimelinePlayback::stop() {
  m_state = State::Stopped;
  m_currentTime = 0.0;

  if (m_onStateChanged) {
    m_onStateChanged(m_state);
  }
  if (m_onTimeChanged) {
    m_onTimeChanged(m_currentTime);
  }
}

void TimelinePlayback::setPlaybackRate(f64 rate) {
  m_playbackRate = rate;
}

void TimelinePlayback::setCurrentTime(f64 time) {
  if (!m_timeline)
    return;

  m_currentTime = std::clamp(time, 0.0, m_timeline->getDuration());

  if (m_onTimeChanged) {
    m_onTimeChanged(m_currentTime);
  }
}

void TimelinePlayback::seekToFrame(i64 frame) {
  if (!m_timeline)
    return;

  f64 time = static_cast<f64>(frame) / m_timeline->getFrameRate();
  setCurrentTime(time);
}

i64 TimelinePlayback::getCurrentFrame() const {
  if (!m_timeline)
    return 0;
  return static_cast<i64>(m_currentTime * m_timeline->getFrameRate());
}

void TimelinePlayback::seekToMarker(const std::string& markerName) {
  if (!m_timeline)
    return;

  for (const auto& marker : m_timeline->getMarkers()) {
    if (marker.name == markerName) {
      setCurrentTime(marker.time);
      return;
    }
  }
}

void TimelinePlayback::seekToNextMarker() {
  if (!m_timeline)
    return;

  for (const auto& marker : m_timeline->getMarkers()) {
    if (marker.time > m_currentTime + 0.001) {
      setCurrentTime(marker.time);
      return;
    }
  }
}

void TimelinePlayback::seekToPreviousMarker() {
  if (!m_timeline)
    return;

  const auto& markers = m_timeline->getMarkers();
  for (auto it = markers.rbegin(); it != markers.rend(); ++it) {
    if (it->time < m_currentTime - 0.001) {
      setCurrentTime(it->time);
      return;
    }
  }
}

void TimelinePlayback::setLoopRange(f64 start, f64 end) {
  m_loopStart = start;
  m_loopEnd = end;
  m_hasLoopRange = true;
}

void TimelinePlayback::clearLoopRange() {
  m_hasLoopRange = false;
}

void TimelinePlayback::update(f64 deltaTime) {
  if (m_state != State::Playing || !m_timeline)
    return;

  f64 oldTime = m_currentTime;
  m_currentTime += deltaTime * m_playbackRate;

  // Check loop
  f64 endTime = m_hasLoopRange ? m_loopEnd : m_timeline->getDuration();
  f64 startTime = m_hasLoopRange ? m_loopStart : 0.0;

  if (m_currentTime >= endTime) {
    if (m_loop) {
      m_currentTime = startTime + std::fmod(m_currentTime - startTime, endTime - startTime);
    } else {
      m_currentTime = endTime;
      stop();
    }
  }

  // Check for markers
  if (m_onMarkerReached) {
    for (const auto& marker : m_timeline->getMarkers()) {
      if (marker.time > oldTime && marker.time <= m_currentTime) {
        m_onMarkerReached(marker);
      }
    }
  }

  if (m_onTimeChanged) {
    m_onTimeChanged(m_currentTime);
  }
}

// =============================================================================
// TimelineEditor Implementation
// =============================================================================

TimelineEditor::TimelineEditor() {}

TimelineEditor::~TimelineEditor() = default;

void TimelineEditor::update(f64 deltaTime) {
  m_playback.update(deltaTime);
  handleInput();
}

void TimelineEditor::render() {
  // Note: isVisible() check removed - EditorPanel base class not yet
  // implemented
  renderTimeline();
}

void TimelineEditor::onResize(i32 width, i32 height) {
  // Note: m_width and m_height not yet implemented - EditorPanel base class
  // missing
  (void)width;
  (void)height;
}

void TimelineEditor::setTimeline(Timeline* timeline) {
  m_timeline = timeline;
  m_playback.setTimeline(timeline);
  m_selectedClips.clear();
  m_selectedTrack.clear();
}

void TimelineEditor::newTimeline() {
  // Create new timeline (owned externally)
}

Result<void> TimelineEditor::openTimeline(const std::string& path) {
  auto result = Timeline::load(path);
  if (result.isError()) {
    return Result<void>::error(result.error());
  }
  // Timeline ownership would be transferred externally
  return Result<void>::ok();
}

Result<void> TimelineEditor::saveTimeline(const std::string& path) {
  if (!m_timeline) {
    return Result<void>::error("No timeline to save");
  }
  return m_timeline->save(path);
}

void TimelineEditor::selectClip(const std::string& clipId) {
  if (std::find(m_selectedClips.begin(), m_selectedClips.end(), clipId) == m_selectedClips.end()) {
    m_selectedClips.push_back(clipId);
  }

  if (m_onClipSelected && m_timeline) {
    for (const auto& track : m_timeline->getTracks()) {
      if (auto* clip = track->getClip(clipId)) {
        m_onClipSelected(clip);
        break;
      }
    }
  }
}

void TimelineEditor::selectTrack(const std::string& trackId) {
  m_selectedTrack = trackId;
}

void TimelineEditor::clearSelection() {
  m_selectedClips.clear();
  m_selectedTrack.clear();
}

void TimelineEditor::frameAll() {
  if (!m_timeline)
    return;

  m_scrollX = 0;
  // Note: m_width not yet available - EditorPanel base class missing
  // Using default zoom of 1.0 instead
  m_zoom = 1.0;
}

void TimelineEditor::frameSelection() {
  // Focus on selected clips
}

void TimelineEditor::setOnClipSelected(std::function<void(TimelineClip*)> callback) {
  m_onClipSelected = std::move(callback);
}

void TimelineEditor::setOnTimelineModified(std::function<void()> callback) {
  m_onTimelineModified = std::move(callback);
}

void TimelineEditor::renderTimeline() {
  renderTimeRuler();
  renderTrackHeaders();
  renderTrackContents();
  renderPlayhead();

  if (m_showCurveEditor) {
    renderCurveEditor();
  }
}

void TimelineEditor::renderTrackHeaders() {
  // Track header rendering implementation
}

void TimelineEditor::renderTrackContents() {
  if (!m_timeline)
    return;

  // Render each track's clips
  for (const auto& track : m_timeline->getTracks()) {
    for (const auto& clip : track->getClips()) {
      // Clip rendering implementation
      (void)clip;
    }
  }
}

void TimelineEditor::renderPlayhead() {
  // Playhead rendering implementation
}

void TimelineEditor::renderTimeRuler() {
  // Time ruler rendering implementation
}

void TimelineEditor::renderCurveEditor() {
  // Curve editor sub-panel rendering
}

void TimelineEditor::handleInput() {
  handleDragDrop();
}

void TimelineEditor::handleDragDrop() {
  // Drag and drop implementation
}

f64 TimelineEditor::screenToTime(f32 screenX) const {
  return (static_cast<f64>(screenX) + m_scrollX) / m_zoom;
}

f32 TimelineEditor::timeToScreen(f64 time) const {
  return static_cast<f32>(time * m_zoom - m_scrollX);
}

} // namespace NovelMind::editor
