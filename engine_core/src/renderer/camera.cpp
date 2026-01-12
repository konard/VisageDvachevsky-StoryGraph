/**
 * @file camera.cpp
 * @brief Implementation of 2D Camera System for NovelMind
 */

#include "NovelMind/renderer/camera.hpp"
#include "NovelMind/core/logger.hpp"
#include <algorithm>
#include <cmath>

namespace NovelMind::renderer {

// =============================================================================
// CameraPath Implementation
// =============================================================================

CameraPath::CameraPath() {}

void CameraPath::addPoint(const CameraPathPoint& point) {
  m_points.push_back(point);
}

void CameraPath::removePoint(size_t index) {
  if (index < m_points.size()) {
    m_points.erase(m_points.begin() + static_cast<std::ptrdiff_t>(index));
  }
}

void CameraPath::updatePoint(size_t index, const CameraPathPoint& point) {
  if (index < m_points.size()) {
    m_points[index] = point;
  }
}

void CameraPath::clear() {
  m_points.clear();
}

Vec2 CameraPath::evaluatePosition(f32 t) const {
  // Defensive check: need at least 2 points for interpolation
  if (m_points.size() < 2) {
    return m_points.empty() ? Vec2{0.0f, 0.0f} : m_points[0].position;
  }

  f32 totalTime = m_totalDuration;
  if (m_loop && t > totalTime) {
    t = std::fmod(t, totalTime);
  }
  t = std::clamp(t, 0.0f, totalTime);

  // Now safe: m_points.size() >= 2 guaranteed
  f32 segmentDuration = totalTime / static_cast<f32>(m_points.size() - 1);

  // Protect against division by zero if segmentDuration is 0
  if (segmentDuration <= 0.0f) {
    return m_points[0].position;
  }

  size_t index = static_cast<size_t>(t / segmentDuration);
  // Bounds check for segment index
  if (index >= m_points.size() - 1) {
    index = m_points.size() - 2;
  }

  f32 localT = (t - static_cast<f32>(index) * segmentDuration) / segmentDuration;
  localT = std::clamp(localT, 0.0f, 1.0f); // Safety clamp

  const auto& p0 = m_points[index];
  const auto& p1 = m_points[index + 1];

  // Apply easing
  localT = scene::ease(p1.easing, localT);

  return {p0.position.x + (p1.position.x - p0.position.x) * localT,
          p0.position.y + (p1.position.y - p0.position.y) * localT};
}

f32 CameraPath::evaluateZoom(f32 t) const {
  // Defensive check: need at least 2 points for interpolation
  if (m_points.size() < 2) {
    return m_points.empty() ? 1.0f : m_points[0].zoom;
  }

  f32 totalTime = m_totalDuration;
  t = std::clamp(t, 0.0f, totalTime);

  // Now safe: m_points.size() >= 2 guaranteed
  f32 segmentDuration = totalTime / static_cast<f32>(m_points.size() - 1);

  // Protect against division by zero if segmentDuration is 0
  if (segmentDuration <= 0.0f) {
    return m_points[0].zoom;
  }

  size_t index = static_cast<size_t>(t / segmentDuration);
  // Bounds check for segment index
  if (index >= m_points.size() - 1) {
    index = m_points.size() - 2;
  }

  f32 localT = (t - static_cast<f32>(index) * segmentDuration) / segmentDuration;
  localT = std::clamp(localT, 0.0f, 1.0f); // Safety clamp

  const auto& p0 = m_points[index];
  const auto& p1 = m_points[index + 1];

  localT = scene::ease(p1.easing, localT);

  return p0.zoom + (p1.zoom - p0.zoom) * localT;
}

f32 CameraPath::evaluateRotation(f32 t) const {
  // Defensive check: need at least 2 points for interpolation
  if (m_points.size() < 2) {
    return m_points.empty() ? 0.0f : m_points[0].rotation;
  }

  f32 totalTime = m_totalDuration;
  t = std::clamp(t, 0.0f, totalTime);

  // Now safe: m_points.size() >= 2 guaranteed
  f32 segmentDuration = totalTime / static_cast<f32>(m_points.size() - 1);

  // Protect against division by zero if segmentDuration is 0
  if (segmentDuration <= 0.0f) {
    return m_points[0].rotation;
  }

  size_t index = static_cast<size_t>(t / segmentDuration);
  // Bounds check for segment index
  if (index >= m_points.size() - 1) {
    index = m_points.size() - 2;
  }

  f32 localT = (t - static_cast<f32>(index) * segmentDuration) / segmentDuration;
  localT = std::clamp(localT, 0.0f, 1.0f); // Safety clamp

  const auto& p0 = m_points[index];
  const auto& p1 = m_points[index + 1];

  localT = scene::ease(p1.easing, localT);

  return p0.rotation + (p1.rotation - p0.rotation) * localT;
}

// =============================================================================
// Camera2D Implementation
// =============================================================================

Camera2D::Camera2D() : m_shakeRng(std::random_device{}()) {}

void Camera2D::setPosition(f32 x, f32 y) {
  m_position.x = x;
  m_position.y = y;
  applyBounds();
}

void Camera2D::setPosition(const Vec2& pos) {
  setPosition(pos.x, pos.y);
}

void Camera2D::move(f32 dx, f32 dy) {
  setPosition(m_position.x + dx, m_position.y + dy);
}

void Camera2D::moveTo(f32 x, f32 y, f32 duration, scene::EaseType easing) {
  m_startPosition = m_position;
  m_targetPosition = {x, y};
  m_transitionDuration = duration;
  m_transitionElapsed = 0.0f;
  m_transitionEasing = easing;
  m_isTransitioning = true;
}

void Camera2D::setZoom(f32 zoom) {
  m_zoom = std::max(0.01f, zoom);
}

void Camera2D::zoomTo(f32 zoom, f32 duration, scene::EaseType easing) {
  m_startZoom = m_zoom;
  m_targetZoom = zoom;
  m_transitionDuration = duration;
  m_transitionElapsed = 0.0f;
  m_transitionEasing = easing;
  m_isTransitioning = true;
}

void Camera2D::zoomAt(f32 zoom, f32 x, f32 y, f32 duration, scene::EaseType easing) {
  // Calculate position to keep point (x, y) at the same screen position
  f32 currentZoom = m_zoom;
  f32 dx = (x - m_position.x) * (1.0f - currentZoom / zoom);
  f32 dy = (y - m_position.y) * (1.0f - currentZoom / zoom);

  m_startPosition = m_position;
  m_targetPosition = {m_position.x + dx, m_position.y + dy};
  m_startZoom = currentZoom;
  m_targetZoom = zoom;
  m_transitionDuration = duration;
  m_transitionElapsed = 0.0f;
  m_transitionEasing = easing;
  m_isTransitioning = true;
}

void Camera2D::setRotation(f32 angle) {
  m_rotation = angle;
}

void Camera2D::rotateTo(f32 angle, f32 duration, scene::EaseType easing) {
  m_startRotation = m_rotation;
  m_targetRotation = angle;
  m_transitionDuration = duration;
  m_transitionElapsed = 0.0f;
  m_transitionEasing = easing;
  m_isTransitioning = true;
}

void Camera2D::transitionTo(f32 x, f32 y, f32 zoom, f32 rotation, f32 duration,
                            scene::EaseType easing) {
  m_startPosition = m_position;
  m_targetPosition = {x, y};
  m_startZoom = m_zoom;
  m_targetZoom = zoom;
  m_startRotation = m_rotation;
  m_targetRotation = rotation;
  m_transitionDuration = duration;
  m_transitionElapsed = 0.0f;
  m_transitionEasing = easing;
  m_isTransitioning = true;
}

void Camera2D::setViewportSize(f32 width, f32 height) {
  m_viewportSize = {width, height};
}

void Camera2D::setBounds(const CameraBounds& bounds) {
  m_bounds = bounds;
  applyBounds();
}

void Camera2D::clearBounds() {
  m_bounds.enabled = false;
}

void Camera2D::shake(const CameraShake& shake) {
  m_currentShake = shake;
  m_shakeElapsed = 0.0f;
  m_shakeActive = true;

  // Randomize initial phase
  std::uniform_real_distribution<f32> phaseDist(0.0f, 6.28318f);
  m_shakePhaseX = phaseDist(m_shakeRng);
  m_shakePhaseY = phaseDist(m_shakeRng);
}

void Camera2D::shake(f32 intensity, f32 duration) {
  CameraShake s;
  s.intensity = intensity;
  s.duration = duration;
  shake(s);
}

void Camera2D::addTrauma(f32 amount) {
  m_trauma = std::min(1.0f, m_trauma + amount);
  if (m_trauma > 0.0f && !m_shakeActive) {
    CameraShake s;
    s.useTrauma = true;
    s.duration = 10.0f; // Long duration, trauma will decay
    shake(s);
  }
}

void Camera2D::stopShake() {
  m_shakeActive = false;
  m_trauma = 0.0f;
}

void Camera2D::followPath(const CameraPath& path) {
  m_currentPath = path;
  m_pathElapsed = 0.0f;
  m_pathActive = true;
}

void Camera2D::stopPath() {
  m_pathActive = false;
}

void Camera2D::setTarget(const CameraTarget& target) {
  m_target = target;
  m_hasTarget = true;
}

void Camera2D::clearTarget() {
  m_hasTarget = false;
}

void Camera2D::setTargetResolver(TargetResolver resolver) {
  m_targetResolver = std::move(resolver);
}

void Camera2D::addParallaxLayer(const ParallaxLayer& layer) {
  m_parallaxLayers.push_back(layer);
}

void Camera2D::removeParallaxLayer(const std::string& layerId) {
  m_parallaxLayers.erase(
      std::remove_if(m_parallaxLayers.begin(), m_parallaxLayers.end(),
                     [&layerId](const ParallaxLayer& l) { return l.id == layerId; }),
      m_parallaxLayers.end());
}

void Camera2D::updateParallaxLayer(const std::string& layerId, const ParallaxLayer& layer) {
  for (auto& l : m_parallaxLayers) {
    if (l.id == layerId) {
      l = layer;
      return;
    }
  }
}

const ParallaxLayer* Camera2D::getParallaxLayer(const std::string& layerId) const {
  for (const auto& l : m_parallaxLayers) {
    if (l.id == layerId) {
      return &l;
    }
  }
  return nullptr;
}

std::vector<ParallaxLayer> Camera2D::getParallaxLayers() const {
  return m_parallaxLayers;
}

Vec2 Camera2D::getParallaxOffset(const std::string& layerId) const {
  const auto* layer = getParallaxLayer(layerId);
  if (!layer)
    return {0.0f, 0.0f};
  return getParallaxOffset(layer->depth);
}

Vec2 Camera2D::getParallaxOffset(f32 depth) const {
  return {m_position.x * (1.0f - depth), m_position.y * (1.0f - depth)};
}

void Camera2D::update(f64 deltaTime) {
  updateTransitions(deltaTime);
  updatePath(deltaTime);
  updateTarget(deltaTime);
  updateShake(deltaTime);
  applyBounds();
}

void Camera2D::reset() {
  m_position = {0.0f, 0.0f};
  m_zoom = 1.0f;
  m_rotation = 0.0f;
  m_isTransitioning = false;
  m_shakeActive = false;
  m_pathActive = false;
  m_hasTarget = false;
  m_trauma = 0.0f;
}

Transform2D Camera2D::getViewTransform() const {
  Vec2 shakeOffset = calculateShakeOffset();

  Transform2D transform;
  transform.x = m_position.x + shakeOffset.x;
  transform.y = m_position.y + shakeOffset.y;
  transform.scaleX = m_zoom;
  transform.scaleY = m_zoom;
  transform.rotation = m_rotation;
  return transform;
}

Rect Camera2D::getViewBounds() const {
  f32 halfWidth = m_viewportSize.x / (2.0f * m_zoom);
  f32 halfHeight = m_viewportSize.y / (2.0f * m_zoom);

  return {m_position.x - halfWidth, m_position.y - halfHeight, halfWidth * 2.0f, halfHeight * 2.0f};
}

Vec2 Camera2D::screenToWorld(f32 screenX, f32 screenY) const {
  f32 worldX = (screenX - m_viewportSize.x * 0.5f) / m_zoom + m_position.x;
  f32 worldY = (screenY - m_viewportSize.y * 0.5f) / m_zoom + m_position.y;
  return {worldX, worldY};
}

Vec2 Camera2D::worldToScreen(f32 worldX, f32 worldY) const {
  f32 screenX = (worldX - m_position.x) * m_zoom + m_viewportSize.x * 0.5f;
  f32 screenY = (worldY - m_position.y) * m_zoom + m_viewportSize.y * 0.5f;
  return {screenX, screenY};
}

bool Camera2D::isVisible(const Rect& bounds) const {
  return isVisible(bounds.x, bounds.y, bounds.width, bounds.height);
}

bool Camera2D::isVisible(f32 x, f32 y, f32 width, f32 height) const {
  Rect viewBounds = getViewBounds();

  return !(x + width < viewBounds.x || x > viewBounds.x + viewBounds.width ||
           y + height < viewBounds.y || y > viewBounds.y + viewBounds.height);
}

void Camera2D::setOnTransitionComplete(std::function<void()> callback) {
  m_onTransitionComplete = std::move(callback);
}

void Camera2D::updateTransitions(f64 deltaTime) {
  if (!m_isTransitioning)
    return;

  m_transitionElapsed += static_cast<f32>(deltaTime);
  f32 t = std::min(1.0f, m_transitionElapsed / m_transitionDuration);
  f32 easedT = scene::ease(m_transitionEasing, t);

  m_position.x = m_startPosition.x + (m_targetPosition.x - m_startPosition.x) * easedT;
  m_position.y = m_startPosition.y + (m_targetPosition.y - m_startPosition.y) * easedT;
  m_zoom = m_startZoom + (m_targetZoom - m_startZoom) * easedT;
  m_rotation = m_startRotation + (m_targetRotation - m_startRotation) * easedT;

  if (t >= 1.0f) {
    m_isTransitioning = false;
    if (m_onTransitionComplete) {
      m_onTransitionComplete();
    }
  }
}

void Camera2D::updateShake(f64 deltaTime) {
  if (!m_shakeActive)
    return;

  m_shakeElapsed += static_cast<f32>(deltaTime);

  if (m_currentShake.useTrauma) {
    // Decay trauma over time
    m_trauma = std::max(0.0f, m_trauma - static_cast<f32>(deltaTime) * m_currentShake.damping);
    if (m_trauma <= 0.0f) {
      m_shakeActive = false;
    }
  } else {
    if (m_shakeElapsed >= m_currentShake.duration) {
      m_shakeActive = false;
    }
  }

  // Update phase for smooth noise-like shake
  m_shakePhaseX += static_cast<f32>(deltaTime) * m_currentShake.frequency;
  m_shakePhaseY += static_cast<f32>(deltaTime) * m_currentShake.frequency * 1.3f;
}

void Camera2D::updatePath(f64 deltaTime) {
  if (!m_pathActive)
    return;

  m_pathElapsed += static_cast<f32>(deltaTime);

  m_position = m_currentPath.evaluatePosition(m_pathElapsed);
  m_zoom = m_currentPath.evaluateZoom(m_pathElapsed);
  m_rotation = m_currentPath.evaluateRotation(m_pathElapsed);

  if (m_pathElapsed >= m_currentPath.getTotalDuration()) {
    if (m_currentPath.isLoop()) {
      m_pathElapsed = 0.0f;
    } else {
      m_pathActive = false;
    }
  }
}

void Camera2D::updateTarget(f64 deltaTime) {
  if (!m_hasTarget)
    return;

  if (!m_targetResolver) {
    return;
  }

  auto targetPos = m_targetResolver(m_target.objectId);
  if (!targetPos.has_value()) {
    return;
  }

  Vec2 desired = {targetPos->x + m_target.offset.x, targetPos->y + m_target.offset.y};

  // Estimate velocity for lookahead
  if (deltaTime > 0.0) {
    m_targetVelocity.x = (targetPos->x - m_targetLastPosition.x) / static_cast<f32>(deltaTime);
    m_targetVelocity.y = (targetPos->y - m_targetLastPosition.y) / static_cast<f32>(deltaTime);
  }

  desired.x += m_targetVelocity.x * m_target.lookaheadX;
  desired.y += m_targetVelocity.y * m_target.lookaheadY;

  if (m_target.smoothFollow) {
    f32 t = std::min(1.0f, m_target.smoothSpeed * static_cast<f32>(deltaTime));
    m_position.x += (desired.x - m_position.x) * t;
    m_position.y += (desired.y - m_position.y) * t;
  } else {
    m_position = desired;
  }

  m_targetLastPosition = *targetPos;
}

void Camera2D::applyBounds() {
  if (!m_bounds.enabled)
    return;

  f32 halfWidth = m_viewportSize.x / (2.0f * m_zoom);
  f32 halfHeight = m_viewportSize.y / (2.0f * m_zoom);

  f32 minX = m_bounds.minX + halfWidth;
  f32 maxX = m_bounds.maxX - halfWidth;
  f32 minY = m_bounds.minY + halfHeight;
  f32 maxY = m_bounds.maxY - halfHeight;

  if (m_bounds.softBounds) {
    // Soft bounds with elastic return
    if (m_position.x < minX) {
      m_position.x += (minX - m_position.x) * m_bounds.softness;
    } else if (m_position.x > maxX) {
      m_position.x += (maxX - m_position.x) * m_bounds.softness;
    }

    if (m_position.y < minY) {
      m_position.y += (minY - m_position.y) * m_bounds.softness;
    } else if (m_position.y > maxY) {
      m_position.y += (maxY - m_position.y) * m_bounds.softness;
    }
  } else {
    // Hard bounds
    m_position.x = std::clamp(m_position.x, minX, maxX);
    m_position.y = std::clamp(m_position.y, minY, maxY);
  }
}

Vec2 Camera2D::calculateShakeOffset() const {
  if (!m_shakeActive)
    return {0.0f, 0.0f};

  f32 intensity = m_currentShake.intensity;
  if (m_currentShake.useTrauma) {
    // Trauma-based: shake^2 for more dramatic effect
    intensity *= m_trauma * m_trauma;
  } else {
    // Time-based decay
    f32 decay = 1.0f - (m_shakeElapsed / m_currentShake.duration);
    intensity *= decay;
  }

  f32 offsetX = 0.0f;
  f32 offsetY = 0.0f;

  // Use sin/cos with varying phases for pseudo-random but smooth shake
  if (!m_currentShake.verticalOnly) {
    offsetX = std::sin(m_shakePhaseX) * intensity;
  }
  if (!m_currentShake.horizontalOnly) {
    offsetY = std::cos(m_shakePhaseY) * intensity;
  }

  return {offsetX, offsetY};
}

// =============================================================================
// CameraManager Implementation
// =============================================================================

CameraManager::CameraManager() {
  // Create default camera
  addCamera("main", std::make_unique<Camera2D>());
  setActiveCamera("main");
}

void CameraManager::addCamera(const std::string& name, std::unique_ptr<Camera2D> camera) {
  m_cameras[name] = std::move(camera);
}

void CameraManager::removeCamera(const std::string& name) {
  if (name == m_activeCameraName)
    return; // Can't remove active camera
  m_cameras.erase(name);
}

Camera2D* CameraManager::getCamera(const std::string& name) {
  auto it = m_cameras.find(name);
  if (it != m_cameras.end()) {
    return it->second.get();
  }
  return nullptr;
}

std::vector<std::string> CameraManager::getCameraNames() const {
  std::vector<std::string> names;
  names.reserve(m_cameras.size());
  for (const auto& [name, camera] : m_cameras) {
    names.push_back(name);
  }
  return names;
}

void CameraManager::setActiveCamera(const std::string& name) {
  if (m_cameras.find(name) != m_cameras.end()) {
    m_activeCameraName = name;
  }
}

Camera2D* CameraManager::getActiveCamera() {
  return getCamera(m_activeCameraName);
}

void CameraManager::transitionToCamera(const std::string& cameraName, f32 duration,
                                       scene::EaseType easing) {
  if (m_cameras.find(cameraName) == m_cameras.end())
    return;
  if (cameraName == m_activeCameraName)
    return;

  m_transitionFromCamera = m_activeCameraName;
  m_transitionToCamera = cameraName;
  m_cameraTransitionDuration = duration;
  m_cameraTransitionElapsed = 0.0f;
  m_cameraTransitionEasing = easing;
  m_isCameraTransition = true;
}

void CameraManager::update(f64 deltaTime) {
  // Update all cameras
  for (auto& [name, camera] : m_cameras) {
    camera->update(deltaTime);
  }

  // Update camera transition
  if (m_isCameraTransition) {
    m_cameraTransitionElapsed += static_cast<f32>(deltaTime);
    if (m_cameraTransitionElapsed >= m_cameraTransitionDuration) {
      m_activeCameraName = m_transitionToCamera;
      m_isCameraTransition = false;
    }
  }
}

Transform2D CameraManager::getBlendedTransform() const {
  if (!m_isCameraTransition) {
    auto it = m_cameras.find(m_activeCameraName);
    if (it != m_cameras.end()) {
      return it->second->getViewTransform();
    }
    return Transform2D{};
  }

  // Blend between two cameras
  auto fromIt = m_cameras.find(m_transitionFromCamera);
  auto toIt = m_cameras.find(m_transitionToCamera);

  if (fromIt == m_cameras.end() || toIt == m_cameras.end()) {
    return Transform2D{};
  }

  f32 t = m_cameraTransitionElapsed / m_cameraTransitionDuration;
  t = scene::ease(m_cameraTransitionEasing, t);

  Transform2D from = fromIt->second->getViewTransform();
  Transform2D to = toIt->second->getViewTransform();

  Transform2D blended;
  blended.x = from.x + (to.x - from.x) * t;
  blended.y = from.y + (to.y - from.y) * t;
  blended.scaleX = from.scaleX + (to.scaleX - from.scaleX) * t;
  blended.scaleY = from.scaleY + (to.scaleY - from.scaleY) * t;
  blended.rotation = from.rotation + (to.rotation - from.rotation) * t;

  return blended;
}

// =============================================================================
// Camera Presets
// =============================================================================

namespace CameraPresets {

CameraShake createLightShake() {
  CameraShake s;
  s.intensity = 3.0f;
  s.duration = 0.2f;
  s.frequency = 25.0f;
  return s;
}

CameraShake createMediumShake() {
  CameraShake s;
  s.intensity = 8.0f;
  s.duration = 0.4f;
  s.frequency = 20.0f;
  return s;
}

CameraShake createHeavyShake() {
  CameraShake s;
  s.intensity = 15.0f;
  s.duration = 0.6f;
  s.frequency = 15.0f;
  s.damping = 3.0f;
  return s;
}

CameraShake createExplosionShake() {
  CameraShake s;
  s.intensity = 25.0f;
  s.duration = 1.0f;
  s.frequency = 30.0f;
  s.damping = 4.0f;
  return s;
}

CameraShake createHeartbeatShake() {
  CameraShake s;
  s.intensity = 5.0f;
  s.duration = 0.8f;
  s.frequency = 4.0f;
  s.verticalOnly = true;
  return s;
}

void applySlowPan(Camera2D& camera, f32 targetX, f32 targetY) {
  camera.moveTo(targetX, targetY, 3.0f, scene::EaseType::EaseInOutQuad);
}

void applyCinematicPan(Camera2D& camera, f32 targetX, f32 targetY) {
  camera.moveTo(targetX, targetY, 2.0f, scene::EaseType::EaseOutCubic);
}

void applyDramaticZoom(Camera2D& camera, f32 targetZoom) {
  camera.zoomTo(targetZoom, 1.5f, scene::EaseType::EaseOutQuad);
}

void applyCloseUp(Camera2D& camera, f32 x, f32 y) {
  camera.transitionTo(x, y, 1.5f, 0.0f, 1.0f, scene::EaseType::EaseOutCubic);
}

void applyEstablishingShot(Camera2D& camera) {
  camera.transitionTo(0.0f, 0.0f, 0.8f, 0.0f, 2.0f, scene::EaseType::EaseInOutQuad);
}

std::vector<ParallaxLayer> createSimpleParallax() {
  std::vector<ParallaxLayer> layers;

  ParallaxLayer background;
  background.id = "background";
  background.depth = 0.2f;
  layers.push_back(background);

  ParallaxLayer midground;
  midground.id = "midground";
  midground.depth = 0.5f;
  layers.push_back(midground);

  ParallaxLayer foreground;
  foreground.id = "foreground";
  foreground.depth = 1.2f;
  layers.push_back(foreground);

  return layers;
}

std::vector<ParallaxLayer> createDeepParallax() {
  std::vector<ParallaxLayer> layers;

  ParallaxLayer sky;
  sky.id = "sky";
  sky.depth = 0.0f;
  layers.push_back(sky);

  ParallaxLayer farBackground;
  farBackground.id = "far_background";
  farBackground.depth = 0.1f;
  layers.push_back(farBackground);

  ParallaxLayer background;
  background.id = "background";
  background.depth = 0.3f;
  layers.push_back(background);

  ParallaxLayer midground;
  midground.id = "midground";
  midground.depth = 0.6f;
  layers.push_back(midground);

  ParallaxLayer characters;
  characters.id = "characters";
  characters.depth = 1.0f;
  layers.push_back(characters);

  ParallaxLayer foreground;
  foreground.id = "foreground";
  foreground.depth = 1.3f;
  layers.push_back(foreground);

  ParallaxLayer overlay;
  overlay.id = "overlay";
  overlay.depth = 1.5f;
  layers.push_back(overlay);

  return layers;
}

} // namespace CameraPresets

} // namespace NovelMind::renderer
