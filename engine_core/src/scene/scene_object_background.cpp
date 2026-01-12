/**
 * @file scene_object_background.cpp
 * @brief Background scene object implementation
 */

#include "NovelMind/scene/scene_graph.hpp"

#include <sstream>

#include "scene_graph_detail.hpp"

namespace NovelMind::scene {

// ============================================================================
// BackgroundObject Implementation
// ============================================================================

BackgroundObject::BackgroundObject(const std::string& id)
    : SceneObjectBase(id, SceneObjectType::Background) {}

void BackgroundObject::setTextureId(const std::string& textureId) {
  std::string oldValue = m_textureId;
  m_textureId = textureId;
  notifyPropertyChanged("textureId", oldValue, textureId);
}

void BackgroundObject::setTint(const renderer::Color& color) {
  m_tint = color;
}

void BackgroundObject::render(renderer::IRenderer& renderer) {
  if (!m_visible || m_alpha <= 0.0f) {
    return;
  }

  if (!m_resources || m_textureId.empty()) {
    return;
  }

  auto texResult = m_resources->loadTexture(m_textureId);
  if (texResult.isError()) {
    return;
  }

  const auto& texture = *texResult.value();
  if (!texture.isValid()) {
    return;
  }

  renderer::Transform2D transform = m_transform;
  const float desiredW = detail::parseFloat(getProperty("width"), -1.0f);
  const float desiredH = detail::parseFloat(getProperty("height"), -1.0f);
  if (desiredW > 0.0f) {
    transform.scaleX = desiredW / static_cast<float>(texture.getWidth());
  }
  if (desiredH > 0.0f) {
    transform.scaleY = desiredH / static_cast<float>(texture.getHeight());
  }
  transform.anchorX = m_anchorX * static_cast<f32>(texture.getWidth());
  transform.anchorY = m_anchorY * static_cast<f32>(texture.getHeight());

  renderer::Color tint = m_tint;
  tint.a = static_cast<u8>(tint.a * m_alpha);
  renderer.drawSprite(texture, transform, tint);
}

SceneObjectState BackgroundObject::saveState() const {
  auto state = SceneObjectBase::saveState();
  state.properties["textureId"] = m_textureId;
  std::stringstream ss;
  ss << static_cast<int>(m_tint.r) << "," << static_cast<int>(m_tint.g) << ","
     << static_cast<int>(m_tint.b) << "," << static_cast<int>(m_tint.a);
  state.properties["tint"] = ss.str();
  return state;
}

void BackgroundObject::loadState(const SceneObjectState& state) {
  SceneObjectBase::loadState(state);
  auto texIt = state.properties.find("textureId");
  if (texIt != state.properties.end()) {
    m_textureId = texIt->second;
  }
}

} // namespace NovelMind::scene
