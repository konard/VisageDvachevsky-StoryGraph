#pragma once

/**
 * @file story_graph_layout.hpp
 * @brief Auto-layout algorithms for Story Graph
 *
 * Provides graph layout functionality using Sugiyama-style hierarchical
 * layout algorithm with edge crossing minimization.
 */

#include <QHash>
#include <QList>
#include <QPointF>
#include <QSet>
#include <cstdint>

namespace NovelMind::editor::qt {

class NMGraphNodeItem;
class NMGraphConnectionItem;
class NMStoryGraphScene;

namespace layout {

/**
 * @brief Apply auto-layout to story graph using Sugiyama algorithm
 *
 * This function implements a hierarchical graph layout that:
 * 1. Assigns nodes to layers using longest path from sources
 * 2. Minimizes edge crossings using barycenter heuristic
 * 3. Places nodes with proper spacing
 * 4. Handles orphaned nodes (disconnected components)
 *
 * @param scene The story graph scene to layout
 * @param nodes List of nodes to layout
 * @param connections List of connections between nodes
 */
void applyAutoLayout(NMStoryGraphScene *scene,
                     const QList<NMGraphNodeItem *> &nodes,
                     const QList<NMGraphConnectionItem *> &connections);

} // namespace layout
} // namespace NovelMind::editor::qt
