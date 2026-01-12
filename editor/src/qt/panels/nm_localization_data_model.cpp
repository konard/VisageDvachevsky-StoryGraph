#include "NovelMind/editor/qt/panels/nm_localization_data_model.hpp"

namespace NovelMind::editor::qt {

NMLocalizationDataModel::NMLocalizationDataModel(
    ::NovelMind::localization::LocalizationManager& localization)
    : m_localization(localization) {}

NMLocalizationDataModel::~NMLocalizationDataModel() = default;

void NMLocalizationDataModel::syncFromManager(const QString& defaultLocale,
                                              const QString& currentLocale) {
  m_entries.clear();
  m_deletedKeys.clear();

  NovelMind::localization::LocaleId defaultLoc;
  defaultLoc.language = defaultLocale.toStdString();
  NovelMind::localization::LocaleId currentLoc;
  currentLoc.language = currentLocale.toStdString();

  const auto* defaultTable = m_localization.getStringTable(defaultLoc);
  const auto* currentTable = m_localization.getStringTable(currentLoc);

  std::vector<std::string> ids;
  if (defaultTable) {
    ids = defaultTable->getStringIds();
  }
  if (currentTable) {
    auto currentIds = currentTable->getStringIds();
    ids.insert(ids.end(), currentIds.begin(), currentIds.end());
  }

  std::sort(ids.begin(), ids.end());
  ids.erase(std::unique(ids.begin(), ids.end()), ids.end());

  for (const auto& id : ids) {
    LocalizationEntry entry;
    entry.key = QString::fromStdString(id);

    if (defaultTable) {
      auto val = defaultTable->getString(id);
      if (val) {
        entry.translations[defaultLocale] = QString::fromStdString(*val);
      }
    }
    if (currentTable && currentLocale != defaultLocale) {
      auto val = currentTable->getString(id);
      if (val) {
        entry.translations[currentLocale] = QString::fromStdString(*val);
      }
    }

    // Check if missing translation for current locale
    entry.isMissing =
        !entry.translations.contains(currentLocale) || entry.translations.value(currentLocale).isEmpty();

    m_entries[entry.key] = entry;
  }
}

void NMLocalizationDataModel::syncToManager(const QString& defaultLocale,
                                            const QString& currentLocale) {
  NovelMind::localization::LocaleId defaultLoc;
  defaultLoc.language = defaultLocale.toStdString();
  NovelMind::localization::LocaleId currentLoc;
  currentLoc.language = currentLocale.toStdString();

  // Remove deleted keys from manager
  for (const QString& key : m_deletedKeys) {
    m_localization.removeString(defaultLoc, key.toStdString());
    m_localization.removeString(currentLoc, key.toStdString());
  }

  // Update/add entries
  for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
    const LocalizationEntry& entry = it.value();
    if (entry.isDeleted) {
      continue;
    }

    const std::string id = entry.key.toStdString();

    if (entry.translations.contains(defaultLocale)) {
      m_localization.setString(defaultLoc, id, entry.translations.value(defaultLocale).toStdString());
    }
    if (entry.translations.contains(currentLocale)) {
      m_localization.setString(currentLoc, id, entry.translations.value(currentLocale).toStdString());
    }
  }
}

bool NMLocalizationDataModel::addKey(const QString& key, const QString& defaultValue,
                                     const QString& defaultLocale, const QString& currentLocale) {
  if (key.isEmpty()) {
    return false;
  }

  // Check if key already exists (and is not deleted)
  if (m_entries.contains(key) && !m_entries.value(key).isDeleted) {
    return false;
  }

  LocalizationEntry entry;
  entry.key = key;
  entry.translations[defaultLocale] = defaultValue;
  entry.isNew = true;
  entry.isMissing = (currentLocale != defaultLocale);

  m_entries[key] = entry;

  // Add to manager
  NovelMind::localization::LocaleId locale;
  locale.language = defaultLocale.toStdString();
  m_localization.setString(locale, key.toStdString(), defaultValue.toStdString());

  return true;
}

bool NMLocalizationDataModel::deleteKey(const QString& key, const QString& defaultLocale,
                                        const QString& currentLocale) {
  if (!m_entries.contains(key)) {
    return false;
  }

  m_entries[key].isDeleted = true;
  m_deletedKeys.insert(key);

  // Remove from manager
  NovelMind::localization::LocaleId defaultLoc;
  defaultLoc.language = defaultLocale.toStdString();
  NovelMind::localization::LocaleId currentLoc;
  currentLoc.language = currentLocale.toStdString();

  m_localization.removeString(defaultLoc, key.toStdString());
  m_localization.removeString(currentLoc, key.toStdString());

  return true;
}

bool NMLocalizationDataModel::hasTranslation(const QString& key, const QString& currentLocale,
                                             const QString& defaultLocale) const {
  if (!m_entries.contains(key)) {
    return false;
  }

  const LocalizationEntry& entry = m_entries.value(key);
  if (entry.isDeleted) {
    return false;
  }

  // Check if translation exists for current locale
  if (currentLocale == defaultLocale) {
    return entry.translations.contains(defaultLocale) &&
           !entry.translations.value(defaultLocale).isEmpty();
  }

  return entry.translations.contains(currentLocale) &&
         !entry.translations.value(currentLocale).isEmpty();
}

QString NMLocalizationDataModel::getTranslation(const QString& key, const QString& currentLocale,
                                                const QString& defaultLocale) const {
  if (!m_entries.contains(key)) {
    return QString();
  }

  const LocalizationEntry& entry = m_entries.value(key);
  if (entry.isDeleted) {
    return QString();
  }

  // Return translation for current locale, fallback to default
  if (entry.translations.contains(currentLocale)) {
    return entry.translations.value(currentLocale);
  }

  return entry.translations.value(defaultLocale);
}

void NMLocalizationDataModel::setTranslationValue(const QString& key, const QString& locale,
                                                  const QString& value) {
  if (m_entries.contains(key)) {
    m_entries[key].translations[locale] = value;
    m_entries[key].isMissing = value.isEmpty();
    m_entries[key].isModified = true;
  }
}

int NMLocalizationDataModel::importDialogueEntries(const QList<QPair<QString, QString>>& entries,
                                                   const QString& defaultLocale,
                                                   const QString& currentLocale) {
  if (entries.isEmpty()) {
    return 0;
  }

  int imported = 0;
  for (const auto& entry : entries) {
    const QString& key = entry.first;
    const QString& sourceText = entry.second;

    // Skip if key already exists and has a value
    if (m_entries.contains(key) && !m_entries[key].isDeleted) {
      // Update source text if different
      if (m_entries[key].translations.value(defaultLocale) != sourceText) {
        m_entries[key].translations[defaultLocale] = sourceText;
        m_entries[key].isModified = true;
        ++imported;
      }
      continue;
    }

    // Add new entry
    LocalizationEntry newEntry;
    newEntry.key = key;
    newEntry.translations[defaultLocale] = sourceText;
    newEntry.isNew = true;
    newEntry.isMissing = (currentLocale != defaultLocale);

    m_entries[key] = newEntry;

    // Add to localization manager
    NovelMind::localization::LocaleId locale;
    locale.language = defaultLocale.toStdString();
    m_localization.setString(locale, key.toStdString(), sourceText.toStdString());

    ++imported;
  }

  return imported;
}

void NMLocalizationDataModel::clear() {
  m_entries.clear();
  m_deletedKeys.clear();
}

} // namespace NovelMind::editor::qt
