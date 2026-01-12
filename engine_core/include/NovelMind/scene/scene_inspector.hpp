#pragma once

/**
 * @file scene_inspector.hpp
 * @brief SceneInspectorAPI - Editor integration for scene inspection
 *
 * Provides an API for the Editor to:
 * - Query scene structure and objects
 * - Modify object properties in real-time
 * - Subscribe to scene changes
 * - Support undo/redo operations
 */

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include "NovelMind/scene/scene_graph.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <stack>
#include <string>
#include <vector>

namespace NovelMind::scene {

// Forward declaration for command classes
class SceneInspectorAPI;

/**
 * @brief Property descriptor for editor UI
 */
struct PropertyDescriptor {
  enum class Type : u8 {
    String,
    Int,
    Float,
    Bool,
    Color,
    Vector2,
    Enum,
    Resource
  };

  std::string name;
  std::string displayName;
  Type type;
  std::string value;
  std::string defaultValue;
  bool readOnly = false;
  std::vector<std::string> enumOptions; // For Enum type
  std::string resourceType; // For Resource type (texture, sound, etc.)
  f32 minValue = 0.0f;
  f32 maxValue = 1.0f;
};

/**
 * @brief Object descriptor for editor UI
 */
struct ObjectDescriptor {
  std::string id;
  std::string displayName;
  SceneObjectType type;
  std::string layer;
  bool visible;
  bool expanded = false; // UI state
  std::vector<PropertyDescriptor> properties;
  std::vector<std::string> childIds;
};

/**
 * @brief Layer descriptor for editor UI
 */
struct LayerDescriptor {
  std::string name;
  LayerType type;
  bool visible;
  f32 alpha;
  std::vector<std::string> objectIds;
};

/**
 * @brief Command for undo/redo support
 */
class ICommand {
public:
  virtual ~ICommand() = default;
  virtual void execute() = 0;
  virtual void undo() = 0;
  [[nodiscard]] virtual std::string getDescription() const = 0;
};

/**
 * @brief Property change command
 */
class SetPropertyCommand : public ICommand {
public:
  SetPropertyCommand(SceneInspectorAPI *inspector, const std::string &objectId,
                     const std::string &propertyName,
                     const std::string &oldValue, const std::string &newValue);

  void execute() override;
  void undo() override;
  [[nodiscard]] std::string getDescription() const override;

private:
  SceneInspectorAPI *m_inspector;
  std::string m_objectId;
  std::string m_propertyName;
  std::string m_oldValue;
  std::string m_newValue;
};

/**
 * @brief Add object command
 */
class AddObjectCommand : public ICommand {
public:
  AddObjectCommand(SceneInspectorAPI *inspector, LayerType layer,
                   std::unique_ptr<SceneObjectBase> object);

  void execute() override;
  void undo() override;
  [[nodiscard]] std::string getDescription() const override;

private:
  SceneInspectorAPI *m_inspector;
  LayerType m_layer;
  std::unique_ptr<SceneObjectBase> m_object;
  std::string m_objectId;
  bool m_executed = false;
};

/**
 * @brief Remove object command
 */
class RemoveObjectCommand : public ICommand {
public:
  RemoveObjectCommand(SceneInspectorAPI *inspector,
                      const std::string &objectId);

  void execute() override;
  void undo() override;
  [[nodiscard]] std::string getDescription() const override;

private:
  SceneInspectorAPI *m_inspector;
  std::string m_objectId;
  SceneObjectState m_savedState;
  LayerType m_layer;
  std::unique_ptr<SceneObjectBase> m_removedObject;
};

/**
 * @brief Composite command that groups multiple commands
 */
class CompositeCommand : public ICommand {
public:
  explicit CompositeCommand(const std::string &description);

  void addCommand(std::unique_ptr<ICommand> command);
  void execute() override;
  void undo() override;
  [[nodiscard]] std::string getDescription() const override;

private:
  std::string m_description;
  std::vector<std::unique_ptr<ICommand>> m_commands;
};

// Forward declaration already at top of namespace

/**
 * @brief Listener interface for inspector events
 */
class IInspectorListener {
public:
  virtual ~IInspectorListener() = default;
  virtual void
  onSelectionChanged(const std::vector<std::string> &selectedIds) = 0;
  virtual void onSceneModified() = 0;
  virtual void onUndoStackChanged(bool canUndo, bool canRedo) = 0;
};

/**
 * @brief SceneInspectorAPI - Main API for editor scene inspection
 *
 * Provides:
 * - Scene structure query
 * - Property inspection and modification
 * - Selection management
 * - Undo/redo support
 * - Real-time change notifications
 */
class SceneInspectorAPI : public ISceneObserver {
public:
  explicit SceneInspectorAPI(SceneGraph *sceneGraph);
  ~SceneInspectorAPI() override;

