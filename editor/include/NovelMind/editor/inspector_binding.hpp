#pragma once

/**
 * @file inspector_binding.hpp
 * @brief Inspector Backend Binding System for NovelMind Editor
 *
 * Provides the binding layer between the Inspector panel and backend objects:
 * - Connects property introspection to actual object values
 * - Handles property change callbacks (onBefore/onAfter)
 * - Automatic dependent system updates
 * - Undo/Redo integration for property changes
 *
 * This bridges the Property Introspection System with the actual objects,
 * enabling the GUI Inspector to display and edit properties.
 */

#include "NovelMind/core/property_system.hpp"
#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include "NovelMind/scripting/ir.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <variant>
#include <vector>

namespace NovelMind::editor {

// Forward declarations
class InspectorBindingManager;
class UndoManager;
class EventBus;

/**
 * @brief Target object types for inspection
 */
enum class InspectorTargetType : u8 {
  None = 0,
  SceneObject,
  StoryGraphNode,
  TimelineTrack,
  TimelineKeyframe,
  Asset,
  ProjectSettings,
  EditorSettings
};

/**
 * @brief Generic target identifier
 */
struct InspectorTarget {
  InspectorTargetType type = InspectorTargetType::None;
  std::string id;
  void* object = nullptr;
  std::type_index typeIndex = std::type_index(typeid(void));

  InspectorTarget() = default;

  template <typename T>
  InspectorTarget(InspectorTargetType t, const std::string& targetId, T* obj)
      : type(t), id(targetId), object(obj), typeIndex(typeid(T)) {}

  [[nodiscard]] bool isValid() const {
    return type != InspectorTargetType::None && object != nullptr;
  }
};

/**
 * @brief Property change context
 */
struct PropertyChangeContext {
  InspectorTarget target;
  std::string propertyName;
  PropertyValue oldValue;
  PropertyValue newValue;
  bool fromUndo = false;
  bool fromRedo = false;
};

/**
 * @brief Property change handler types
 */
using BeforePropertyChangeHandler = std::function<bool(const PropertyChangeContext&)>;
using AfterPropertyChangeHandler = std::function<void(const PropertyChangeContext&)>;
using PropertyValidatorHandler =
    std::function<std::optional<std::string>(const PropertyChangeContext&)>;

/**
 * @brief Binding configuration for a property
 */
struct PropertyBinding {
  std::string propertyName;
  BeforePropertyChangeHandler beforeChange;
  AfterPropertyChangeHandler afterChange;
  PropertyValidatorHandler validator;
  std::vector<std::string> dependentProperties; // Properties to refresh when
                                                // this changes
  bool recordUndo = true;
  bool notifyEventBus = true;
};

/**
 * @brief Listener for inspector binding events
 */
class IInspectorBindingListener {
public:
  virtual ~IInspectorBindingListener() = default;

  virtual void onTargetChanged(const InspectorTarget& /*target*/) {}
  virtual void onPropertyWillChange(const PropertyChangeContext& /*context*/) {}
  virtual void onPropertyDidChange(const PropertyChangeContext& /*context*/) {}
  virtual void onPropertiesRefreshed() {}
};

/**
 * @brief Property group for UI organization
 */
struct PropertyGroup {
  std::string name;
  std::string category;
  bool collapsed = false;
  std::vector<const IPropertyAccessor*> properties;
};

/**
 * @brief Inspector binding manager
 *
 * Responsibilities:
 * - Manage the current inspection target
 * - Provide property values for the Inspector UI
 * - Handle property changes with proper validation and callbacks
 * - Integrate with undo/redo system
 * - Notify dependent systems of changes
 */
class InspectorBindingManager {
public:
  InspectorBindingManager();
  ~InspectorBindingManager();

  // Prevent copying
  InspectorBindingManager(const InspectorBindingManager&) = delete;
  InspectorBindingManager& operator=(const InspectorBindingManager&) = delete;

  /**
   * @brief Get singleton instance
   */
  static InspectorBindingManager& instance();

