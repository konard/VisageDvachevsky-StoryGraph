#include "NovelMind/editor/mediators/scene_registry_mediator.hpp"

namespace NovelMind::editor::mediators {

SceneRegistryMediator::SceneRegistryMediator(SceneRegistry* sceneRegistry, QObject* parent)
    : QObject(parent), m_sceneRegistry(sceneRegistry) {}

SceneRegistryMediator::~SceneRegistryMediator() {
  shutdown();
}

void SceneRegistryMediator::initialize() {
  if (!m_sceneRegistry) {
    qWarning() << "SceneRegistryMediator: Cannot initialize with null "
                  "SceneRegistry";
    return;
  }

  // Connect SceneRegistry Qt signals to our slots
  connect(m_sceneRegistry, &SceneRegistry::sceneRegistered, this,
          &SceneRegistryMediator::onSceneRegistered);

  connect(m_sceneRegistry, &SceneRegistry::sceneRenamed, this,
          &SceneRegistryMediator::onSceneRenamed);

  connect(m_sceneRegistry, &SceneRegistry::sceneUnregistered, this,
          &SceneRegistryMediator::onSceneUnregistered);

  connect(m_sceneRegistry, &SceneRegistry::sceneThumbnailUpdated, this,
          &SceneRegistryMediator::onSceneThumbnailUpdated);

  connect(m_sceneRegistry, &SceneRegistry::sceneMetadataChanged, this,
          &SceneRegistryMediator::onSceneMetadataChanged);

  qDebug() << "SceneRegistryMediator initialized";
}

void SceneRegistryMediator::shutdown() {
  if (m_sceneRegistry) {
    disconnect(m_sceneRegistry, nullptr, this, nullptr);
  }
  qDebug() << "SceneRegistryMediator shutdown";
}

void SceneRegistryMediator::onSceneRegistered(const QString& sceneId) {
  if (!m_sceneRegistry) {
    return;
  }

  // Get scene metadata for the event
  SceneMetadata metadata = m_sceneRegistry->getSceneMetadata(sceneId);

  // Publish SceneCreatedEvent to EventBus
  events::SceneCreatedEvent event;
  event.sceneId = sceneId;
  event.sceneName = metadata.name;

  EventBus::instance().publish(event);

  qDebug() << "SceneRegistryMediator: Published SceneCreatedEvent for" << sceneId;
}

void SceneRegistryMediator::onSceneRenamed(const QString& sceneId, const QString& oldName,
                                           const QString& newName) {
  if (!m_sceneRegistry) {
    return;
  }

  // Publish SceneRenamedEvent with both old and new names
  events::SceneRenamedEvent event;
  event.sceneId = sceneId;
  event.oldName = oldName;
  event.newName = newName;

  EventBus::instance().publish(event);

  qDebug() << "SceneRegistryMediator: Published SceneRenamedEvent for" << sceneId << "from"
           << oldName << "to" << newName;
}

void SceneRegistryMediator::onSceneUnregistered(const QString& sceneId) {
  // Publish SceneDeletedEvent to EventBus
  events::SceneDeletedEvent event;
  event.sceneId = sceneId;

  EventBus::instance().publish(event);

  qDebug() << "SceneRegistryMediator: Published SceneDeletedEvent for" << sceneId;
}

void SceneRegistryMediator::onSceneThumbnailUpdated(const QString& sceneId) {
  if (!m_sceneRegistry) {
    return;
  }

  // Get thumbnail path from registry
  QString thumbnailPath = m_sceneRegistry->getSceneThumbnailPath(sceneId);

  // Publish SceneThumbnailUpdatedEvent to EventBus
  events::SceneThumbnailUpdatedEvent event;
  event.sceneId = sceneId;
  event.thumbnailPath = thumbnailPath;

  EventBus::instance().publish(event);

  qDebug() << "SceneRegistryMediator: Published SceneThumbnailUpdatedEvent for" << sceneId;
}

void SceneRegistryMediator::onSceneMetadataChanged(const QString& sceneId) {
  // Publish SceneMetadataUpdatedEvent to EventBus
  events::SceneMetadataUpdatedEvent event;
  event.sceneId = sceneId;

  EventBus::instance().publish(event);

  qDebug() << "SceneRegistryMediator: Published SceneMetadataUpdatedEvent for" << sceneId;
}

} // namespace NovelMind::editor::mediators
