#pragma once

/**
 * @file inspector_property_editor.hpp
 * @brief Property editing functionality for inspector panel
 *
 * Handles creation and management of property editors for different object types
 */

#include <QList>
#include <QPointF>
#include <QString>
#include <QStringList>

namespace NovelMind::editor {
class SceneRegistry;
}

namespace NovelMind::editor::qt {

class NMPropertyGroup;
class NMInspectorPanel;
class NMSceneObject;
class NMGraphNodeItem;

/**
 * @brief Handles property editor creation for different object types
 */
class InspectorPropertyEditor {
public:
  explicit InspectorPropertyEditor(NMInspectorPanel* panel);

  /**
   * @brief Create property editors for a scene object
   */
  void createSceneObjectProperties(NMSceneObject* object, bool editable, SceneRegistry* registry);

  /**
   * @brief Create property editors for a story graph node
   */
  void createStoryGraphNodeProperties(NMGraphNodeItem* node, bool editable,
                                      SceneRegistry* registry);

  /**
   * @brief Create property editors for multiple objects
   */
  void createMultiObjectProperties(const QList<NMSceneObject*>& objects, bool editable);

private:
  NMInspectorPanel* m_panel = nullptr;

  // Helper methods for specific property group creation
  void createGeneralGroup(NMSceneObject* object, bool editable);
  void createTransformGroup(NMSceneObject* object, bool editable, QPointF& lastScale,
                            bool& lockAspectRatio);
  void createRenderingGroup(NMSceneObject* object, bool editable);
  void createTagsGroup(NMSceneObject* object, bool editable);

  void createNodeGeneralGroup(NMGraphNodeItem* node, bool editable);
  void createNodeContentGroup(NMGraphNodeItem* node, bool editable);
  void createNodeBranchMappingGroup(NMGraphNodeItem* node, bool editable);
  void createNodeScriptGroup(NMGraphNodeItem* node, bool editable);
  void createNodeSceneGroup(NMGraphNodeItem* node, bool editable, SceneRegistry* registry);
  void createNodeConditionGroup(NMGraphNodeItem* node, bool editable);
};

} // namespace NovelMind::editor::qt
