#pragma once

/**
 * @file timeline_editor.hpp
 * @brief Timeline Editor for NovelMind
 *
 * Professional timeline editor inspired by Unity's Timeline:
 * - Multi-track animation sequencing
 * - Character position/opacity tracks
 * - Dialogue and voice tracks
 * - Background transition tracks
 * - Camera pan/zoom tracks
 * - Keyframe editing with curves
 */

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include "NovelMind/renderer/renderer.hpp"
#include "NovelMind/scene/animation.hpp"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace NovelMind::editor {

// Forward declarations
class TimelineEditor;
class TimelineTrack;
class TimelineClip;

/**
 * @brief Type of timeline track
 */
enum class TrackType : u8 {
  Character,  // Character animation (position, scale, opacity)
  Background, // Background transitions
  Dialogue,   // Dialogue text display
  Voice,      // Voice audio playback
  BGM,        // Background music
  SFX,        // Sound effects
  Camera,     // Camera movement
  Effect,     // Visual effects
  Event,      // Script events
  Group       // Container for child tracks
};

/**
 * @brief Keyframe interpolation type
 */
enum class KeyframeInterpolation : u8 {
  Constant,  // No interpolation (step)
  Linear,    // Linear interpolation
  EaseIn,    // Ease in
  EaseOut,   // Ease out
  EaseInOut, // Ease in and out
  Bezier,    // Custom bezier curve
  Custom     // Custom curve reference
};

/**
 * @brief A single keyframe
 */
struct Keyframe {
  f64 time; // Time in seconds
  std::variant<f32, renderer::Vec2, renderer::Color, std::string> value;
  KeyframeInterpolation interpolation = KeyframeInterpolation::Linear;

  // Bezier tangents (for Bezier interpolation)
  f32 inTangent = 0.0f;
  f32 outTangent = 0.0f;
  f32 inWeight = 0.33f;
  f32 outWeight = 0.33f;

  // Custom curve reference
  std::string customCurveId;

  // UI state
  bool selected = false;
};

/**
 * @brief Property track within a clip
 */
struct PropertyTrack {
  std::string propertyName; // e.g., "position.x", "opacity", "color"
  std::string displayName;  // Human-readable name
  std::vector<Keyframe> keyframes;

  // Track settings
  bool muted = false;
  bool locked = false;
  bool expanded = true;

  // Value range (for UI)
  f32 minValue = 0.0f;
  f32 maxValue = 1.0f;
};

/**
 * @brief A clip on a track (represents a segment of animation/audio/etc)
 */
class TimelineClip {
public:
  TimelineClip(const std::string& id, const std::string& name);
  virtual ~TimelineClip() = default;

  [[nodiscard]] const std::string& getId() const { return m_id; }
  [[nodiscard]] const std::string& getName() const { return m_name; }

  // Timing
  void setStartTime(f64 time) { m_startTime = time; }
  [[nodiscard]] f64 getStartTime() const { return m_startTime; }

  void setDuration(f64 duration) { m_duration = duration; }
  [[nodiscard]] f64 getDuration() const { return m_duration; }

  [[nodiscard]] f64 getEndTime() const { return m_startTime + m_duration; }

  // Clip offset (for trimmed clips)
  void setClipIn(f64 time) { m_clipIn = time; }
  [[nodiscard]] f64 getClipIn() const { return m_clipIn; }

  // Speed/time scale
  void setTimeScale(f64 scale) { m_timeScale = scale; }
  [[nodiscard]] f64 getTimeScale() const { return m_timeScale; }

  // Property tracks
  void addPropertyTrack(const PropertyTrack& track);
  [[nodiscard]] PropertyTrack* getPropertyTrack(const std::string& propertyName);
  [[nodiscard]] const std::vector<PropertyTrack>& getPropertyTracks() const {
    return m_propertyTracks;
  }

  // Keyframe operations
  Result<void> addKeyframe(const std::string& propertyName, const Keyframe& keyframe);
  Result<void> removeKeyframe(const std::string& propertyName, f64 time);
  Result<void> moveKeyframe(const std::string& propertyName, f64 oldTime, f64 newTime);

  // Evaluation
  template <typename T> T evaluate(const std::string& propertyName, f64 time) const;

  // State
  void setMuted(bool muted) { m_muted = muted; }
  [[nodiscard]] bool isMuted() const { return m_muted; }

  void setLocked(bool locked) { m_locked = locked; }
  [[nodiscard]] bool isLocked() const { return m_locked; }

