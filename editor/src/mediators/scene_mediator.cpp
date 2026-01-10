#include "NovelMind/editor/mediators/scene_mediator.hpp"
#include "NovelMind/editor/qt/panels/nm_scene_view_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_story_graph_panel.hpp"
#include "NovelMind/editor/scene_registry.hpp"

namespace NovelMind::editor::mediators {

SceneMediator::SceneMediator(QObject* parent) : QObject(parent) {}

SceneMediator::~SceneMediator() {
  shutdown();
}

void SceneMediator::initialize(SceneRegistry* sceneRegistry, qt::NMSceneViewPanel* sceneView,
                               qt::NMStoryGraphPanel* storyGraph) {
  if (m_initialized) {
    return;
  }

  m_sceneRegistry = sceneRegistry;
  m_sceneView = sceneView;
  m_storyGraph = storyGraph;

  // Connect to SceneRegistry signals
  if (m_sceneRegistry) {
    connect(m_sceneRegistry, &SceneRegistry::sceneRegistered, this,
            &SceneMediator::onSceneRegistered);
    connect(m_sceneRegistry, &SceneRegistry::sceneUnregistered, this,
            &SceneMediator::onSceneUnregistered);
    connect(m_sceneRegistry, &SceneRegistry::sceneIdChanged, this,
            &SceneMediator::onSceneIdChanged);
    connect(m_sceneRegistry, &SceneRegistry::sceneRenamed, this, &SceneMediator::onSceneRenamed);
  }

  // Connect to StoryGraph signals
  if (m_storyGraph && m_storyGraph->graphScene()) {
    connect(m_storyGraph->graphScene(), &qt::NMStoryGraphScene::nodeAdded, this,
            &SceneMediator::onNodeAdded);
    connect(m_storyGraph->graphScene(), &qt::NMStoryGraphScene::nodeDeleted, this,
            &SceneMediator::onNodeDeleted);
  }

  // Connect to SceneView signals
  if (m_sceneView) {
    connect(m_sceneView, &qt::NMSceneViewPanel::sceneChanged, this, &SceneMediator::onSceneChanged);
  }

  // Initial sync of references
  syncSceneReferencesFromGraph();

  m_initialized = true;
}

void SceneMediator::shutdown() {
  if (!m_initialized) {
    return;
  }

  // Disconnect all signals
  if (m_sceneRegistry) {
    disconnect(m_sceneRegistry, nullptr, this, nullptr);
  }
  if (m_storyGraph && m_storyGraph->graphScene()) {
    disconnect(m_storyGraph->graphScene(), nullptr, this, nullptr);
  }
  if (m_sceneView) {
    disconnect(m_sceneView, nullptr, this, nullptr);
  }

  // Unsubscribe from EventBus
  auto& bus = EventBus::instance();
  for (auto& sub : m_subscriptions) {
    bus.unsubscribe(sub);
  }
  m_subscriptions.clear();

  m_sceneRegistry = nullptr;
  m_sceneView = nullptr;
  m_storyGraph = nullptr;
  m_initialized = false;
}

void SceneMediator::syncSceneReferencesFromGraph() {
  if (!m_sceneRegistry || !m_storyGraph || !m_storyGraph->graphScene()) {
    return;
  }

  // Clear all existing references (will rebuild from scratch)
  for (const QString& sceneId : m_sceneRegistry->getAllSceneIds()) {
    SceneMetadata meta = m_sceneRegistry->getSceneMetadata(sceneId);
    meta.referencingNodes.clear();
    m_sceneRegistry->updateSceneMetadata(sceneId, meta);
  }

  // Scan all nodes in the graph
  const auto& nodes = m_storyGraph->graphScene()->nodes();
  for (const auto* node : nodes) {
    if (node->isSceneNode() && !node->sceneId().isEmpty()) {
      const QString sceneId = node->sceneId();
      const QString nodeIdString = node->nodeIdString();

      // Add reference if scene exists
      if (m_sceneRegistry->sceneExists(sceneId)) {
        m_sceneRegistry->addSceneReference(sceneId, nodeIdString);
      }
    }
  }

  emit sceneReferencesUpdated();
}

QStringList SceneMediator::validateSceneReferences() const {
  QStringList errors;

  if (!m_sceneRegistry || !m_storyGraph || !m_storyGraph->graphScene()) {
    return errors;
  }

  // Check each scene node in the graph
  const auto& nodes = m_storyGraph->graphScene()->nodes();
  for (const auto* node : nodes) {
    if (node->isSceneNode()) {
      const QString sceneId = node->sceneId();
      const QString nodeIdString = node->nodeIdString();

      if (sceneId.isEmpty()) {
        errors << QString("Scene node '%1' has no scene ID assigned").arg(nodeIdString);
      } else if (!m_sceneRegistry->sceneExists(sceneId)) {
        errors << QString("Scene node '%1' references non-existent scene '%2'")
                      .arg(nodeIdString, sceneId);
      }
    }
  }

  // Check for broken references in the registry
  QStringList broken = m_sceneRegistry->getInvalidSceneReferences();
  for (const QString& sceneId : broken) {
    errors << QString("Scene '%1' has missing document file").arg(sceneId);
  }

  return errors;
}

void SceneMediator::onSceneRegistered(const QString& sceneId) {
  Q_UNUSED(sceneId);
  // No immediate action needed - nodes will add references when they use this
  // scene
}

void SceneMediator::onSceneUnregistered(const QString& sceneId) {
  // Check if any nodes still reference this scene
  if (!m_storyGraph || !m_storyGraph->graphScene()) {
    return;
  }

  QStringList affectedNodes;
  const auto& nodes = m_storyGraph->graphScene()->nodes();
  for (const auto* node : nodes) {
    if (node->isSceneNode() && node->sceneId() == sceneId) {
      affectedNodes << node->nodeIdString();
    }
  }

  if (!affectedNodes.isEmpty()) {
    emit invalidSceneReference(sceneId, affectedNodes);
  }
}

void SceneMediator::onSceneIdChanged(const QString& oldId, const QString& newId) {
  if (!m_storyGraph || !m_storyGraph->graphScene()) {
    return;
  }

  // Update all scene nodes that reference the old ID
  const auto& nodes = m_storyGraph->graphScene()->nodes();
  for (auto* node : nodes) {
    if (node->isSceneNode() && node->sceneId() == oldId) {
      node->setSceneId(newId);
      node->update();
    }
  }
}

void SceneMediator::onSceneRenamed(const QString& sceneId, const QString& oldName,
                                   const QString& newName) {
  Q_UNUSED(sceneId);
  Q_UNUSED(oldName);
  Q_UNUSED(newName);
  // Display name change doesn't affect node references
}

void SceneMediator::onNodeAdded(uint64_t nodeId, const QString& nodeIdString,
                                const QString& nodeType) {
  Q_UNUSED(nodeId);

  // Only track scene nodes
  if (nodeType.compare("Scene", Qt::CaseInsensitive) != 0) {
    return;
  }

  // Get the scene ID from the node
  QString sceneId = getNodeSceneId(nodeIdString);
  if (!sceneId.isEmpty() && m_sceneRegistry) {
    m_sceneRegistry->addSceneReference(sceneId, nodeIdString);
  }
}

void SceneMediator::onNodeDeleted(uint64_t nodeId) {
  if (!m_storyGraph || !m_sceneRegistry) {
    return;
  }

  // Find the node by ID to get its scene ID
  auto* node = m_storyGraph->graphScene()->findNode(nodeId);
  if (node && node->isSceneNode() && !node->sceneId().isEmpty()) {
    m_sceneRegistry->removeSceneReference(node->sceneId(), node->nodeIdString());
  }
}

void SceneMediator::onSceneChanged(const QString& sceneId) {
  Q_UNUSED(sceneId);
  // Scene was loaded/changed in Scene View
  // Could trigger thumbnail regeneration here if needed
}

void SceneMediator::updateNodeSceneReference(const QString& nodeIdString, const QString& oldSceneId,
                                             const QString& newSceneId) {
  if (!m_sceneRegistry) {
    return;
  }

  // Remove old reference
  if (!oldSceneId.isEmpty()) {
    m_sceneRegistry->removeSceneReference(oldSceneId, nodeIdString);
  }

  // Add new reference
  if (!newSceneId.isEmpty()) {
    m_sceneRegistry->addSceneReference(newSceneId, nodeIdString);
  }
}

QString SceneMediator::getNodeSceneId(const QString& nodeIdString) const {
  if (!m_storyGraph) {
    return QString();
  }

  auto* node = m_storyGraph->findNodeByIdString(nodeIdString);
  if (node && node->isSceneNode()) {
    return node->sceneId();
  }

  return QString();
}

} // namespace NovelMind::editor::mediators
