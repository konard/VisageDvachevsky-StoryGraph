#pragma once

/**
 * @file selection_system.hpp
 * @brief Editor Selection System for NovelMind
 *
 * Provides a centralized selection management system for the editor:
 * - Tracks what is currently selected across all panels
 * - Supports multiple selection types (scene objects, graph nodes, timeline
 * items)
 * - Notifies listeners when selection changes
 * - Integrates with Inspector panel for property editing
 *
 * This is a critical system for v0.2.0 GUI as it enables:
 * - Inspector panel to know what properties to display
 * - SceneView to know what objects to highlight
 * - StoryGraph to know what nodes are selected
 * - Timeline to know what items are selected
 */

#include "NovelMind/core/types.hpp"
#include "NovelMind/scripting/ir.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <variant>
#include <vector>

namespace NovelMind::editor {

// Forward declarations
class EditorSelectionManager;

/**
 * @brief Object ID type for scene objects
 */
using ObjectId = std::string;

/**
 * @brief Timeline item ID
 */
struct TimelineItemId {
  std::string trackId;
  u64 keyframeIndex = 0;

  bool operator==(const TimelineItemId& other) const {
    return trackId == other.trackId && keyframeIndex == other.keyframeIndex;
  }

  bool operator!=(const TimelineItemId& other) const { return !(*this == other); }
};

/**
 * @brief Asset ID for asset browser selections
 */
struct AssetId {
  std::string path;
  std::string type;

  bool operator==(const AssetId& other) const { return path == other.path; }

  bool operator!=(const AssetId& other) const { return !(*this == other); }
};

/**
 * @brief Selection types supported by the editor
 */
enum class SelectionType : u8 {
  None = 0,
  SceneObject,     // Objects in the scene view
  StoryGraphNode,  // Nodes in the story graph
  TimelineItem,    // Keyframes/clips in the timeline
  Asset,           // Assets in the asset browser
  InspectorTarget, // Generic target for inspector
};

/**
 * @brief Represents a single selected item
 */
struct SelectionItem {
  SelectionType type = SelectionType::None;
  std::variant<std::monostate, ObjectId, scripting::NodeId, TimelineItemId, AssetId> id;

  SelectionItem() = default;

  explicit SelectionItem(ObjectId objectId)
      : type(SelectionType::SceneObject), id(std::move(objectId)) {}

  explicit SelectionItem(scripting::NodeId nodeId)
      : type(SelectionType::StoryGraphNode), id(nodeId) {}

  explicit SelectionItem(TimelineItemId timelineId)
      : type(SelectionType::TimelineItem), id(std::move(timelineId)) {}

  explicit SelectionItem(AssetId assetId) : type(SelectionType::Asset), id(std::move(assetId)) {}

  [[nodiscard]] bool isValid() const { return type != SelectionType::None; }

  [[nodiscard]] std::string getDisplayName() const;

  bool operator==(const SelectionItem& other) const { return type == other.type && id == other.id; }

  bool operator!=(const SelectionItem& other) const { return !(*this == other); }
};

/**
 * @brief Selection proxy for scene objects
 */
class SceneObjectSelection {
public:
  explicit SceneObjectSelection(const ObjectId& objectId);

  [[nodiscard]] const ObjectId& getObjectId() const { return m_objectId; }
  [[nodiscard]] bool isValid() const;

  // Property access for Inspector
  [[nodiscard]] std::vector<std::string> getPropertyNames() const;
  [[nodiscard]] std::string getPropertyValue(const std::string& name) const;
  void setPropertyValue(const std::string& name, const std::string& value);

private:
  ObjectId m_objectId;
};

/**
 * @brief Selection proxy for story graph nodes
 */
class StoryGraphNodeSelection {
public:
  explicit StoryGraphNodeSelection(scripting::NodeId nodeId,
                                   scripting::VisualGraph* graph = nullptr);

  [[nodiscard]] scripting::NodeId getNodeId() const { return m_nodeId; }
  [[nodiscard]] scripting::VisualGraph* getGraph() const { return m_graph; }
  [[nodiscard]] bool isValid() const;

  // Property access for Inspector
  [[nodiscard]] const scripting::VisualGraphNode* getNode() const;
  [[nodiscard]] std::vector<std::string> getPropertyNames() const;
  [[nodiscard]] std::string getPropertyValue(const std::string& name) const;
  void setPropertyValue(const std::string& name, const std::string& value);

private:
  scripting::NodeId m_nodeId;
  scripting::VisualGraph* m_graph;
};

/**
 * @brief Selection proxy for timeline items
 */
class TimelineItemSelection {
public:
  explicit TimelineItemSelection(const TimelineItemId& itemId);