  // UI state
  void setSelected(bool selected) { m_selected = selected; }
  [[nodiscard]] bool isSelected() const { return m_selected; }

  void setColor(const renderer::Color& color) { m_color = color; }
  [[nodiscard]] const renderer::Color& getColor() const { return m_color; }

protected:
  std::string m_id;
  std::string m_name;

  f64 m_startTime = 0.0;
  f64 m_duration = 1.0;
  f64 m_clipIn = 0.0;
  f64 m_timeScale = 1.0;

  std::vector<PropertyTrack> m_propertyTracks;

  bool m_muted = false;
  bool m_locked = false;
  bool m_selected = false;

  renderer::Color m_color = {102, 153, 204, 255}; // Light blue
};

/**
 * @brief Character animation clip
 */
class CharacterClip : public TimelineClip {
public:
  CharacterClip(const std::string& id, const std::string& characterId);

  [[nodiscard]] const std::string& getCharacterId() const { return m_characterId; }

  // Preset animations
  void setExpression(const std::string& expression, f64 time);
  void addPositionKeyframe(f64 time, f32 x, f32 y);
  void addOpacityKeyframe(f64 time, f32 opacity);
  void addScaleKeyframe(f64 time, f32 scaleX, f32 scaleY);

private:
  std::string m_characterId;
};

/**
 * @brief Dialogue clip
 */
class DialogueClip : public TimelineClip {
public:
  DialogueClip(const std::string& id);

  void setSpeaker(const std::string& speakerId) { m_speakerId = speakerId; }
  [[nodiscard]] const std::string& getSpeaker() const { return m_speakerId; }

  void setText(const std::string& text) { m_text = text; }
  [[nodiscard]] const std::string& getText() const { return m_text; }

  void setLocalizationKey(const std::string& key) { m_localizationKey = key; }
  [[nodiscard]] const std::string& getLocalizationKey() const { return m_localizationKey; }

  // Typewriter settings
  void setTypewriterSpeed(f32 charsPerSecond) { m_typewriterSpeed = charsPerSecond; }
  [[nodiscard]] f32 getTypewriterSpeed() const { return m_typewriterSpeed; }

private:
  std::string m_speakerId;
  std::string m_text;
  std::string m_localizationKey;
  f32 m_typewriterSpeed = 30.0f;
};

/**
 * @brief Voice/audio clip
 */
class AudioClip : public TimelineClip {
public:
  enum class AudioType : u8 { Voice, BGM, SFX, Ambient };

  AudioClip(const std::string& id, AudioType type);

  [[nodiscard]] AudioType getAudioType() const { return m_audioType; }

  void setAudioFile(const std::string& path) { m_audioFile = path; }
  [[nodiscard]] const std::string& getAudioFile() const { return m_audioFile; }

  void setVolume(f32 volume) { m_volume = volume; }
  [[nodiscard]] f32 getVolume() const { return m_volume; }

  // Fade settings
  void setFadeIn(f64 duration) { m_fadeIn = duration; }
  [[nodiscard]] f64 getFadeIn() const { return m_fadeIn; }

  void setFadeOut(f64 duration) { m_fadeOut = duration; }
  [[nodiscard]] f64 getFadeOut() const { return m_fadeOut; }

  void setLoop(bool loop) { m_loop = loop; }
  [[nodiscard]] bool isLoop() const { return m_loop; }

private:
  AudioType m_audioType;
  std::string m_audioFile;
  f32 m_volume = 1.0f;
  f64 m_fadeIn = 0.0;
  f64 m_fadeOut = 0.0;
  bool m_loop = false;
};

/**
 * @brief Camera movement clip
 */
class CameraClip : public TimelineClip {
public:
  CameraClip(const std::string& id);

  void addPositionKeyframe(f64 time, f32 x, f32 y);
  void addZoomKeyframe(f64 time, f32 zoom);
  void addRotationKeyframe(f64 time, f32 angle);

  // Shake effect
  void setShake(f64 startTime, f64 duration, f32 intensity);
};

/**
 * @brief A track in the timeline
 */
class TimelineTrack {
public:
  TimelineTrack(const std::string& id, const std::string& name, TrackType type);
  virtual ~TimelineTrack() = default;

  [[nodiscard]] const std::string& getId() const { return m_id; }
  [[nodiscard]] const std::string& getName() const { return m_name; }
  [[nodiscard]] TrackType getType() const { return m_type; }

