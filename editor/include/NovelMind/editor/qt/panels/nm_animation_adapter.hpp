#pragma once

/**
 * @file nm_animation_adapter.hpp
 * @brief Adapter layer between Timeline GUI and engine_core animation system
 *
 * This adapter provides a clean separation between the GUI timeline model
 * and the runtime animation system. It converts Timeline tracks and keyframes
 * into engine_core AnimationTimeline objects for preview playback.
 *
 * Architecture:
 * - Timeline Panel: GUI editing and keyframe management
 * - Animation Adapter: Conversion and synchronization layer (this file)
 * - Engine Core: Runtime animation playback system
 * - Scene View: Visual preview rendering
 */

#include "NovelMind/editor/qt/panels/nm_timeline_panel.hpp"
#include "NovelMind/scene/animation.hpp"
#include "NovelMind/scene/scene_manager.hpp"
#include <QObject>
#include <QString>
#include <QVariant>
#include <memory>
#include <unordered_map>

namespace NovelMind::editor::qt {

// Forward declarations
class NMSceneViewPanel;
class NMTimelinePanel;

/**
 * @brief Property type for animation binding
 */
enum class AnimatedProperty {
  PositionX,
  PositionY,
  Position, // Both X and Y
  ScaleX,
  ScaleY,
  Scale, // Uniform scale
  Rotation,
  Alpha,
  Color,
  Visible,
  Custom
};

/**
 * @brief Animation track binding information
 *
 * Maps a Timeline track to a specific object property in the scene.
 */
struct AnimationBinding {
  QString trackId;            // Timeline track ID
  QString objectId;           // Scene object ID
  AnimatedProperty property;  // Which property to animate
  QString customPropertyName; // For custom properties

  bool isValid() const { return !trackId.isEmpty() && !objectId.isEmpty(); }
};

/**
 * @brief Runtime animation state for a track
 */
struct AnimationPlaybackState {
  std::unique_ptr<scene::AnimationTimeline> timeline;
  AnimationBinding binding;
  bool isPlaying = false;
  f64 currentTime = 0.0;
  f64 duration = 0.0;

  AnimationPlaybackState() = default;
  AnimationPlaybackState(AnimationPlaybackState &&) = default;
  AnimationPlaybackState &operator=(AnimationPlaybackState &&) = default;
  // Prevent copying due to unique_ptr
  AnimationPlaybackState(const AnimationPlaybackState &) = delete;
  AnimationPlaybackState &operator=(const AnimationPlaybackState &) = delete;
};

/**
 * @brief Adapter for Timeline ↔ engine_core animation system integration
 *
 * This class is responsible for:
 * - Converting Timeline tracks to engine_core AnimationTimeline objects
 * - Managing animation playback state for preview
 * - Synchronizing Timeline changes with runtime animations
 * - Applying animated values to scene objects
 *
 * RAII Compliance:
 * - All engine_core objects owned via unique_ptr/shared_ptr
 * - Clean shutdown in destructor
 * - No manual memory management
 */
class NMAnimationAdapter : public QObject {
  Q_OBJECT

public:
  /**
   * @brief Construct animation adapter
   * @param sceneManager Reference to scene manager for object access
   * @param parent Parent QObject
   */
  explicit NMAnimationAdapter(scene::SceneManager *sceneManager,
                              QObject *parent = nullptr);

  /**
   * @brief Destructor - ensures clean shutdown
   */
  ~NMAnimationAdapter() override;

  /**
   * @brief Connect to Timeline panel for synchronization
   * @param timeline Timeline panel to sync with
   */
  void connectTimeline(NMTimelinePanel *timeline);

  /**
   * @brief Connect to Scene View panel for preview rendering
   * @param sceneView Scene view panel to sync with
   */
  void connectSceneView(NMSceneViewPanel *sceneView);

  /**
   * @brief Create animation binding for a track
   * @param trackId Timeline track ID
   * @param objectId Scene object ID
   * @param property Property to animate
   * @return true if binding created successfully
   */
  bool createBinding(const QString &trackId, const QString &objectId,
                     AnimatedProperty property);

  /**
   * @brief Remove animation binding
   * @param trackId Timeline track ID
   */
  void removeBinding(const QString &trackId);

  /**
   * @brief Get all current bindings
   */
  [[nodiscard]] QList<AnimationBinding> getBindings() const;

  /**
   * @brief Start preview playback
   */
  void startPreview();

  /**
   * @brief Stop preview playback
   */
  void stopPreview();

