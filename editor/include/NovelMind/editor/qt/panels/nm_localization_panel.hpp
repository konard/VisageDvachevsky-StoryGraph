#pragma once

/**
 * @file nm_localization_panel.hpp
 * @brief Localization and translation management
 *
 * Provides:
 * - Search and filter functionality
 * - Missing translation highlighting
 * - Navigate to usage locations
 * - Batch operations (add key, delete, duplicate)
 * - Import/export CSV and JSON
 */

#include "NovelMind/editor/qt/nm_dock_panel.hpp"
#include "NovelMind/localization/localization_manager.hpp"

#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QWidget>

class QTableWidget;
class QTableWidgetItem;
class QToolBar;
class QComboBox;
class QPushButton;
class QLineEdit;
class QCheckBox;
class QLabel;
class QMenu;

namespace NovelMind::editor::qt {

/**
 * @brief Filter options for localization entries
 */
enum class LocalizationFilter { All, MissingTranslations, Unused, Modified, NewKeys };

/**
 * @brief Localization entry with status tracking
 */
struct LocalizationEntry {
  QString key;
  QHash<QString, QString> translations; // locale -> translation
  QStringList usageLocations;           // file paths where key is used
  bool isMissing = false;               // has missing translations
  bool isUnused = false;                // not used anywhere in project
  bool isModified = false;              // recently modified
  bool isNew = false;                   // newly added key
  bool isDeleted = false;               // marked for deletion
};

class NMLocalizationPanel : public NMDockPanel {
  Q_OBJECT

public:
  explicit NMLocalizationPanel(QWidget* parent = nullptr);
  ~NMLocalizationPanel() override;

  void onInitialize() override;
  void onShutdown() override;
  void onUpdate(double deltaTime) override;

  /**
   * @brief Navigate to usage location
   */
  void navigateToUsage(const QString& key, int usageIndex = 0);

  /**
   * @brief Add a new localization key
   * @param key The key to add
   * @param defaultValue Default value for the key
   * @return true if key was added successfully
   */
  bool addKey(const QString& key, const QString& defaultValue = QString());

  /**
   * @brief Delete a localization key
   * @param key The key to delete
   * @return true if key was deleted successfully
   */
  bool deleteKey(const QString& key);

  /**
   * @brief Find missing translations for current locale
   * @param locale The locale to check
   * @return List of keys with missing translations
   */
  QStringList findMissingTranslations(const QString& locale) const;

  /**
   * @brief Find unused keys in the project
   */
  QStringList findUnusedKeys() const;

  /**
   * @brief Scan project for key usages
   */
  void scanProjectForUsages();

  /**
   * @brief Check if panel has unsaved changes
   */
  bool isDirty() const { return m_dirty; }

  /**
   * @brief Save changes to localization files
   */
  bool saveChanges();

  /**
   * @brief Import dialogue entries from story graph
   * @param entries List of dialogue entries (key, sourceText pairs)
   * @return Number of entries imported
   */
  int importDialogueEntries(const QList<QPair<QString, QString>>& entries);

  /**
   * @brief Check if a translation exists for a key in the current locale
   * @param key The localization key to check
   * @return true if translation exists, false otherwise
   */
  bool hasTranslation(const QString& key) const;

  /**
   * @brief Get the translation for a key in the current locale
   * @param key The localization key
   * @return The translated text, or empty string if not found
   */
  QString getTranslation(const QString& key) const;

  /**
   * @brief Get the list of available locales
   */
  QStringList availableLocales() const { return m_availableLocales; }

  /**
   * @brief Get the current locale code
   */
  QString currentLocale() const { return m_currentLocale; }

