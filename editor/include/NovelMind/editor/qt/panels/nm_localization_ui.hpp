#pragma once

/**
 * @file nm_localization_ui.hpp
 * @brief UI setup and management for localization panel
 *
 * Handles:
 * - Table population and styling
 * - Status bar updates
 * - Preview panel management
 * - RTL layout support
 * - Missing translation highlighting
 */

#include "NovelMind/editor/qt/panels/nm_localization_data_model.hpp"
#include "NovelMind/localization/localization_manager.hpp"

#include <QHash>
#include <QString>

class QTableWidget;
class QLabel;
class QLineEdit;

namespace NovelMind::editor::qt {

/**
 * @brief UI helper for localization panel
 */
class NMLocalizationUI {
public:
  explicit NMLocalizationUI(NMLocalizationDataModel& dataModel,
                            ::NovelMind::localization::LocalizationManager& localization);
  ~NMLocalizationUI();

  /**
   * @brief Rebuild the entire localization table
   * @param table The table widget to populate
   * @param currentLocale Current locale code
   * @param defaultLocale Default locale code
   * @param keyToRowMap Output map for key-to-row mapping (for O(1) lookups)
   */
  void rebuildTable(QTableWidget* table, const QString& currentLocale, const QString& defaultLocale,
                    QHash<QString, int>& keyToRowMap);

  /**
   * @brief Apply search and filter to table rows
   * @param table The table widget
   * @param searchText Search query
   * @param showMissingOnly Filter flag for missing translations
   */
  void applyFilters(QTableWidget* table, const QString& searchText, bool showMissingOnly);

  /**
   * @brief Highlight missing translations in table
   * @param table The table widget
   */
  void highlightMissingTranslations(QTableWidget* table);

  /**
   * @brief Update status bar with statistics
   * @param statusLabel The status label widget
   * @param table The table widget
   * @param isDirty Whether panel has unsaved changes
   */
  void updateStatusBar(QLabel* statusLabel, QTableWidget* table, bool isDirty);

  /**
   * @brief Update preview panel with variable interpolation
   * @param previewOutput Output label for preview
   * @param table The table widget (to get selected row)
   * @param previewVariables Variables for interpolation
   * @param currentLocale Current locale code
   */
  void updatePreview(QLabel* previewOutput, QTableWidget* table,
                     const QHash<QString, QString>& previewVariables, const QString& currentLocale);

  /**
   * @brief Apply RTL layout to table and preview
   * @param table The table widget
   * @param previewOutput Preview label widget
   * @param rtl Whether to enable RTL layout
   */
  void applyRTLLayout(QTableWidget* table, QLabel* previewOutput, bool rtl);

  /**
   * @brief Update a single table cell value
   * @param table The table widget
   * @param key The localization key
   * @param value The new value
   * @param keyToRowMap Key-to-row mapping for O(1) lookup
   */
  void updateTableCell(QTableWidget* table, const QString& key, const QString& value,
                       const QHash<QString, int>& keyToRowMap);

private:
  NMLocalizationDataModel& m_dataModel;
  ::NovelMind::localization::LocalizationManager& m_localization;
};

} // namespace NovelMind::editor::qt
