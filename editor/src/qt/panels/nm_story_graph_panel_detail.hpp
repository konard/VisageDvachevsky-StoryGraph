#pragma once

#include "NovelMind/editor/qt/panels/nm_story_graph_panel.hpp"

namespace NovelMind::editor::qt::detail {

bool loadGraphLayout(QHash<QString, NMStoryGraphPanel::LayoutNode> &nodes,
                     QString &entryScene);
void saveGraphLayout(const QHash<QString, NMStoryGraphPanel::LayoutNode> &nodes,
                     const QString &entryScene);
QString resolveScriptPath(const NMGraphNodeItem *node);
bool updateSceneGraphBlock(const QString &sceneId, const QString &scriptPath,
                           const QStringList &targets);
bool updateSceneSayStatement(const QString &sceneId, const QString &scriptPath,
                             const QString &speaker, const QString &text);
QStringList splitChoiceLines(const QString &raw);
NMStoryGraphPanel::LayoutNode buildLayoutFromNode(const NMGraphNodeItem *node);

} // namespace NovelMind::editor::qt::detail
