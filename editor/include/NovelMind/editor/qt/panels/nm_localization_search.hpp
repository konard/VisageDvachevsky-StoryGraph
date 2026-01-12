#pragma once

/**
 * @file nm_localization_search.hpp
 * @brief Search and validation functionality for localization
 *
 * Handles:
 * - Key validation
 * - Search/filter functionality
 * - Missing translation detection
 * - Unused key detection
 * - Project usage scanning
 */

#include "NovelMind/editor/qt/panels/nm_localization_data_model.hpp"

#include <QRegularExpression>
#include <QString>
#include <QStringList>

namespace NovelMind::editor::qt {

/**
 * @brief Search and validation handler for localization
 */
class NMLocalizationSearch {
public:
  explicit NMLocalizationSearch(NMLocalizationDataModel& dataModel);
  ~NMLocalizationSearch();

  /**
   * @brief Validate key name format
   * @param key The key to validate
   * @return true if key name is valid
   */
  bool isValidKeyName(const QString& key) const;

  /**
   * @brief Check if key is unique (not already in use)
   * @param key The key to check
   * @return true if key is unique
   */
  bool isKeyUnique(const QString& key) const;

  /**
   * @brief Find missing translations for a locale
   * @param locale The locale to check
   * @return List of keys with missing translations
   */
  QStringList findMissingTranslations(const QString& locale) const;

  /**
   * @brief Find unused keys in the project
   * @return List of unused keys
   */
  QStringList findUnusedKeys() const;

  /**
   * @brief Scan project for key usages
   * @param projectPath Root path of the project
   */
  void scanProjectForUsages(const QString& projectPath);

  /**
   * @brief Get key validation regex
   */
  static const QRegularExpression& keyValidationRegex();

private:
  NMLocalizationDataModel& m_dataModel;

  // Key validation: allows alphanumeric, underscore, dot, dash
  static const QRegularExpression s_keyValidationRegex;
};

} // namespace NovelMind::editor::qt
