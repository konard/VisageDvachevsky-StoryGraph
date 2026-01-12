#include "NovelMind/editor/curve_editor.hpp"

#include <algorithm>

namespace NovelMind::editor {

// CurveLibrary Implementation
// =============================================================================

CurveLibrary::CurveLibrary() {
  loadPresets();
}

void CurveLibrary::addCurve(const AnimationCurve& curve) {
  m_curves[curve.getId()] = curve;
}

void CurveLibrary::removeCurve(const std::string& id) {
  m_curves.erase(id);
}

void CurveLibrary::updateCurve(const std::string& id, const AnimationCurve& curve) {
  auto it = m_curves.find(id);
  if (it != m_curves.end()) {
    it->second = curve;
  }
}

const AnimationCurve* CurveLibrary::getCurve(const std::string& id) const {
  auto it = m_curves.find(id);
  if (it != m_curves.end()) {
    return &it->second;
  }
  return nullptr;
}

std::vector<std::string> CurveLibrary::getCurveIds() const {
  std::vector<std::string> ids;
  ids.reserve(m_curves.size());
  for (const auto& [id, curve] : m_curves) {
    ids.push_back(id);
  }
  return ids;
}

std::vector<std::string> CurveLibrary::getCurveNames() const {
  std::vector<std::string> names;
  names.reserve(m_curves.size());
  for (const auto& [id, curve] : m_curves) {
    names.push_back(curve.getName());
  }
  return names;
}

void CurveLibrary::setCurveCategory(const std::string& curveId, const std::string& category) {
  m_curveCategories[curveId] = category;
}

std::vector<std::string> CurveLibrary::getCurvesInCategory(const std::string& category) const {
  std::vector<std::string> result;
  for (const auto& [id, cat] : m_curveCategories) {
    if (cat == category) {
      result.push_back(id);
    }
  }
  return result;
}

std::vector<std::string> CurveLibrary::getCategories() const {
  std::vector<std::string> categories;
  for (const auto& [id, cat] : m_curveCategories) {
    if (std::find(categories.begin(), categories.end(), cat) == categories.end()) {
      categories.push_back(cat);
    }
  }
  return categories;
}

void CurveLibrary::loadPresets() {
  // Add all preset curves
  auto addPreset = [this](const AnimationCurve& curve, const std::string& category) {
    m_curves[curve.getId()] = curve;
    m_curveCategories[curve.getId()] = category;
    m_presetIds.push_back(curve.getId());
  };

  addPreset(AnimationCurve::createLinear(), "Basic");
  addPreset(AnimationCurve::createEaseIn(), "Basic");
  addPreset(AnimationCurve::createEaseOut(), "Basic");
  addPreset(AnimationCurve::createEaseInOut(), "Basic");

  addPreset(AnimationCurve::createEaseInQuad(), "Quadratic");
  addPreset(AnimationCurve::createEaseOutQuad(), "Quadratic");
  addPreset(AnimationCurve::createEaseInOutQuad(), "Quadratic");

  addPreset(AnimationCurve::createEaseInCubic(), "Cubic");
  addPreset(AnimationCurve::createEaseOutCubic(), "Cubic");
  addPreset(AnimationCurve::createEaseInOutCubic(), "Cubic");

  addPreset(AnimationCurve::createEaseInBack(), "Back");
  addPreset(AnimationCurve::createEaseOutBack(), "Back");
  addPreset(AnimationCurve::createEaseInOutBack(), "Back");

  addPreset(AnimationCurve::createEaseInBounce(), "Bounce");
  addPreset(AnimationCurve::createEaseOutBounce(), "Bounce");
  addPreset(AnimationCurve::createEaseInOutBounce(), "Bounce");

  addPreset(AnimationCurve::createEaseInElastic(), "Elastic");
  addPreset(AnimationCurve::createEaseOutElastic(), "Elastic");
  addPreset(AnimationCurve::createEaseInOutElastic(), "Elastic");

  addPreset(AnimationCurve::createStep(), "Special");
  addPreset(AnimationCurve::createSmooth(), "Special");
}

std::vector<std::string> CurveLibrary::getPresetIds() const {
  return m_presetIds;
}

Result<void> CurveLibrary::saveLibrary(const std::string& path) {
  (void)path;
  return Result<void>::ok();
}

Result<void> CurveLibrary::loadLibrary(const std::string& path) {
  (void)path;
  return Result<void>::ok();
}

} // namespace NovelMind::editor
