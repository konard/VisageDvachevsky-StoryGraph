#pragma once

/**
 * @file camera.hpp
 * @brief 2D Camera System for NovelMind
 *
 * Provides professional camera functionality:
 * - Pan and zoom with smooth transitions
 * - Parallax layer support
 * - Camera shake effects
 * - Cinematic movement with easing
 * - Focus tracking
 */

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include "NovelMind/renderer/renderer.hpp"
#include "NovelMind/scene/animation.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace NovelMind::renderer {

// Forward declarations
class Camera2D;

/**
 * @brief Parallax layer configuration
 */
struct ParallaxLayer {
  std::string id;
  f32 depth = 1.0f; // 1.0 = moves with camera, 0.0 = stationary, >1.0 = foreground
  f32 offsetX = 0.0f;
  f32 offsetY = 0.0f;
  bool repeatX = false;
  bool repeatY = false;
};

/**
 * @brief Camera shake configuration
 */
struct CameraShake {
  f32 intensity = 10.0f; // Pixels of displacement
  f32 frequency = 20.0f; // Shakes per second
  f32 duration = 0.5f;   // Total duration
  f32 damping = 2.0f;    // Decay rate

  // Directional shake
  bool horizontalOnly = false;
  bool verticalOnly = false;

  // Trauma-based shake
  bool useTrauma = false; // Use trauma system instead of fixed intensity
};

/**
 * @brief Camera bounds/limits
 */
struct CameraBounds {
  bool enabled = false;
  f32 minX = 0.0f;
  f32 maxX = 1920.0f;
  f32 minY = 0.0f;
  f32 maxY = 1080.0f;

  // Soft bounds (allows slight overshoot with elastic return)
  bool softBounds = false;
  f32 softness = 0.2f;
};

/**
 * @brief Camera movement path point
 */
struct CameraPathPoint {
  Vec2 position;
  f32 zoom = 1.0f;
  f32 rotation = 0.0f;
  f32 holdTime = 0.0f; // Time to hold at this point
  scene::EaseType easing = scene::EaseType::EaseInOutQuad;
};

/**
 * @brief Camera movement path
 */
class CameraPath {
public:
  CameraPath();
  ~CameraPath() = default;

  void addPoint(const CameraPathPoint& point);
  void removePoint(size_t index);
  void updatePoint(size_t index, const CameraPathPoint& point);
  void clear();

  [[nodiscard]] const std::vector<CameraPathPoint>& getPoints() const { return m_points; }
  [[nodiscard]] size_t getPointCount() const { return m_points.size(); }

  [[nodiscard]] f32 getTotalDuration() const { return m_totalDuration; }
  void setTotalDuration(f32 duration) { m_totalDuration = duration; }

  // Evaluation
  [[nodiscard]] Vec2 evaluatePosition(f32 t) const;
  [[nodiscard]] f32 evaluateZoom(f32 t) const;
  [[nodiscard]] f32 evaluateRotation(f32 t) const;

  // Looping
  void setLoop(bool loop) { m_loop = loop; }
  [[nodiscard]] bool isLoop() const { return m_loop; }

private:
  std::vector<CameraPathPoint> m_points;
  f32 m_totalDuration = 1.0f;
  bool m_loop = false;
};

/**
 * @brief Camera target for follow behavior
 */
struct CameraTarget {
  std::string objectId;       // Object to follow
  Vec2 offset = {0.0f, 0.0f}; // Offset from target
  f32 lookaheadX = 0.0f;      // Look ahead based on velocity
  f32 lookaheadY = 0.0f;
  bool smoothFollow = true;
  f32 smoothSpeed = 5.0f;
};

/**
 * @brief 2D Camera
 *
 * Provides comprehensive 2D camera functionality for visual novels:
 * - Smooth pan and zoom transitions
 * - Parallax scrolling support
 * - Screen shake effects
 * - Cinematic camera paths
 * - Focus tracking
 */
class Camera2D {
public:
  Camera2D();
  ~Camera2D() = default;

