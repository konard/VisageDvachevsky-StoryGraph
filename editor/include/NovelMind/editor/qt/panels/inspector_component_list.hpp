#pragma once

/**
 * @file inspector_component_list.hpp
 * @brief Component management for multi-object inspection
 *
 * Handles inspection and editing of multiple objects simultaneously
 */

#include <QList>
#include <QString>
#include <string>
#include <vector>

namespace NovelMind::editor::qt {

class NMInspectorPanel;
class NMSceneObject;

/**
 * @brief Manages component lists for multi-object inspection
 */
class InspectorComponentList {
public:
  explicit InspectorComponentList(NMInspectorPanel* panel);

  /**
   * @brief Create property groups for multiple objects using InspectorBindingManager
   */
  void createMultiObjectPropertyGroups(const QList<NMSceneObject*>& objects, bool editable);

private:
  NMInspectorPanel* m_panel = nullptr;

  /**
   * @brief Convert objects to binding manager format
   */
  void prepareBindingTargets(const QList<NMSceneObject*>& objects,
                             std::vector<std::string>& objectIds,
                             std::vector<void*>& objectPtrs);
};

} // namespace NovelMind::editor::qt