  [[nodiscard]] const TimelineItemId& getItemId() const { return m_itemId; }
  [[nodiscard]] bool isValid() const;

  // Property access for Inspector
  [[nodiscard]] std::vector<std::string> getPropertyNames() const;
  [[nodiscard]] std::string getPropertyValue(const std::string& name) const;
  void setPropertyValue(const std::string& name, const std::string& value);

private:
  TimelineItemId m_itemId;
};

/**
 * @brief Listener interface for selection changes
 */
class ISelectionListener {
public:
  virtual ~ISelectionListener() = default;

  /**
   * @brief Called when selection changes
   */
  virtual void onSelectionChanged(SelectionType /*type*/,
                                  const std::vector<SelectionItem>& /*selection*/) {}

  /**
   * @brief Called when selection is cleared
   */
  virtual void onSelectionCleared() {}

  /**
   * @brief Called when primary selection changes (first item in multi-select)
   */
  virtual void onPrimarySelectionChanged(const SelectionItem& /*item*/) {}
};

/**
 * @brief Central selection manager for the editor
 *
 * Responsibilities:
 * - Track current selection across all editor panels
 * - Notify listeners when selection changes
 * - Support multi-selection within same type
 * - Provide selection queries for Inspector
 * - Support selection history for navigation
 */
class EditorSelectionManager {
public:
  EditorSelectionManager();
  ~EditorSelectionManager();

  // Prevent copying
  EditorSelectionManager(const EditorSelectionManager&) = delete;
  EditorSelectionManager& operator=(const EditorSelectionManager&) = delete;

  /**
   * @brief Get singleton instance
   */
  static EditorSelectionManager& instance();

  // =========================================================================
  // Selection Operations
  // =========================================================================

  /**
   * @brief Select a single item (clears previous selection)
   */
  void select(const SelectionItem& item);

  /**
   * @brief Select a scene object by ID
   */
  void selectObject(const ObjectId& objectId);

  /**
   * @brief Select a story graph node by ID
   */
  void selectNode(scripting::NodeId nodeId);

  /**
   * @brief Select a timeline item
   */
  void selectTimelineItem(const TimelineItemId& itemId);

  /**
   * @brief Select an asset
   */
  void selectAsset(const AssetId& assetId);

  /**
   * @brief Add item to selection (multi-select)
   */
  void addToSelection(const SelectionItem& item);

  /**
   * @brief Remove item from selection
   */
  void removeFromSelection(const SelectionItem& item);

  /**
   * @brief Toggle item selection
   */
  void toggleSelection(const SelectionItem& item);

  /**
   * @brief Select multiple items
   */
  void selectMultiple(const std::vector<SelectionItem>& items);

  /**
   * @brief Select multiple scene objects
   */
  void selectObjects(const std::vector<ObjectId>& objectIds);

  /**
   * @brief Select multiple story graph nodes
   */
  void selectNodes(const std::vector<scripting::NodeId>& nodeIds);

  /**
   * @brief Clear all selection
   */
  void clearSelection();

  /**
   * @brief Clear selection of a specific type
   */
  void clearSelectionOfType(SelectionType type);

  // =========================================================================
  // Selection Queries
  // =========================================================================

  /**
   * @brief Check if anything is selected
   */
  [[nodiscard]] bool hasSelection() const;

  /**
   * @brief Check if a specific type has selection
   */
  [[nodiscard]] bool hasSelectionOfType(SelectionType type) const;

  /**
   * @brief Get current selection type
   */
  [[nodiscard]] SelectionType getCurrentSelectionType() const;

  /**
   * @brief Get all selected items
   */
  [[nodiscard]] const std::vector<SelectionItem>& getSelection() const;

  /**
   * @brief Get selection of a specific type
   */
  [[nodiscard]] std::vector<SelectionItem> getSelectionOfType(SelectionType type) const;

  /**
   * @brief Get primary selection (first selected item)
   */
  [[nodiscard]] std::optional<SelectionItem> getPrimarySelection() const;

  /**
   * @brief Get selected object IDs
   */
  [[nodiscard]] std::vector<ObjectId> getSelectedObjectIds() const;

