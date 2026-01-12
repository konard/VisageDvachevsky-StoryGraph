/**
 * @file scene_layer.cpp
 * @brief Scene layer implementation
 */

#include "NovelMind/scene/scene_graph.hpp"

#include <algorithm>

namespace NovelMind::scene {

// ============================================================================
// Layer Implementation
// ============================================================================

Layer::Layer(const std::string& name, LayerType type) : m_name(name), m_type(type) {}

void Layer::addObject(std::unique_ptr<SceneObjectBase> object) {
  if (object) {
    m_objects.push_back(std::move(object));
    sortByZOrder();
  }
}

std::unique_ptr<SceneObjectBase> Layer::removeObject(const std::string& id) {
  auto it = std::find_if(m_objects.begin(), m_objects.end(),
                         [&id](const auto& obj) { return obj->getId() == id; });

  if (it != m_objects.end()) {
    auto obj = std::move(*it);
    m_objects.erase(it);
    return obj;
  }
  return nullptr;
}

void Layer::clear() {
  m_objects.clear();
}

SceneObjectBase* Layer::findObject(const std::string& id) {
  auto it = std::find_if(m_objects.begin(), m_objects.end(),
                         [&id](const auto& obj) { return obj->getId() == id; });
  return (it != m_objects.end()) ? it->get() : nullptr;
}

const SceneObjectBase* Layer::findObject(const std::string& id) const {
  auto it = std::find_if(m_objects.begin(), m_objects.end(),
                         [&id](const auto& obj) { return obj->getId() == id; });
  return (it != m_objects.end()) ? it->get() : nullptr;
}

void Layer::setVisible(bool visible) {
  m_visible = visible;
}

void Layer::setAlpha(f32 alpha) {
  m_alpha = std::max(0.0f, std::min(1.0f, alpha));
}

void Layer::sortByZOrder() {
  std::stable_sort(m_objects.begin(), m_objects.end(),
                   [](const auto& a, const auto& b) { return a->getZOrder() < b->getZOrder(); });
}

void Layer::update(f64 deltaTime) {
  if (!m_visible) {
    return;
  }

  for (auto& obj : m_objects) {
    obj->update(deltaTime);
  }
}

void Layer::render(renderer::IRenderer& renderer) {
  if (!m_visible || m_alpha <= 0.0f) {
    return;
  }

  for (auto& obj : m_objects) {
    if (obj->isVisible()) {
      obj->render(renderer);
    }
  }
}

} // namespace NovelMind::scene
