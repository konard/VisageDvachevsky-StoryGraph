#pragma once

/**
 * @file selection_mediator.hpp
 * @brief Mediator for coordinating selection across multiple panels
 *
 * The SelectionMediator handles:
 * - Scene object selection synchronization (SceneView ↔ Hierarchy ↔ Inspector)
 * - Story graph node selection (StoryGraph ↔ Inspector ↔ SceneView preview)
 * - Asset selection (AssetBrowser → status bar)
 */

#include "NovelMind/editor/event_bus.hpp"
#include "NovelMind/editor/events/panel_events.hpp"
#include <QObject>
#include <memory>
#include <vector>

namespace NovelMind::editor::qt {
class NMSceneViewPanel;
class NMHierarchyPanel;
class NMInspectorPanel;
class NMStoryGraphPanel;
} // namespace NovelMind::editor::qt

namespace NovelMind::editor::mediators {

/**
 * @brief Mediator for selection coordination across panels
 *
 * This class subscribes to selection events from various panels and
 * coordinates updates to ensure all relevant panels stay synchronized.
 */
class SelectionMediator : public QObject {
  Q_OBJECT

public:
  /**
   * @brief Construct the selection mediator
   * @param sceneView Scene view panel (for object selection)
   * @param hierarchy Hierarchy panel (for object tree selection)
   * @param inspector Inspector panel (for property display)
   * @param storyGraph Story graph panel (for node selection)
   * @param parent QObject parent
   */
  SelectionMediator(qt::NMSceneViewPanel *sceneView,
                    qt::NMHierarchyPanel *hierarchy,
                    qt::NMInspectorPanel *inspector,
                    qt::NMStoryGraphPanel *storyGraph,
                    QObject *parent = nullptr);

  ~SelectionMediator() override;

  /**
   * @brief Initialize event subscriptions
   */
  void initialize();

  /**
   * @brief Shutdown and cleanup subscriptions
   */
  void shutdown();

private:
  void onSceneObjectSelected(const events::SceneObjectSelectedEvent &event);
  void onStoryGraphNodeSelected(const events::StoryGraphNodeSelectedEvent &event);
  void onAssetSelected(const events::AssetSelectedEvent &event);
  void onHierarchyObjectDoubleClicked(const events::HierarchyObjectDoubleClickedEvent &event);

  qt::NMSceneViewPanel *m_sceneView = nullptr;
  qt::NMHierarchyPanel *m_hierarchy = nullptr;
  qt::NMInspectorPanel *m_inspector = nullptr;
  qt::NMStoryGraphPanel *m_storyGraph = nullptr;

  std::vector<EventSubscription> m_subscriptions;
  bool m_processingSelection = false; // Prevent feedback loops
};

} // namespace NovelMind::editor::mediators