  // Scene structure
  [[nodiscard]] std::vector<LayerDescriptor> getLayers() const;
  [[nodiscard]] std::vector<ObjectDescriptor> getObjects() const;
  [[nodiscard]] std::optional<ObjectDescriptor>
  getObject(const std::string &id) const;

  // Property access
  [[nodiscard]] std::vector<PropertyDescriptor>
  getProperties(const std::string &objectId) const;
  [[nodiscard]] std::optional<std::string>
  getProperty(const std::string &objectId,
              const std::string &propertyName) const;
  Result<void> setProperty(const std::string &objectId,
                           const std::string &propertyName,
                           const std::string &value, bool recordUndo = true);

  // Object creation/deletion
  Result<std::string> createObject(LayerType layer, SceneObjectType type,
                                   const std::string &id = "",
                                   bool recordUndo = true);
  Result<void> deleteObject(const std::string &id, bool recordUndo = true);
  Result<void> duplicateObject(const std::string &sourceId,
                               bool recordUndo = true);

  // Object manipulation
  Result<void> moveObject(const std::string &id, f32 x, f32 y,
                          bool recordUndo = true);
  Result<void> scaleObject(const std::string &id, f32 scaleX, f32 scaleY,
                           bool recordUndo = true);
  Result<void> rotateObject(const std::string &id, f32 angle,
                            bool recordUndo = true);
  Result<void> setObjectLayer(const std::string &id, LayerType layer,
                              bool recordUndo = true);
  Result<void> setObjectZOrder(const std::string &id, i32 zOrder,
                               bool recordUndo = true);

  // Selection management
  void selectObject(const std::string &id, bool addToSelection = false);
  void deselectObject(const std::string &id);
  void clearSelection();
  [[nodiscard]] const std::vector<std::string> &getSelection() const;
  [[nodiscard]] bool isSelected(const std::string &id) const;

  // Multi-selection operations
  Result<void> moveSelection(f32 deltaX, f32 deltaY, bool recordUndo = true);
  Result<void> deleteSelection(bool recordUndo = true);
  Result<void> duplicateSelection(bool recordUndo = true);

  // Undo/Redo
  void executeCommand(std::unique_ptr<ICommand> command);
  void undo();
  void redo();
  [[nodiscard]] bool canUndo() const;
  [[nodiscard]] bool canRedo() const;
  void clearHistory();
  [[nodiscard]] std::vector<std::string> getUndoHistory() const;
  [[nodiscard]] std::vector<std::string> getRedoHistory() const;

  // Listener management
  void addListener(IInspectorListener *listener);
  void removeListener(IInspectorListener *listener);

  // Clipboard operations
  void copySelection();
  void cutSelection();
  void paste(f32 offsetX = 0.0f, f32 offsetY = 0.0f);
  [[nodiscard]] bool hasClipboardContent() const;

  // Scene snapshot (for preview)
  [[nodiscard]] SceneState getSceneSnapshot() const;
  void restoreSceneSnapshot(const SceneState &snapshot);

  // Direct scene graph access (for internal use)
  [[nodiscard]] SceneGraph *getSceneGraph() const { return m_sceneGraph; }

  /**
   * @brief Find which layer an object belongs to
   */
  [[nodiscard]] LayerType findObjectLayer(const std::string &id) const;

  // ISceneObserver implementation
  void onObjectAdded(const std::string &objectId,
                     SceneObjectType type) override;
  void onObjectRemoved(const std::string &objectId) override;
  void onPropertyChanged(const PropertyChange &change) override;
  void onLayerChanged(const std::string &objectId,
                      const std::string &newLayer) override;

private:
  PropertyDescriptor createPropertyDescriptor(const std::string &name,
                                              PropertyDescriptor::Type type,
                                              const std::string &value) const;
  std::vector<PropertyDescriptor>
  getBaseProperties(const SceneObjectBase *obj) const;
  // findObjectLayer moved to public section above

  void notifySelectionChanged();
  void notifySceneModified();
  void notifyUndoStackChanged();

  std::string generateUniqueId(const std::string &base) const;
  std::unique_ptr<SceneObjectBase>
  createObjectOfType(SceneObjectType type, const std::string &id) const;

  SceneGraph *m_sceneGraph;
  std::vector<std::string> m_selection;
  std::vector<IInspectorListener *> m_listeners;

  // Undo/Redo stacks
  std::stack<std::unique_ptr<ICommand>> m_undoStack;
  std::stack<std::unique_ptr<ICommand>> m_redoStack;
  size_t m_maxHistorySize = 100;

  // Clipboard
  std::vector<SceneObjectState> m_clipboard;
};

} // namespace NovelMind::scene
