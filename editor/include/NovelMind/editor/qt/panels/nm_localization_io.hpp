#pragma once

/**
 * @file nm_localization_io.hpp
 * @brief Import/export functionality for localization files
 *
 * Handles:
 * - Loading locales from files (CSV, JSON, PO, XLIFF)
 * - Exporting locales to files
 * - Format detection
 * - Missing translations export
 */

#include "NovelMind/editor/qt/panels/nm_localization_data_model.hpp"
#include "NovelMind/localization/localization_manager.hpp"

#include <QString>
#include <QStringList>

class QWidget;

namespace NovelMind::editor::qt {

/**
 * @brief Import/Export handler for localization files
 */
class NMLocalizationIO {
public:
  explicit NMLocalizationIO(::NovelMind::localization::LocalizationManager& localization,
                            NMLocalizationDataModel& dataModel);
  ~NMLocalizationIO();

  /**
   * @brief Load a locale from disk
   * @param localeCode The locale code to load
   * @param localizationRoot Root directory for localization files
   * @return true if loaded successfully
   */
  bool loadLocale(const QString& localeCode, const QString& localizationRoot);

  /**
   * @brief Save changes to all locale files
   * @param availableLocales List of available locales
   * @param localizationRoot Root directory for localization files
   * @param parentWidget Parent widget for error dialogs
   * @return true if saved successfully
   */
  bool saveChanges(const QStringList& availableLocales, const QString& localizationRoot,
                   QWidget* parentWidget);

  /**
   * @brief Export current locale to file
   * @param currentLocale The current locale code
   * @param localizationRoot Root directory for localization files
   * @param parentWidget Parent widget for file dialog
   */
  void exportLocale(const QString& currentLocale, const QString& localizationRoot,
                    QWidget* parentWidget);

  /**
   * @brief Import locale from file
   * @param localizationRoot Root directory for localization files
   * @param parentWidget Parent widget for file dialog
   * @param outLocaleCode Output parameter for imported locale code
   * @return true if imported successfully
   */
  bool importLocale(const QString& localizationRoot, QWidget* parentWidget,
                    QString& outLocaleCode);

  /**
   * @brief Export missing translations for current locale
   * @param currentLocale The current locale code
   * @param localizationRoot Root directory for localization files
   * @param parentWidget Parent widget for dialogs
   */
  void exportMissingStrings(const QString& currentLocale, const QString& localizationRoot,
                            QWidget* parentWidget);

  /**
   * @brief Export to CSV file
   * @param filePath Output file path
   * @param currentLocale The current locale code
   */
  void exportToCsv(const QString& filePath, const QString& currentLocale);

  /**
   * @brief Export to JSON file
   * @param filePath Output file path
   * @param currentLocale The current locale code
   */
  void exportToJson(const QString& filePath, const QString& currentLocale);

  /**
   * @brief Import from CSV file
   * @param filePath Input file path
   * @param currentLocale The current locale code
   * @param parentWidget Parent widget for error dialogs
   * @return true if imported successfully
   */
  bool importFromCsv(const QString& filePath, const QString& currentLocale, QWidget* parentWidget);

  /**
   * @brief Import from JSON file
   * @param filePath Input file path
   * @param currentLocale The current locale code
   * @param parentWidget Parent widget for error dialogs
   * @return true if imported successfully
   */
  bool importFromJson(const QString& filePath, const QString& currentLocale, QWidget* parentWidget);

  /**
   * @brief Detect file format from extension
   * @param extension File extension (e.g., "csv", "json")
   * @return Localization format enum
   */
  static ::NovelMind::localization::LocalizationFormat
  formatForExtension(const QString& extension);

private:
  ::NovelMind::localization::LocalizationManager& m_localization;
  NMLocalizationDataModel& m_dataModel;
};

} // namespace NovelMind::editor::qt
