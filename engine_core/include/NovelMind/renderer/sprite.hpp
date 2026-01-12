#pragma once

#include "NovelMind/core/types.hpp"
#include "NovelMind/renderer/color.hpp"
#include "NovelMind/renderer/texture.hpp"
#include "NovelMind/renderer/transform.hpp"
#include <memory>

namespace NovelMind::renderer {

class Sprite {
public:
  Sprite();
  explicit Sprite(std::shared_ptr<Texture> texture);
  ~Sprite() = default;

  void setTexture(std::shared_ptr<Texture> texture);
  [[nodiscard]] const Texture* getTexture() const;

  void setSourceRect(const Rect& rect);
  [[nodiscard]] const Rect& getSourceRect() const;

  void setTransform(const Transform2D& transform);
  [[nodiscard]] const Transform2D& getTransform() const;
  [[nodiscard]] Transform2D& getTransform();

  void setColor(const Color& color);
  [[nodiscard]] const Color& getColor() const;

  void setVisible(bool visible);
  [[nodiscard]] bool isVisible() const;

private:
  std::shared_ptr<Texture> m_texture;
  Rect m_sourceRect;
  Transform2D m_transform;
  Color m_color;
  bool m_visible;
};

} // namespace NovelMind::renderer
