#include "NovelMind/scene/scene_object.hpp"

namespace NovelMind::scene {

SceneObject::SceneObject(const std::string& id) : m_id(id), m_alpha(1.0f), m_visible(true) {}

const std::string& SceneObject::getId() const {
  return m_id;
}

void SceneObject::setPosition(f32 x, f32 y) {
  m_transform.x = x;
  m_transform.y = y;
}

void SceneObject::setScale(f32 scaleX, f32 scaleY) {
  m_transform.scaleX = scaleX;
  m_transform.scaleY = scaleY;
}

void SceneObject::setRotation(f32 angle) {
  m_transform.rotation = angle;
}

void SceneObject::setAlpha(f32 alpha) {
  m_alpha = alpha;
}

void SceneObject::setVisible(bool visible) {
  m_visible = visible;
}

const renderer::Transform2D& SceneObject::getTransform() const {
  return m_transform;
}

renderer::Transform2D& SceneObject::getTransform() {
  return m_transform;
}

f32 SceneObject::getAlpha() const {
  return m_alpha;
}

bool SceneObject::isVisible() const {
  return m_visible;
}

void SceneObject::update(f64 /*deltaTime*/) {
  // Override in derived classes
}

} // namespace NovelMind::scene
