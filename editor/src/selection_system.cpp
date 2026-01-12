#include "NovelMind/editor/selection_system.hpp"
#include <algorithm>

namespace NovelMind::editor {

// Static instance
std::unique_ptr<EditorSelectionManager> EditorSelectionManager::s_instance = nullptr;

// ============================================================================
// SelectionItem Implementation
// ============================================================================

std::string SelectionItem::getDisplayName() const {
  switch (type) {
  case SelectionType::SceneObject:
    if (auto* objId = std::get_if<ObjectId>(&this->id)) {
      return "Object: " + *objId;
    }
    break;
  case SelectionType::StoryGraphNode:
    if (auto* nodeId = std::get_if<scripting::NodeId>(&this->id)) {
      return "Node: " + std::to_string(*nodeId);
    }
    break;
  case SelectionType::TimelineItem:
    if (auto* timelineId = std::get_if<TimelineItemId>(&this->id)) {
      return "Timeline: " + timelineId->trackId + "[" + std::to_string(timelineId->keyframeIndex) +
             "]";
    }
    break;
  case SelectionType::Asset:
    if (auto* assetId = std::get_if<AssetId>(&this->id)) {
      return "Asset: " + assetId->path;
    }
    break;
  default:
    break;
  }
  return "<none>";
}

// ============================================================================
// SceneObjectSelection Implementation
// ============================================================================

SceneObjectSelection::SceneObjectSelection(const ObjectId& objectId) : m_objectId(objectId) {}

bool SceneObjectSelection::isValid() const {
  return !m_objectId.empty();
}

std::vector<std::string> SceneObjectSelection::getPropertyNames() const {
  // This would be populated from the actual scene object
  return {"name", "position_x", "position_y", "rotation", "scale_x", "scale_y", "visible", "alpha"};
}

std::string SceneObjectSelection::getPropertyValue(const std::string& /*name*/) const {
  // This would fetch from the actual scene object
  return "";
}

void SceneObjectSelection::setPropertyValue(const std::string& /*name*/,
                                            const std::string& /*value*/) {
  // This would set on the actual scene object
}

// ============================================================================
// StoryGraphNodeSelection Implementation
// ============================================================================

StoryGraphNodeSelection::StoryGraphNodeSelection(scripting::NodeId nodeId,
                                                 scripting::VisualGraph* graph)
    : m_nodeId(nodeId), m_graph(graph) {}

bool StoryGraphNodeSelection::isValid() const {
  return m_nodeId != 0 && m_graph != nullptr;
}

const scripting::VisualGraphNode* StoryGraphNodeSelection::getNode() const {
  if (!m_graph) {
    return nullptr;
  }
  return m_graph->findNode(m_nodeId);
}

std::vector<std::string> StoryGraphNodeSelection::getPropertyNames() const {
  const auto* node = getNode();
  if (!node) {
    return {};
  }

  std::vector<std::string> names;
  names.reserve(node->properties.size() + 2);
  names.push_back("type");
  names.push_back("position");

  for (const auto& prop : node->properties) {
    names.push_back(prop.first);
  }

  return names;
}

std::string StoryGraphNodeSelection::getPropertyValue(const std::string& name) const {
  const auto* node = getNode();
  if (!node) {
    return "";
  }

  if (name == "type") {
    return node->type; // VisualGraphNode.type is already a string
  }
  if (name == "position") {
    return std::to_string(node->x) + "," + std::to_string(node->y);
  }

  auto it = node->properties.find(name);
  if (it != node->properties.end()) {
    return it->second;
  }

  return "";
}

void StoryGraphNodeSelection::setPropertyValue(const std::string& name, const std::string& value) {
  if (!m_graph) {
    return;
  }

  auto* node = m_graph->findNode(m_nodeId);
  if (!node) {
    return;
  }

  if (name == "position") {
    // Parse "x,y" format
    auto commaPos = value.find(',');
    if (commaPos != std::string::npos) {
      try {
        node->x = std::stof(value.substr(0, commaPos));
        node->y = std::stof(value.substr(commaPos + 1));
      } catch (...) {
        // Invalid format, ignore
      }
    }
  } else {
    node->properties[name] = value;
  }
}

// ============================================================================
// TimelineItemSelection Implementation
// ============================================================================

TimelineItemSelection::TimelineItemSelection(const TimelineItemId& itemId) : m_itemId(itemId) {}

bool TimelineItemSelection::isValid() const {
  return !m_itemId.trackId.empty();
}

std::vector<std::string> TimelineItemSelection::getPropertyNames() const {
  return {"track", "time", "value", "easing"};
}

std::string TimelineItemSelection::getPropertyValue(const std::string& /*name*/) const {
  // This would fetch from the actual timeline system
  return "";
}

void TimelineItemSelection::setPropertyValue(const std::string& /*name*/,
                                             const std::string& /*value*/) {
  // This would set on the actual timeline system
}

// ============================================================================
// EditorSelectionManager Implementation
// ============================================================================

EditorSelectionManager::EditorSelectionManager() {
  m_history.reserve(MAX_HISTORY_SIZE);
}

EditorSelectionManager::~EditorSelectionManager() = default;

EditorSelectionManager& EditorSelectionManager::instance() {
  if (!s_instance) {
    s_instance = std::make_unique<EditorSelectionManager>();
  }
  return *s_instance;
}

// Selection Operations

void EditorSelectionManager::select(const SelectionItem& item) {
  if (!item.isValid()) {
    clearSelection();
    return;
  }

  pushToHistory();
  m_selection.clear();
  m_selection.push_back(item);
  m_currentType = item.type;
  notifySelectionChanged();
  notifyPrimarySelectionChanged();
}

void EditorSelectionManager::selectObject(const ObjectId& objectId) {
  select(SelectionItem(objectId));
}

void EditorSelectionManager::selectNode(scripting::NodeId nodeId) {
  select(SelectionItem(nodeId));
}

void EditorSelectionManager::selectTimelineItem(const TimelineItemId& itemId) {
  select(SelectionItem(itemId));
}

void EditorSelectionManager::selectAsset(const AssetId& assetId) {
  select(SelectionItem(assetId));
}

void EditorSelectionManager::addToSelection(const SelectionItem& item) {
  if (!item.isValid()) {
    return;
  }

  // Can only multi-select same type
  if (!m_selection.empty() && m_currentType != item.type) {
    select(item);
    return;
  }

  // Check if already selected
  if (isSelected(item)) {
    return;
  }

  pushToHistory();
  m_selection.push_back(item);
  m_currentType = item.type;
  notifySelectionChanged();
}

void EditorSelectionManager::removeFromSelection(const SelectionItem& item) {
  auto it = std::find(m_selection.begin(), m_selection.end(), item);
  if (it != m_selection.end()) {
    bool wasPrimary = (it == m_selection.begin());
    pushToHistory();
    m_selection.erase(it);

    if (m_selection.empty()) {
      m_currentType = SelectionType::None;
    }

    notifySelectionChanged();
    if (wasPrimary) {
      notifyPrimarySelectionChanged();
    }
  }
}

void EditorSelectionManager::toggleSelection(const SelectionItem& item) {
  if (isSelected(item)) {
    removeFromSelection(item);
  } else {
    addToSelection(item);
  }
}

void EditorSelectionManager::selectMultiple(const std::vector<SelectionItem>& items) {
  if (items.empty()) {
    clearSelection();
    return;
  }

  pushToHistory();
  m_selection = items;
  m_currentType = items.front().type;
  notifySelectionChanged();
  notifyPrimarySelectionChanged();
}

void EditorSelectionManager::selectObjects(const std::vector<ObjectId>& objectIds) {
  std::vector<SelectionItem> items;
  items.reserve(objectIds.size());
  for (const auto& id : objectIds) {
    items.emplace_back(id);
  }
  selectMultiple(items);
}

void EditorSelectionManager::selectNodes(const std::vector<scripting::NodeId>& nodeIds) {
  std::vector<SelectionItem> items;
  items.reserve(nodeIds.size());
  for (auto id : nodeIds) {
    items.emplace_back(id);
  }
  selectMultiple(items);
}

void EditorSelectionManager::clearSelection() {
  if (m_selection.empty()) {
    return;
  }

  pushToHistory();
  m_selection.clear();
  m_currentType = SelectionType::None;
  notifySelectionCleared();
}

void EditorSelectionManager::clearSelectionOfType(SelectionType type) {
  if (m_currentType != type) {
    return;
  }
  clearSelection();
}

// Selection Queries

bool EditorSelectionManager::hasSelection() const {
  return !m_selection.empty();
}

bool EditorSelectionManager::hasSelectionOfType(SelectionType type) const {
  return m_currentType == type && !m_selection.empty();
}

SelectionType EditorSelectionManager::getCurrentSelectionType() const {
  return m_currentType;
}

const std::vector<SelectionItem>& EditorSelectionManager::getSelection() const {
  return m_selection;
}

std::vector<SelectionItem> EditorSelectionManager::getSelectionOfType(SelectionType type) const {
  if (m_currentType != type) {
    return {};
  }
  return m_selection;
}

std::optional<SelectionItem> EditorSelectionManager::getPrimarySelection() const {
  if (m_selection.empty()) {
    return std::nullopt;
  }
  return m_selection.front();
}

std::vector<ObjectId> EditorSelectionManager::getSelectedObjectIds() const {
  std::vector<ObjectId> result;
  if (m_currentType != SelectionType::SceneObject) {
    return result;
  }

  result.reserve(m_selection.size());
  for (const auto& item : m_selection) {
    if (auto* id = std::get_if<ObjectId>(&item.id)) {
      result.push_back(*id);
    }
  }
  return result;
}

std::vector<scripting::NodeId> EditorSelectionManager::getSelectedNodeIds() const {
  std::vector<scripting::NodeId> result;
  if (m_currentType != SelectionType::StoryGraphNode) {
    return result;
  }

  result.reserve(m_selection.size());
  for (const auto& item : m_selection) {
    if (auto* id = std::get_if<scripting::NodeId>(&item.id)) {
      result.push_back(*id);
    }
  }
  return result;
}

std::vector<TimelineItemId> EditorSelectionManager::getSelectedTimelineItemIds() const {
  std::vector<TimelineItemId> result;
  if (m_currentType != SelectionType::TimelineItem) {
    return result;
  }

  result.reserve(m_selection.size());
  for (const auto& item : m_selection) {
    if (auto* id = std::get_if<TimelineItemId>(&item.id)) {
      result.push_back(*id);
    }
  }
  return result;
}

bool EditorSelectionManager::isSelected(const SelectionItem& item) const {
  return std::find(m_selection.begin(), m_selection.end(), item) != m_selection.end();
}

bool EditorSelectionManager::isObjectSelected(const ObjectId& objectId) const {
  return isSelected(SelectionItem(objectId));
}

bool EditorSelectionManager::isNodeSelected(scripting::NodeId nodeId) const {
  return isSelected(SelectionItem(nodeId));
}

size_t EditorSelectionManager::getSelectionCount() const {
  return m_selection.size();
}

// Selection Proxies

std::optional<SceneObjectSelection> EditorSelectionManager::getSceneObjectSelection() const {
  if (m_currentType != SelectionType::SceneObject || m_selection.empty()) {
    return std::nullopt;
  }

  if (auto* id = std::get_if<ObjectId>(&m_selection.front().id)) {
    return SceneObjectSelection(*id);
  }
  return std::nullopt;
}

std::optional<StoryGraphNodeSelection> EditorSelectionManager::getStoryGraphNodeSelection() const {
  if (m_currentType != SelectionType::StoryGraphNode || m_selection.empty()) {
    return std::nullopt;
  }

  if (auto* id = std::get_if<scripting::NodeId>(&m_selection.front().id)) {
    return StoryGraphNodeSelection(*id, m_activeGraph);
  }
  return std::nullopt;
}

std::optional<TimelineItemSelection> EditorSelectionManager::getTimelineItemSelection() const {
  if (m_currentType != SelectionType::TimelineItem || m_selection.empty()) {
    return std::nullopt;
  }

  if (auto* id = std::get_if<TimelineItemId>(&m_selection.front().id)) {
    return TimelineItemSelection(*id);
  }
  return std::nullopt;
}

// Context Management

void EditorSelectionManager::setActiveVisualGraph(scripting::VisualGraph* graph) {
  m_activeGraph = graph;
}

scripting::VisualGraph* EditorSelectionManager::getActiveVisualGraph() const {
  return m_activeGraph;
}

// Selection History

void EditorSelectionManager::selectPrevious() {
  if (!canSelectPrevious()) {
    return;
  }

  m_historyIndex--;
  m_selection = m_history[m_historyIndex];
  if (!m_selection.empty()) {
    m_currentType = m_selection.front().type;
  } else {
    m_currentType = SelectionType::None;
  }
  notifySelectionChanged();
  notifyPrimarySelectionChanged();
}

void EditorSelectionManager::selectNext() {
  if (!canSelectNext()) {
    return;
  }

  m_historyIndex++;
  m_selection = m_history[m_historyIndex];
  if (!m_selection.empty()) {
    m_currentType = m_selection.front().type;
  } else {
    m_currentType = SelectionType::None;
  }
  notifySelectionChanged();
  notifyPrimarySelectionChanged();
}

bool EditorSelectionManager::canSelectPrevious() const {
  return m_historyIndex > 0;
}

bool EditorSelectionManager::canSelectNext() const {
  return m_historyIndex + 1 < m_history.size();
}

void EditorSelectionManager::clearHistory() {
  m_history.clear();
  m_historyIndex = 0;
}

// Listener Management

void EditorSelectionManager::addListener(ISelectionListener* listener) {
  if (listener &&
      std::find(m_listeners.begin(), m_listeners.end(), listener) == m_listeners.end()) {
    m_listeners.push_back(listener);
  }
}

void EditorSelectionManager::removeListener(ISelectionListener* listener) {
  m_listeners.erase(std::remove(m_listeners.begin(), m_listeners.end(), listener),
                    m_listeners.end());
}

// Callbacks

void EditorSelectionManager::setOnSelectionChanged(
    std::function<void(SelectionType, const std::vector<SelectionItem>&)> callback) {
  m_onSelectionChanged = std::move(callback);
}

void EditorSelectionManager::setOnSelectionCleared(std::function<void()> callback) {
  m_onSelectionCleared = std::move(callback);
}

// Private Methods

void EditorSelectionManager::notifySelectionChanged() {
  for (auto* listener : m_listeners) {
    listener->onSelectionChanged(m_currentType, m_selection);
  }

  if (m_onSelectionChanged) {
    m_onSelectionChanged(m_currentType, m_selection);
  }
}

void EditorSelectionManager::notifySelectionCleared() {
  for (auto* listener : m_listeners) {
    listener->onSelectionCleared();
  }

  if (m_onSelectionCleared) {
    m_onSelectionCleared();
  }
}

void EditorSelectionManager::notifyPrimarySelectionChanged() {
  auto primary = getPrimarySelection();
  if (primary) {
    for (auto* listener : m_listeners) {
      listener->onPrimarySelectionChanged(*primary);
    }
  }
}

void EditorSelectionManager::pushToHistory() {
  // Remove any forward history when adding new entry
  if (m_historyIndex + 1 < m_history.size()) {
    m_history.resize(m_historyIndex + 1);
  }

  // Add current selection to history
  m_history.push_back(m_selection);
  m_historyIndex = m_history.size() - 1;

  // Trim if too large
  if (m_history.size() > MAX_HISTORY_SIZE) {
    m_history.erase(m_history.begin());
    m_historyIndex--;
  }
}

// ============================================================================
// SelectionScope Implementation
// ============================================================================

SelectionScope::SelectionScope(EditorSelectionManager* manager) : m_manager(manager) {
  if (m_manager) {
    m_originalSelection = m_manager->getSelection();
  }
}

SelectionScope::~SelectionScope() {
  // Could be extended to batch notifications
}

} // namespace NovelMind::editor
