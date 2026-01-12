#pragma once

/**
 * @file nm_localization_data_model.hpp
 * @brief Data model for localization entries
 *
 * Manages the in-memory data model for localization entries,
 * including synchronization with the localization manager.
 */

#include "NovelMind/localization/localization_manager.hpp"

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>

namespace NovelMind::editor::qt {

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

/**
 * @brief Data model manager for localization entries
 *
 * Handles:
 * - Entry storage and management
 * - Synchronization with LocalizationManager
 * - Key addition/deletion
 * - Translation queries
 */
class NMLocalizationDataModel {
public:
  explicit NMLocalizationDataModel(::NovelMind::localization::LocalizationManager& localization);
  ~NMLocalizationDataModel();

  /**
   * @brief Sync entries from localization manager to in-memory model
   */
  void syncFromManager(const QString& defaultLocale, const QString& currentLocale);

  /**
   * @brief Sync entries from in-memory model to localization manager
   */
  void syncToManager(const QString& defaultLocale, const QString& currentLocale);

  /**
   * @brief Add a new localization key
   * @param key The key to add
   * @param defaultValue Default value for the key
   * @param defaultLocale The default locale
   * @param currentLocale The current locale
   * @return true if key was added successfully
   */
  bool addKey(const QString& key, const QString& defaultValue, const QString& defaultLocale,
              const QString& currentLocale);

  /**
   * @brief Delete a localization key
   * @param key The key to delete
   * @param defaultLocale The default locale
   * @param currentLocale The current locale
   * @return true if key was deleted successfully
   */
  bool deleteKey(const QString& key, const QString& defaultLocale, const QString& currentLocale);

  /**
   * @brief Check if a translation exists for a key
   * @param key The localization key
   * @param currentLocale The locale to check
   * @param defaultLocale Fallback locale
   * @return true if translation exists
   */
  bool hasTranslation(const QString& key, const QString& currentLocale,
                      const QString& defaultLocale) const;

  /**
   * @brief Get the translation for a key
   * @param key The localization key
   * @param currentLocale The current locale
   * @param defaultLocale Fallback locale
   * @return The translated text, or empty string if not found
   */
  QString getTranslation(const QString& key, const QString& currentLocale,
                         const QString& defaultLocale) const;

  /**
   * @brief Set a translation value
   * @param key The localization key
   * @param locale The locale code
   * @param value The translation value
   */
  void setTranslationValue(const QString& key, const QString& locale, const QString& value);

  /**
   * @brief Import dialogue entries from story graph
   * @param entries List of dialogue entries (key, sourceText pairs)
   * @param defaultLocale The default locale
   * @param currentLocale The current locale
   * @return Number of entries imported
   */
  int importDialogueEntries(const QList<QPair<QString, QString>>& entries,
                            const QString& defaultLocale, const QString& currentLocale);

  /**
   * @brief Clear all entries
   */
  void clear();

  /**
   * @brief Get all entries
   */
  const QHash<QString, LocalizationEntry>& entries() const { return m_entries; }

  /**
   * @brief Get mutable access to entries
   */
  QHash<QString, LocalizationEntry>& entries() { return m_entries; }

  /**
   * @brief Get deleted keys
   */
  const QSet<QString>& deletedKeys() const { return m_deletedKeys; }

  /**
   * @brief Clear deleted keys
   */
  void clearDeletedKeys() { m_deletedKeys.clear(); }

  /**
   * @brief Get reference to LocalizationManager
   */
  ::NovelMind::localization::LocalizationManager& localizationManager() { return m_localization; }

  /**
   * @brief Get const reference to LocalizationManager
   */
  const ::NovelMind::localization::LocalizationManager& localizationManager() const {
    return m_localization;
  }

private:
  QHash<QString, LocalizationEntry> m_entries;
  QSet<QString> m_deletedKeys; // Keys pending deletion
  ::NovelMind::localization::LocalizationManager& m_localization;
};

} // namespace NovelMind::editor::qt