  /**
   * @brief Check if preview is active
   */
  [[nodiscard]] bool isPreviewActive() const { return m_isPreviewActive; }

  /**
   * @brief Set frames per second for time conversion
   */
  void setFPS(int fps) { m_fps = fps; }

  /**
   * @brief Get current FPS
   */
  [[nodiscard]] int getFPS() const { return m_fps; }

  /**
   * @brief Convert easing type from Timeline to engine_core
   */
  [[nodiscard]] static scene::EaseType mapEasingType(EasingType timelineEasing);

signals:
  /**
   * @brief Emitted when scene needs to be redrawn due to animation update
   */
  void sceneUpdateRequired();

  /**
   * @brief Emitted when preview playback starts
   */
  void previewStarted();

  /**
   * @brief Emitted when preview playback stops
   */
  void previewStopped();

  /**
   * @brief Emitted when an error occurs
   */
  void errorOccurred(const QString &message);

public slots:
  /**
   * @brief Handle timeline frame change
   * @param frame Current frame number
   */
  void onTimelineFrameChanged(int frame);

  /**
   * @brief Handle timeline playback state change
   * @param playing true if playing, false if paused/stopped
   */
  void onTimelinePlaybackStateChanged(bool playing);

  /**
   * @brief Handle keyframe modification
   * @param trackName Track name/ID
   * @param frame Frame number
   */
  void onKeyframeModified(const QString &trackName, int frame);

  /**
   * @brief Rebuild animations from timeline data
   */
  void rebuildAnimations();

  /**
   * @brief Handle object deletion - invalidate cache entry
   * @param objectId ID of the deleted object
   */
  void onObjectDeleted(const QString &objectId);

private:
  /**
   * @brief Build animation timeline from a track
   * @param track Timeline track containing keyframes
   * @param binding Animation binding information
   * @return Constructed animation timeline, or nullptr on error
   */
  [[nodiscard]] std::unique_ptr<scene::AnimationTimeline>
  buildAnimationFromTrack(TimelineTrack *track,
                          const AnimationBinding &binding);

  /**
   * @brief Create a tween for a specific property and keyframe segment
   * @param binding Animation binding
   * @param kf1 Start keyframe
   * @param kf2 End keyframe
   * @param duration Duration in seconds
   * @return Created tween, or nullptr on error
   */
  [[nodiscard]] std::unique_ptr<scene::Tween>
  createTweenForProperty(const AnimationBinding &binding, const Keyframe &kf1,
                         const Keyframe &kf2, f32 duration);

  /**
   * @brief Apply animation state to scene object at current time
   * @param binding Animation binding
   * @param time Current time in seconds
   */
  void applyAnimationToScene(const AnimationBinding &binding, f64 time);

  /**
   * @brief Get interpolated value from track at specific time
   * @param track Timeline track
   * @param time Time in seconds
   * @return Interpolated value
   */
  [[nodiscard]] QVariant interpolateTrackValue(TimelineTrack *track, f64 time);

  /**
   * @brief Seek all animations to a specific time
   * @param time Time in seconds
   */
  void seekToTime(f64 time);

  /**
   * @brief Clean up all animation state
   */
  void cleanupAnimations();

  // Dependencies
  scene::SceneManager *m_sceneManager = nullptr;
  NMTimelinePanel *m_timeline = nullptr;
  NMSceneViewPanel *m_sceneView = nullptr;

  // Animation state
  std::unordered_map<QString, AnimationPlaybackState> m_animationStates;
  std::unordered_map<QString, AnimationBinding> m_bindings;
  scene::AnimationManager m_animationManager;

  // Playback state
  bool m_isPreviewActive = false;
  int m_fps = 30;
  f64 m_currentTime = 0.0;

  // Object property storage for animation targets
  // These are used as animation target pointers
  struct PropertyStorage {
    f32 positionX = 0.0f;
    f32 positionY = 0.0f;
    f32 scaleX = 1.0f;
    f32 scaleY = 1.0f;
    f32 rotation = 0.0f;
    f32 alpha = 1.0f;
  };
  std::unordered_map<QString, PropertyStorage> m_propertyStorage;

  // PERF-6: Object pointer cache to avoid repeated findObjectById lookups
  // during animation playback. Cleared when scene changes or preview stops.
  QHash<QString, class NMSceneObject *> m_objectCache;

  // Clear object cache when objects may have changed
  void clearObjectCache() { m_objectCache.clear(); }
};

} // namespace NovelMind::editor::qt
