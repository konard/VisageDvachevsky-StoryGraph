/**
 * @file scene_object_character.cpp
 * @brief Character scene object implementation
 */

#include "NovelMind/scene/scene_graph.hpp"

#include "scene_graph_detail.hpp"

namespace NovelMind::scene {

// ============================================================================
// CharacterObject Implementation
// ============================================================================

CharacterObject::CharacterObject(const std::string& id, const std::string& characterId)
    : SceneObjectBase(id, SceneObjectType::Character), m_characterId(characterId) {}

void CharacterObject::setCharacterId(const std::string& characterId) {
  m_characterId = characterId;
}

void CharacterObject::setDisplayName(const std::string& name) {
  m_displayName = name;
}

void CharacterObject::setExpression(const std::string& expression) {
  std::string oldValue = m_expression;
  m_expression = expression;
  notifyPropertyChanged("expression", oldValue, expression);
}

void CharacterObject::setPose(const std::string& pose) {
  std::string oldValue = m_pose;
  m_pose = pose;
  notifyPropertyChanged("pose", oldValue, pose);
}

void CharacterObject::setSlotPosition(Position pos) {
  m_slotPosition = pos;
}

void CharacterObject::setNameColor(const renderer::Color& color) {
  m_nameColor = color;
}

void CharacterObject::setHighlighted(bool highlighted) {
  m_highlighted = highlighted;
}

void CharacterObject::render(renderer::IRenderer& renderer) {
  if (!m_visible || m_alpha <= 0.0f) {
    return;
  }

  if (!m_resources) {
    return;
  }

  std::string textureId = detail::getTextProperty(*this, "textureId", m_characterId);
  if (textureId.empty()) {
    return;
  }

  auto texResult = m_resources->loadTexture(textureId);
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

  renderer::Color tint = renderer::Color::White;
  tint.a = static_cast<u8>(255 * m_alpha);
  if (!m_highlighted) {
    tint.r = static_cast<u8>(tint.r * 0.75f);
    tint.g = static_cast<u8>(tint.g * 0.75f);
    tint.b = static_cast<u8>(tint.b * 0.75f);
  }

  renderer.drawSprite(texture, transform, tint);
}

SceneObjectState CharacterObject::saveState() const {
  auto state = SceneObjectBase::saveState();
  state.properties["characterId"] = m_characterId;
  state.properties["displayName"] = m_displayName;
  state.properties["expression"] = m_expression;
  state.properties["pose"] = m_pose;
  state.properties["slotPosition"] = std::to_string(static_cast<int>(m_slotPosition));
  state.properties["highlighted"] = m_highlighted ? "true" : "false";
  return state;
}

void CharacterObject::loadState(const SceneObjectState& state) {
  SceneObjectBase::loadState(state);

  auto it = state.properties.find("characterId");
  if (it != state.properties.end())
    m_characterId = it->second;

  it = state.properties.find("displayName");
  if (it != state.properties.end())
    m_displayName = it->second;

  it = state.properties.find("expression");
  if (it != state.properties.end())
    m_expression = it->second;

  it = state.properties.find("pose");
  if (it != state.properties.end())
    m_pose = it->second;

  it = state.properties.find("slotPosition");
  if (it != state.properties.end()) {
    m_slotPosition = static_cast<Position>(std::stoi(it->second));
  }

  it = state.properties.find("highlighted");
  if (it != state.properties.end()) {
    m_highlighted = (it->second == "true");
  }
}

void CharacterObject::animateToSlot(Position slot, f32 duration, EaseType easing) {
  // Calculate target position based on slot
  f32 targetX = 0.0f;
  switch (slot) {
  case Position::Left:
    targetX = 200.0f;
    break;
  case Position::Center:
    targetX = 640.0f;
    break;
  case Position::Right:
    targetX = 1080.0f;
    break;
  case Position::Custom:
    return; // Don't animate custom positions
  }

  m_slotPosition = slot;
  animatePosition(targetX, m_transform.y, duration, easing);
}

} // namespace NovelMind::scene
