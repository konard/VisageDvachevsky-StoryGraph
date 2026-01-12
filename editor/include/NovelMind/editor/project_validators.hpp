#pragma once

/**
 * @file project_validators.hpp
 * @brief Validation Rules Module for Project Integrity
 *
 * Implements validation rules for:
 * - Project configuration
 * - Scene references
 * - Voice lines
 * - Localization keys
 * - Script syntax
 * - Resource conflicts
 */

#include "NovelMind/editor/project_integrity.hpp"
#include <string>
#include <unordered_map>
#include <vector>

namespace NovelMind::editor {

/**
 * @brief Validates project configuration and resources
 */
class ProjectValidators {
public:
  ProjectValidators();
  ~ProjectValidators() = default;

  /**
   * @brief Set the project path
   */
  void setProjectPath(const std::string& projectPath);

  /**
   * @brief Set the list of locales to check
   */
  void setLocales(const std::vector<std::string>& locales);

  /**
   * @brief Check project configuration
   */
  void checkProjectConfiguration(std::vector<IntegrityIssue>& issues);

  /**
   * @brief Check scene references
   */
  void checkSceneReferences(std::vector<IntegrityIssue>& issues);

  /**
   * @brief Check voice line files
   */
  void checkVoiceLines(std::vector<IntegrityIssue>& issues);

  /**
   * @brief Scan localization files
   */
  void scanLocalizationFiles();

  /**
   * @brief Check localization keys
   */
  void checkLocalizationKeys(std::vector<IntegrityIssue>& issues);

  /**
   * @brief Check for missing translations
   */
  void checkMissingTranslations(std::vector<IntegrityIssue>& issues);

  /**
   * @brief Check for unused localization strings
   */
  void checkUnusedStrings(std::vector<IntegrityIssue>& issues);

  /**
   * @brief Check script syntax
   */
  void checkScriptSyntax(std::vector<IntegrityIssue>& issues);

  /**
   * @brief Check resource conflicts
   */
  void checkResourceConflicts(std::vector<IntegrityIssue>& issues);

  /**
   * @brief Get localization strings map
   */
  const std::unordered_map<std::string, std::vector<std::string>>& getLocalizationStrings() const {
    return m_localizationStrings;
  }

private:
  std::string m_projectPath;
  std::vector<std::string> m_locales;
  std::unordered_map<std::string, std::vector<std::string>> m_localizationStrings;
};

} // namespace NovelMind::editor