  // =========================================================================
  // Target Management
  // =========================================================================

  /**
   * @brief Set the current inspection target
   */
  void setTarget(const InspectorTarget& target);

  /**
   * @brief Set multiple inspection targets (multi-object editing)
   */
  void setTargets(const std::vector<InspectorTarget>& targets);

  /**
   * @brief Set target to a scene object
   */
  void inspectSceneObject(const std::string& objectId, void* object);

  /**
   * @brief Set targets to multiple scene objects (multi-object editing)
   */
  void inspectSceneObjects(const std::vector<std::string>& objectIds,
                           const std::vector<void*>& objects);

  /**
   * @brief Set target to a story graph node
   */
  void inspectStoryGraphNode(scripting::NodeId nodeId, scripting::VisualGraphNode* node);

  /**
   * @brief Set target to timeline track
   */
  void inspectTimelineTrack(const std::string& trackId, void* track);

  /**
   * @brief Set target to a keyframe
   */
  void inspectTimelineKeyframe(const std::string& trackId, u64 keyframeIndex, void* keyframe);

  /**
   * @brief Clear the current target
   */
  void clearTarget();

  /**
   * @brief Get the current target
   */
  [[nodiscard]] const InspectorTarget& getTarget() const;

  /**
   * @brief Check if there's a valid target
   */
  [[nodiscard]] bool hasTarget() const;

  /**
   * @brief Get all targets (for multi-object editing)
   */
  [[nodiscard]] const std::vector<InspectorTarget>& getTargets() const;

  /**
   * @brief Check if inspecting multiple targets
   */
  [[nodiscard]] bool isMultiEdit() const;

  /**
   * @brief Get count of targets
   */
  [[nodiscard]] size_t getTargetCount() const;

  // =========================================================================
  // Property Access
  // =========================================================================

  /**
   * @brief Get all properties for the current target
   */
  [[nodiscard]] std::vector<const IPropertyAccessor*> getProperties() const;

  /**
   * @brief Get properties organized by group/category
   */
  [[nodiscard]] std::vector<PropertyGroup> getPropertyGroups() const;

  /**
   * @brief Get a specific property accessor
   */
  [[nodiscard]] const IPropertyAccessor* getProperty(const std::string& name) const;

  /**
   * @brief Get property value
   */
  [[nodiscard]] PropertyValue getPropertyValue(const std::string& name) const;

  /**
   * @brief Get property value as string
   */
  [[nodiscard]] std::string getPropertyValueAsString(const std::string& name) const;

  /**
   * @brief Set property value
   * @return Error message if validation fails, empty optional on success
   */
  std::optional<std::string> setPropertyValue(const std::string& name, const PropertyValue& value);

  /**
   * @brief Set property value from string
   */
  std::optional<std::string> setPropertyValueFromString(const std::string& name,
                                                        const std::string& value);

  /**
   * @brief Begin a batch of property changes (single undo entry)
   */
  void beginPropertyBatch(const std::string& description);

  /**
   * @brief End the current property batch
   */
  void endPropertyBatch();

  /**
   * @brief Check if in batch mode
   */
  [[nodiscard]] bool isInBatch() const;

  // =========================================================================
  // Property Binding Configuration
  // =========================================================================

  /**
   * @brief Register a property binding
   */
  void registerBinding(const std::string& propertyName, const PropertyBinding& binding);

  /**
   * @brief Register before-change handler for a property
   */
  void onBeforePropertyChange(const std::string& propertyName, BeforePropertyChangeHandler handler);

  /**
   * @brief Register after-change handler for a property
   */
  void onAfterPropertyChange(const std::string& propertyName, AfterPropertyChangeHandler handler);

  /**
   * @brief Register property validator
   */
  void addPropertyValidator(const std::string& propertyName, PropertyValidatorHandler validator);

  /**
   * @brief Set property dependencies
   */
  void setPropertyDependencies(const std::string& propertyName,
                               const std::vector<std::string>& dependencies);

  // =========================================================================
  // Type Registration
  // =========================================================================

