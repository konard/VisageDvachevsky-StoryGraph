/**
 * @file scene_object_base.cpp
 * @brief Base scene object implementation
 */

#include "NovelMind/scene/scene_graph.hpp"

#include <algorithm>
#include <atomic>

namespace NovelMind::scene {

// ============================================================================
// SceneObjectBase Implementation
// ============================================================================

// Global generation counter for thread-safe handle validation
// Each new object gets a unique generation number
static std::atomic<u64> g_nextGeneration{1};

SceneObjectBase::SceneObjectBase(const std::string &id, SceneObjectType type)
    : m_id(id), m_type(type), m_generation(g_nextGeneration.fetch_add(1)) {
  m_transform.x = 0.0f;
  m_transform.y = 0.0f;
  m_transform.scaleX = 1.0f;
  m_transform.scaleY = 1.0f;
  m_transform.rotation = 0.0f;
}

const char *SceneObjectBase::getTypeName() const {
  switch (m_type) {
  case SceneObjectType::Base:
    return "Base";
  case SceneObjectType::Background:
    return "Background";
  case SceneObjectType::Character:
    return "Character";
  case SceneObjectType::DialogueUI:
    return "DialogueUI";
  case SceneObjectType::ChoiceUI:
    return "ChoiceUI";
  case SceneObjectType::EffectOverlay:
    return "EffectOverlay";
  case SceneObjectType::Sprite:
    return "Sprite";
  case SceneObjectType::TextLabel:
    return "TextLabel";
  case SceneObjectType::Panel:
    return "Panel";
  case SceneObjectType::Custom:
    return "Custom";
  }
  return "Unknown";
}

void SceneObjectBase::setPosition(f32 x, f32 y) {
  std::string oldX = std::to_string(m_transform.x);
  std::string oldY = std::to_string(m_transform.y);
  m_transform.x = x;
  m_transform.y = y;
  notifyPropertyChanged("x", oldX, std::to_string(x));
  notifyPropertyChanged("y", oldY, std::to_string(y));
}

void SceneObjectBase::setScale(f32 scaleX, f32 scaleY) {
  std::string oldScaleX = std::to_string(m_transform.scaleX);
  std::string oldScaleY = std::to_string(m_transform.scaleY);
  m_transform.scaleX = scaleX;
  m_transform.scaleY = scaleY;
  notifyPropertyChanged("scaleX", oldScaleX, std::to_string(scaleX));
  notifyPropertyChanged("scaleY", oldScaleY, std::to_string(scaleY));
}

void SceneObjectBase::setUniformScale(f32 scale) { setScale(scale, scale); }

void SceneObjectBase::setRotation(f32 angle) {
  std::string oldValue = std::to_string(m_transform.rotation);
  m_transform.rotation = angle;
  notifyPropertyChanged("rotation", oldValue, std::to_string(angle));
}

void SceneObjectBase::setAnchor(f32 anchorX, f32 anchorY) {
  m_anchorX = anchorX;
  m_anchorY = anchorY;
}

void SceneObjectBase::setVisible(bool visible) {
  std::string oldValue = m_visible ? "true" : "false";
  m_visible = visible;
  notifyPropertyChanged("visible", oldValue, visible ? "true" : "false");
}

void SceneObjectBase::setAlpha(f32 alpha) {
  std::string oldValue = std::to_string(m_alpha);
  m_alpha = std::max(0.0f, std::min(1.0f, alpha));
  notifyPropertyChanged("alpha", oldValue, std::to_string(m_alpha));
}

void SceneObjectBase::setZOrder(i32 zOrder) {
  std::string oldValue = std::to_string(m_zOrder);
  m_zOrder = zOrder;
  notifyPropertyChanged("zOrder", oldValue, std::to_string(zOrder));
}

void SceneObjectBase::setParent(SceneObjectBase *parent) { m_parent = parent; }

void SceneObjectBase::addChild(std::unique_ptr<SceneObjectBase> child) {
  if (child) {
    child->setParent(this);
    m_children.push_back(std::move(child));
  }
}

std::unique_ptr<SceneObjectBase>
SceneObjectBase::removeChild(const std::string &id) {
  auto it =
      std::find_if(m_children.begin(), m_children.end(),
                   [&id](const auto &child) { return child->getId() == id; });

  if (it != m_children.end()) {
    auto child = std::move(*it);
    child->setParent(nullptr);
    m_children.erase(it);
    return child;
  }
  return nullptr;
}

SceneObjectBase *SceneObjectBase::findChild(const std::string &id) {
  return findChildRecursive(id, 0);
}

SceneObjectBase *SceneObjectBase::findChildRecursive(const std::string &id,
                                                     int depth) {
  // Prevent infinite recursion with depth limit
  if (depth >= MAX_SCENE_DEPTH) {
    // Log is not available here without adding header dependency,
    // but we safely return nullptr to prevent stack overflow
    return nullptr;
  }

  for (auto &child : m_children) {
    if (child->getId() == id) {
      return child.get();
    }
    if (auto *found = child->findChildRecursive(id, depth + 1)) {
      return found;
    }
  }
  return nullptr;
}

void SceneObjectBase::addTag(const std::string &tag) {
  if (std::find(m_tags.begin(), m_tags.end(), tag) == m_tags.end()) {
    m_tags.push_back(tag);
  }
}

void SceneObjectBase::removeTag(const std::string &tag) {
  auto it = std::find(m_tags.begin(), m_tags.end(), tag);
  if (it != m_tags.end()) {
    m_tags.erase(it);
  }
}

bool SceneObjectBase::hasTag(const std::string &tag) const {
  return std::find(m_tags.begin(), m_tags.end(), tag) != m_tags.end();
}

void SceneObjectBase::setProperty(const std::string &name,
                                  const std::string &value) {
  auto it = m_properties.find(name);
  std::string oldValue = (it != m_properties.end()) ? it->second : "";
  m_properties[name] = value;
  notifyPropertyChanged(name, oldValue, value);
}

std::optional<std::string>
SceneObjectBase::getProperty(const std::string &name) const {
  auto it = m_properties.find(name);
  if (it != m_properties.end()) {
    return it->second;
  }
  return std::nullopt;
}

void SceneObjectBase::update(f64 deltaTime) {
  // Update animations
  for (auto it = m_animations.begin(); it != m_animations.end();) {
    if (*it && !(*it)->update(deltaTime)) {
      it = m_animations.erase(it);
    } else {
      ++it;
    }
  }

  // Update children
  for (auto &child : m_children) {
    child->update(deltaTime);
  }
}

SceneObjectState SceneObjectBase::saveState() const {
  SceneObjectState state;
  state.id = m_id;
  state.type = m_type;
  state.x = m_transform.x;
  state.y = m_transform.y;
  state.scaleX = m_transform.scaleX;
  state.scaleY = m_transform.scaleY;
  state.rotation = m_transform.rotation;
  state.alpha = m_alpha;
  state.visible = m_visible;
  state.zOrder = m_zOrder;
  state.properties = m_properties;
  return state;
}

void SceneObjectBase::loadState(const SceneObjectState &state) {
  m_transform.x = state.x;
  m_transform.y = state.y;
  m_transform.scaleX = state.scaleX;
  m_transform.scaleY = state.scaleY;
  m_transform.rotation = state.rotation;
  m_alpha = state.alpha;
  m_visible = state.visible;
  m_zOrder = state.zOrder;
  m_properties = state.properties;
}

void SceneObjectBase::animatePosition(f32 toX, f32 toY, f32 duration,
                                      EaseType easing) {
  auto tween = std::make_unique<PositionTween>(&m_transform.x, &m_transform.y,
                                               m_transform.x, m_transform.y,
                                               toX, toY, duration, easing);
  tween->start();
  m_animations.push_back(std::move(tween));
}

void SceneObjectBase::animateAlpha(f32 toAlpha, f32 duration, EaseType easing) {
  auto tween = std::make_unique<FloatTween>(&m_alpha, m_alpha, toAlpha,
                                            duration, easing);
  tween->start();
  m_animations.push_back(std::move(tween));
}

void SceneObjectBase::animateScale(f32 toScaleX, f32 toScaleY, f32 duration,
                                   EaseType easing) {
  auto tweenX = std::make_unique<FloatTween>(
      &m_transform.scaleX, m_transform.scaleX, toScaleX, duration, easing);
  auto tweenY = std::make_unique<FloatTween>(
      &m_transform.scaleY, m_transform.scaleY, toScaleY, duration, easing);
  tweenX->start();
  tweenY->start();
  m_animations.push_back(std::move(tweenX));
  m_animations.push_back(std::move(tweenY));
}

void SceneObjectBase::notifyPropertyChanged(const std::string &property,
                                            const std::string &oldValue,
                                            const std::string &newValue) {
  if (m_observer) {
    PropertyChange change;
    change.objectId = m_id;
    change.propertyName = property;
    change.oldValue = oldValue;
    change.newValue = newValue;
    m_observer->onPropertyChanged(change);
  }
}

} // namespace NovelMind::scene
