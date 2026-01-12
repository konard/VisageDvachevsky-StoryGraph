#pragma once

/**
 * @file navigation_location.hpp
 * @brief Unified location format for cross-panel navigation
 *
 * Defines a type-safe location format for navigating between different
 * panels in the editor. Supports:
 * - StoryGraph:node_id - Navigate to a story graph node
 * - Script:path:line - Navigate to a specific line in a script file
 */

#include <QString>
#include <QVariant>
#include <optional>

namespace NovelMind::editor::qt {

/**
 * @brief Type of navigation source
 */
enum class NavigationSourceType {
  StoryGraph, ///< Story graph node location
  Script      ///< Script file and line location
};

/**
 * @brief Represents a location in the editor that can be navigated to
 *
 * This class provides a unified, type-safe way to represent locations
 * across different panels. It supports parsing from string format and
 * validation of location data.
 */
class NavigationLocation {
public:
  /**
   * @brief Construct an invalid/empty location
   */
  NavigationLocation() = default;

  /**
   * @brief Construct a StoryGraph location
   * @param nodeId The node ID string
   */
  static NavigationLocation makeStoryGraph(const QString& nodeId);

  /**
   * @brief Construct a Script location
   * @param filePath The script file path
   * @param lineNumber The line number (1-based, -1 for no specific line)
   */
  static NavigationLocation makeScript(const QString& filePath, int lineNumber = -1);

  /**
   * @brief Parse a location string into a NavigationLocation
   * @param locationString String in format "StoryGraph:node_id" or
   * "Script:path:line"
   * @return Parsed location, or std::nullopt if parsing failed
   */
  static std::optional<NavigationLocation> parse(const QString& locationString);

  /**
   * @brief Check if this location is valid
   */
  [[nodiscard]] bool isValid() const { return m_isValid; }

  /**
   * @brief Get the source type
   */
  [[nodiscard]] NavigationSourceType sourceType() const { return m_sourceType; }

  /**
   * @brief Get the node ID for StoryGraph locations
   * @return Node ID, or empty string if not a StoryGraph location
   */
  [[nodiscard]] QString nodeId() const;

  /**
   * @brief Get the file path for Script locations
   * @return File path, or empty string if not a Script location
   */
  [[nodiscard]] QString filePath() const;

  /**
   * @brief Get the line number for Script locations
   * @return Line number (1-based), or -1 if not a Script location or no line
   * specified
   */
  [[nodiscard]] int lineNumber() const;

  /**
   * @brief Convert to string format
   * @return String representation (e.g., "StoryGraph:node_id" or
   * "Script:path:line")
   */
  [[nodiscard]] QString toString() const;

  /**
   * @brief Get a human-readable description of the location
   */
  [[nodiscard]] QString description() const;

private:
  bool m_isValid = false;
  NavigationSourceType m_sourceType = NavigationSourceType::StoryGraph;
  QString m_nodeId;      ///< For StoryGraph
  QString m_filePath;    ///< For Script
  int m_lineNumber = -1; ///< For Script
};

} // namespace NovelMind::editor::qt