  /**
   * @brief Register type info for inspection
   */
  template <typename T>
  void registerType(const std::string& typeName, std::unique_ptr<TypeInfo> info) {
    m_typeInfoMap[std::type_index(typeid(T))] = std::move(info);
    m_typeNames[std::type_index(typeid(T))] = typeName;
  }

  /**
   * @brief Get type info for a type
   */
  template <typename T> [[nodiscard]] const TypeInfo* getTypeInfo() const {
    auto it = m_typeInfoMap.find(std::type_index(typeid(T)));
    if (it != m_typeInfoMap.end()) {
      return it->second.get();
    }
    return PropertyRegistry::instance().getTypeInfo<T>();
  }

  // =========================================================================
  // Integration
  // =========================================================================

  /**
   * @brief Set the undo manager for property change recording
   */
  void setUndoManager(UndoManager* manager);

  /**
   * @brief Set the event bus for notifications
   */
  void setEventBus(EventBus* bus);

  // =========================================================================
  // Listeners
  // =========================================================================

  /**
   * @brief Add a binding listener
   */
  void addListener(IInspectorBindingListener* listener);

  /**
   * @brief Remove a binding listener
   */
  void removeListener(IInspectorBindingListener* listener);

  // =========================================================================
  // Utility
  // =========================================================================

  /**
   * @brief Refresh all property values from the target
   */
  void refreshProperties();

  /**
   * @brief Check if a property has changed since last refresh
   */
  [[nodiscard]] bool hasPropertyChanged(const std::string& name) const;

private:
  // Internal methods
  const TypeInfo* getTypeInfoForTarget() const;
  bool validatePropertyChange(const PropertyChangeContext& context, std::string& error) const;
  void notifyTargetChanged();
  void notifyPropertyWillChange(const PropertyChangeContext& context);
  void notifyPropertyDidChange(const PropertyChangeContext& context);
  void recordPropertyChange(const PropertyChangeContext& context);
  void applyPropertyChangeFromUndo(const PropertyChangeContext& context, const PropertyValue& value,
                                   bool isUndo);
  void refreshDependentProperties(const std::string& propertyName);
  void publishPropertyChangedEvent(const PropertyChangeContext& context);

  // Current target (for single-object editing, first element of m_targets)
  InspectorTarget m_target;

  // All targets (for multi-object editing, size > 1 means multi-edit mode)
  std::vector<InspectorTarget> m_targets;

  // Property bindings
  std::unordered_map<std::string, PropertyBinding> m_bindings;

  // Type information cache
  std::unordered_map<std::type_index, std::unique_ptr<TypeInfo>> m_typeInfoMap;
  std::unordered_map<std::type_index, std::string> m_typeNames;

  // Cached property values (for change detection)
  std::unordered_map<std::string, PropertyValue> m_cachedValues;

  // Batch mode
  bool m_inBatch = false;
  std::string m_batchDescription;
  std::vector<PropertyChangeContext> m_batchChanges;
  bool m_applyingUndoRedo = false;

  // Integration
  UndoManager* m_undoManager = nullptr;
  EventBus* m_eventBus = nullptr;

  // Listeners
  std::vector<IInspectorBindingListener*> m_listeners;

  // Singleton instance
  static std::unique_ptr<InspectorBindingManager> s_instance;
};

/**
 * @brief RAII helper for property batch changes
 */
class PropertyBatchScope {
public:
  explicit PropertyBatchScope(InspectorBindingManager* manager,
                              const std::string& description = "Edit Properties")
      : m_manager(manager) {
    if (m_manager) {
      m_manager->beginPropertyBatch(description);
    }
  }

  ~PropertyBatchScope() {
    if (m_manager) {
      m_manager->endPropertyBatch();
    }
  }

  // Prevent copying
  PropertyBatchScope(const PropertyBatchScope&) = delete;
  PropertyBatchScope& operator=(const PropertyBatchScope&) = delete;

private:
  InspectorBindingManager* m_manager;
};

} // namespace NovelMind::editor
