#include "story_graph_edge_manager.hpp"
#include "NovelMind/editor/qt/panels/nm_story_graph_panel.hpp"
#include "nm_story_graph_panel_detail.hpp"

namespace NovelMind::editor::qt::edge_manager {

void handleConnectionAdded(NMStoryGraphPanel *panel, NMStoryGraphScene *scene,
                           uint64_t fromNodeId, uint64_t toNodeId) {
  auto *from = panel->findNodeById(fromNodeId);
  auto *to = panel->findNodeById(toNodeId);
  if (!from || !to) {
    return;
  }

  // Collect all connections from this source node
  QList<NMGraphConnectionItem *> outgoingConns;
  for (auto *conn : scene->connections()) {
    if (!conn || !conn->startNode() || !conn->endNode()) {
      continue;
    }
    if (conn->startNode()->nodeId() == fromNodeId) {
      outgoingConns.append(conn);
    }
  }

  // Update connection labels for Choice nodes
  if (from->isChoiceNode()) {
    const QStringList choices = from->choiceOptions();
    QHash<QString, QString> newTargets;

    for (int i = 0; i < outgoingConns.size(); ++i) {
      auto *conn = outgoingConns[i];
      QString label;
      if (i < choices.size()) {
        label = choices[i];
      } else {
        // Issue #323: Generate default option labels when choice options are
        // empty This ensures choiceTargets gets populated even without
        // predefined options
        label = QString("Option %1").arg(i + 1);
      }
      // Always map the label to target, regardless of whether it's a
      // predefined choice or a generated default label
      newTargets.insert(label, conn->endNode()->nodeIdString());
      conn->setLabel(label);
      conn->setBranchIndex(i);
      conn->update();
    }

    // Update the node's choice targets mapping
    from->setChoiceTargets(newTargets);
  }

  // Update connection labels for Condition nodes
  if (from->isConditionNode()) {
    QStringList outputs = from->conditionOutputs();
    if (outputs.isEmpty()) {
      outputs << "true"
              << "false"; // Default outputs
    }

    QHash<QString, QString> newTargets;
    for (int i = 0; i < outgoingConns.size(); ++i) {
      auto *conn = outgoingConns[i];
      QString label;
      if (i < outputs.size()) {
        label = outputs[i];
      } else {
        // Issue #323: Generate default branch labels when outputs exceed
        // defined labels
        label = QString("branch_%1").arg(i + 1);
      }
      // Always map the label to target, regardless of whether it's predefined
      newTargets.insert(label, conn->endNode()->nodeIdString());
      conn->setLabel(label);
      conn->setBranchIndex(i);
      conn->update();
    }

    // Update the node's condition targets mapping
    from->setConditionTargets(newTargets);
  }

  // Build targets list for script update
  QStringList targets;
  for (auto *conn : outgoingConns) {
    targets << conn->endNode()->nodeIdString();
  }

  detail::updateSceneGraphBlock(from->nodeIdString(),
                                 detail::resolveScriptPath(from), targets);

  // Note: Layout saving is handled by the panel
}

void handleConnectionDeleted(NMStoryGraphPanel *panel, NMStoryGraphScene *scene,
                             uint64_t fromNodeId, uint64_t toNodeId) {
  Q_UNUSED(toNodeId);

  auto *from = panel->findNodeById(fromNodeId);
  if (!from) {
    return;
  }

  // Collect remaining connections from this source node
  QList<NMGraphConnectionItem *> outgoingConns;
  for (auto *conn : scene->connections()) {
    if (!conn || !conn->startNode() || !conn->endNode()) {
      continue;
    }
    if (conn->startNode()->nodeId() == fromNodeId) {
      outgoingConns.append(conn);
    }
  }

  // Update connection labels for Choice nodes
  if (from->isChoiceNode()) {
    const QStringList choices = from->choiceOptions();
    QHash<QString, QString> newTargets;

    for (int i = 0; i < outgoingConns.size(); ++i) {
      auto *conn = outgoingConns[i];
      QString label;
      if (i < choices.size()) {
        label = choices[i];
      } else {
        // Issue #323: Generate default option labels when choice options are
        // empty
        label = QString("Option %1").arg(i + 1);
      }
      // Always map the label to target
      newTargets.insert(label, conn->endNode()->nodeIdString());
      conn->setLabel(label);
      conn->setBranchIndex(i);
      conn->update();
    }

    from->setChoiceTargets(newTargets);
  }

  // Update connection labels for Condition nodes
  if (from->isConditionNode()) {
    QStringList outputs = from->conditionOutputs();
    if (outputs.isEmpty()) {
      outputs << "true"
              << "false";
    }

    QHash<QString, QString> newTargets;
    for (int i = 0; i < outgoingConns.size(); ++i) {
      auto *conn = outgoingConns[i];
      QString label;
      if (i < outputs.size()) {
        label = outputs[i];
      } else {
        // Issue #323: Generate default branch labels when outputs exceed
        // defined labels
        label = QString("branch_%1").arg(i + 1);
      }
      // Always map the label to target
      newTargets.insert(label, conn->endNode()->nodeIdString());
      conn->setLabel(label);
      conn->setBranchIndex(i);
      conn->update();
    }

    from->setConditionTargets(newTargets);
  }

  // Build targets list for script update
  QStringList targets;
  for (auto *conn : outgoingConns) {
    targets << conn->endNode()->nodeIdString();
  }

  detail::updateSceneGraphBlock(from->nodeIdString(),
                                 detail::resolveScriptPath(from), targets);

  // Note: Layout saving is handled by the panel
}

} // namespace NovelMind::editor::qt::edge_manager
