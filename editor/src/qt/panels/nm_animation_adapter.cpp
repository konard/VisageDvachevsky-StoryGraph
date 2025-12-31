/**
 * @file nm_animation_adapter.cpp
 * @brief Animation Adapter implementation
 */

#include "NovelMind/editor/qt/panels/nm_animation_adapter.hpp"
#include "NovelMind/core/logger.hpp"
#include "NovelMind/editor/qt/panels/nm_scene_view_panel.hpp"

namespace NovelMind::editor::qt {

NMAnimationAdapter::NMAnimationAdapter(scene::SceneManager *sceneManager,
                                       QObject *parent)
    : QObject(parent), m_sceneManager(sceneManager) {
  NOVELMIND_LOG_INFO("[AnimationAdapter] Created");
}

NMAnimationAdapter::~NMAnimationAdapter() { cleanupAnimations(); }

void NMAnimationAdapter::connectTimeline(NMTimelinePanel *timeline) {
  if (!timeline) {
    NOVELMIND_LOG_ERROR("[AnimationAdapter] Cannot connect null timeline");
    return;
  }

  m_timeline = timeline;

  // Connect timeline signals to adapter slots
  connect(m_timeline, &NMTimelinePanel::frameChanged, this,
          &NMAnimationAdapter::onTimelineFrameChanged);
  connect(m_timeline, &NMTimelinePanel::playbackStateChanged, this,
          &NMAnimationAdapter::onTimelinePlaybackStateChanged);
  connect(m_timeline, &NMTimelinePanel::keyframeModified, this,
          &NMAnimationAdapter::onKeyframeModified);

  // Sync FPS
  m_fps = m_timeline->getFPS();

  NOVELMIND_LOG_INFO(
      std::string("[AnimationAdapter] Connected to Timeline (FPS: ") +
      std::to_string(m_fps) + ")");
}

void NMAnimationAdapter::connectSceneView(NMSceneViewPanel *sceneView) {
  if (!sceneView) {
    NOVELMIND_LOG_ERROR("[AnimationAdapter] Cannot connect null scene view");
    return;
  }

  m_sceneView = sceneView;

  // Connect adapter signals to scene view - trigger viewport refresh for visual
  // updates during animation playback
  connect(this, &NMAnimationAdapter::sceneUpdateRequired, m_sceneView,
          [this]() {
            if (m_sceneView) {
              // Must update the graphics view's viewport, not the dock panel
              // widget. QWidget::update() on the panel doesn't refresh the
              // QGraphicsScene contents.
              if (auto *view = m_sceneView->graphicsView()) {
                if (view->viewport()) {
                  view->viewport()->update();
                }
              }
            }
          });

  NOVELMIND_LOG_INFO("[AnimationAdapter] Connected to Scene View");
}

bool NMAnimationAdapter::createBinding(const QString &trackId,
                                       const QString &objectId,
                                       AnimatedProperty property) {
  if (trackId.isEmpty() || objectId.isEmpty()) {
    NOVELMIND_LOG_WARN(
        "[AnimationAdapter] Cannot create binding with empty IDs");
    return false;
  }

  AnimationBinding binding;
  binding.trackId = trackId;
  binding.objectId = objectId;
  binding.property = property;

  m_bindings[trackId] = binding;

  NOVELMIND_LOG_INFO(
      std::string("[AnimationAdapter] Created binding: track '") +
      trackId.toStdString() + "' -> object '" + objectId.toStdString() +
      "' property " + std::to_string(static_cast<int>(property)));

  return true;
}

void NMAnimationAdapter::removeBinding(const QString &trackId) {
  auto it = m_bindings.find(trackId);
  if (it != m_bindings.end()) {
    m_bindings.erase(it);
    NOVELMIND_LOG_INFO(
        std::string("[AnimationAdapter] Removed binding for track '") +
        trackId.toStdString() + "'");
  }
}

QList<AnimationBinding> NMAnimationAdapter::getBindings() const {
  QList<AnimationBinding> result;
  for (const auto &[key, binding] : m_bindings) {
    result.append(binding);
  }
  return result;
}

void NMAnimationAdapter::startPreview() {
  if (m_isPreviewActive) {
    NOVELMIND_LOG_WARN("[AnimationAdapter] Preview already active");
    return;
  }

  m_isPreviewActive = true;
  NOVELMIND_LOG_INFO("[AnimationAdapter] Preview started");
  emit previewStarted();
}

void NMAnimationAdapter::stopPreview() {
  if (!m_isPreviewActive) {
    return;
  }

  m_isPreviewActive = false;
  NOVELMIND_LOG_INFO("[AnimationAdapter] Preview stopped");
  emit previewStopped();
}

scene::EaseType NMAnimationAdapter::mapEasingType(EasingType timelineEasing) {
  switch (timelineEasing) {
  case EasingType::Linear:
    return scene::EaseType::Linear;
  case EasingType::EaseIn:
  case EasingType::EaseInQuad:
    return scene::EaseType::EaseInQuad;
  case EasingType::EaseOut:
  case EasingType::EaseOutQuad:
    return scene::EaseType::EaseOutQuad;
  case EasingType::EaseInOut:
  case EasingType::EaseInOutQuad:
    return scene::EaseType::EaseInOutQuad;
  case EasingType::EaseInCubic:
    return scene::EaseType::EaseInCubic;
  case EasingType::EaseOutCubic:
    return scene::EaseType::EaseOutCubic;
  case EasingType::EaseInOutCubic:
    return scene::EaseType::EaseInOutCubic;
  case EasingType::EaseInElastic:
    return scene::EaseType::EaseInElastic;
  case EasingType::EaseOutElastic:
    return scene::EaseType::EaseOutElastic;
  case EasingType::EaseInBounce:
    return scene::EaseType::EaseInBounce;
  case EasingType::EaseOutBounce:
    return scene::EaseType::EaseOutBounce;
  case EasingType::Step:
    // No exact step equivalent, use linear
    return scene::EaseType::Linear;
  case EasingType::Custom:
    // For custom curves, default to ease in-out
    return scene::EaseType::EaseInOutQuad;
  default:
    return scene::EaseType::Linear;
  }
}

void NMAnimationAdapter::onTimelineFrameChanged(int frame) {
  if (!m_timeline || !m_sceneView) {
    return;
  }

  // Convert frame to time
  f64 time = static_cast<f64>(frame) / static_cast<f64>(m_fps);
  m_currentTime = time;

  // Apply current frame state to scene
  seekToTime(time);

  // Request scene update
  emit sceneUpdateRequired();
}

void NMAnimationAdapter::onTimelinePlaybackStateChanged(bool playing) {
  if (playing) {
    startPreview();
  } else {
    stopPreview();
  }
}

void NMAnimationAdapter::onKeyframeModified(const QString &trackName,
                                            int frame) {
  NOVELMIND_LOG_DEBUG(
      std::string("[AnimationAdapter] Keyframe modified: track '") +
      trackName.toStdString() + "' frame " + std::to_string(frame));

  // Rebuild animations for this track
  // Update the animation at the modified frame position
  if (m_timeline) {
    onTimelineFrameChanged(frame);
  }
}

void NMAnimationAdapter::rebuildAnimations() {
  if (!m_timeline) {
    NOVELMIND_LOG_WARN(
        "[AnimationAdapter] Cannot rebuild animations without timeline");
    return;
  }

  NOVELMIND_LOG_INFO("[AnimationAdapter] Rebuilding animations");

  // Clear existing animation states
  m_animationStates.clear();

  // Build animations for each bound track
  const auto &tracks = m_timeline->getTracks();
  for (auto it = tracks.constBegin(); it != tracks.constEnd(); ++it) {
    const QString &trackName = it.key();
    TimelineTrack *track = it.value();

    // Check if this track has a binding
    auto bindingIt = m_bindings.find(trackName);
    if (bindingIt == m_bindings.end()) {
      continue;
    }

    const AnimationBinding &binding = bindingIt->second;

    // Build animation timeline from track
    auto animTimeline = buildAnimationFromTrack(track, binding);
    if (animTimeline) {
      AnimationPlaybackState state;
      state.timeline = std::move(animTimeline);
      state.binding = binding;
      state.duration = static_cast<f64>(track->keyframes.size() > 0
                                            ? track->keyframes.last().frame
                                            : 0) /
                       static_cast<f64>(m_fps);

      m_animationStates[trackName] = std::move(state);

      NOVELMIND_LOG_INFO(
          std::string("[AnimationAdapter] Built animation for track '") +
          trackName.toStdString() + "'");
    }
  }
}

std::unique_ptr<scene::AnimationTimeline>
NMAnimationAdapter::buildAnimationFromTrack(
    TimelineTrack *track, [[maybe_unused]] const AnimationBinding &binding) {
  if (!track || track->keyframes.isEmpty()) {
    return nullptr;
  }

  // For now, return nullptr as we'll handle interpolation directly in
  // seekToTime This is a simplified approach that doesn't use the full
  // AnimationTimeline system
  return nullptr;
}

std::unique_ptr<scene::Tween> NMAnimationAdapter::createTweenForProperty(
    [[maybe_unused]] const AnimationBinding &binding,
    [[maybe_unused]] const Keyframe &kf1, [[maybe_unused]] const Keyframe &kf2,
    [[maybe_unused]] f32 duration) {
  // Simplified: we handle tweening directly in interpolation
  return nullptr;
}

void NMAnimationAdapter::applyAnimationToScene(const AnimationBinding &binding,
                                               f64 time) {
  if (!m_sceneView || !m_timeline) {
    return;
  }

  // Get the track
  TimelineTrack *track = m_timeline->getTrack(binding.trackId);
  if (!track) {
    return;
  }

  // Interpolate value at current time
  QVariant value = interpolateTrackValue(track, time);
  if (!value.isValid()) {
    return;
  }

  // Get current object state from scene
  NMSceneObject *obj = m_sceneView->findObjectById(binding.objectId);
  if (!obj) {
    NOVELMIND_LOG_WARN(std::string("[AnimationAdapter] Object not found: ") +
                       binding.objectId.toStdString());
    return;
  }

  // Apply value to scene object based on property type
  bool applied = false;
  switch (binding.property) {
  case AnimatedProperty::PositionX: {
    QPointF currentPos = obj->pos();
    applied = m_sceneView->moveObject(
        binding.objectId, QPointF(value.toDouble(), currentPos.y()));
    break;
  }
  case AnimatedProperty::PositionY: {
    QPointF currentPos = obj->pos();
    applied = m_sceneView->moveObject(
        binding.objectId, QPointF(currentPos.x(), value.toDouble()));
    break;
  }
  case AnimatedProperty::Position: {
    // Expects QPointF value
    if (value.canConvert<QPointF>()) {
      applied = m_sceneView->moveObject(binding.objectId, value.toPointF());
    } else if (value.canConvert<QVariantList>()) {
      QVariantList list = value.toList();
      if (list.size() >= 2) {
        applied = m_sceneView->moveObject(
            binding.objectId, QPointF(list[0].toDouble(), list[1].toDouble()));
      }
    }
    break;
  }
  case AnimatedProperty::ScaleX: {
    qreal currentScaleY = obj->scaleY();
    applied = m_sceneView->scaleObject(binding.objectId, value.toDouble(),
                                       currentScaleY);
    break;
  }
  case AnimatedProperty::ScaleY: {
    qreal currentScaleX = obj->scaleX();
    applied = m_sceneView->scaleObject(binding.objectId, currentScaleX,
                                       value.toDouble());
    break;
  }
  case AnimatedProperty::Scale: {
    // Uniform scale
    qreal scale = value.toDouble();
    applied = m_sceneView->scaleObject(binding.objectId, scale, scale);
    break;
  }
  case AnimatedProperty::Rotation: {
    applied = m_sceneView->rotateObject(binding.objectId, value.toDouble());
    break;
  }
  case AnimatedProperty::Alpha: {
    applied = m_sceneView->setObjectOpacity(binding.objectId, value.toDouble());
    break;
  }
  case AnimatedProperty::Visible: {
    applied = m_sceneView->setObjectVisible(binding.objectId, value.toBool());
    break;
  }
  case AnimatedProperty::Color:
  case AnimatedProperty::Custom:
    // Custom properties not yet supported
    NOVELMIND_LOG_DEBUG(
        "[AnimationAdapter] Custom/Color properties not yet implemented");
    break;
  }

  if (applied) {
    NOVELMIND_LOG_DEBUG(
        std::string("[AnimationAdapter] Applied animation: object '") +
        binding.objectId.toStdString() + "' property " +
        std::to_string(static_cast<int>(binding.property)) + " = " +
        value.toString().toStdString());
  }
}

QVariant NMAnimationAdapter::interpolateTrackValue(TimelineTrack *track,
                                                   f64 time) {
  if (!track || track->keyframes.isEmpty()) {
    return QVariant();
  }

  // Convert time to frame
  int frame = static_cast<int>(time * m_fps);

  // Use the track's interpolate method
  Keyframe interpolated = track->interpolate(frame);
  return interpolated.value;
}

void NMAnimationAdapter::seekToTime(f64 time) {
  if (!m_timeline || !m_sceneView) {
    return;
  }

  // Apply all bound track animations at this time
  for (const auto &[trackId, binding] : m_bindings) {
    applyAnimationToScene(binding, time);
  }
}

void NMAnimationAdapter::cleanupAnimations() {
  m_animationStates.clear();
  m_bindings.clear();
  m_propertyStorage.clear();
  NOVELMIND_LOG_INFO("[AnimationAdapter] Cleaned up animations");
}

} // namespace NovelMind::editor::qt
