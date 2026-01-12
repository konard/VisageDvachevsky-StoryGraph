/**
 * @file timeline_track_operations.cpp
 * @brief TimelineTrack implementation for keyframe operations
 *
 * Extracted from timeline_keyframes.cpp to keep files under 500 lines.
 * Handles TimelineTrack class methods including:
 * - Keyframe CRUD operations (add, remove, move, get)
 * - Keyframe interpolation with easing functions
 * - Bezier curve evaluation for custom easing
 * - Selection operations within tracks
 */

#include "NovelMind/editor/qt/panels/nm_timeline_panel.hpp"

#include <algorithm>
#include <climits>
#include <cmath>

namespace NovelMind::editor::qt {

// Forward declaration for easing function
static float applyEasingFunction(float t, EasingType easing);

// Helper function for cubic Bezier curve evaluation
static float evaluateCubicBezier(float t, float p0, float p1, float p2, float p3) {
  // Standard cubic Bezier formula: B(t) = (1-t)^3*P0 + 3*(1-t)^2*t*P1 +
  // 3*(1-t)*t^2*P2 + t^3*P3
  float oneMinusT = 1.0f - t;
  return oneMinusT * oneMinusT * oneMinusT * p0 + 3.0f * oneMinusT * oneMinusT * t * p1 +
         3.0f * oneMinusT * t * t * p2 + t * t * t * p3;
}

// Helper function to find t for a given x in a cubic Bezier curve (Newton's
// method)
static float solveBezierX(float x, float p0x, float p1x, float p2x, float p3x) {
  // Use Newton-Raphson iteration to find t such that Bezier_x(t) = x
  float t = x; // Initial guess
  for (int i = 0; i < 8; ++i) {
    float currentX = evaluateCubicBezier(t, p0x, p1x, p2x, p3x);
    if (std::abs(currentX - x) < 0.001f) {
      break; // Close enough
    }
    // Derivative of cubic Bezier
    float derivative = 3.0f * (1.0f - t) * (1.0f - t) * (p1x - p0x) +
                       6.0f * (1.0f - t) * t * (p2x - p1x) + 3.0f * t * t * (p3x - p2x);
    if (std::abs(derivative) < 0.00001f) {
      break; // Avoid division by zero
    }
    t -= (currentX - x) / derivative;
    t = std::max(0.0f, std::min(1.0f, t)); // Clamp to [0,1]
  }
  return t;
}

// =============================================================================
// TimelineTrack Implementation
// =============================================================================

void TimelineTrack::addKeyframe(int frame, const QVariant& value, EasingType easing) {
  // PERF-1: Use binary search to find insertion point (O(log N) instead of O(N))
  // Keyframes are maintained in sorted order by frame
  auto it = std::lower_bound(keyframes.begin(), keyframes.end(), frame,
                             [](const Keyframe& kf, int f) { return kf.frame < f; });

  // Check if keyframe already exists at this frame
  if (it != keyframes.end() && it->frame == frame) {
    // Update existing keyframe
    // Preserve bezier curve handles when updating value/easing
    // Only reset handles if changing from Custom to non-Custom easing
    if (it->easing == EasingType::Custom && easing != EasingType::Custom) {
      // Transitioning away from custom curve - reset handles
      it->handleInX = 0.0f;
      it->handleInY = 0.0f;
      it->handleOutX = 0.0f;
      it->handleOutY = 0.0f;
    }
    it->value = value;
    it->easing = easing;
    return;
  }

  // Add new keyframe at the sorted position (maintains sort order)
  Keyframe newKeyframe;
  newKeyframe.frame = frame;
  newKeyframe.value = value;
  newKeyframe.easing = easing;
  keyframes.insert(it, newKeyframe);
}

void TimelineTrack::removeKeyframe(int frame) {
  // PERF-1: Use binary search for O(log N) lookup
  auto it = std::lower_bound(keyframes.begin(), keyframes.end(), frame,
                             [](const Keyframe& kf, int f) { return kf.frame < f; });

  if (it != keyframes.end() && it->frame == frame) {
    keyframes.erase(it);
  }
}

void TimelineTrack::moveKeyframe(int fromFrame, int toFrame) {
  Keyframe* kf = getKeyframe(fromFrame);
  if (kf && fromFrame != toFrame) {
    // Store keyframe data including bezier curve handles
    QVariant value = kf->value;
    EasingType easing = kf->easing;
    bool selected = kf->isSelected;
    float handleInX = kf->handleInX;
    float handleInY = kf->handleInY;
    float handleOutX = kf->handleOutX;
    float handleOutY = kf->handleOutY;

    // Remove old keyframe
    removeKeyframe(fromFrame);

    // Add at new position
    addKeyframe(toFrame, value, easing);

    // Restore selection state and bezier handles
    Keyframe* newKf = getKeyframe(toFrame);
    if (newKf) {
      newKf->isSelected = selected;
      newKf->handleInX = handleInX;
      newKf->handleInY = handleInY;
      newKf->handleOutX = handleOutX;
      newKf->handleOutY = handleOutY;
    }
  }
}

Keyframe* TimelineTrack::getKeyframe(int frame) {
  // PERF-1: Use binary search for O(log N) lookup
  auto it = std::lower_bound(keyframes.begin(), keyframes.end(), frame,
                             [](const Keyframe& kf, int f) { return kf.frame < f; });

  if (it != keyframes.end() && it->frame == frame) {
    return &(*it);
  }
  return nullptr;
}

Keyframe TimelineTrack::interpolate(int frame) const {
  if (keyframes.isEmpty()) {
    return Keyframe();
  }

  // If only one keyframe, return it
  if (keyframes.size() == 1) {
    return keyframes.first();
  }

  // PERF-1: Use binary search for O(log N) lookup instead of O(N)
  // Find the first keyframe with frame >= target frame
  auto it = std::lower_bound(keyframes.begin(), keyframes.end(), frame,
                             [](const Keyframe& kf, int f) { return kf.frame < f; });

  // Exact match
  if (it != keyframes.end() && it->frame == frame) {
    return *it;
  }

  // Before first keyframe - return first
  if (it == keyframes.begin()) {
    return keyframes.first();
  }

  // After last keyframe - return last
  if (it == keyframes.end()) {
    return keyframes.last();
  }

  // Find surrounding keyframes for interpolation
  const Keyframe* nextKf = &(*it);
  const Keyframe* prevKf = &(*(it - 1));

  // Interpolate between prevKf and nextKf
  Keyframe result;
  result.frame = frame;
  result.easing = prevKf->easing;

  // Calculate interpolation factor (0 to 1)
  // Safe division with epsilon check to prevent division by zero
  float frameDiff = static_cast<float>(nextKf->frame - prevKf->frame);
  if (frameDiff < 0.0001f) {
    // Same frame or extremely close - no interpolation needed
    return *prevKf;
  }
  float t = static_cast<float>(frame - prevKf->frame) / frameDiff;
  t = std::clamp(t, 0.0f, 1.0f); // Additional safety clamp

  // Apply easing function to t
  double easedT;
  if (prevKf->easing == EasingType::Custom) {
    // Use Bezier curve data from keyframe handles
    // Construct cubic Bezier curve from handles
    // P0 = (0, 0), P1 = (handleOutX, handleOutY)
    // P2 = (1 + handleInX, 1 + handleInY), P3 = (1, 1)
    float p0x = 0.0f, p0y = 0.0f;
    float p1x = prevKf->handleOutX, p1y = prevKf->handleOutY;
    float p2x = 1.0f + nextKf->handleInX, p2y = 1.0f + nextKf->handleInY;
    float p3x = 1.0f, p3y = 1.0f;

    // Solve for the parameter value that gives us the current x position
    float bezierT = solveBezierX(t, p0x, p1x, p2x, p3x);

    // Evaluate the y value at that parameter
    easedT = static_cast<double>(evaluateCubicBezier(bezierT, p0y, p1y, p2y, p3y));
  } else {
    // Use standard easing function
    easedT = static_cast<double>(applyEasingFunction(t, prevKf->easing));
  }

  // Interpolate value based on type
  int typeId = prevKf->value.typeId();

  if (typeId == QMetaType::Double || typeId == QMetaType::Int) {
    // Numeric interpolation
    double startVal = prevKf->value.toDouble();
    double endVal = nextKf->value.toDouble();
    result.value = startVal + (endVal - startVal) * easedT;
  } else if (typeId == QMetaType::QPointF) {
    // Point interpolation
    QPointF startPt = prevKf->value.toPointF();
    QPointF endPt = nextKf->value.toPointF();
    result.value = QPointF(startPt.x() + (endPt.x() - startPt.x()) * easedT,
                           startPt.y() + (endPt.y() - startPt.y()) * easedT);
  } else if (typeId == QMetaType::QColor) {
    // Color interpolation
    QColor startColor = prevKf->value.value<QColor>();
    QColor endColor = nextKf->value.value<QColor>();
    result.value = QColor(
        static_cast<int>(static_cast<double>(startColor.red()) +
                         static_cast<double>(endColor.red() - startColor.red()) * easedT),
        static_cast<int>(static_cast<double>(startColor.green()) +
                         static_cast<double>(endColor.green() - startColor.green()) * easedT),
        static_cast<int>(static_cast<double>(startColor.blue()) +
                         static_cast<double>(endColor.blue() - startColor.blue()) * easedT),
        static_cast<int>(static_cast<double>(startColor.alpha()) +
                         static_cast<double>(endColor.alpha() - startColor.alpha()) * easedT));
  } else {
    // For unsupported types, use step interpolation (use prev value)
    result.value = prevKf->value;
  }

  return result;
}

// Helper function to apply easing
static float applyEasingFunction(float t, EasingType easing) {
  // Clamp t to [0, 1]
  t = std::max(0.0f, std::min(1.0f, t));

  switch (easing) {
  case EasingType::Linear:
    return t;

  case EasingType::EaseIn:
  case EasingType::EaseInQuad:
    return t * t;

  case EasingType::EaseOut:
  case EasingType::EaseOutQuad:
    return t * (2.0f - t);

  case EasingType::EaseInOut:
  case EasingType::EaseInOutQuad:
    return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;

  case EasingType::EaseInCubic:
    return t * t * t;

  case EasingType::EaseOutCubic: {
    float f = t - 1.0f;
    return f * f * f + 1.0f;
  }

  case EasingType::EaseInOutCubic:
    return t < 0.5f ? 4.0f * t * t * t
                    : 1.0f + (t - 1.0f) * (2.0f * (t - 1.0f)) * (2.0f * (t - 1.0f));

  case EasingType::EaseInElastic: {
    if (t == 0.0f || t == 1.0f)
      return t;
    float p = 0.3f;
    return -std::pow(2.0f, 10.0f * (t - 1.0f)) *
           std::sin((t - 1.0f - p / 4.0f) * (2.0f * 3.14159f) / p);
  }

  case EasingType::EaseOutElastic: {
    if (t == 0.0f || t == 1.0f)
      return t;
    float p = 0.3f;
    return std::pow(2.0f, -10.0f * t) * std::sin((t - p / 4.0f) * (2.0f * 3.14159f) / p) + 1.0f;
  }

  case EasingType::EaseInBounce:
    return 1.0f - applyEasingFunction(1.0f - t, EasingType::EaseOutBounce);

  case EasingType::EaseOutBounce: {
    if (t < (1.0f / 2.75f)) {
      return 7.5625f * t * t;
    } else if (t < (2.0f / 2.75f)) {
      t -= 1.5f / 2.75f;
      return 7.5625f * t * t + 0.75f;
    } else if (t < (2.5f / 2.75f)) {
      t -= 2.25f / 2.75f;
      return 7.5625f * t * t + 0.9375f;
    } else {
      t -= 2.625f / 2.75f;
      return 7.5625f * t * t + 0.984375f;
    }
  }

  case EasingType::Step:
    return t < 1.0f ? 0.0f : 1.0f;

  case EasingType::Custom:
    // Use Bezier curve data from keyframe handles
    // This is intentionally left as a simplified fallback since full Bezier
    // interpolation requires access to both keyframes (start and end) and their
    // handles. The actual Bezier curve interpolation should be implemented in
    // TimelineTrack::interpolate() where both keyframes are available.
    // For standalone easing function, use cubic ease-in-out as approximation.
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;

  default:
    return t;
  }
}

QList<Keyframe*> TimelineTrack::selectedKeyframes() {
  QList<Keyframe*> selected;
  for (auto& kf : keyframes) {
    if (kf.isSelected) {
      selected.append(&kf);
    }
  }
  return selected;
}

void TimelineTrack::selectKeyframesInRange(int startFrame, int endFrame) {
  for (auto& kf : keyframes) {
    if (kf.frame >= startFrame && kf.frame <= endFrame) {
      kf.isSelected = true;
    }
  }
}

void TimelineTrack::clearSelection() {
  for (auto& kf : keyframes) {
    kf.isSelected = false;
  }
}

} // namespace NovelMind::editor::qt
