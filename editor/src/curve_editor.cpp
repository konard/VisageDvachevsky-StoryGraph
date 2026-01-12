/**
 * @file curve_editor.cpp
 * @brief Implementation of Animation Curve Editor for NovelMind
 */

#include "NovelMind/editor/curve_editor.hpp"
#include "NovelMind/core/logger.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>

namespace NovelMind::editor {

// =============================================================================
// AnimationCurve Implementation
// =============================================================================

AnimationCurve::AnimationCurve()
    : m_id(std::to_string(reinterpret_cast<uintptr_t>(this))), m_name("Unnamed Curve") {
  // Add default start and end points
  CurvePoint start;
  start.time = 0.0f;
  start.value = 0.0f;
  m_points.push_back(start);

  CurvePoint end;
  end.time = 1.0f;
  end.value = 1.0f;
  m_points.push_back(end);
}

AnimationCurve::AnimationCurve(const std::string& name)
    : m_id(std::to_string(reinterpret_cast<uintptr_t>(this))), m_name(name) {
  CurvePoint start;
  start.time = 0.0f;
  start.value = 0.0f;
  m_points.push_back(start);

  CurvePoint end;
  end.time = 1.0f;
  end.value = 1.0f;
  m_points.push_back(end);
}

void AnimationCurve::addPoint(const CurvePoint& point) {
  // Insert in sorted order by time
  auto it =
      std::lower_bound(m_points.begin(), m_points.end(), point,
                       [](const CurvePoint& a, const CurvePoint& b) { return a.time < b.time; });
  m_points.insert(it, point);
}

void AnimationCurve::removePoint(size_t index) {
  if (index < m_points.size() && m_points.size() > 2) {
    m_points.erase(m_points.begin() + static_cast<std::ptrdiff_t>(index));
  }
}

void AnimationCurve::updatePoint(size_t index, const CurvePoint& point) {
  if (index < m_points.size()) {
    m_points[index] = point;
    // Re-sort if necessary
    std::sort(m_points.begin(), m_points.end(),
              [](const CurvePoint& a, const CurvePoint& b) { return a.time < b.time; });
  }
}

f32 AnimationCurve::evaluate(f32 t) const {
  if (m_points.empty())
    return 0.0f;
  if (m_points.size() == 1)
    return m_points[0].value;

  t = std::clamp(t, 0.0f, 1.0f);

  // Find the segment
  i32 segIndex = findSegment(t);
  if (segIndex < 0)
    return m_points[0].value;
  if (segIndex >= static_cast<i32>(m_points.size()) - 1) {
    return m_points.back().value;
  }

  const auto& p0 = m_points[static_cast<size_t>(segIndex)];
  const auto& p1 = m_points[static_cast<size_t>(segIndex + 1)];

  // Calculate local t for this segment
  f32 segmentLength = p1.time - p0.time;
  if (segmentLength < 0.0001f)
    return p0.value;

  f32 localT = (t - p0.time) / segmentLength;

  return evaluateBezierSegment(p0, p1, localT);
}

f32 AnimationCurve::evaluateDerivative(f32 t) const {
  // Numerical derivative
  const f32 epsilon = 0.0001f;
  f32 t0 = std::max(0.0f, t - epsilon);
  f32 t1 = std::min(1.0f, t + epsilon);
  return (evaluate(t1) - evaluate(t0)) / (t1 - t0);
}

std::vector<renderer::Vec2> AnimationCurve::sample(i32 numSamples) const {
  std::vector<renderer::Vec2> samples;
  samples.reserve(static_cast<size_t>(numSamples));

  for (i32 i = 0; i < numSamples; ++i) {
    f32 t = static_cast<f32>(i) / static_cast<f32>(numSamples - 1);
    samples.push_back({t, evaluate(t)});
  }

  return samples;
}

AnimationCurve AnimationCurve::createLinear() {
  AnimationCurve curve("Linear");
  curve.m_points.clear();

  CurvePoint start;
  start.time = 0.0f;
  start.value = 0.0f;
  start.handleMode = CurvePoint::HandleMode::Vector;
  curve.m_points.push_back(start);

  CurvePoint end;
  end.time = 1.0f;
  end.value = 1.0f;
  end.handleMode = CurvePoint::HandleMode::Vector;
  curve.m_points.push_back(end);

  return curve;
}

AnimationCurve AnimationCurve::createEaseIn() {
  AnimationCurve curve("Ease In");
  curve.m_points.clear();

  CurvePoint start;
  start.time = 0.0f;
  start.value = 0.0f;
  start.outHandleX = 0.42f;
  start.outHandleY = 0.0f;
  curve.m_points.push_back(start);

  CurvePoint end;
  end.time = 1.0f;
  end.value = 1.0f;
  end.inHandleX = -0.42f;
  end.inHandleY = 0.0f;
  curve.m_points.push_back(end);

  return curve;
}

AnimationCurve AnimationCurve::createEaseOut() {
  AnimationCurve curve("Ease Out");
  curve.m_points.clear();

  CurvePoint start;
  start.time = 0.0f;
  start.value = 0.0f;
  start.outHandleX = 0.0f;
  start.outHandleY = 0.42f;
  curve.m_points.push_back(start);

  CurvePoint end;
  end.time = 1.0f;
  end.value = 1.0f;
  end.inHandleX = -0.58f;
  end.inHandleY = 0.0f;
  curve.m_points.push_back(end);

  return curve;
}

AnimationCurve AnimationCurve::createEaseInOut() {
  AnimationCurve curve("Ease In Out");
  curve.m_points.clear();

  CurvePoint start;
  start.time = 0.0f;
  start.value = 0.0f;
  start.outHandleX = 0.42f;
  start.outHandleY = 0.0f;
  curve.m_points.push_back(start);

  CurvePoint end;
  end.time = 1.0f;
  end.value = 1.0f;
  end.inHandleX = -0.42f;
  end.inHandleY = 0.0f;
  curve.m_points.push_back(end);

  return curve;
}

AnimationCurve AnimationCurve::createEaseInQuad() {
  AnimationCurve curve("Ease In Quad");
  curve.m_points.clear();

  CurvePoint start;
  start.time = 0.0f;
  start.value = 0.0f;
  start.outHandleX = 0.55f;
  start.outHandleY = 0.0f;
  curve.m_points.push_back(start);

  CurvePoint end;
  end.time = 1.0f;
  end.value = 1.0f;
  end.inHandleX = -0.15f;
  end.inHandleY = 0.0f;
  curve.m_points.push_back(end);

  return curve;
}

AnimationCurve AnimationCurve::createEaseOutQuad() {
  AnimationCurve curve("Ease Out Quad");
  curve.m_points.clear();

  CurvePoint start;
  start.time = 0.0f;
  start.value = 0.0f;
  start.outHandleX = 0.25f;
  start.outHandleY = 0.46f;
  curve.m_points.push_back(start);

  CurvePoint end;
  end.time = 1.0f;
  end.value = 1.0f;
  end.inHandleX = -0.25f;
  end.inHandleY = 0.0f;
  curve.m_points.push_back(end);

  return curve;
}

AnimationCurve AnimationCurve::createEaseInOutQuad() {
  AnimationCurve curve("Ease In Out Quad");
  curve.m_points.clear();

  CurvePoint start;
  start.time = 0.0f;
  start.value = 0.0f;
  start.outHandleX = 0.46f;
  start.outHandleY = 0.03f;
  curve.m_points.push_back(start);

  CurvePoint end;
  end.time = 1.0f;
  end.value = 1.0f;
  end.inHandleX = -0.46f;
  end.inHandleY = 0.03f;
  curve.m_points.push_back(end);

  return curve;
}

AnimationCurve AnimationCurve::createEaseInCubic() {
  AnimationCurve curve("Ease In Cubic");
  curve.m_points.clear();

  CurvePoint start;
  start.time = 0.0f;
  start.value = 0.0f;
  start.outHandleX = 0.55f;
  start.outHandleY = 0.0f;
  curve.m_points.push_back(start);

  CurvePoint end;
  end.time = 1.0f;
  end.value = 1.0f;
  end.inHandleX = -0.325f;
  end.inHandleY = 0.0f;
  curve.m_points.push_back(end);

  return curve;
}

AnimationCurve AnimationCurve::createEaseOutCubic() {
  AnimationCurve curve("Ease Out Cubic");
  curve.m_points.clear();

  CurvePoint start;
  start.time = 0.0f;
  start.value = 0.0f;
  start.outHandleX = 0.215f;
  start.outHandleY = 0.61f;
  curve.m_points.push_back(start);

  CurvePoint end;
  end.time = 1.0f;
  end.value = 1.0f;
  end.inHandleX = -0.145f;
  end.inHandleY = 0.0f;
  curve.m_points.push_back(end);

  return curve;
}

AnimationCurve AnimationCurve::createEaseInOutCubic() {
  AnimationCurve curve("Ease In Out Cubic");
  curve.m_points.clear();

  CurvePoint start;
  start.time = 0.0f;
  start.value = 0.0f;
  start.outHandleX = 0.645f;
  start.outHandleY = 0.045f;
  curve.m_points.push_back(start);

  CurvePoint end;
  end.time = 1.0f;
  end.value = 1.0f;
  end.inHandleX = -0.645f;
  end.inHandleY = 0.045f;
  curve.m_points.push_back(end);

  return curve;
}

AnimationCurve AnimationCurve::createEaseInBack() {
  AnimationCurve curve("Ease In Back");
  curve.m_points.clear();

  CurvePoint start;
  start.time = 0.0f;
  start.value = 0.0f;
  start.outHandleX = 0.36f;
  start.outHandleY = 0.0f;
  curve.m_points.push_back(start);

  CurvePoint end;
  end.time = 1.0f;
  end.value = 1.0f;
  end.inHandleX = -0.45f;
  end.inHandleY = -0.29f;
  curve.m_points.push_back(end);

  return curve;
}

AnimationCurve AnimationCurve::createEaseOutBack() {
  AnimationCurve curve("Ease Out Back");
  curve.m_points.clear();

  CurvePoint start;
  start.time = 0.0f;
  start.value = 0.0f;
  start.outHandleX = 0.175f;
  start.outHandleY = 0.885f;
  curve.m_points.push_back(start);

  CurvePoint end;
  end.time = 1.0f;
  end.value = 1.0f;
  end.inHandleX = -0.32f;
  end.inHandleY = 0.0f;
  curve.m_points.push_back(end);

  return curve;
}

AnimationCurve AnimationCurve::createEaseInOutBack() {
  AnimationCurve curve("Ease In Out Back");
  curve.m_points.clear();

  CurvePoint start;
  start.time = 0.0f;
  start.value = 0.0f;
  start.outHandleX = 0.68f;
  start.outHandleY = -0.55f;
  curve.m_points.push_back(start);

  CurvePoint end;
  end.time = 1.0f;
  end.value = 1.0f;
  end.inHandleX = -0.68f;
  end.inHandleY = 0.55f;
  curve.m_points.push_back(end);

  return curve;
}

AnimationCurve AnimationCurve::createEaseInBounce() {
  AnimationCurve curve("Ease In Bounce");
  // Bounce curves need multiple points
  curve.m_points.clear();

  CurvePoint p0;
  p0.time = 0.0f;
  p0.value = 0.0f;
  curve.m_points.push_back(p0);

  CurvePoint p1;
  p1.time = 0.09f;
  p1.value = 0.03f;
  curve.m_points.push_back(p1);

  CurvePoint p2;
  p2.time = 0.27f;
  p2.value = 0.06f;
  curve.m_points.push_back(p2);

  CurvePoint p3;
  p3.time = 0.55f;
  p3.value = 0.23f;
  curve.m_points.push_back(p3);

  CurvePoint p4;
  p4.time = 1.0f;
  p4.value = 1.0f;
  curve.m_points.push_back(p4);

  return curve;
}

AnimationCurve AnimationCurve::createEaseOutBounce() {
  AnimationCurve curve("Ease Out Bounce");
  curve.m_points.clear();

  CurvePoint p0;
  p0.time = 0.0f;
  p0.value = 0.0f;
  curve.m_points.push_back(p0);

  CurvePoint p1;
  p1.time = 0.36f;
  p1.value = 0.77f;
  curve.m_points.push_back(p1);

  CurvePoint p2;
  p2.time = 0.73f;
  p2.value = 0.94f;
  curve.m_points.push_back(p2);

  CurvePoint p3;
  p3.time = 0.91f;
  p3.value = 0.97f;
  curve.m_points.push_back(p3);

  CurvePoint p4;
  p4.time = 1.0f;
  p4.value = 1.0f;
  curve.m_points.push_back(p4);

  return curve;
}

AnimationCurve AnimationCurve::createEaseInOutBounce() {
  AnimationCurve curve("Ease In Out Bounce");
  curve.m_points.clear();

  CurvePoint p0;
  p0.time = 0.0f;
  p0.value = 0.0f;
  curve.m_points.push_back(p0);

  CurvePoint p1;
  p1.time = 0.15f;
  p1.value = 0.03f;
  curve.m_points.push_back(p1);

  CurvePoint p2;
  p2.time = 0.5f;
  p2.value = 0.5f;
  curve.m_points.push_back(p2);

  CurvePoint p3;
  p3.time = 0.85f;
  p3.value = 0.97f;
  curve.m_points.push_back(p3);

  CurvePoint p4;
  p4.time = 1.0f;
  p4.value = 1.0f;
  curve.m_points.push_back(p4);

  return curve;
}

AnimationCurve AnimationCurve::createEaseInElastic() {
  AnimationCurve curve("Ease In Elastic");
  curve.m_points.clear();

  CurvePoint p0;
  p0.time = 0.0f;
  p0.value = 0.0f;
  curve.m_points.push_back(p0);

  CurvePoint p1;
  p1.time = 0.4f;
  p1.value = -0.01f;
  curve.m_points.push_back(p1);

  CurvePoint p2;
  p2.time = 0.7f;
  p2.value = 0.02f;
  curve.m_points.push_back(p2);

  CurvePoint p3;
  p3.time = 0.85f;
  p3.value = -0.06f;
  curve.m_points.push_back(p3);

  CurvePoint p4;
  p4.time = 1.0f;
  p4.value = 1.0f;
  curve.m_points.push_back(p4);

  return curve;
}

AnimationCurve AnimationCurve::createEaseOutElastic() {
  AnimationCurve curve("Ease Out Elastic");
  curve.m_points.clear();

  CurvePoint p0;
  p0.time = 0.0f;
  p0.value = 0.0f;
  curve.m_points.push_back(p0);

  CurvePoint p1;
  p1.time = 0.15f;
  p1.value = 1.06f;
  curve.m_points.push_back(p1);

  CurvePoint p2;
  p2.time = 0.3f;
  p2.value = 0.98f;
  curve.m_points.push_back(p2);

  CurvePoint p3;
  p3.time = 0.6f;
  p3.value = 1.01f;
  curve.m_points.push_back(p3);

  CurvePoint p4;
  p4.time = 1.0f;
  p4.value = 1.0f;
  curve.m_points.push_back(p4);

  return curve;
}

AnimationCurve AnimationCurve::createEaseInOutElastic() {
  AnimationCurve curve("Ease In Out Elastic");
  curve.m_points.clear();

  CurvePoint p0;
  p0.time = 0.0f;
  p0.value = 0.0f;
  curve.m_points.push_back(p0);

  CurvePoint p1;
  p1.time = 0.35f;
  p1.value = 0.0f;
  curve.m_points.push_back(p1);

  CurvePoint p2;
  p2.time = 0.45f;
  p2.value = -0.03f;
  curve.m_points.push_back(p2);

  CurvePoint p3;
  p3.time = 0.5f;
  p3.value = 0.5f;
  curve.m_points.push_back(p3);

  CurvePoint p4;
  p4.time = 0.55f;
  p4.value = 1.03f;
  curve.m_points.push_back(p4);

  CurvePoint p5;
  p5.time = 0.65f;
  p5.value = 1.0f;
  curve.m_points.push_back(p5);

  CurvePoint p6;
  p6.time = 1.0f;
  p6.value = 1.0f;
  curve.m_points.push_back(p6);

  return curve;
}

AnimationCurve AnimationCurve::createStep() {
  AnimationCurve curve("Step");
  curve.m_points.clear();

  CurvePoint start;
  start.time = 0.0f;
  start.value = 0.0f;
  start.handleMode = CurvePoint::HandleMode::Broken;
  curve.m_points.push_back(start);

  CurvePoint mid;
  mid.time = 0.5f;
  mid.value = 1.0f;
  mid.handleMode = CurvePoint::HandleMode::Broken;
  curve.m_points.push_back(mid);

  CurvePoint end;
  end.time = 1.0f;
  end.value = 1.0f;
  end.handleMode = CurvePoint::HandleMode::Broken;
  curve.m_points.push_back(end);

  return curve;
}

AnimationCurve AnimationCurve::createSmooth() {
  AnimationCurve curve("Smooth");
  curve.m_points.clear();

  CurvePoint start;
  start.time = 0.0f;
  start.value = 0.0f;
  start.outHandleX = 0.25f;
  start.outHandleY = 0.1f;
  curve.m_points.push_back(start);

  CurvePoint end;
  end.time = 1.0f;
  end.value = 1.0f;
  end.inHandleX = -0.25f;
  end.inHandleY = -0.1f;
  curve.m_points.push_back(end);

  return curve;
}

bool AnimationCurve::isValid() const {
  if (m_points.size() < 2)
    return false;

  // Check that points are sorted
  for (size_t i = 1; i < m_points.size(); ++i) {
    if (m_points[i].time < m_points[i - 1].time)
      return false;
  }

  // Check range
  if (m_points.front().time < 0.0f || m_points.back().time > 1.0f)
    return false;

  return true;
}

void AnimationCurve::normalize() {
  // Sort by time
  std::sort(m_points.begin(), m_points.end(),
            [](const CurvePoint& a, const CurvePoint& b) { return a.time < b.time; });

  // Clamp values
  for (auto& point : m_points) {
    point.time = std::clamp(point.time, 0.0f, 1.0f);
  }
}

Result<void> AnimationCurve::save(const std::string& path) const {
  (void)path;
  return Result<void>::ok();
}

Result<AnimationCurve> AnimationCurve::load(const std::string& path) {
  (void)path;
  return Result<AnimationCurve>::ok(AnimationCurve());
}

std::string AnimationCurve::toJson() const {
  std::ostringstream json;
  json << "{\"name\":\"" << m_name << "\",\"points\":[";
  for (size_t i = 0; i < m_points.size(); ++i) {
    const auto& p = m_points[i];
    if (i > 0)
      json << ",";
    json << "{\"t\":" << p.time << ",\"v\":" << p.value << "}";
  }
  json << "]}";
  return json.str();
}

Result<AnimationCurve> AnimationCurve::fromJson(const std::string& json) {
  (void)json;
  return Result<AnimationCurve>::ok(AnimationCurve());
}

f32 AnimationCurve::evaluateBezierSegment(const CurvePoint& p0, const CurvePoint& p1,
                                          f32 localT) const {
  // Cubic bezier interpolation
  f32 t2 = localT * localT;
  f32 t3 = t2 * localT;
  f32 mt = 1.0f - localT;
  f32 mt2 = mt * mt;
  f32 mt3 = mt2 * mt;

  // Control points
  f32 cp0 = p0.value;
  f32 cp1 = p0.value + p0.outHandleY;
  f32 cp2 = p1.value + p1.inHandleY;
  f32 cp3 = p1.value;

  return mt3 * cp0 + 3.0f * mt2 * localT * cp1 + 3.0f * mt * t2 * cp2 + t3 * cp3;
}

i32 AnimationCurve::findSegment(f32 t) const {
  for (size_t i = 0; i < m_points.size() - 1; ++i) {
    if (t >= m_points[i].time && t <= m_points[i + 1].time) {
      return static_cast<i32>(i);
    }
  }
  return static_cast<i32>(m_points.size()) - 2;
}

} // namespace NovelMind::editor
