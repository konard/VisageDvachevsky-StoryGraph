#pragma once

/**
 * @file story_graph_edge_manager.hpp
 * @brief Edge/connection management for Story Graph
 *
 * Handles connection creation, deletion, and label updates for branching nodes
 * (Choice and Condition nodes).
 */

#include <cstdint>

namespace NovelMind::editor::qt {

class NMGraphNodeItem;
class NMStoryGraphPanel;
class NMStoryGraphScene;

namespace edge_manager {

/**
 * @brief Handle connection added event
 * @param panel The story graph panel
 * @param scene The story graph scene
 * @param fromNodeId Source node ID
 * @param toNodeId Target node ID
 */
void handleConnectionAdded(NMStoryGraphPanel *panel, NMStoryGraphScene *scene,
                           uint64_t fromNodeId, uint64_t toNodeId);

/**
 * @brief Handle connection deleted event
 * @param panel The story graph panel
 * @param scene The story graph scene
 * @param fromNodeId Source node ID
 * @param toNodeId Target node ID (deleted connection)
 */
void handleConnectionDeleted(NMStoryGraphPanel *panel,
                             NMStoryGraphScene *scene, uint64_t fromNodeId,
                             uint64_t toNodeId);

} // namespace edge_manager
} // namespace NovelMind::editor::qt
