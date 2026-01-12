#pragma once

/**
 * @file story_graph_node_factory.hpp
 * @brief Node creation and lifecycle management for Story Graph
 *
 * Handles node creation, property updates, and related events.
 */

#include <QString>
#include <cstdint>

namespace NovelMind::editor::qt {

class NMGraphNodeItem;
class NMStoryGraphPanel;

namespace node_factory {

/**
 * @brief Handle node click event
 * @param panel The story graph panel
 * @param nodeId The clicked node ID
 */
void handleNodeClick(NMStoryGraphPanel *panel, uint64_t nodeId);

/**
 * @brief Handle node double-click event
 * @param panel The story graph panel
 * @param nodeId The double-clicked node ID
 */
void handleNodeDoubleClick(NMStoryGraphPanel *panel, uint64_t nodeId);

/**
 * @brief Handle node added event
 * @param panel The story graph panel
 * @param nodeId The new node ID
 * @param nodeIdString The new node ID string
 * @param nodeType The node type
 */
void handleNodeAdded(NMStoryGraphPanel *panel, uint64_t nodeId,
                     const QString &nodeIdString, const QString &nodeType);

/**
 * @brief Handle node deleted event
 * @param panel The story graph panel
 * @param nodeId The deleted node ID
 */
void handleNodeDeleted(NMStoryGraphPanel *panel, uint64_t nodeId);

} // namespace node_factory
} // namespace NovelMind::editor::qt
