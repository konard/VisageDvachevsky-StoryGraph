#include "NovelMind/scene/scene_inspector.hpp"
#include <algorithm>
#include <random>
#include <sstream>

namespace NovelMind::scene {

// ============================================================================
// SetPropertyCommand Implementation
// ============================================================================

SetPropertyCommand::SetPropertyCommand(SceneInspectorAPI *inspector,
                                       const std::string &objectId,
                                       const std::string &propertyName,
                                       const std::string &oldValue,
                                       const std::string &newValue)
    : m_inspector(inspector), m_objectId(objectId),
      m_propertyName(propertyName), m_oldValue(oldValue), m_newValue(newValue) {
}

void SetPropertyCommand::execute() {
  m_inspector->setProperty(m_objectId, m_propertyName, m_newValue, false);
}

void SetPropertyCommand::undo() {
  m_inspector->setProperty(m_objectId, m_propertyName, m_oldValue, false);
}

std::string SetPropertyCommand::getDescription() const {
  return "Set " + m_propertyName + " on " + m_objectId;
}

// ============================================================================
// AddObjectCommand Implementation
// ============================================================================

AddObjectCommand::AddObjectCommand(SceneInspectorAPI *inspector,
                                   LayerType layer,
                                   std::unique_ptr<SceneObjectBase> object)
    : m_inspector(inspector), m_layer(layer), m_object(std::move(object)) {
  if (m_object) {
    m_objectId = m_object->getId();
  }
}

void AddObjectCommand::execute() {
  if (m_object) {
    m_inspector->getSceneGraph()->addToLayer(m_layer, std::move(m_object));
    m_executed = true;
  }
}

void AddObjectCommand::undo() {
  if (m_executed) {
    m_object =
        m_inspector->getSceneGraph()->removeFromLayer(m_layer, m_objectId);
    m_executed = false;
  }
}

std::string AddObjectCommand::getDescription() const {
  return "Add object " + m_objectId;
}

// ============================================================================
// RemoveObjectCommand Implementation
// ============================================================================

RemoveObjectCommand::RemoveObjectCommand(SceneInspectorAPI *inspector,
                                         const std::string &objectId)
    : m_inspector(inspector), m_objectId(objectId) {
  auto *obj = m_inspector->getSceneGraph()->findObject(objectId);
  if (obj) {
    m_savedState = obj->saveState();
    m_layer = m_inspector->findObjectLayer(objectId);
  }
}

void RemoveObjectCommand::execute() {
  m_removedObject =
      m_inspector->getSceneGraph()->removeFromLayer(m_layer, m_objectId);
}

void RemoveObjectCommand::undo() {
  if (m_removedObject) {
    m_inspector->getSceneGraph()->addToLayer(m_layer,
                                             std::move(m_removedObject));
  }
}

std::string RemoveObjectCommand::getDescription() const {
  return "Remove object " + m_objectId;
}

// ============================================================================
// SceneInspectorAPI Implementation
// ============================================================================

SceneInspectorAPI::SceneInspectorAPI(SceneGraph *sceneGraph)
    : m_sceneGraph(sceneGraph) {
  if (m_sceneGraph) {
    m_sceneGraph->addObserver(this);
  }
}

SceneInspectorAPI::~SceneInspectorAPI() {
  if (m_sceneGraph) {
    m_sceneGraph->removeObserver(this);
  }
}

std::vector<LayerDescriptor> SceneInspectorAPI::getLayers() const {
  std::vector<LayerDescriptor> layers;

  auto addLayer = [&layers](const Layer &layer, LayerType type) {
    LayerDescriptor desc;
    desc.name = layer.getName();
    desc.type = type;
    desc.visible = layer.isVisible();
    desc.alpha = layer.getAlpha();
    for (const auto &obj : layer.getObjects()) {
      desc.objectIds.push_back(obj->getId());
    }
    layers.push_back(desc);
  };

  addLayer(m_sceneGraph->getBackgroundLayer(), LayerType::Background);
  addLayer(m_sceneGraph->getCharacterLayer(), LayerType::Characters);
  addLayer(m_sceneGraph->getUILayer(), LayerType::UI);
  addLayer(m_sceneGraph->getEffectLayer(), LayerType::Effects);

  return layers;
}

std::vector<ObjectDescriptor> SceneInspectorAPI::getObjects() const {
  std::vector<ObjectDescriptor> objects;

  auto addLayerObjects = [this, &objects](const Layer &layer,
                                          const std::string &layerName) {
    for (const auto &obj : layer.getObjects()) {
      ObjectDescriptor desc;
      desc.id = obj->getId();
      desc.displayName = obj->getId();
      desc.type = obj->getType();
      desc.layer = layerName;
      desc.visible = obj->isVisible();
      desc.properties = getProperties(obj->getId());

      // Add child IDs
      for (const auto &child : obj->getChildren()) {
        desc.childIds.push_back(child->getId());
      }

      objects.push_back(desc);
    }
  };

  addLayerObjects(m_sceneGraph->getBackgroundLayer(), "Background");
  addLayerObjects(m_sceneGraph->getCharacterLayer(), "Characters");
  addLayerObjects(m_sceneGraph->getUILayer(), "UI");
  addLayerObjects(m_sceneGraph->getEffectLayer(), "Effects");

  return objects;
}

std::optional<ObjectDescriptor>
SceneInspectorAPI::getObject(const std::string &id) const {
  auto *obj = m_sceneGraph->findObject(id);
  if (!obj) {
    return std::nullopt;
  }

  ObjectDescriptor desc;
  desc.id = obj->getId();
  desc.displayName = obj->getId();
  desc.type = obj->getType();
  desc.layer = "Unknown";
  desc.visible = obj->isVisible();
  desc.properties = getProperties(id);

  // Find layer
  LayerType layer = findObjectLayer(id);
  switch (layer) {
  case LayerType::Background:
    desc.layer = "Background";
    break;
  case LayerType::Characters:
    desc.layer = "Characters";
    break;
  case LayerType::UI:
    desc.layer = "UI";
    break;
  case LayerType::Effects:
    desc.layer = "Effects";
    break;
  }

  for (const auto &child : obj->getChildren()) {
    desc.childIds.push_back(child->getId());
  }

  return desc;
}

std::vector<PropertyDescriptor>
SceneInspectorAPI::getProperties(const std::string &objectId) const {
  auto *obj = m_sceneGraph->findObject(objectId);
  if (!obj) {
    return {};
  }

  std::vector<PropertyDescriptor> props = getBaseProperties(obj);

  // Add type-specific properties
  switch (obj->getType()) {
  case SceneObjectType::Background: {
    auto *bg = static_cast<const BackgroundObject *>(obj);
    props.push_back(createPropertyDescriptor(
        "textureId", PropertyDescriptor::Type::Resource, bg->getTextureId()));
    break;
  }
  case SceneObjectType::Character: {
    auto *character = static_cast<const CharacterObject *>(obj);
    props.push_back(createPropertyDescriptor("characterId",
                                             PropertyDescriptor::Type::String,
                                             character->getCharacterId()));
    props.push_back(createPropertyDescriptor("displayName",
                                             PropertyDescriptor::Type::String,
                                             character->getDisplayName()));
    props.push_back(createPropertyDescriptor("expression",
                                             PropertyDescriptor::Type::String,
                                             character->getExpression()));
    props.push_back(createPropertyDescriptor(
        "pose", PropertyDescriptor::Type::String, character->getPose()));

    PropertyDescriptor slotProp;
    slotProp.name = "slotPosition";
    slotProp.displayName = "Slot Position";
    slotProp.type = PropertyDescriptor::Type::Enum;
    slotProp.value =
        std::to_string(static_cast<int>(character->getSlotPosition()));
    slotProp.enumOptions = {"Left", "Center", "Right", "Custom"};
    props.push_back(slotProp);

    props.push_back(createPropertyDescriptor(
        "highlighted", PropertyDescriptor::Type::Bool,
        character->isHighlighted() ? "true" : "false"));
    break;
  }
  case SceneObjectType::DialogueUI: {
    auto *dialogue = static_cast<const DialogueUIObject *>(obj);
    props.push_back(createPropertyDescriptor(
        "speaker", PropertyDescriptor::Type::String, dialogue->getSpeaker()));
    props.push_back(createPropertyDescriptor(
        "text", PropertyDescriptor::Type::String, dialogue->getText()));
    props.push_back(createPropertyDescriptor(
        "backgroundTextureId", PropertyDescriptor::Type::Resource,
        dialogue->getBackgroundTextureId()));
    props.push_back(createPropertyDescriptor(
        "typewriterEnabled", PropertyDescriptor::Type::Bool,
        dialogue->isTypewriterEnabled() ? "true" : "false"));

    PropertyDescriptor speedProp;
    speedProp.name = "typewriterSpeed";
    speedProp.displayName = "Typewriter Speed";
    speedProp.type = PropertyDescriptor::Type::Float;
    speedProp.value = std::to_string(dialogue->getTypewriterSpeed());
    speedProp.minValue = 1.0f;
    speedProp.maxValue = 200.0f;
    props.push_back(speedProp);
    break;
  }
  case SceneObjectType::EffectOverlay: {
    auto *effect = static_cast<const EffectOverlayObject *>(obj);

    PropertyDescriptor typeProp;
    typeProp.name = "effectType";
    typeProp.displayName = "Effect Type";
    typeProp.type = PropertyDescriptor::Type::Enum;
    typeProp.value = std::to_string(static_cast<int>(effect->getEffectType()));
    typeProp.enumOptions = {"None", "Fade", "Flash", "Shake",
                            "Rain", "Snow", "Custom"};
    props.push_back(typeProp);

    PropertyDescriptor intensityProp;
    intensityProp.name = "intensity";
    intensityProp.displayName = "Intensity";
    intensityProp.type = PropertyDescriptor::Type::Float;
    intensityProp.value = std::to_string(effect->getIntensity());
    intensityProp.minValue = 0.0f;
    intensityProp.maxValue = 1.0f;
    props.push_back(intensityProp);
    break;
  }
  default:
    break;
  }

  // Add custom properties
  for (const auto &[name, value] : obj->getProperties()) {
    props.push_back(createPropertyDescriptor(
        name, PropertyDescriptor::Type::String, value));
  }

  return props;
}

std::optional<std::string>
SceneInspectorAPI::getProperty(const std::string &objectId,
                               const std::string &propertyName) const {
  auto *obj = m_sceneGraph->findObject(objectId);
  if (!obj) {
    return std::nullopt;
  }

  // Check standard properties
  if (propertyName == "x")
    return std::to_string(obj->getX());
  if (propertyName == "y")
    return std::to_string(obj->getY());
  if (propertyName == "scaleX")
    return std::to_string(obj->getScaleX());
  if (propertyName == "scaleY")
    return std::to_string(obj->getScaleY());
  if (propertyName == "rotation")
    return std::to_string(obj->getRotation());
  if (propertyName == "alpha")
    return std::to_string(obj->getAlpha());
  if (propertyName == "visible")
    return obj->isVisible() ? "true" : "false";
  if (propertyName == "zOrder")
    return std::to_string(obj->getZOrder());

  // Check custom properties
  return obj->getProperty(propertyName);
}

Result<void> SceneInspectorAPI::setProperty(const std::string &objectId,
                                            const std::string &propertyName,
                                            const std::string &value,
                                            bool recordUndo) {
  auto *obj = m_sceneGraph->findObject(objectId);
  if (!obj) {
    return Result<void>::error("Object not found: " + objectId);
  }

  std::string oldValue;
  auto oldValueOpt = getProperty(objectId, propertyName);
  if (oldValueOpt) {
    oldValue = *oldValueOpt;
  }

  // Record undo before making changes
  if (recordUndo && oldValue != value) {
    auto cmd = std::make_unique<SetPropertyCommand>(
        this, objectId, propertyName, oldValue, value);
    m_undoStack.push(std::move(cmd));
    while (!m_redoStack.empty()) {
      m_redoStack.pop();
    }
    notifyUndoStackChanged();
  }

  // Apply property change
  try {
    if (propertyName == "x") {
      obj->setPosition(std::stof(value), obj->getY());
    } else if (propertyName == "y") {
      obj->setPosition(obj->getX(), std::stof(value));
    } else if (propertyName == "scaleX") {
      obj->setScale(std::stof(value), obj->getScaleY());
    } else if (propertyName == "scaleY") {
      obj->setScale(obj->getScaleX(), std::stof(value));
    } else if (propertyName == "rotation") {
      obj->setRotation(std::stof(value));
    } else if (propertyName == "alpha") {
      obj->setAlpha(std::stof(value));
    } else if (propertyName == "visible") {
      obj->setVisible(value == "true" || value == "1");
    } else if (propertyName == "zOrder") {
      obj->setZOrder(std::stoi(value));
    } else {
      // Type-specific properties or custom properties
      obj->setProperty(propertyName, value);
    }
  } catch (const std::exception &e) {
    return Result<void>::error(std::string("Failed to set property: ") +
                               e.what());
  }

  notifySceneModified();
  return Result<void>::ok();
}

Result<std::string> SceneInspectorAPI::createObject(LayerType layer,
                                                    SceneObjectType type,
                                                    const std::string &id,
                                                    bool recordUndo) {
  std::string objectId = id.empty() ? generateUniqueId("object") : id;
  auto obj = createObjectOfType(type, objectId);

  if (!obj) {
    return Result<std::string>::error("Failed to create object of type");
  }

  if (recordUndo) {
    auto cmd = std::make_unique<AddObjectCommand>(this, layer, std::move(obj));
    cmd->execute();
    m_undoStack.push(std::move(cmd));
    while (!m_redoStack.empty()) {
      m_redoStack.pop();
    }
    notifyUndoStackChanged();
  } else {
    m_sceneGraph->addToLayer(layer, std::move(obj));
  }

  notifySceneModified();
  return Result<std::string>::ok(objectId);
}

Result<void> SceneInspectorAPI::deleteObject(const std::string &id,
                                             bool recordUndo) {
  auto *obj = m_sceneGraph->findObject(id);
  if (!obj) {
    return Result<void>::error("Object not found: " + id);
  }

  if (recordUndo) {
    auto cmd = std::make_unique<RemoveObjectCommand>(this, id);
    cmd->execute();
    m_undoStack.push(std::move(cmd));
    while (!m_redoStack.empty()) {
      m_redoStack.pop();
    }
    notifyUndoStackChanged();
  } else {
    LayerType layer = findObjectLayer(id);
    m_sceneGraph->removeFromLayer(layer, id);
  }

  // Remove from selection
  deselectObject(id);

  notifySceneModified();
  return Result<void>::ok();
}

Result<void> SceneInspectorAPI::duplicateObject(const std::string &sourceId,
                                                bool recordUndo) {
  auto *obj = m_sceneGraph->findObject(sourceId);
  if (!obj) {
    return Result<void>::error("Object not found: " + sourceId);
  }

  SceneObjectState state = obj->saveState();
  std::string newId = generateUniqueId(sourceId + "_copy");
  state.id = newId;
  state.x += 20.0f; // Offset position
  state.y += 20.0f;

  LayerType layer = findObjectLayer(sourceId);
  auto newObj = createObjectOfType(obj->getType(), newId);
  if (newObj) {
    newObj->loadState(state);

    if (recordUndo) {
      auto cmd =
          std::make_unique<AddObjectCommand>(this, layer, std::move(newObj));
      cmd->execute();
      m_undoStack.push(std::move(cmd));
      while (!m_redoStack.empty()) {
        m_redoStack.pop();
      }
      notifyUndoStackChanged();
    } else {
      m_sceneGraph->addToLayer(layer, std::move(newObj));
    }
  }

  notifySceneModified();
  return Result<void>::ok();
}

Result<void> SceneInspectorAPI::moveObject(const std::string &id, f32 x, f32 y,
                                           bool recordUndo) {
  auto *obj = m_sceneGraph->findObject(id);
  if (!obj) {
    return Result<void>::error("Object not found: " + id);
  }

  if (recordUndo) {
    std::string oldX = std::to_string(obj->getX());
    std::string oldY = std::to_string(obj->getY());

    auto cmdX = std::make_unique<SetPropertyCommand>(this, id, "x", oldX,
                                                     std::to_string(x));
    auto cmdY = std::make_unique<SetPropertyCommand>(this, id, "y", oldY,
                                                     std::to_string(y));

    // Execute both
    cmdX->execute();
    cmdY->execute();

    // For simplicity, only push one command
    m_undoStack.push(std::move(cmdX));
    while (!m_redoStack.empty()) {
      m_redoStack.pop();
    }
    notifyUndoStackChanged();
  } else {
    obj->setPosition(x, y);
  }

  notifySceneModified();
  return Result<void>::ok();
}

Result<void> SceneInspectorAPI::scaleObject(const std::string &id, f32 scaleX,
                                            f32 scaleY, bool recordUndo) {
  auto *obj = m_sceneGraph->findObject(id);
  if (!obj) {
    return Result<void>::error("Object not found: " + id);
  }

  if (recordUndo) {
    std::string oldScaleX = std::to_string(obj->getScaleX());
    std::string oldScaleY = std::to_string(obj->getScaleY());

    auto cmd = std::make_unique<SetPropertyCommand>(
        this, id, "scaleX", oldScaleX, std::to_string(scaleX));
    cmd->execute();
    m_undoStack.push(std::move(cmd));
    while (!m_redoStack.empty()) {
      m_redoStack.pop();
    }
    notifyUndoStackChanged();
  }

  obj->setScale(scaleX, scaleY);
  notifySceneModified();
  return Result<void>::ok();
}

Result<void> SceneInspectorAPI::rotateObject(const std::string &id, f32 angle,
                                             bool recordUndo) {
  auto *obj = m_sceneGraph->findObject(id);
  if (!obj) {
    return Result<void>::error("Object not found: " + id);
  }

  if (recordUndo) {
    std::string oldAngle = std::to_string(obj->getRotation());
    auto cmd = std::make_unique<SetPropertyCommand>(
        this, id, "rotation", oldAngle, std::to_string(angle));
    cmd->execute();
    m_undoStack.push(std::move(cmd));
    while (!m_redoStack.empty()) {
      m_redoStack.pop();
    }
    notifyUndoStackChanged();
  } else {
    obj->setRotation(angle);
  }

  notifySceneModified();
  return Result<void>::ok();
}

Result<void> SceneInspectorAPI::setObjectLayer(const std::string &id,
                                               LayerType layer,
                                               bool /*recordUndo*/) {
  LayerType currentLayer = findObjectLayer(id);
  if (currentLayer == layer) {
    return Result<void>::ok();
  }

  auto obj = m_sceneGraph->removeFromLayer(currentLayer, id);
  if (!obj) {
    return Result<void>::error("Failed to remove object from current layer");
  }

  m_sceneGraph->addToLayer(layer, std::move(obj));
  notifySceneModified();
  return Result<void>::ok();
}

Result<void> SceneInspectorAPI::setObjectZOrder(const std::string &id,
                                                i32 zOrder, bool recordUndo) {
  return setProperty(id, "zOrder", std::to_string(zOrder), recordUndo);
}

void SceneInspectorAPI::selectObject(const std::string &id,
                                     bool addToSelection) {
  if (!addToSelection) {
    m_selection.clear();
  }

  if (std::find(m_selection.begin(), m_selection.end(), id) ==
      m_selection.end()) {
    m_selection.push_back(id);
  }

  notifySelectionChanged();
}

void SceneInspectorAPI::deselectObject(const std::string &id) {
  auto it = std::find(m_selection.begin(), m_selection.end(), id);
  if (it != m_selection.end()) {
    m_selection.erase(it);
    notifySelectionChanged();
  }
}

void SceneInspectorAPI::clearSelection() {
  if (!m_selection.empty()) {
    m_selection.clear();
    notifySelectionChanged();
  }
}

const std::vector<std::string> &SceneInspectorAPI::getSelection() const {
  return m_selection;
}

bool SceneInspectorAPI::isSelected(const std::string &id) const {
  return std::find(m_selection.begin(), m_selection.end(), id) !=
         m_selection.end();
}

Result<void> SceneInspectorAPI::moveSelection(f32 deltaX, f32 deltaY,
                                              bool recordUndo) {
  for (const auto &id : m_selection) {
    auto *obj = m_sceneGraph->findObject(id);
    if (obj) {
      moveObject(id, obj->getX() + deltaX, obj->getY() + deltaY, recordUndo);
    }
  }
  return Result<void>::ok();
}

Result<void> SceneInspectorAPI::deleteSelection(bool recordUndo) {
  auto selectionCopy = m_selection;
  for (const auto &id : selectionCopy) {
    deleteObject(id, recordUndo);
  }
  return Result<void>::ok();
}

Result<void> SceneInspectorAPI::duplicateSelection(bool recordUndo) {
  auto selectionCopy = m_selection;
  clearSelection();

  for (const auto &id : selectionCopy) {
    duplicateObject(id, recordUndo);
  }
  return Result<void>::ok();
}

void SceneInspectorAPI::executeCommand(std::unique_ptr<ICommand> command) {
  if (command) {
    command->execute();
    m_undoStack.push(std::move(command));
    while (!m_redoStack.empty()) {
      m_redoStack.pop();
    }

    // Limit stack size - remove oldest entries while preserving order
    if (m_undoStack.size() > m_maxHistorySize) {
      // Transfer all elements to a vector (this reverses the order)
      std::vector<std::unique_ptr<ICommand>> temp;
      while (!m_undoStack.empty()) {
        temp.push_back(std::move(m_undoStack.top()));
        m_undoStack.pop();
      }

      // temp now has [newest, ..., oldest]
      // Remove oldest entries from the end
      while (temp.size() > m_maxHistorySize) {
        temp.pop_back();
      }

      // Restore to stack in reverse order to maintain correct sequence
      for (auto it = temp.rbegin(); it != temp.rend(); ++it) {
        m_undoStack.push(std::move(*it));
      }
    }

    notifyUndoStackChanged();
  }
}

void SceneInspectorAPI::undo() {
  if (m_undoStack.empty()) {
    return;
  }

  auto cmd = std::move(m_undoStack.top());
  m_undoStack.pop();
  cmd->undo();
  m_redoStack.push(std::move(cmd));

  notifyUndoStackChanged();
  notifySceneModified();
}

void SceneInspectorAPI::redo() {
  if (m_redoStack.empty()) {
    return;
  }

  auto cmd = std::move(m_redoStack.top());
  m_redoStack.pop();
  cmd->execute();
  m_undoStack.push(std::move(cmd));

  notifyUndoStackChanged();
  notifySceneModified();
}

bool SceneInspectorAPI::canUndo() const { return !m_undoStack.empty(); }

bool SceneInspectorAPI::canRedo() const { return !m_redoStack.empty(); }

void SceneInspectorAPI::clearHistory() {
  while (!m_undoStack.empty()) {
    m_undoStack.pop();
  }
  while (!m_redoStack.empty()) {
    m_redoStack.pop();
  }
  notifyUndoStackChanged();
}

std::vector<std::string> SceneInspectorAPI::getUndoHistory() const {
  std::vector<std::string> history;
  std::stack<std::unique_ptr<ICommand>> tempStack;

  // Note: This is a simplified implementation. In production,
  // you'd want to maintain a separate list for display purposes.
  return history;
}

std::vector<std::string> SceneInspectorAPI::getRedoHistory() const {
  std::vector<std::string> history;
  return history;
}

void SceneInspectorAPI::addListener(IInspectorListener *listener) {
  if (listener && std::find(m_listeners.begin(), m_listeners.end(), listener) ==
                      m_listeners.end()) {
    m_listeners.push_back(listener);
  }
}

void SceneInspectorAPI::removeListener(IInspectorListener *listener) {
  auto it = std::find(m_listeners.begin(), m_listeners.end(), listener);
  if (it != m_listeners.end()) {
    m_listeners.erase(it);
  }
}

void SceneInspectorAPI::copySelection() {
  m_clipboard.clear();
  for (const auto &id : m_selection) {
    auto *obj = m_sceneGraph->findObject(id);
    if (obj) {
      m_clipboard.push_back(obj->saveState());
    }
  }
}

void SceneInspectorAPI::cutSelection() {
  copySelection();
  deleteSelection(true);
}

void SceneInspectorAPI::paste(f32 offsetX, f32 offsetY) {
  clearSelection();

  for (const auto &state : m_clipboard) {
    std::string newId = generateUniqueId(state.id);
    auto obj = createObjectOfType(state.type, newId);
    if (obj) {
      SceneObjectState newState = state;
      newState.id = newId;
      newState.x += offsetX;
      newState.y += offsetY;
      obj->loadState(newState);

      LayerType layer = LayerType::Background;
      switch (state.type) {
      case SceneObjectType::Background:
        layer = LayerType::Background;
        break;
      case SceneObjectType::Character:
        layer = LayerType::Characters;
        break;
      case SceneObjectType::DialogueUI:
      case SceneObjectType::ChoiceUI:
        layer = LayerType::UI;
        break;
      case SceneObjectType::EffectOverlay:
        layer = LayerType::Effects;
        break;
      default:
        break;
      }

      m_sceneGraph->addToLayer(layer, std::move(obj));
      selectObject(newId, true);
    }
  }

  notifySceneModified();
}

bool SceneInspectorAPI::hasClipboardContent() const {
  return !m_clipboard.empty();
}

SceneState SceneInspectorAPI::getSceneSnapshot() const {
  return m_sceneGraph->saveState();
}

void SceneInspectorAPI::restoreSceneSnapshot(const SceneState &snapshot) {
  m_sceneGraph->loadState(snapshot);
  clearSelection();
  notifySceneModified();
}

void SceneInspectorAPI::onObjectAdded(const std::string & /*objectId*/,
                                      SceneObjectType /*type*/) {
  notifySceneModified();
}

void SceneInspectorAPI::onObjectRemoved(const std::string &objectId) {
  deselectObject(objectId);
  notifySceneModified();
}

void SceneInspectorAPI::onPropertyChanged(const PropertyChange & /*change*/) {
  notifySceneModified();
}

void SceneInspectorAPI::onLayerChanged(const std::string & /*objectId*/,
                                       const std::string & /*newLayer*/) {
  notifySceneModified();
}

PropertyDescriptor
SceneInspectorAPI::createPropertyDescriptor(const std::string &name,
                                            PropertyDescriptor::Type type,
                                            const std::string &value) const {
  PropertyDescriptor desc;
  desc.name = name;
  desc.displayName = name; // Could be prettified
  desc.type = type;
  desc.value = value;
  desc.defaultValue = "";
  return desc;
}

std::vector<PropertyDescriptor>
SceneInspectorAPI::getBaseProperties(const SceneObjectBase *obj) const {
  std::vector<PropertyDescriptor> props;

  PropertyDescriptor idProp;
  idProp.name = "id";
  idProp.displayName = "ID";
  idProp.type = PropertyDescriptor::Type::String;
  idProp.value = obj->getId();
  idProp.readOnly = true;
  props.push_back(idProp);

  PropertyDescriptor typeProp;
  typeProp.name = "type";
  typeProp.displayName = "Type";
  typeProp.type = PropertyDescriptor::Type::String;
  typeProp.value = obj->getTypeName();
  typeProp.readOnly = true;
  props.push_back(typeProp);

  props.push_back(createPropertyDescriptor("x", PropertyDescriptor::Type::Float,
                                           std::to_string(obj->getX())));
  props.push_back(createPropertyDescriptor("y", PropertyDescriptor::Type::Float,
                                           std::to_string(obj->getY())));
  props.push_back(createPropertyDescriptor("scaleX",
                                           PropertyDescriptor::Type::Float,
                                           std::to_string(obj->getScaleX())));
  props.push_back(createPropertyDescriptor("scaleY",
                                           PropertyDescriptor::Type::Float,
                                           std::to_string(obj->getScaleY())));
  props.push_back(createPropertyDescriptor("rotation",
                                           PropertyDescriptor::Type::Float,
                                           std::to_string(obj->getRotation())));
  props.push_back(createPropertyDescriptor("alpha",
                                           PropertyDescriptor::Type::Float,
                                           std::to_string(obj->getAlpha())));
  props.push_back(
      createPropertyDescriptor("visible", PropertyDescriptor::Type::Bool,
                               obj->isVisible() ? "true" : "false"));
  props.push_back(createPropertyDescriptor("zOrder",
                                           PropertyDescriptor::Type::Int,
                                           std::to_string(obj->getZOrder())));

  return props;
}

LayerType SceneInspectorAPI::findObjectLayer(const std::string &id) const {
  if (m_sceneGraph->getBackgroundLayer().findObject(id)) {
    return LayerType::Background;
  }
  if (m_sceneGraph->getCharacterLayer().findObject(id)) {
    return LayerType::Characters;
  }
  if (m_sceneGraph->getUILayer().findObject(id)) {
    return LayerType::UI;
  }
  if (m_sceneGraph->getEffectLayer().findObject(id)) {
    return LayerType::Effects;
  }
  return LayerType::Background; // Default
}

void SceneInspectorAPI::notifySelectionChanged() {
  for (auto *listener : m_listeners) {
    if (listener) {
      listener->onSelectionChanged(m_selection);
    }
  }
}

void SceneInspectorAPI::notifySceneModified() {
  for (auto *listener : m_listeners) {
    if (listener) {
      listener->onSceneModified();
    }
  }
}

void SceneInspectorAPI::notifyUndoStackChanged() {
  for (auto *listener : m_listeners) {
    if (listener) {
      listener->onUndoStackChanged(canUndo(), canRedo());
    }
  }
}

std::string SceneInspectorAPI::generateUniqueId(const std::string &base) const {
  std::string candidate = base;
  int counter = 1;

  while (m_sceneGraph->findObject(candidate) != nullptr) {
    candidate = base + "_" + std::to_string(counter);
    ++counter;
  }

  return candidate;
}

std::unique_ptr<SceneObjectBase>
SceneInspectorAPI::createObjectOfType(SceneObjectType type,
                                      const std::string &id) const {
  switch (type) {
  case SceneObjectType::Background:
    return std::make_unique<BackgroundObject>(id);
  case SceneObjectType::Character:
    return std::make_unique<CharacterObject>(id, "");
  case SceneObjectType::DialogueUI:
    return std::make_unique<DialogueUIObject>(id);
  case SceneObjectType::ChoiceUI:
    return std::make_unique<ChoiceUIObject>(id);
  case SceneObjectType::EffectOverlay:
    return std::make_unique<EffectOverlayObject>(id);
  default:
    return nullptr;
  }
}

} // namespace NovelMind::scene
