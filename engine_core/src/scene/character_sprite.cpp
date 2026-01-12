#include "NovelMind/scene/character_sprite.hpp"
#include <cmath>

namespace NovelMind::Scene {

CharacterSprite::CharacterSprite(const std::string& id, const std::string& characterId)
    : scene::SceneObject(id), m_characterId(characterId), m_displayName(),
      m_nameColor(renderer::Color::White), m_currentExpression("default"), m_flipped(false),
      m_anchorX(0.5f), m_anchorY(1.0f), m_moving(false), m_moveStartX(0.0f), m_moveStartY(0.0f),
      m_moveTargetX(0.0f), m_moveTargetY(0.0f), m_moveDuration(0.0f), m_moveElapsed(0.0f) {}

CharacterSprite::~CharacterSprite() = default;

void CharacterSprite::setDisplayName(const std::string& name) {
  m_displayName = name;
}

const std::string& CharacterSprite::getDisplayName() const {
  return m_displayName;
}

void CharacterSprite::setCharacterId(const std::string& characterId) {
  m_characterId = characterId;
}

const std::string& CharacterSprite::getCharacterId() const {
  return m_characterId;
}

void CharacterSprite::setNameColor(const renderer::Color& color) {
  m_nameColor = color;
}

const renderer::Color& CharacterSprite::getNameColor() const {
  return m_nameColor;
}

void CharacterSprite::addExpression(const std::string& expressionId,
                                    std::shared_ptr<renderer::Texture> texture) {
  m_expressions[expressionId] = std::move(texture);
}

void CharacterSprite::setExpression(const std::string& expressionId, bool /*immediate*/) {
  if (m_expressions.find(expressionId) != m_expressions.end()) {
    m_currentExpression = expressionId;
  }
}

const std::string& CharacterSprite::getCurrentExpression() const {
  return m_currentExpression;
}

void CharacterSprite::setPresetPosition(CharacterPosition position, f32 screenWidth,
                                        f32 screenHeight) {
  f32 x = 0.0f;
  f32 y = screenHeight;

  switch (position) {
  case CharacterPosition::Left:
    x = screenWidth * 0.25f;
    break;
  case CharacterPosition::Center:
    x = screenWidth * 0.5f;
    break;
  case CharacterPosition::Right:
    x = screenWidth * 0.75f;
    break;
  case CharacterPosition::Custom:
    // Keep current position
    return;
  }

  setPosition(x, y);
}

void CharacterSprite::setFlipped(bool flipped) {
  m_flipped = flipped;
}

bool CharacterSprite::isFlipped() const {
  return m_flipped;
}

void CharacterSprite::setAnchor(f32 anchorX, f32 anchorY) {
  m_anchorX = anchorX;
  m_anchorY = anchorY;
}

void CharacterSprite::moveTo(f32 targetX, f32 targetY, f32 duration) {
  m_moving = true;
  m_moveStartX = m_transform.x;
  m_moveStartY = m_transform.y;
  m_moveTargetX = targetX;
  m_moveTargetY = targetY;
  m_moveDuration = duration;
  m_moveElapsed = 0.0f;
}

bool CharacterSprite::isMoving() const {
  return m_moving;
}

void CharacterSprite::update(f64 deltaTime) {
  SceneObject::update(deltaTime);

  // Handle movement animation
  if (m_moving) {
    m_moveElapsed += static_cast<f32>(deltaTime);

    if (m_moveElapsed >= m_moveDuration) {
      // Movement complete
      setPosition(m_moveTargetX, m_moveTargetY);
      m_moving = false;
    } else {
      // Interpolate position using smooth-step
      f32 t = m_moveElapsed / m_moveDuration;
      // Smooth-step: 3t^2 - 2t^3
      t = t * t * (3.0f - 2.0f * t);

      f32 newX = m_moveStartX + (m_moveTargetX - m_moveStartX) * t;
      f32 newY = m_moveStartY + (m_moveTargetY - m_moveStartY) * t;
      setPosition(newX, newY);
    }
  }
}

void CharacterSprite::render(renderer::IRenderer& renderer) {
  if (!m_visible) {
    return;
  }

  // Find current expression texture
  auto it = m_expressions.find(m_currentExpression);
  if (it == m_expressions.end() || !it->second) {
    // Try default expression
    it = m_expressions.find("default");
    if (it == m_expressions.end() || !it->second) {
      return;
    }
  }

  const auto& texture = it->second;

  // Calculate actual position based on anchor
  f32 texWidth = static_cast<f32>(texture->getWidth());
  f32 texHeight = static_cast<f32>(texture->getHeight());

  renderer::Transform2D drawTransform = m_transform;
  drawTransform.x -= texWidth * m_anchorX * m_transform.scaleX;
  drawTransform.y -= texHeight * m_anchorY * m_transform.scaleY;

  // Handle flip
  if (m_flipped) {
    drawTransform.scaleX *= -1.0f;
    drawTransform.x += texWidth * m_transform.scaleX;
  }

  // Apply alpha to tint color
  renderer::Color tint = renderer::Color::White;
  tint.a = static_cast<u8>(m_alpha * 255.0f);

  renderer.drawSprite(*texture, drawTransform, tint);
}

} // namespace NovelMind::Scene
