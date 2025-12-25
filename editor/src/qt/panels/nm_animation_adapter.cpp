/**
 * @file nm_animation_adapter.cpp
 * @brief Animation Adapter implementation
 */

#include "NovelMind/editor/qt/panels/nm_animation_adapter.hpp"
#include "NovelMind/editor/qt/panels/nm_scene_view_panel.hpp"
#include "NovelMind/core/logger.hpp"

namespace NovelMind::editor::qt {

NMAnimationAdapter::NMAnimationAdapter(scene::SceneManager *sceneManager,
                                       QObject *parent)
    : QObject(parent), m_sceneManager(sceneManager) {
  LOG_INFO("[AnimationAdapter] Created");
}

NMAnimationAdapter::~NMAnimationAdapter() {
  cleanupAnimations();
}

void NMAnimationAdapter::connectTimeline(NMTimelinePanel *timeline) {
  if (!timeline) {
    LOG_ERROR("[AnimationAdapter] Cannot connect null timeline");
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

  LOG_INFO("[AnimationAdapter] Connected to Timeline (FPS: {})", m_fps);
}

void NMAnimationAdapter::connectSceneView(NMSceneViewPanel *sceneView) {
  if (!sceneView) {
    LOG_ERROR("[AnimationAdapter] Cannot connect null scene view");
    return;
  }

  m_sceneView = sceneView;

  // Connect adapter signals to scene view
  connect(this, &NMAnimationAdapter::sceneUpdateRequired, m_sceneView,
          [this]() {
            if (m_sceneView) {
              m_sceneView->update();
            }
          });

  LOG_INFO("[AnimationAdapter] Connected to Scene View");
}

bool NMAnimationAdapter::createBinding(const QString &trackId,
                                       const QString &objectId,
                                       AnimatedProperty property) {
  if (trackId.isEmpty() || objectId.isEmpty()) {
    LOG_WARNING("[AnimationAdapter] Cannot create binding with empty IDs");
    return false;
  }

  AnimationBinding binding;
  binding.trackId = trackId;
  binding.objectId = objectId;
  binding.property = property;

  m_bindings[trackId] = binding;

  LOG_INFO("[AnimationAdapter] Created binding: track '{}' -> object '{}' property {}",
           trackId.toStdString(), objectId.toStdString(),
           static_cast<int>(property));

  return true;
}

void NMAnimationAdapter::removeBinding(const QString &trackId) {
  auto it = m_bindings.find(trackId);
  if (it != m_bindings.end()) {
    m_bindings.erase(it);
    LOG_INFO("[AnimationAdapter] Removed binding for track '{}'",
             trackId.toStdString());
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
    LOG_WARNING("[AnimationAdapter] Preview already active");
    return;
  }

  m_isPreviewActive = true;
  LOG_INFO("[AnimationAdapter] Preview started");
  emit previewStarted();
}

void NMAnimationAdapter::stopPreview() {
  if (!m_isPreviewActive) {
    return;
  }

  m_isPreviewActive = false;
  LOG_INFO("[AnimationAdapter] Preview stopped");
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

void NMAnimationAdapter::onKeyframeModified(const QString &trackName, int frame) {
  LOG_DEBUG("[AnimationAdapter] Keyframe modified: track '{}' frame {}",
            trackName.toStdString(), frame);

  // Rebuild animations for this track
  // For now, we'll just update the current frame
  if (m_timeline) {
    onTimelineFrameChanged(m_timeline->getFPS());
  }
}

void NMAnimationAdapter::rebuildAnimations() {
  if (!m_timeline) {
    LOG_WARNING("[AnimationAdapter] Cannot rebuild animations without timeline");
    return;
  }

  LOG_INFO("[AnimationAdapter] Rebuilding animations");

  // Clear existing animation states
  m_animationStates.clear();

  // Build animations for each bound track
  const auto &tracks = m_timeline->getTracks();
  for (const auto &[trackName, track] : tracks) {
    // Check if this track has a binding
    auto bindingIt = m_bindings.find(trackName);
    if (bindingIt == m_bindings.end()) {
      continue;
    }

    const AnimationBinding &binding = bindingIt->second;

    // Build animation timeline from track
    auto timeline = buildAnimationFromTrack(track, binding);
    if (timeline) {
      AnimationPlaybackState state;
      state.timeline = std::move(timeline);
      state.binding = binding;
      state.duration = static_cast<f64>(track->keyframes.size() > 0
                                            ? track->keyframes.last().frame
                                            : 0) /
                       static_cast<f64>(m_fps);

      m_animationStates[trackName] = std::move(state);

      LOG_INFO("[AnimationAdapter] Built animation for track '{}'",
               trackName.toStdString());
    }
  }
}

std::unique_ptr<scene::AnimationTimeline>
NMAnimationAdapter::buildAnimationFromTrack(TimelineTrack *track,
                                            const AnimationBinding &binding) {
  if (!track || track->keyframes.isEmpty()) {
    return nullptr;
  }

  // For now, return nullptr as we'll handle interpolation directly in
  // seekToTime This is a simplified approach that doesn't use the full
  // AnimationTimeline system
  return nullptr;
}

std::unique_ptr<scene::Tween>
NMAnimationAdapter::createTweenForProperty(const AnimationBinding &binding,
                                          const Keyframe &kf1,
                                          const Keyframe &kf2, f32 duration) {
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

  // Apply value to scene object based on property type
  // Note: We need access to the scene graphics scene
  // For now, we'll log the values. Full implementation would need
  // NMSceneViewPanel to expose its scene management methods

  LOG_DEBUG("[AnimationAdapter] Applying animation: object '{}' property {} = {}",
            binding.objectId.toStdString(), static_cast<int>(binding.property),
            value.toString().toStdString());

  // TODO: Once NMSceneViewPanel exposes scene object setters, apply values here
  // Example:
  // if (binding.property == AnimatedProperty::PositionX) {
  //   m_sceneView->setObjectPositionX(binding.objectId, value.toDouble());
  // }
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
  LOG_INFO("[AnimationAdapter] Cleaned up animations");
}

} // namespace NovelMind::editor::qt