  // Clips
  void addClip(std::unique_ptr<TimelineClip> clip);
  void removeClip(const std::string& clipId);
  [[nodiscard]] TimelineClip* getClip(const std::string& clipId);
  [[nodiscard]] const std::vector<std::unique_ptr<TimelineClip>>& getClips() const {
    return m_clips;
  }

  // Find clip at time
  [[nodiscard]] TimelineClip* getClipAtTime(f64 time);
  [[nodiscard]] std::vector<TimelineClip*> getClipsInRange(f64 startTime, f64 endTime);

  // Track state
  void setMuted(bool muted) { m_muted = muted; }
  [[nodiscard]] bool isMuted() const { return m_muted; }

  void setLocked(bool locked) { m_locked = locked; }
  [[nodiscard]] bool isLocked() const { return m_locked; }

  void setSolo(bool solo) { m_solo = solo; }
  [[nodiscard]] bool isSolo() const { return m_solo; }

  // UI state
  void setExpanded(bool expanded) { m_expanded = expanded; }
  [[nodiscard]] bool isExpanded() const { return m_expanded; }

  void setHeight(f32 height) { m_height = height; }
  [[nodiscard]] f32 getHeight() const { return m_height; }

  void setColor(const renderer::Color& color) { m_color = color; }
  [[nodiscard]] const renderer::Color& getColor() const { return m_color; }

  // Target binding (e.g., which character this track controls)
  void setTargetId(const std::string& targetId) { m_targetId = targetId; }
  [[nodiscard]] const std::string& getTargetId() const { return m_targetId; }

  // Child tracks (for group tracks)
  void addChildTrack(std::unique_ptr<TimelineTrack> track);
  [[nodiscard]] const std::vector<std::unique_ptr<TimelineTrack>>& getChildTracks() const {
    return m_childTracks;
  }

protected:
  std::string m_id;
  std::string m_name;
  TrackType m_type;
  std::string m_targetId;

  std::vector<std::unique_ptr<TimelineClip>> m_clips;
  std::vector<std::unique_ptr<TimelineTrack>> m_childTracks;

  bool m_muted = false;
  bool m_locked = false;
  bool m_solo = false;
  bool m_expanded = true;

  f32 m_height = 30.0f;
  renderer::Color m_color = {77, 77, 77, 255}; // Dark gray
};

/**
 * @brief Timeline data structure
 */
class Timeline {
public:
  Timeline(const std::string& name);
  ~Timeline() = default;

  [[nodiscard]] const std::string& getName() const { return m_name; }
  void setName(const std::string& name) { m_name = name; }

  // Duration
  [[nodiscard]] f64 getDuration() const { return m_duration; }
  void setDuration(f64 duration) { m_duration = duration; }

  // Frame rate
  [[nodiscard]] f64 getFrameRate() const { return m_frameRate; }
  void setFrameRate(f64 fps) { m_frameRate = fps; }

  // Tracks
  void addTrack(std::unique_ptr<TimelineTrack> track);
  void removeTrack(const std::string& trackId);
  void moveTrack(const std::string& trackId, size_t newIndex);
  [[nodiscard]] TimelineTrack* getTrack(const std::string& trackId);
  [[nodiscard]] const std::vector<std::unique_ptr<TimelineTrack>>& getTracks() const {
    return m_tracks;
  }

  // Markers
  struct Marker {
    f64 time;
    std::string name;
    renderer::Color color;
  };
  void addMarker(const Marker& marker);
  void removeMarker(f64 time);
  [[nodiscard]] const std::vector<Marker>& getMarkers() const { return m_markers; }

  // Serialization
  Result<void> save(const std::string& path);
  static Result<std::unique_ptr<Timeline>> load(const std::string& path);

private:
  std::string m_name;
  f64 m_duration = 60.0;
  f64 m_frameRate = 30.0;

  std::vector<std::unique_ptr<TimelineTrack>> m_tracks;
  std::vector<Marker> m_markers;
};

/**
 * @brief Timeline playback controller
 */
class TimelinePlayback {
public:
  enum class State : u8 { Stopped, Playing, Paused };

  TimelinePlayback();
  ~TimelinePlayback() = default;

  void setTimeline(Timeline* timeline);

  // Playback control
  void play();
  void pause();
  void stop();
  void setPlaybackRate(f64 rate);

  // Seeking
  void setCurrentTime(f64 time);
  [[nodiscard]] f64 getCurrentTime() const { return m_currentTime; }

  void seekToFrame(i64 frame);
  [[nodiscard]] i64 getCurrentFrame() const;

