#include "story_graph_node_factory.hpp"
#include "NovelMind/editor/project_manager.hpp"
#include "NovelMind/editor/qt/panels/nm_story_graph_panel.hpp"
#include "nm_story_graph_panel_detail.hpp"

#include <QDebug>

namespace NovelMind::editor::qt::node_factory {

void handleNodeClick(NMStoryGraphPanel *panel, uint64_t nodeId) {
  auto *node = panel->findNodeById(nodeId);
  if (!node) {
    return;
  }

  // Single click should only select the node, not open Script Editor
  // Script Editor should only open on double-click (handled in
  // handleNodeDoubleClick)
  emit panel->nodeSelected(node->nodeIdString());
}

void handleNodeDoubleClick(NMStoryGraphPanel *panel, uint64_t nodeId) {
  auto *node = panel->findNodeById(nodeId);
  if (!node) {
    return;
  }

  auto *scene = panel->graphScene();
  if (scene) {
    scene->clearSelection();
  }
  node->setSelected(true);

  auto *view = panel->graphView();
  if (view) {
    view->centerOn(node);
  }

  emit panel->nodeSelected(node->nodeIdString());
  emit panel->nodeActivated(node->nodeIdString());

  // Scene Node specific: emit signal to open Scene View
  if (node->isSceneNode()) {
    const QString sceneId =
        node->sceneId().isEmpty() ? node->nodeIdString() : node->sceneId();
    qDebug() << "[StoryGraph] Scene node double-clicked, emitting "
                "sceneNodeDoubleClicked:"
             << sceneId;
    emit panel->sceneNodeDoubleClicked(sceneId);
  } else {
    // For non-Scene nodes, double-click should open Script Editor
    if (!node->scriptPath().isEmpty()) {
      emit panel->scriptNodeRequested(node->scriptPath());
    }
  }
}

void handleNodeAdded(NMStoryGraphPanel *panel, uint64_t nodeId,
                     const QString &nodeIdString, const QString &nodeType) {
  Q_UNUSED(nodeIdString);

  auto *node = panel->findNodeById(nodeId);
  if (!node) {
    return;
  }

  auto *scene = panel->graphScene();
  if (scene) {
    scene->clearSelection();
  }
  node->setSelected(true);

  auto *view = panel->graphView();
  if (view) {
    view->centerOn(node);
  }

  emit panel->nodeSelected(node->nodeIdString());

  // Creating a node should keep focus in Story Graph panel
  // Script Editor should only open on explicit double-click

  NMStoryGraphPanel::LayoutNode layout = detail::buildLayoutFromNode(node);
  if ((nodeType.contains("Dialogue", Qt::CaseInsensitive) ||
       nodeType.contains("Choice", Qt::CaseInsensitive)) &&
      layout.speaker.isEmpty()) {
    layout.speaker = "Narrator";
    node->setDialogueSpeaker(layout.speaker);
  }

  // Note: Layout saving is handled by the panel
}

void handleNodeDeleted(NMStoryGraphPanel *panel, uint64_t nodeId) {
  Q_UNUSED(panel);
  Q_UNUSED(nodeId);
  // Note: Layout cleanup is handled by the panel
}

} // namespace NovelMind::editor::qt::node_factory
