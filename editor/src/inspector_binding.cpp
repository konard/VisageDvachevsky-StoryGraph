#include "NovelMind/editor/inspector_binding.hpp"
#include "NovelMind/editor/event_bus.hpp"
#include "NovelMind/editor/qt/nm_undo_manager.hpp"
#include "NovelMind/editor/undo_system.hpp"
#include <algorithm>

namespace NovelMind::editor {

using qt::NMUndoManager;
using qt::PropertyChangeCommand;

// Static instance
std::unique_ptr<InspectorBindingManager> InspectorBindingManager::s_instance = nullptr;

InspectorBindingManager::InspectorBindingManager() = default;

InspectorBindingManager::~InspectorBindingManager() = default;

InspectorBindingManager& InspectorBindingManager::instance() {
  if (!s_instance) {
    s_instance = std::make_unique<InspectorBindingManager>();
  }
  return *s_instance;
}

// ============================================================================
// Target Management
// ============================================================================

void InspectorBindingManager::setTarget(const InspectorTarget& target) {
  if (m_inBatch) {
    endPropertyBatch();
  }

  m_target = target;

  // Single target mode: m_targets contains only one element
  if (m_target.isValid()) {
    m_targets = {m_target};
  } else {
    m_targets.clear();
  }

  m_cachedValues.clear();

  if (m_target.isValid()) {
    // Cache current property values
    auto properties = getProperties();
    for (const auto* prop : properties) {
      if (prop) {
        m_cachedValues[prop->getMeta().name] = prop->getValue(m_target.object);
      }
    }
  }

  notifyTargetChanged();
}

void InspectorBindingManager::setTargets(const std::vector<InspectorTarget>& targets) {
  if (m_inBatch) {
    endPropertyBatch();
  }

  m_targets = targets;
  if (!m_targets.empty()) {
    m_target = m_targets[0]; // Primary target
  } else {
    m_target = InspectorTarget();
  }

  m_cachedValues.clear();

  if (!m_targets.empty()) {
    // Cache property values (for multi-edit, cache from primary target)
    auto properties = getProperties();
    for (const auto* prop : properties) {
      if (prop) {
        m_cachedValues[prop->getMeta().name] = prop->getValue(m_target.object);
      }
    }
  }

  notifyTargetChanged();
}

void InspectorBindingManager::inspectSceneObject(const std::string& objectId, void* object) {
  setTarget(InspectorTarget(InspectorTargetType::SceneObject, objectId, object));
}

void InspectorBindingManager::inspectSceneObjects(const std::vector<std::string>& objectIds,
                                                  const std::vector<void*>& objects) {
  if (objectIds.size() != objects.size()) {
    return; // Invalid input
  }

  std::vector<InspectorTarget> targets;
  targets.reserve(objectIds.size());

  for (size_t i = 0; i < objectIds.size(); ++i) {
    targets.emplace_back(InspectorTargetType::SceneObject, objectIds[i], objects[i]);
  }

  setTargets(targets);
}

void InspectorBindingManager::inspectStoryGraphNode(scripting::NodeId nodeId,
                                                    scripting::VisualGraphNode* node) {
  setTarget(InspectorTarget(InspectorTargetType::StoryGraphNode, std::to_string(nodeId), node));
}

void InspectorBindingManager::inspectTimelineTrack(const std::string& trackId, void* track) {
  setTarget(InspectorTarget(InspectorTargetType::TimelineTrack, trackId, track));
}

void InspectorBindingManager::inspectTimelineKeyframe(const std::string& trackId, u64 keyframeIndex,
                                                      void* keyframe) {
  std::string id = trackId + "_" + std::to_string(keyframeIndex);
  setTarget(InspectorTarget(InspectorTargetType::TimelineKeyframe, id, keyframe));
}

void InspectorBindingManager::clearTarget() {
  if (m_inBatch) {
    endPropertyBatch();
  }

  m_target = InspectorTarget();
  m_targets.clear();
  m_cachedValues.clear();
  notifyTargetChanged();
}

const InspectorTarget& InspectorBindingManager::getTarget() const {
  return m_target;
}

bool InspectorBindingManager::hasTarget() const {
  return m_target.isValid();
}

const std::vector<InspectorTarget>& InspectorBindingManager::getTargets() const {
  return m_targets;
}

bool InspectorBindingManager::isMultiEdit() const {
  return m_targets.size() > 1;
}

size_t InspectorBindingManager::getTargetCount() const {
  return m_targets.size();
}

// ============================================================================
// Property Access
// ============================================================================

std::vector<const IPropertyAccessor*> InspectorBindingManager::getProperties() const {
  std::vector<const IPropertyAccessor*> result;

  if (!m_target.isValid()) {
    return result;
  }

  const TypeInfo* typeInfo = getTypeInfoForTarget();
  if (!typeInfo) {
    return result;
  }

  const auto& properties = typeInfo->getProperties();
  result.reserve(properties.size());

  for (const auto& prop : properties) {
    result.push_back(prop.get());
  }

  return result;
}

std::vector<PropertyGroup> InspectorBindingManager::getPropertyGroups() const {
  std::vector<PropertyGroup> groups;

  if (!m_target.isValid()) {
    return groups;
  }

  const TypeInfo* typeInfo = getTypeInfoForTarget();
  if (!typeInfo) {
    return groups;
  }

  // Group properties by category
  std::unordered_map<std::string, PropertyGroup> groupMap;

  const auto& properties = typeInfo->getProperties();
  for (const auto& prop : properties) {
    const auto& meta = prop->getMeta();

    // Skip hidden properties
    if (hasFlag(meta.flags, PropertyFlags::Hidden)) {
      continue;
    }

    std::string category = meta.category.empty() ? "General" : meta.category;

    auto& group = groupMap[category];
    if (group.name.empty()) {
      group.name = category;
      group.category = category;
    }
    group.properties.push_back(prop.get());
  }

  // Convert to vector and sort
  groups.reserve(groupMap.size());
  for (auto& pair : groupMap) {
    // Sort properties within group by order
    std::sort(pair.second.properties.begin(), pair.second.properties.end(),
              [](const IPropertyAccessor* a, const IPropertyAccessor* b) {
                return a->getMeta().order < b->getMeta().order;
              });
    groups.push_back(std::move(pair.second));
  }

  // Sort groups (General first, then alphabetically)
  std::sort(groups.begin(), groups.end(), [](const PropertyGroup& a, const PropertyGroup& b) {
    if (a.name == "General")
      return true;
    if (b.name == "General")
      return false;
    return a.name < b.name;
  });

  return groups;
}

const IPropertyAccessor* InspectorBindingManager::getProperty(const std::string& name) const {
  if (!m_target.isValid()) {
    return nullptr;
  }

  const TypeInfo* typeInfo = getTypeInfoForTarget();
  if (!typeInfo) {
    return nullptr;
  }

  return typeInfo->findProperty(name);
}

PropertyValue InspectorBindingManager::getPropertyValue(const std::string& name) const {
  if (!m_target.isValid()) {
    return nullptr;
  }

  const auto* prop = getProperty(name);
  if (!prop) {
    return nullptr;
  }

  // Single target mode
  if (m_targets.size() <= 1) {
    return prop->getValue(m_target.object);
  }

  // Multi-edit mode: check if all targets have the same value
  PropertyValue firstValue = prop->getValue(m_targets[0].object);

  for (size_t i = 1; i < m_targets.size(); ++i) {
    PropertyValue currentValue = prop->getValue(m_targets[i].object);
    if (currentValue != firstValue) {
      // Values differ, return MultipleValues marker
      return MultipleValues{};
    }
  }

  // All values are the same
  return firstValue;
}

std::string InspectorBindingManager::getPropertyValueAsString(const std::string& name) const {
  return PropertyUtils::toString(getPropertyValue(name));
}

std::optional<std::string> InspectorBindingManager::setPropertyValue(const std::string& name,
                                                                     const PropertyValue& value) {
  if (!m_target.isValid()) {
    return "No target selected";
  }

  if (m_applyingUndoRedo) {
    // Avoid re-entrancy while applying an undo/redo
    return std::nullopt;
  }

  const auto* prop = getProperty(name);
  if (!prop) {
    return "Property not found: " + name;
  }

  const auto& meta = prop->getMeta();

  // Check read-only
  if (hasFlag(meta.flags, PropertyFlags::ReadOnly)) {
    return "Property is read-only";
  }

  // Multi-edit mode: apply to all targets
  if (m_targets.size() > 1) {
    // Use batch to ensure atomic undo/redo for all changes
    bool wasInBatch = m_inBatch;
    if (!wasInBatch) {
      beginPropertyBatch("Edit " + std::to_string(m_targets.size()) + " objects");
    }

    // Apply to all targets
    for (auto& target : m_targets) {
      PropertyValue oldValue = prop->getValue(target.object);

      PropertyChangeContext context;
      context.target = target;
      context.propertyName = name;
      context.oldValue = oldValue;
      context.newValue = value;

      // Validate
      std::string error;
      if (!validatePropertyChange(context, error)) {
        if (!wasInBatch) {
          endPropertyBatch();
        }
        return error;
      }

      // Notify before change
      notifyPropertyWillChange(context);

      // Check binding handlers
      auto bindingIt = m_bindings.find(name);
      if (bindingIt != m_bindings.end() && bindingIt->second.beforeChange) {
        if (!bindingIt->second.beforeChange(context)) {
          if (!wasInBatch) {
            endPropertyBatch();
          }
          return "Change rejected by handler";
        }
      }

      // Apply change
      prop->setValue(target.object, value);

      // Record for batch undo
      m_batchChanges.push_back(context);

      // Notify after change
      notifyPropertyDidChange(context);

      // Call after-change handler
      if (bindingIt != m_bindings.end() && bindingIt->second.afterChange) {
        bindingIt->second.afterChange(context);
      }

      // Publish event
      publishPropertyChangedEvent(context);
    }

    // Update cache with new value
    m_cachedValues[name] = value;

    // Refresh dependent properties
    refreshDependentProperties(name);

    if (!wasInBatch) {
      endPropertyBatch();
    }

    return std::nullopt;
  }

  // Single target mode (original behavior)
  PropertyValue oldValue = prop->getValue(m_target.object);

  // Create change context
  PropertyChangeContext context;
  context.target = m_target;
  context.propertyName = name;
  context.oldValue = oldValue;
  context.newValue = value;

  // Validate
  std::string error;
  if (!validatePropertyChange(context, error)) {
    return error;
  }

  // Notify before change
  notifyPropertyWillChange(context);

  // Check binding handlers
  auto bindingIt = m_bindings.find(name);
  if (bindingIt != m_bindings.end() && bindingIt->second.beforeChange) {
    if (!bindingIt->second.beforeChange(context)) {
      return "Change rejected by handler";
    }
  }

  // Apply change
  prop->setValue(m_target.object, value);

  // Update cache
  m_cachedValues[name] = value;

  // Record undo
  if (!m_inBatch && !m_applyingUndoRedo) {
    recordPropertyChange(context);
  } else if (m_inBatch && !m_applyingUndoRedo) {
    m_batchChanges.push_back(context);
  }

  // Notify after change
  notifyPropertyDidChange(context);

  // Call after-change handler
  if (bindingIt != m_bindings.end() && bindingIt->second.afterChange) {
    bindingIt->second.afterChange(context);
  }

  // Refresh dependent properties
  refreshDependentProperties(name);

  // Publish event
  publishPropertyChangedEvent(context);

  return std::nullopt;
}

std::optional<std::string>
InspectorBindingManager::setPropertyValueFromString(const std::string& name,
                                                    const std::string& value) {
  if (!m_target.isValid()) {
    return "No target selected";
  }

  const auto* prop = getProperty(name);
  if (!prop) {
    return "Property not found: " + name;
  }

  PropertyValue parsedValue = PropertyUtils::fromString(prop->getMeta().type, value);

  return setPropertyValue(name, parsedValue);
}

void InspectorBindingManager::beginPropertyBatch(const std::string& description) {
  if (m_inBatch) {
    endPropertyBatch();
  }

  m_inBatch = true;
  m_batchDescription = description;
  m_batchChanges.clear();
}

void InspectorBindingManager::endPropertyBatch() {
  if (!m_inBatch) {
    return;
  }

  m_inBatch = false;

  // Record all batch changes as a single undo entry
  if (!m_batchChanges.empty()) {
    NMUndoManager::instance().beginMacro(QString::fromStdString(
        m_batchDescription.empty() ? "Batch Property Change" : m_batchDescription));
    for (const auto& ctx : m_batchChanges) {
      recordPropertyChange(ctx);
    }
    NMUndoManager::instance().endMacro();
  }

  m_batchChanges.clear();
  m_batchDescription.clear();
}

bool InspectorBindingManager::isInBatch() const {
  return m_inBatch;
}

// ============================================================================
// Property Binding Configuration
// ============================================================================

void InspectorBindingManager::registerBinding(const std::string& propertyName,
                                              const PropertyBinding& binding) {
  m_bindings[propertyName] = binding;
}

void InspectorBindingManager::onBeforePropertyChange(const std::string& propertyName,
                                                     BeforePropertyChangeHandler handler) {
  m_bindings[propertyName].beforeChange = std::move(handler);
}

void InspectorBindingManager::onAfterPropertyChange(const std::string& propertyName,
                                                    AfterPropertyChangeHandler handler) {
  m_bindings[propertyName].afterChange = std::move(handler);
}

void InspectorBindingManager::addPropertyValidator(const std::string& propertyName,
                                                   PropertyValidatorHandler validator) {
  m_bindings[propertyName].validator = std::move(validator);
}

void InspectorBindingManager::setPropertyDependencies(
    const std::string& propertyName, const std::vector<std::string>& dependencies) {
  m_bindings[propertyName].dependentProperties = dependencies;
}

// ============================================================================
// Integration
// ============================================================================

void InspectorBindingManager::setUndoManager(UndoManager* manager) {
  m_undoManager = manager;
}

void InspectorBindingManager::setEventBus(EventBus* bus) {
  m_eventBus = bus;
}

// ============================================================================
// Listeners
// ============================================================================

void InspectorBindingManager::addListener(IInspectorBindingListener* listener) {
  if (listener &&
      std::find(m_listeners.begin(), m_listeners.end(), listener) == m_listeners.end()) {
    m_listeners.push_back(listener);
  }
}

void InspectorBindingManager::removeListener(IInspectorBindingListener* listener) {
  m_listeners.erase(std::remove(m_listeners.begin(), m_listeners.end(), listener),
                    m_listeners.end());
}

// ============================================================================
// Utility
// ============================================================================

void InspectorBindingManager::refreshProperties() {
  if (!m_target.isValid()) {
    return;
  }

  auto properties = getProperties();
  for (const auto* prop : properties) {
    if (prop) {
      m_cachedValues[prop->getMeta().name] = prop->getValue(m_target.object);
    }
  }

  for (auto* listener : m_listeners) {
    listener->onPropertiesRefreshed();
  }
}

bool InspectorBindingManager::hasPropertyChanged(const std::string& name) const {
  if (!m_target.isValid()) {
    return false;
  }

  const auto* prop = getProperty(name);
  if (!prop) {
    return false;
  }

  auto cachedIt = m_cachedValues.find(name);
  if (cachedIt == m_cachedValues.end()) {
    return true;
  }

  PropertyValue currentValue = prop->getValue(m_target.object);
  return currentValue != cachedIt->second;
}

// ============================================================================
// Private Methods
// ============================================================================

const TypeInfo* InspectorBindingManager::getTypeInfoForTarget() const {
  if (!m_target.isValid()) {
    return nullptr;
  }

  // Check local type info first
  auto it = m_typeInfoMap.find(m_target.typeIndex);
  if (it != m_typeInfoMap.end()) {
    return it->second.get();
  }

  // Check global registry
  return PropertyRegistry::instance().getTypeInfo(m_target.typeIndex);
}

bool InspectorBindingManager::validatePropertyChange(const PropertyChangeContext& context,
                                                     std::string& error) const {
  // Check property meta validation
  const auto* prop = getProperty(context.propertyName);
  if (!prop) {
    error = "Property not found";
    return false;
  }

  if (!PropertyUtils::validate(context.newValue, prop->getMeta(), &error)) {
    return false;
  }

  // Check custom validator
  auto bindingIt = m_bindings.find(context.propertyName);
  if (bindingIt != m_bindings.end() && bindingIt->second.validator) {
    auto validationError = bindingIt->second.validator(context);
    if (validationError.has_value()) {
      error = validationError.value();
      return false;
    }
  }

  return true;
}

void InspectorBindingManager::notifyTargetChanged() {
  for (auto* listener : m_listeners) {
    listener->onTargetChanged(m_target);
  }
}

void InspectorBindingManager::notifyPropertyWillChange(const PropertyChangeContext& context) {
  for (auto* listener : m_listeners) {
    listener->onPropertyWillChange(context);
  }
}

void InspectorBindingManager::notifyPropertyDidChange(const PropertyChangeContext& context) {
  for (auto* listener : m_listeners) {
    listener->onPropertyDidChange(context);
  }
}

void InspectorBindingManager::recordPropertyChange(const PropertyChangeContext& context) {
  auto bindingIt = m_bindings.find(context.propertyName);
  if (bindingIt != m_bindings.end() && !bindingIt->second.recordUndo) {
    return;
  }

  if (!m_target.isValid()) {
    return;
  }

  const auto* prop = getProperty(context.propertyName);
  if (!prop) {
    return;
  }

  PropertyChangeContext captured = context;

  auto apply = [this, captured](const PropertyValue& value, bool isUndo) {
    applyPropertyChangeFromUndo(captured, value, isUndo);
  };

  NMUndoManager::instance().pushCommand(new PropertyChangeCommand(
      QString::fromStdString(context.target.id), QString::fromStdString(context.propertyName),
      context.oldValue, context.newValue, apply));
}

void InspectorBindingManager::applyPropertyChangeFromUndo(const PropertyChangeContext& context,
                                                          const PropertyValue& value, bool isUndo) {
  if (!m_target.isValid() || context.target.id != m_target.id ||
      context.target.type != m_target.type) {
    return; // Target changed; cannot safely apply.
  }

  const auto* prop = getProperty(context.propertyName);
  if (!prop) {
    return;
  }

  PropertyChangeContext ctx = context;
  ctx.newValue = value;
  ctx.fromUndo = isUndo;
  ctx.fromRedo = !isUndo;

  m_applyingUndoRedo = true;

  notifyPropertyWillChange(ctx);

  prop->setValue(m_target.object, value);
  m_cachedValues[context.propertyName] = value;

  auto bindingIt = m_bindings.find(context.propertyName);
  if (bindingIt != m_bindings.end() && bindingIt->second.afterChange) {
    bindingIt->second.afterChange(ctx);
  }

  notifyPropertyDidChange(ctx);
  refreshDependentProperties(context.propertyName);
  publishPropertyChangedEvent(ctx);

  m_applyingUndoRedo = false;
}

void InspectorBindingManager::refreshDependentProperties(const std::string& propertyName) {
  auto bindingIt = m_bindings.find(propertyName);
  if (bindingIt == m_bindings.end()) {
    return;
  }

  for (const auto& depName : bindingIt->second.dependentProperties) {
    const auto* prop = getProperty(depName);
    if (prop) {
      m_cachedValues[depName] = prop->getValue(m_target.object);
    }
  }
}

void InspectorBindingManager::publishPropertyChangedEvent(const PropertyChangeContext& context) {
  if (!m_eventBus) {
    return;
  }

  auto bindingIt = m_bindings.find(context.propertyName);
  if (bindingIt != m_bindings.end() && !bindingIt->second.notifyEventBus) {
    return;
  }

  PropertyChangedEvent event;
  event.objectId = context.target.id;
  event.propertyName = context.propertyName;
  event.oldValue = PropertyUtils::toString(context.oldValue);
  event.newValue = PropertyUtils::toString(context.newValue);
  event.source = "InspectorBinding";

  m_eventBus->publish(event);
}

} // namespace NovelMind::editor
