#pragma once

/**
 * @file scene_registry_mediator.hpp
 * @brief Mediator for bridging SceneRegistry Qt signals to EventBus events
 *
 * The SceneRegistryMediator connects the SceneRegistry's Qt signals
 * to the EventBus, enabling decoupled communication between the registry
 * and panels (Story Graph, Scene View, etc.) for auto-sync functionality.
 *
 * Addresses issue #213: Add Auto-sync Events for Scene-Story Graph Integration
 */

#include "NovelMind/editor/event_bus.hpp"
#include "NovelMind/editor/events/panel_events.hpp"
#include "NovelMind/editor/scene_registry.hpp"
#include <QObject>

namespace NovelMind::editor::mediators {

/**
 * @brief Mediator for Scene Registry auto-sync events
 *
 * This mediator listens to SceneRegistry Qt signals and publishes
 * corresponding EventBus events that panels can subscribe to.
 *
 * Signal Flow:
 * SceneRegistry::sceneRegistered() → SceneCreatedEvent
 * SceneRegistry::sceneRenamed() → SceneRenamedEvent
 * SceneRegistry::sceneUnregistered() → SceneDeletedEvent
 * SceneRegistry::sceneThumbnailUpdated() → SceneThumbnailUpdatedEvent
 * SceneRegistry::sceneMetadataChanged() → SceneMetadataUpdatedEvent
 */
class SceneRegistryMediator : public QObject {
  Q_OBJECT

public:
  /**
   * @brief Construct the scene registry mediator
   * @param sceneRegistry Pointer to the SceneRegistry instance
   * @param parent Parent QObject (optional)
   */
  explicit SceneRegistryMediator(SceneRegistry* sceneRegistry, QObject* parent = nullptr);

  ~SceneRegistryMediator() override;

  /**
   * @brief Initialize signal-to-event connections
   */
  void initialize();

  /**
   * @brief Shutdown and disconnect signals
   */
  void shutdown();

private slots:
  /**
   * @brief Handle scene registration signal
   * @param sceneId ID of the newly registered scene
   */
  void onSceneRegistered(const QString& sceneId);

  /**
   * @brief Handle scene rename signal
   * @param sceneId Scene ID (unchanged)
   * @param oldName Previous display name
   * @param newName New display name
   */
  void onSceneRenamed(const QString& sceneId, const QString& oldName, const QString& newName);

  /**
   * @brief Handle scene unregistration signal
   * @param sceneId ID of the unregistered scene
   */
  void onSceneUnregistered(const QString& sceneId);

  /**
   * @brief Handle thumbnail update signal
   * @param sceneId ID of the scene with updated thumbnail
   */
  void onSceneThumbnailUpdated(const QString& sceneId);

  /**
   * @brief Handle metadata change signal
   * @param sceneId ID of the scene with updated metadata
   */
  void onSceneMetadataChanged(const QString& sceneId);

private:
  SceneRegistry* m_sceneRegistry = nullptr; ///< Scene registry instance
};

} // namespace NovelMind::editor::mediators