  /**
   * @brief Get selected node IDs
   */
  [[nodiscard]] std::vector<scripting::NodeId> getSelectedNodeIds() const;

  /**
   * @brief Get selected timeline item IDs
   */
  [[nodiscard]] std::vector<TimelineItemId> getSelectedTimelineItemIds() const;

  /**
   * @brief Check if a specific item is selected
   */
  [[nodiscard]] bool isSelected(const SelectionItem& item) const;

  /**
   * @brief Check if a specific object is selected
   */
  [[nodiscard]] bool isObjectSelected(const ObjectId& objectId) const;

  /**
   * @brief Check if a specific node is selected
   */
  [[nodiscard]] bool isNodeSelected(scripting::NodeId nodeId) const;

  /**
   * @brief Get selection count
   */
  [[nodiscard]] size_t getSelectionCount() const;

  // =========================================================================
  // Selection Proxies
  // =========================================================================

  /**
   * @brief Get scene object selection proxy for primary selection
   */
  [[nodiscard]] std::optional<SceneObjectSelection> getSceneObjectSelection() const;

  /**
   * @brief Get story graph node selection proxy for primary selection
   */
  [[nodiscard]] std::optional<StoryGraphNodeSelection> getStoryGraphNodeSelection() const;

  /**
   * @brief Get timeline item selection proxy for primary selection
   */
  [[nodiscard]] std::optional<TimelineItemSelection> getTimelineItemSelection() const;

  // =========================================================================
  // Context Management
  // =========================================================================

  /**
   * @brief Set the active visual graph (for node selection context)
   */
  void setActiveVisualGraph(scripting::VisualGraph* graph);

  /**
   * @brief Get the active visual graph
   */
  [[nodiscard]] scripting::VisualGraph* getActiveVisualGraph() const;

  // =========================================================================
  // Selection History
  // =========================================================================

  /**
   * @brief Navigate to previous selection
   */
  void selectPrevious();

  /**
   * @brief Navigate to next selection
   */
  void selectNext();

  /**
   * @brief Check if can navigate to previous selection
   */
  [[nodiscard]] bool canSelectPrevious() const;

  /**
   * @brief Check if can navigate to next selection
   */
  [[nodiscard]] bool canSelectNext() const;

  /**
   * @brief Clear selection history
   */
  void clearHistory();

  // =========================================================================
  // Listener Management
  // =========================================================================

  /**
   * @brief Add a selection listener
   */
  void addListener(ISelectionListener* listener);

  /**
   * @brief Remove a selection listener
   */
  void removeListener(ISelectionListener* listener);

  // =========================================================================
  // Callbacks
  // =========================================================================

  /**
   * @brief Set callback for selection changed
   */
  void setOnSelectionChanged(
      std::function<void(SelectionType, const std::vector<SelectionItem>&)> callback);

  /**
   * @brief Set callback for selection cleared
   */
  void setOnSelectionCleared(std::function<void()> callback);

private:
  void notifySelectionChanged();
  void notifySelectionCleared();
  void notifyPrimarySelectionChanged();
  void pushToHistory();

  std::vector<SelectionItem> m_selection;
  SelectionType m_currentType = SelectionType::None;

  // Context
  scripting::VisualGraph* m_activeGraph = nullptr;

  // History for navigation
  std::vector<std::vector<SelectionItem>> m_history;
  size_t m_historyIndex = 0;
  static constexpr size_t MAX_HISTORY_SIZE = 50;

  // Listeners
  std::vector<ISelectionListener*> m_listeners;

  // Callbacks
  std::function<void(SelectionType, const std::vector<SelectionItem>&)> m_onSelectionChanged;
  std::function<void()> m_onSelectionCleared;

  // Singleton instance
  static std::unique_ptr<EditorSelectionManager> s_instance;
};

/**
 * @brief RAII helper for batch selection changes
 *
 * Defers notification until destruction, useful for:
 * - Loading a scene with many objects
 * - Programmatic multi-selection
 * - Undo/redo operations
 */
class SelectionScope {
public:
  explicit SelectionScope(EditorSelectionManager* manager);
  ~SelectionScope();

  // Prevent copying
  SelectionScope(const SelectionScope&) = delete;
  SelectionScope& operator=(const SelectionScope&) = delete;

private:
  EditorSelectionManager* m_manager;
  std::vector<SelectionItem> m_originalSelection;
};

} // namespace NovelMind::editor
