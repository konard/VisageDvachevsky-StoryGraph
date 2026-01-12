#pragma once

/**
 * @file project_graph_analyzer.hpp
 * @brief StoryGraph Analysis Module for Project Integrity
 *
 * Analyzes StoryGraph structure for:
 * - Graph cycles (potential infinite loops)
 * - Unreachable nodes
 * - Dead ends
 * - Invalid goto references
 */

#include "NovelMind/editor/project_integrity.hpp"
#include <string>
#include <vector>

namespace NovelMind::editor {

/**
 * @brief Analyzes StoryGraph structure and dependencies
 */
class ProjectGraphAnalyzer {
public:
  ProjectGraphAnalyzer();
  ~ProjectGraphAnalyzer() = default;

  /**
   * @brief Set the project path
   */
  void setProjectPath(const std::string& projectPath);

  /**
   * @brief Check overall StoryGraph structure
   */
  void checkStoryGraphStructure(std::vector<IntegrityIssue>& issues);

  /**
   * @brief Analyze node reachability from entry points
   */
  void analyzeReachability(std::vector<IntegrityIssue>& issues);

  /**
   * @brief Detect cycles in the graph
   */
  void detectCycles(std::vector<IntegrityIssue>& issues);

  /**
   * @brief Check for dead ends (nodes with no exits)
   */
  void checkDeadEnds(std::vector<IntegrityIssue>& issues);

private:
  std::string m_projectPath;
};

} // namespace NovelMind::editor
