#include "NovelMind/renderer/sprite.hpp"

namespace NovelMind::renderer {

Sprite::Sprite()
    : m_texture(nullptr), m_sourceRect(), m_transform(), m_color(Color::White), m_visible(true) {}

Sprite::Sprite(std::shared_ptr<Texture> texture)
    : m_texture(std::move(texture)), m_sourceRect(), m_transform(), m_color(Color::White),
      m_visible(true) {
  if (m_texture && m_texture->isValid()) {
    m_sourceRect = Rect(0.0f, 0.0f, static_cast<f32>(m_texture->getWidth()),
                        static_cast<f32>(m_texture->getHeight()));
  }
}

void Sprite::setTexture(std::shared_ptr<Texture> texture) {
  m_texture = std::move(texture);
  if (m_texture && m_texture->isValid()) {
    m_sourceRect = Rect(0.0f, 0.0f, static_cast<f32>(m_texture->getWidth()),
                        static_cast<f32>(m_texture->getHeight()));
  }
}

const Texture* Sprite::getTexture() const {
  return m_texture.get();
}

void Sprite::setSourceRect(const Rect& rect) {
  m_sourceRect = rect;
}

const Rect& Sprite::getSourceRect() const {
  return m_sourceRect;
}

void Sprite::setTransform(const Transform2D& transform) {
  m_transform = transform;
}

const Transform2D& Sprite::getTransform() const {
  return m_transform;
}

Transform2D& Sprite::getTransform() {
  return m_transform;
}

void Sprite::setColor(const Color& color) {
  m_color = color;
}

const Color& Sprite::getColor() const {
  return m_color;
}

void Sprite::setVisible(bool visible) {
  m_visible = visible;
}

bool Sprite::isVisible() const {
  return m_visible;
}

} // namespace NovelMind::renderer