  // Position
  void setPosition(f32 x, f32 y);
  void setPosition(const Vec2& pos);
  [[nodiscard]] Vec2 getPosition() const { return m_position; }

  void move(f32 dx, f32 dy);
  void moveTo(f32 x, f32 y, f32 duration, scene::EaseType easing = scene::EaseType::EaseInOutQuad);

  // Zoom
  void setZoom(f32 zoom);
  [[nodiscard]] f32 getZoom() const { return m_zoom; }

  void zoomTo(f32 zoom, f32 duration, scene::EaseType easing = scene::EaseType::EaseInOutQuad);
  void zoomAt(f32 zoom, f32 x, f32 y, f32 duration,
              scene::EaseType easing = scene::EaseType::EaseInOutQuad);

  // Rotation
  void setRotation(f32 angle);
  [[nodiscard]] f32 getRotation() const { return m_rotation; }

  void rotateTo(f32 angle, f32 duration, scene::EaseType easing = scene::EaseType::EaseInOutQuad);

  // Combined movement
  void transitionTo(f32 x, f32 y, f32 zoom, f32 rotation, f32 duration,
                    scene::EaseType easing = scene::EaseType::EaseInOutQuad);

  // Viewport
  void setViewportSize(f32 width, f32 height);
  [[nodiscard]] Vec2 getViewportSize() const { return m_viewportSize; }

  // Bounds
  void setBounds(const CameraBounds& bounds);
  [[nodiscard]] const CameraBounds& getBounds() const { return m_bounds; }

  void clearBounds();

  // Shake
  void shake(const CameraShake& shake);
  void shake(f32 intensity, f32 duration);
  void addTrauma(f32 amount); // Trauma-based shake
  void stopShake();
  [[nodiscard]] bool isShaking() const { return m_shakeActive; }

  // Path following
  void followPath(const CameraPath& path);
  void stopPath();
  [[nodiscard]] bool isFollowingPath() const { return m_pathActive; }

  // Target following
  void setTarget(const CameraTarget& target);
  void clearTarget();
  [[nodiscard]] bool hasTarget() const { return m_hasTarget; }

  using TargetResolver = std::function<std::optional<Vec2>(const std::string& objectId)>;
  void setTargetResolver(TargetResolver resolver);

  // Parallax
  void addParallaxLayer(const ParallaxLayer& layer);
  void removeParallaxLayer(const std::string& layerId);
  void updateParallaxLayer(const std::string& layerId, const ParallaxLayer& layer);
  [[nodiscard]] const ParallaxLayer* getParallaxLayer(const std::string& layerId) const;
  [[nodiscard]] std::vector<ParallaxLayer> getParallaxLayers() const;

  // Get offset for a parallax layer
  [[nodiscard]] Vec2 getParallaxOffset(const std::string& layerId) const;
  [[nodiscard]] Vec2 getParallaxOffset(f32 depth) const;

  // Update
  void update(f64 deltaTime);

  // Reset
  void reset();

  // Transform calculation
  [[nodiscard]] Transform2D getViewTransform() const;
  [[nodiscard]] Rect getViewBounds() const;

  // Coordinate conversion
  [[nodiscard]] Vec2 screenToWorld(f32 screenX, f32 screenY) const;
  [[nodiscard]] Vec2 worldToScreen(f32 worldX, f32 worldY) const;

  // Visibility check
  [[nodiscard]] bool isVisible(const Rect& bounds) const;
  [[nodiscard]] bool isVisible(f32 x, f32 y, f32 width, f32 height) const;

  // State
  [[nodiscard]] bool isTransitioning() const { return m_isTransitioning; }

  // Callbacks
  void setOnTransitionComplete(std::function<void()> callback);

private:
  void updateTransitions(f64 deltaTime);
  void updateShake(f64 deltaTime);
  void updatePath(f64 deltaTime);
  void updateTarget(f64 deltaTime);
  void applyBounds();

  Vec2 calculateShakeOffset() const;

  // Core state
  Vec2 m_position = {0.0f, 0.0f};
  f32 m_zoom = 1.0f;
  f32 m_rotation = 0.0f;
  Vec2 m_viewportSize = {1920.0f, 1080.0f};

