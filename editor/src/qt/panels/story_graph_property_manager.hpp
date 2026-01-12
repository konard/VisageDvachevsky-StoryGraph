#pragma once

/**
 * @file story_graph_property_manager.hpp
 * @brief Node property management and UI utility functions
 *
 * Handles node property updates, localization, and other utility operations.
 */

#include <QString>
#include <QVector>

namespace NovelMind::editor::qt {

class NMStoryGraphPanel;
struct GraphNodeMove;

namespace property_manager {

/**
 * @brief Apply property change to a node
 * @param panel The story graph panel
 * @param nodeIdString Node ID string
 * @param propertyName Property name to change
 * @param newValue New property value
 */
void applyNodePropertyChange(NMStoryGraphPanel *panel,
                             const QString &nodeIdString,
                             const QString &propertyName,
                             const QString &newValue);

/**
 * @brief Handle node position changes
 * @param panel The story graph panel
 * @param moves Vector of node moves
 */
void handleNodesMoved(NMStoryGraphPanel *panel,
                      const QVector<GraphNodeMove> &moves);

/**
 * @brief Handle locale preview change for localization
 * @param panel The story graph panel
 * @param index Locale selector index
 */
void handleLocalePreviewChange(NMStoryGraphPanel *panel, int index);

/**
 * @brief Export dialogue entries for localization
 * @param panel The story graph panel
 */
void handleExportDialogue(NMStoryGraphPanel *panel);

/**
 * @brief Generate localization keys for dialogue nodes
 * @param panel The story graph panel
 */
void handleGenerateLocalizationKeys(NMStoryGraphPanel *panel);

} // namespace property_manager
} // namespace NovelMind::editor::qt
