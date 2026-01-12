#include "NovelMind/scene/scene_manager.hpp"
#include "NovelMind/core/logger.hpp"
#include <algorithm>

namespace NovelMind::scene {

SceneManager::SceneManager() = default;

SceneManager::~SceneManager() {
  unloadScene();
}

Result<void> SceneManager::loadScene(const std::string& sceneId) {
  unloadScene();
  m_currentSceneId = sceneId;
  NOVELMIND_LOG_INFO("Loaded scene: " + sceneId);
  return Result<void>::ok();
}

void SceneManager::unloadScene() {
  m_backgroundLayer.clear();
  m_charactersLayer.clear();
  m_uiLayer.clear();
  m_effectsLayer.clear();
  m_currentSceneId.clear();
}

void SceneManager::update(f64 deltaTime) {
  for (auto& obj : m_backgroundLayer)
    obj->update(deltaTime);
  for (auto& obj : m_charactersLayer)
    obj->update(deltaTime);
  for (auto& obj : m_uiLayer)
    obj->update(deltaTime);
  for (auto& obj : m_effectsLayer)
    obj->update(deltaTime);
}

void SceneManager::render(renderer::IRenderer& renderer) {
  for (auto& obj : m_backgroundLayer) {
    if (obj->isVisible())
      obj->render(renderer);
  }
  for (auto& obj : m_charactersLayer) {
    if (obj->isVisible())
      obj->render(renderer);
  }
  for (auto& obj : m_uiLayer) {
    if (obj->isVisible())
      obj->render(renderer);
  }
  for (auto& obj : m_effectsLayer) {
    if (obj->isVisible())
      obj->render(renderer);
  }
}

void SceneManager::addToLayer(LayerType layer, std::unique_ptr<SceneObject> object) {
  getLayer(layer).push_back(std::move(object));
}

void SceneManager::removeFromLayer(LayerType layer, const std::string& objectId) {
  auto& layerVec = getLayer(layer);
  layerVec.erase(std::remove_if(layerVec.begin(), layerVec.end(),
                                [&objectId](const auto& obj) { return obj->getId() == objectId; }),
                 layerVec.end());
}

void SceneManager::clearLayer(LayerType layer) {
  getLayer(layer).clear();
}

SceneObject* SceneManager::findObject(const std::string& id) {
  auto findInLayer = [&id](auto& layer) -> SceneObject* {
    auto it = std::find_if(layer.begin(), layer.end(),
                           [&id](const auto& obj) { return obj->getId() == id; });
    return it != layer.end() ? it->get() : nullptr;
  };

  if (auto* obj = findInLayer(m_backgroundLayer))
    return obj;
  if (auto* obj = findInLayer(m_charactersLayer))
    return obj;
  if (auto* obj = findInLayer(m_uiLayer))
    return obj;
  if (auto* obj = findInLayer(m_effectsLayer))
    return obj;

  return nullptr;
}

const std::string& SceneManager::getCurrentSceneId() const {
  return m_currentSceneId;
}

std::vector<std::unique_ptr<SceneObject>>& SceneManager::getLayer(LayerType layer) {
  switch (layer) {
  case LayerType::Background:
    return m_backgroundLayer;
  case LayerType::Characters:
    return m_charactersLayer;
  case LayerType::UI:
    return m_uiLayer;
  case LayerType::Effects:
    return m_effectsLayer;
  }
  return m_backgroundLayer;
}

} // namespace NovelMind::scene