  // Bounds
  CameraBounds m_bounds;

  // Transition state
  bool m_isTransitioning = false;
  Vec2 m_startPosition;
  Vec2 m_targetPosition;
  f32 m_startZoom = 1.0f;
  f32 m_targetZoom = 1.0f;
  f32 m_startRotation = 0.0f;
  f32 m_targetRotation = 0.0f;
  f32 m_transitionDuration = 0.0f;
  f32 m_transitionElapsed = 0.0f;
  scene::EaseType m_transitionEasing = scene::EaseType::Linear;

  // Shake state
  bool m_shakeActive = false;
  CameraShake m_currentShake;
  f32 m_shakeElapsed = 0.0f;
  f32 m_trauma = 0.0f; // Trauma-based shake
  std::mt19937 m_shakeRng;
  f32 m_shakePhaseX = 0.0f;
  f32 m_shakePhaseY = 0.0f;

  // Path following state
  bool m_pathActive = false;
  CameraPath m_currentPath;
  f32 m_pathElapsed = 0.0f;

  // Target following state
  bool m_hasTarget = false;
  CameraTarget m_target;
  Vec2 m_targetLastPosition = {0.0f, 0.0f};
  Vec2 m_targetVelocity = {0.0f, 0.0f};

  // Parallax layers
  std::vector<ParallaxLayer> m_parallaxLayers;

  // Target resolution
  TargetResolver m_targetResolver;

  // Callbacks
  std::function<void()> m_onTransitionComplete;
};

/**
 * @brief Camera Manager for managing multiple cameras
 */
class CameraManager {
public:
  CameraManager();
  ~CameraManager() = default;

  // Camera management
  void addCamera(const std::string& name, std::unique_ptr<Camera2D> camera);
  void removeCamera(const std::string& name);
  [[nodiscard]] Camera2D* getCamera(const std::string& name);
  [[nodiscard]] std::vector<std::string> getCameraNames() const;

  // Active camera
  void setActiveCamera(const std::string& name);
  [[nodiscard]] Camera2D* getActiveCamera();
  [[nodiscard]] const std::string& getActiveCameraName() const { return m_activeCameraName; }

  // Camera transitions
  void transitionToCamera(const std::string& cameraName, f32 duration,
                          scene::EaseType easing = scene::EaseType::EaseInOutQuad);
  [[nodiscard]] bool isTransitioningBetweenCameras() const { return m_isCameraTransition; }

  // Update all cameras
  void update(f64 deltaTime);

  // Get blended transform during camera transition
  [[nodiscard]] Transform2D getBlendedTransform() const;

private:
  std::unordered_map<std::string, std::unique_ptr<Camera2D>> m_cameras;
  std::string m_activeCameraName;

  // Camera transition state
  bool m_isCameraTransition = false;
  std::string m_transitionFromCamera;
  std::string m_transitionToCamera;
  f32 m_cameraTransitionDuration = 0.0f;
  f32 m_cameraTransitionElapsed = 0.0f;
  scene::EaseType m_cameraTransitionEasing = scene::EaseType::Linear;
};

/**
 * @brief Camera effect presets
 */
namespace CameraPresets {
// Shake presets
CameraShake createLightShake();
CameraShake createMediumShake();
CameraShake createHeavyShake();
CameraShake createExplosionShake();
CameraShake createHeartbeatShake();

// Pan presets
void applySlowPan(Camera2D& camera, f32 targetX, f32 targetY);
void applyCinematicPan(Camera2D& camera, f32 targetX, f32 targetY);
void applyDramaticZoom(Camera2D& camera, f32 targetZoom);
void applyCloseUp(Camera2D& camera, f32 x, f32 y);
void applyEstablishingShot(Camera2D& camera);

// Parallax presets
std::vector<ParallaxLayer> createSimpleParallax();
std::vector<ParallaxLayer> createDeepParallax();
} // namespace CameraPresets

} // namespace NovelMind::renderer