  void seekToMarker(const std::string& markerName);
  void seekToNextMarker();
  void seekToPreviousMarker();

  // State
  [[nodiscard]] State getState() const { return m_state; }
  [[nodiscard]] bool isPlaying() const { return m_state == State::Playing; }

  // Loop mode
  void setLoop(bool loop) { m_loop = loop; }
  [[nodiscard]] bool isLoop() const { return m_loop; }

  void setLoopRange(f64 start, f64 end);
  void clearLoopRange();

  // Update
  void update(f64 deltaTime);

  // Callbacks
  void setOnTimeChanged(std::function<void(f64)> callback) { m_onTimeChanged = callback; }
  void setOnStateChanged(std::function<void(State)> callback) { m_onStateChanged = callback; }
  void setOnMarkerReached(std::function<void(const Timeline::Marker&)> callback) {
    m_onMarkerReached = callback;
  }

private:
  Timeline* m_timeline = nullptr;
  State m_state = State::Stopped;
  f64 m_currentTime = 0.0;
  f64 m_playbackRate = 1.0;
  bool m_loop = false;
  f64 m_loopStart = 0.0;
  f64 m_loopEnd = 0.0;
  bool m_hasLoopRange = false;

  std::function<void(f64)> m_onTimeChanged;
  std::function<void(State)> m_onStateChanged;
  std::function<void(const Timeline::Marker&)> m_onMarkerReached;
};

/**
 * @brief Timeline Editor Panel
 */
class TimelineEditor {
public:
  TimelineEditor();
  ~TimelineEditor();

  void update(f64 deltaTime);
  void render();
  void onResize(i32 width, i32 height);

  // Timeline management
  void setTimeline(Timeline* timeline);
  [[nodiscard]] Timeline* getTimeline() const { return m_timeline; }

  void newTimeline();
  Result<void> openTimeline(const std::string& path);
  Result<void> saveTimeline(const std::string& path);

  // Playback
  [[nodiscard]] TimelinePlayback* getPlayback() { return &m_playback; }

  // Selection
  void selectClip(const std::string& clipId);
  void selectTrack(const std::string& trackId);
  void clearSelection();
  [[nodiscard]] const std::vector<std::string>& getSelectedClips() const { return m_selectedClips; }

  // View settings
  void setZoom(f64 zoom) { m_zoom = zoom; }
  [[nodiscard]] f64 getZoom() const { return m_zoom; }

  void setScrollX(f64 scroll) { m_scrollX = scroll; }
  [[nodiscard]] f64 getScrollX() const { return m_scrollX; }

  void setScrollY(f64 scroll) { m_scrollY = scroll; }
  [[nodiscard]] f64 getScrollY() const { return m_scrollY; }

  void frameAll();
  void frameSelection();

  // Snapping
  void setSnapToGrid(bool snap) { m_snapToGrid = snap; }
  void setSnapToMarkers(bool snap) { m_snapToMarkers = snap; }
  void setSnapToClips(bool snap) { m_snapToClips = snap; }
  void setGridSize(f64 size) { m_gridSize = size; }

  // Callbacks
  void setOnClipSelected(std::function<void(TimelineClip*)> callback);
  void setOnTimelineModified(std::function<void()> callback);

private:
  void renderTimeline();
  void renderTrackHeaders();
  void renderTrackContents();
  void renderPlayhead();
  void renderTimeRuler();
  void renderCurveEditor();

  void handleInput();
  void handleDragDrop();

  f64 screenToTime(f32 screenX) const;
  f32 timeToScreen(f64 time) const;

  Timeline* m_timeline = nullptr;
  TimelinePlayback m_playback;

  // View state
  f64 m_zoom = 1.0;
  f64 m_scrollX = 0.0;
  f64 m_scrollY = 0.0;

  // Selection
  std::vector<std::string> m_selectedClips;
  std::string m_selectedTrack;

  // Snapping
  bool m_snapToGrid = true;
  bool m_snapToMarkers = true;
  bool m_snapToClips = true;
  f64 m_gridSize = 0.1; // seconds

  // Curve editor state
  bool m_showCurveEditor = false;
  std::string m_curveEditorPropertyName;

  // Panel state (would come from EditorPanel base class)
  bool m_visible = true;

  bool isVisible() const { return m_visible; }

  // Callbacks
  std::function<void(TimelineClip*)> m_onClipSelected;
  std::function<void()> m_onTimelineModified;
};

} // namespace NovelMind::editor