  /**
   * @brief Set a translation value (for undo/redo commands)
   * @param key The localization key
   * @param locale The locale code
   * @param value The translation value
   */
  void setTranslationValue(const QString& key, const QString& locale, const QString& value);

signals:
  void keySelected(const QString& key);
  void navigateToFile(const QString& filePath, int lineNumber);
  void translationChanged(const QString& key, const QString& locale, const QString& newValue);
  void dirtyStateChanged(bool dirty);
  /**
   * @brief Emitted when localization data changes (key added/deleted)
   *
   * This signal is used by undo/redo commands to notify the panel
   * that the table needs to be rebuilt to reflect data changes.
   */
  void localizationDataChanged();

private slots:
  void onSearchTextChanged(const QString& text);
  void onFilterChanged(int index);
  void onLocaleChanged(int index);
  void onCellChanged(int row, int column);
  void onItemDoubleClicked(QTableWidgetItem* item);
  void onAddKeyClicked();
  void onDeleteKeyClicked();
  void onExportClicked();
  void onImportClicked();
  void onRefreshClicked();
  void onShowOnlyMissingToggled(bool checked);
  void onContextMenu(const QPoint& pos);
  void onSaveClicked();
  void onExportMissingClicked();
  void onEditPluralFormsClicked();
  void onToggleRTLPreview(bool checked);
  void onPreviewVariablesChanged();

private:
  void setupUI();
  void setupToolBar();
  void setupFilterBar();
  void setupTable();
  void refreshLocales();
  void loadLocale(const QString& localeCode);
  void rebuildTable();
  void applyFilters();
  void updateStatusBar();
  void highlightMissingTranslations();
  void exportToCsv(const QString& filePath);
  void exportToJson(const QString& filePath);
  void importFromCsv(const QString& filePath);
  void importFromJson(const QString& filePath);
  void setDirty(bool dirty);
  bool showAddKeyDialog(QString& outKey, QString& outDefaultValue);
  bool isValidKeyName(const QString& key) const;
  bool isKeyUnique(const QString& key) const;
  void syncEntriesToManager();
  void syncEntriesFromManager();
  void exportLocale();
  void importLocale();
  void exportMissingStrings();
  void importLocaleAsync(const QString& filePath,
                         ::NovelMind::localization::LocalizationFormat format);
  void exportLocaleAsync(const QString& filePath,
                         ::NovelMind::localization::LocalizationFormat format);
  bool showPluralFormsDialog(const QString& key);
  void updatePreview();
  void applyRTLLayout(bool rtl);

  // UI Elements
  QToolBar* m_toolbar = nullptr;
  QLineEdit* m_searchEdit = nullptr;
  QComboBox* m_filterCombo = nullptr;
  QComboBox* m_languageSelector = nullptr;
  QCheckBox* m_showMissingOnly = nullptr;
  QTableWidget* m_stringsTable = nullptr;
  QLabel* m_statusLabel = nullptr;
  QPushButton* m_addKeyBtn = nullptr;
  QPushButton* m_deleteKeyBtn = nullptr;
  QPushButton* m_importButton = nullptr;
  QPushButton* m_exportButton = nullptr;
  QPushButton* m_exportMissingBtn = nullptr;
  QPushButton* m_refreshBtn = nullptr;
  QPushButton* m_saveBtn = nullptr;
  QPushButton* m_pluralFormsBtn = nullptr;
  QCheckBox* m_rtlPreviewCheckbox = nullptr;

  // Preview Panel
  QWidget* m_previewPanel = nullptr;
  QLineEdit* m_previewInput = nullptr;
  QLabel* m_previewOutput = nullptr;
  QHash<QString, QString> m_previewVariables;

  // Data
  QHash<QString, LocalizationEntry> m_entries;
  QSet<QString> m_deletedKeys; // Keys pending deletion
  QStringList m_availableLocales;
  QString m_defaultLocale = "en";
  QString m_currentLocale;
  QString m_currentFilter;
  LocalizationFilter m_filterMode = LocalizationFilter::All;
  ::NovelMind::localization::LocalizationManager m_localization;
  bool m_dirty = false;

  // PERF-4: Key to table row index mapping for O(1) row lookup
  // This avoids O(n) linear search when updating specific rows
  mutable QHash<QString, int> m_keyToRowMap;

  // Key validation regex
  static const QRegularExpression s_keyValidationRegex;
};

} // namespace NovelMind::editor::qt
