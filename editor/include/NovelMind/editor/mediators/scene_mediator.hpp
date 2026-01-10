#pragma once

/**
 * @file scene_mediator.hpp
 * @brief Mediator for Scene Registry integration with panels
 *
 * This mediator handles:
 * - Scene registration/unregistration coordination
 * - Cross-reference tracking between Story Graph nodes and scenes
 * - Scene ID change propagation to Story Graph
 * - Scene validation across panels
 *
 * Addresses issue #211: Scene Registry System integration
 */

#include "NovelMind/editor/event_bus.hpp"
#include <QObject>
#include <QString>
#include <QStringList>
#include <vector>

namespace NovelMind::editor {
class SceneRegistry;
}

namespace NovelMind::editor::qt {
class NMSceneViewPanel;
class NMStoryGraphPanel;
} // namespace NovelMind::editor::qt

namespace NovelMind::editor::mediators {

/**
 * @brief Mediator for Scene Registry integration
 *
 * This mediator coordinates between:
 * - SceneRegistry: Central scene management
 * - SceneViewPanel: Scene editing/creation
 * - StoryGraphPanel: Scene node references
 *
 * It handles:
 * 1. Scene lifecycle events (register, rename, delete)
 * 2. Cross-reference tracking (which nodes use which scenes)
 * 3. Scene ID changes and reference updates
 * 4. Validation of scene references
 */
class SceneMediator : public QObject {
  Q_OBJECT

public:
  explicit SceneMediator(QObject* parent = nullptr);
  ~SceneMediator() override;

  /**
   * @brief Initialize the mediator with panels and registry
   * @param sceneRegistry The scene registry instance
   * @param sceneView The scene view panel
   * @param storyGraph The story graph panel
   */
  void initialize(SceneRegistry* sceneRegistry, qt::NMSceneViewPanel* sceneView,
                  qt::NMStoryGraphPanel* storyGraph);

  /**
   * @brief Shutdown and cleanup
   */
  void shutdown();

  /**
   * @brief Check if mediator is initialized
   */
  [[nodiscard]] bool isInitialized() const { return m_initialized; }

  /**
   * @brief Get the scene registry
   */
  [[nodiscard]] SceneRegistry* sceneRegistry() const { return m_sceneRegistry; }

  // =========================================================================
  // Scene-Node Reference Management
  // =========================================================================

  /**
   * @brief Update scene references from Story Graph
   *
   * Scans all Scene nodes in the Story Graph and updates
   * the SceneRegistry's cross-reference tracking.
   */
  void syncSceneReferencesFromGraph();

  /**
   * @brief Validate all scene references in Story Graph
   * @return List of validation errors (empty if all valid)
   */
  [[nodiscard]] QStringList validateSceneReferences() const;

signals:
  /**
   * @brief Emitted when scene references are updated
   */
  void sceneReferencesUpdated();

  /**
   * @brief Emitted when a scene reference validation fails
   * @param sceneId The scene ID that failed validation
   * @param nodeIds The nodes that reference the invalid scene
   */
  void invalidSceneReference(const QString& sceneId, const QStringList& nodeIds);

private slots:
  // Scene Registry event handlers
  void onSceneRegistered(const QString& sceneId);
  void onSceneUnregistered(const QString& sceneId);
  void onSceneIdChanged(const QString& oldId, const QString& newId);
  void onSceneRenamed(const QString& sceneId, const QString& oldName, const QString& newName);

  // Story Graph event handlers
  void onNodeAdded(uint64_t nodeId, const QString& nodeIdString, const QString& nodeType);
  void onNodeDeleted(uint64_t nodeId);

  // Scene View event handlers
  void onSceneChanged(const QString& sceneId);

private:
  /**
   * @brief Update reference tracking when a scene node's sceneId changes
   * @param nodeIdString The node ID
   * @param oldSceneId The old scene ID (empty if new node)
   * @param newSceneId The new scene ID
   */
  void updateNodeSceneReference(const QString& nodeIdString, const QString& oldSceneId,
                                const QString& newSceneId);

  /**
   * @brief Get the sceneId from a node in the Story Graph
   * @param nodeIdString The node ID string
   * @return The scene ID, or empty if not a scene node
   */
  [[nodiscard]] QString getNodeSceneId(const QString& nodeIdString) const;

  SceneRegistry* m_sceneRegistry = nullptr;
  qt::NMSceneViewPanel* m_sceneView = nullptr;
  qt::NMStoryGraphPanel* m_storyGraph = nullptr;

  std::vector<EventSubscription> m_subscriptions;
  bool m_initialized = false;
};

} // namespace NovelMind::editor::mediators
