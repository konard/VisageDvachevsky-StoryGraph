/**
 * @file scene_graph.cpp
 * @brief SceneGraph implementation
 */

#include "NovelMind/scene/scene_graph.hpp"

#include <algorithm>

namespace NovelMind::scene {

// ============================================================================
// SceneGraph Implementation
// ============================================================================

SceneGraph::SceneGraph()
    : m_backgroundLayer("Background", LayerType::Background),
      m_characterLayer("Characters", LayerType::Characters),
      m_uiLayer("UI", LayerType::UI),
      m_effectLayer("Effects", LayerType::Effects) {}

SceneGraph::~SceneGraph() = default;

void SceneGraph::setSceneId(const std::string &id) { m_sceneId = id; }

void SceneGraph::clear() {
  m_backgroundLayer.clear();
  m_characterLayer.clear();
  m_uiLayer.clear();
  m_effectLayer.clear();
}

Layer &SceneGraph::getLayer(LayerType type) {
  switch (type) {
  case LayerType::Background:
    return m_backgroundLayer;
  case LayerType::Characters:
    return m_characterLayer;
  case LayerType::UI:
    return m_uiLayer;
  case LayerType::Effects:
    return m_effectLayer;
  }
  return m_backgroundLayer; // Default fallback
}

void SceneGraph::addToLayer(LayerType layer,
                            std::unique_ptr<SceneObjectBase> object) {
  if (!object) {
    return;
  }

  std::string id = object->getId();
  SceneObjectType type = object->getType();
  registerObject(object.get());
  getLayer(layer).addObject(std::move(object));
  onObjectAdded(id, type);
}

std::unique_ptr<SceneObjectBase>
SceneGraph::removeFromLayer(LayerType layer, const std::string &id) {
  auto obj = getLayer(layer).removeObject(id);
  if (obj) {
    onObjectRemoved(id);
  }
  return obj;
}

SceneObjectBase *SceneGraph::findObject(const std::string &id) {
  if (auto *obj = m_backgroundLayer.findObject(id)) {
    return obj;
  }
  if (auto *obj = m_characterLayer.findObject(id)) {
    return obj;
  }
  if (auto *obj = m_uiLayer.findObject(id)) {
    return obj;
  }
  if (auto *obj = m_effectLayer.findObject(id)) {
    return obj;
  }
  return nullptr;
}

std::vector<SceneObjectBase *>
SceneGraph::findObjectsByTag(const std::string &tag) {
  std::vector<SceneObjectBase *> result;
  auto collect = [&result, &tag](Layer &layer) {
    for (const auto &obj : layer.getObjects()) {
      if (obj->hasTag(tag)) {
        result.push_back(obj.get());
      }
    }
  };
  collect(m_backgroundLayer);
  collect(m_characterLayer);
  collect(m_uiLayer);
  collect(m_effectLayer);
  return result;
}

std::vector<SceneObjectBase *>
SceneGraph::findObjectsByType(SceneObjectType type) {
  std::vector<SceneObjectBase *> result;
  auto collect = [&result, type](Layer &layer) {
    for (const auto &obj : layer.getObjects()) {
      if (obj->getType() == type) {
        result.push_back(obj.get());
      }
    }
  };
  collect(m_backgroundLayer);
  collect(m_characterLayer);
  collect(m_uiLayer);
  collect(m_effectLayer);
  return result;
}

void SceneGraph::showBackground(const std::string &textureId) {
  // Clear existing backgrounds
  m_backgroundLayer.clear();

  auto bg = std::make_unique<BackgroundObject>("main_background");
  bg->setTextureId(textureId);
  registerObject(bg.get());
  m_backgroundLayer.addObject(std::move(bg));
  onObjectAdded("main_background", SceneObjectType::Background);
}

CharacterObject *SceneGraph::showCharacter(const std::string &id,
                                           const std::string &characterId,
                                           CharacterObject::Position position) {
  // Check if character already exists
  auto *existing = findObject(id);
  if (existing && existing->getType() == SceneObjectType::Character) {
    auto *character = static_cast<CharacterObject *>(existing);
    character->setSlotPosition(position);
    character->setVisible(true);
    return character;
  }

  // Create new character
  auto character = std::make_unique<CharacterObject>(id, characterId);
  character->setSlotPosition(position);

  // Set initial position based on slot
  f32 x = 640.0f;
  switch (position) {
  case CharacterObject::Position::Left:
    x = 200.0f;
    break;
  case CharacterObject::Position::Center:
    x = 640.0f;
    break;
  case CharacterObject::Position::Right:
    x = 1080.0f;
    break;
  case CharacterObject::Position::Custom:
    break;
  }
  character->setPosition(x, 400.0f);

  CharacterObject *ptr = character.get();
  registerObject(character.get());
  m_characterLayer.addObject(std::move(character));
  onObjectAdded(id, SceneObjectType::Character);

  return ptr;
}

void SceneGraph::hideCharacter(const std::string &id) {
  auto *obj = findObject(id);
  if (obj) {
    obj->setVisible(false);
  }
}

DialogueUIObject *SceneGraph::showDialogue(const std::string &speaker,
                                           const std::string &text) {
  auto *existing = findObject("dialogue_box");
  if (existing && existing->getType() == SceneObjectType::DialogueUI) {
    auto *dialogue = static_cast<DialogueUIObject *>(existing);
    dialogue->setSpeaker(speaker);
    dialogue->setText(text);
    dialogue->setVisible(true);
    dialogue->startTypewriter();
    return dialogue;
  }

  auto dialogue = std::make_unique<DialogueUIObject>("dialogue_box");
  dialogue->setSpeaker(speaker);
  dialogue->setText(text);
  dialogue->setPosition(0.0f, 600.0f);
  dialogue->startTypewriter();

  DialogueUIObject *ptr = dialogue.get();
  registerObject(dialogue.get());
  m_uiLayer.addObject(std::move(dialogue));
  onObjectAdded("dialogue_box", SceneObjectType::DialogueUI);

  return ptr;
}

void SceneGraph::hideDialogue() {
  auto *obj = findObject("dialogue_box");
  if (obj) {
    obj->setVisible(false);
  }
}

ChoiceUIObject *SceneGraph::showChoices(
    const std::vector<ChoiceUIObject::ChoiceOption> &choices) {
  auto *existing = findObject("choice_menu");
  if (existing && existing->getType() == SceneObjectType::ChoiceUI) {
    auto *menu = static_cast<ChoiceUIObject *>(existing);
    menu->setChoices(choices);
    menu->setVisible(true);
    return menu;
  }

  auto menu = std::make_unique<ChoiceUIObject>("choice_menu");
  menu->setChoices(choices);
  menu->setPosition(400.0f, 300.0f);

  ChoiceUIObject *ptr = menu.get();
  registerObject(menu.get());
  m_uiLayer.addObject(std::move(menu));
  onObjectAdded("choice_menu", SceneObjectType::ChoiceUI);

  return ptr;
}

void SceneGraph::hideChoices() {
  auto *obj = findObject("choice_menu");
  if (obj) {
    obj->setVisible(false);
  }
}

void SceneGraph::update(f64 deltaTime) {
  m_backgroundLayer.update(deltaTime);
  m_characterLayer.update(deltaTime);
  m_uiLayer.update(deltaTime);
  m_effectLayer.update(deltaTime);
}

void SceneGraph::render(renderer::IRenderer &renderer) {
  m_backgroundLayer.render(renderer);
  m_characterLayer.render(renderer);
  m_uiLayer.render(renderer);
  m_effectLayer.render(renderer);
}

void SceneGraph::setResourceManager(resource::ResourceManager *resources) {
  m_resources = resources;
  auto assignLayer = [this](Layer &layer) {
    for (const auto &obj : layer.getObjects()) {
      if (obj) {
        obj->m_resources = m_resources;
      }
    }
  };
  assignLayer(m_backgroundLayer);
  assignLayer(m_characterLayer);
  assignLayer(m_uiLayer);
  assignLayer(m_effectLayer);
}

void SceneGraph::setLocalizationManager(
    localization::LocalizationManager *localization) {
  m_localization = localization;
  auto assignLayer = [this](Layer &layer) {
    for (const auto &obj : layer.getObjects()) {
      if (obj) {
        obj->m_localization = m_localization;
      }
    }
  };
  assignLayer(m_backgroundLayer);
  assignLayer(m_characterLayer);
  assignLayer(m_uiLayer);
  assignLayer(m_effectLayer);
}

SceneState SceneGraph::saveState() const {
  SceneState state;
  state.sceneId = m_sceneId;

  // Save all objects from all layers
  auto saveLayerObjects = [&state](const Layer &layer) {
    for (const auto &obj : layer.getObjects()) {
      state.objects.push_back(obj->saveState());
    }
  };

  saveLayerObjects(m_backgroundLayer);
  saveLayerObjects(m_characterLayer);
  saveLayerObjects(m_uiLayer);
  saveLayerObjects(m_effectLayer);

  // Track active background and characters
  auto *bg =
      const_cast<Layer &>(m_backgroundLayer).findObject("main_background");
  if (bg) {
    // Use dynamic_cast for type-safe downcasting
    if (bg->getType() == SceneObjectType::Background) {
      auto *bgObj = static_cast<const BackgroundObject *>(bg);
      state.activeBackground = bgObj->getTextureId();
    }
    // If type doesn't match, we silently skip - this can happen during scene
    // transitions when objects are being replaced
  }

  for (const auto &obj : m_characterLayer.getObjects()) {
    if (obj->isVisible()) {
      state.visibleCharacters.push_back(obj->getId());
    }
  }

  return state;
}

void SceneGraph::loadState(const SceneState &state) {
  clear();
  m_sceneId = state.sceneId;

  // Recreate objects from saved state
  for (const auto &objState : state.objects) {
    std::unique_ptr<SceneObjectBase> obj;
    LayerType layer = LayerType::Background;

    switch (objState.type) {
    case SceneObjectType::Background: {
      auto bg = std::make_unique<BackgroundObject>(objState.id);
      bg->loadState(objState);
      obj = std::move(bg);
      layer = LayerType::Background;
      break;
    }
    case SceneObjectType::Character: {
      auto character = std::make_unique<CharacterObject>(objState.id, "");
      character->loadState(objState);
      obj = std::move(character);
      layer = LayerType::Characters;
      break;
    }
    case SceneObjectType::DialogueUI: {
      auto dialogue = std::make_unique<DialogueUIObject>(objState.id);
      dialogue->loadState(objState);
      obj = std::move(dialogue);
      layer = LayerType::UI;
      break;
    }
    case SceneObjectType::ChoiceUI: {
      auto choice = std::make_unique<ChoiceUIObject>(objState.id);
      choice->loadState(objState);
      obj = std::move(choice);
      layer = LayerType::UI;
      break;
    }
    case SceneObjectType::EffectOverlay: {
      auto effect = std::make_unique<EffectOverlayObject>(objState.id);
      effect->loadState(objState);
      obj = std::move(effect);
      layer = LayerType::Effects;
      break;
    }
    default:
      continue;
    }

    if (obj) {
      addToLayer(layer, std::move(obj));
    }
  }
}

void SceneGraph::addObserver(ISceneObserver *observer) {
  if (observer && std::find(m_observers.begin(), m_observers.end(), observer) ==
                      m_observers.end()) {
    m_observers.push_back(observer);
  }
}

void SceneGraph::removeObserver(ISceneObserver *observer) {
  auto it = std::find(m_observers.begin(), m_observers.end(), observer);
  if (it != m_observers.end()) {
    m_observers.erase(it);
  }
}

void SceneGraph::onObjectAdded(const std::string &objectId,
                               SceneObjectType type) {
  notifyObservers(
      [&](ISceneObserver *obs) { obs->onObjectAdded(objectId, type); });
}

void SceneGraph::onObjectRemoved(const std::string &objectId) {
  notifyObservers([&](ISceneObserver *obs) { obs->onObjectRemoved(objectId); });
}

void SceneGraph::onPropertyChanged(const PropertyChange &change) {
  notifyObservers([&](ISceneObserver *obs) { obs->onPropertyChanged(change); });
}

void SceneGraph::onLayerChanged(const std::string &objectId,
                                const std::string &newLayer) {
  notifyObservers(
      [&](ISceneObserver *obs) { obs->onLayerChanged(objectId, newLayer); });
}

void SceneGraph::notifyObservers(
    const std::function<void(ISceneObserver *)> &notify) {
  for (auto *observer : m_observers) {
    if (observer) {
      notify(observer);
    }
  }
}

void SceneGraph::registerObject(SceneObjectBase *obj) {
  if (obj) {
    obj->m_observer = this;
    obj->m_resources = m_resources;
    obj->m_localization = m_localization;
  }
}

} // namespace NovelMind::scene
