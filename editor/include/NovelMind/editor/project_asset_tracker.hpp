#pragma once

/**
 * @file project_asset_tracker.hpp
 * @brief Asset Reference Tracking Module for Project Integrity
 *
 * Tracks and validates asset references:
 * - Scans project assets
 * - Collects asset references from scenes/scripts
 * - Finds orphaned (unreferenced) assets
 */

#include "NovelMind/editor/project_integrity.hpp"
#include <string>
#include <unordered_set>
#include <vector>

namespace NovelMind::editor {

/**
 * @brief Tracks asset references across the project
 */
class ProjectAssetTracker {
public:
  ProjectAssetTracker();
  ~ProjectAssetTracker() = default;

  /**
   * @brief Set the project path
   */
  void setProjectPath(const std::string& projectPath);

  /**
   * @brief Scan all project assets
   */
  void scanProjectAssets();

  /**
   * @brief Collect asset references from scenes and scripts
   */
  void collectAssetReferences();

  /**
   * @brief Check for missing asset references
   */
  void checkAssetReferences(std::vector<IntegrityIssue>& issues);

  /**
   * @brief Find orphaned (unreferenced) assets
   */
  void findOrphanedAssets(std::vector<IntegrityIssue>& issues);

  /**
   * @brief Get the set of project assets
   */
  const std::unordered_set<std::string>& getProjectAssets() const { return m_projectAssets; }

  /**
   * @brief Get the set of referenced assets
   */
  const std::unordered_set<std::string>& getReferencedAssets() const { return m_referencedAssets; }

private:
  std::string m_projectPath;
  std::unordered_set<std::string> m_projectAssets;
  std::unordered_set<std::string> m_referencedAssets;
};

} // namespace NovelMind::editor
