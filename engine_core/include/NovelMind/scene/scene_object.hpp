#pragma once

#include "NovelMind/core/types.hpp"
#include "NovelMind/renderer/renderer.hpp"
#include "NovelMind/renderer/transform.hpp"
#include <memory>
#include <string>

namespace NovelMind::scene {

class SceneObject {
public:
  explicit SceneObject(const std::string& id);
  virtual ~SceneObject() = default;

  [[nodiscard]] const std::string& getId() const;

  void setPosition(f32 x, f32 y);
  void setScale(f32 scaleX, f32 scaleY);
  void setRotation(f32 angle);
  void setAlpha(f32 alpha);
  void setVisible(bool visible);

  [[nodiscard]] const renderer::Transform2D& getTransform() const;
  [[nodiscard]] renderer::Transform2D& getTransform();
  [[nodiscard]] f32 getAlpha() const;
  [[nodiscard]] bool isVisible() const;

  virtual void update(f64 deltaTime);
  virtual void render(renderer::IRenderer& renderer) = 0;

protected:
  std::string m_id;
  renderer::Transform2D m_transform;
  f32 m_alpha;
  bool m_visible;
};

} // namespace NovelMind::scene
