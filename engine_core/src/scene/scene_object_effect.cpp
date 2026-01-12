/**
 * @file scene_object_effect.cpp
 * @brief Effect overlay scene object implementation
 */

#include "NovelMind/scene/scene_graph.hpp"

#include <algorithm>

namespace NovelMind::scene {

// ============================================================================
// EffectOverlayObject Implementation
// ============================================================================

EffectOverlayObject::EffectOverlayObject(const std::string& id)
    : SceneObjectBase(id, SceneObjectType::EffectOverlay) {}

void EffectOverlayObject::setEffectType(EffectType type) {
  m_effectType = type;
}

void EffectOverlayObject::setColor(const renderer::Color& color) {
  m_color = color;
}

void EffectOverlayObject::setIntensity(f32 intensity) {
  m_intensity = std::max(0.0f, std::min(1.0f, intensity));
}

void EffectOverlayObject::startEffect(f32 duration) {
  m_effectActive = true;
  m_effectTimer = 0.0f;
  m_effectDuration = duration;
}

void EffectOverlayObject::stopEffect() {
  m_effectActive = false;
  m_effectTimer = 0.0f;
}

void EffectOverlayObject::update(f64 deltaTime) {
  SceneObjectBase::update(deltaTime);

  if (m_effectActive && m_effectDuration > 0.0f) {
    m_effectTimer += static_cast<f32>(deltaTime);
    if (m_effectTimer >= m_effectDuration) {
      m_effectActive = false;
      m_effectTimer = 0.0f;
    }
  }
}

void EffectOverlayObject::render(renderer::IRenderer& renderer) {
  if (!m_visible || m_alpha <= 0.0f || !m_effectActive) {
    return;
  }

  renderer::Color effectColor = m_color;
  f32 progress = m_effectDuration > 0.0f ? m_effectTimer / m_effectDuration : 0.0f;

  switch (m_effectType) {
  case EffectType::Fade: {
    f32 fade = 1.0f - progress;
    effectColor.a = static_cast<u8>(m_color.a * m_alpha * m_intensity * fade);
    renderer::Rect fullscreen{0.0f, 0.0f, 1920.0f, 1080.0f};
    renderer.fillRect(fullscreen, effectColor);
    break;
  }
  case EffectType::Flash: {
    f32 flash = 1.0f - progress * progress;
    effectColor.a = static_cast<u8>(m_color.a * m_alpha * m_intensity * flash);
    renderer::Rect fullscreen{0.0f, 0.0f, 1920.0f, 1080.0f};
    renderer.fillRect(fullscreen, effectColor);
    break;
  }
  case EffectType::Shake:
    // Shake is handled by modifying camera/scene offset, not rendered directly
    break;
  case EffectType::Rain:
  case EffectType::Snow:
    // Particle effects would be rendered here
    break;
  case EffectType::None:
  case EffectType::Custom:
    break;
  }
}

SceneObjectState EffectOverlayObject::saveState() const {
  auto state = SceneObjectBase::saveState();
  state.properties["effectType"] = std::to_string(static_cast<int>(m_effectType));
  state.properties["intensity"] = std::to_string(m_intensity);
  state.properties["effectActive"] = m_effectActive ? "true" : "false";
  return state;
}

void EffectOverlayObject::loadState(const SceneObjectState& state) {
  SceneObjectBase::loadState(state);

  auto it = state.properties.find("effectType");
  if (it != state.properties.end()) {
    m_effectType = static_cast<EffectType>(std::stoi(it->second));
  }

  it = state.properties.find("intensity");
  if (it != state.properties.end()) {
    m_intensity = std::stof(it->second);
  }

  it = state.properties.find("effectActive");
  if (it != state.properties.end()) {
    m_effectActive = (it->second == "true");
  }
}

} // namespace NovelMind::scene
