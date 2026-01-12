#include "NovelMind/editor/qt/panels/nm_localization_io.hpp"
#include "NovelMind/editor/qt/nm_dialogs.hpp"

#include <QDir>
#include <QFileInfo>

namespace NovelMind::editor::qt {

NMLocalizationIO::NMLocalizationIO(::NovelMind::localization::LocalizationManager& localization,
                                   NMLocalizationDataModel& dataModel)
    : m_localization(localization)
    , m_dataModel(dataModel) {}

NMLocalizationIO::~NMLocalizationIO() = default;

::NovelMind::localization::LocalizationFormat
NMLocalizationIO::formatForExtension(const QString& ext) {
  const QString lower = ext.toLower();
  if (lower == "csv")
    return ::NovelMind::localization::LocalizationFormat::CSV;
  if (lower == "json")
    return ::NovelMind::localization::LocalizationFormat::JSON;
  if (lower == "po")
    return ::NovelMind::localization::LocalizationFormat::PO;
  if (lower == "xliff" || lower == "xlf")
    return ::NovelMind::localization::LocalizationFormat::XLIFF;
  return ::NovelMind::localization::LocalizationFormat::CSV;
}

bool NMLocalizationIO::loadLocale(const QString& localeCode, const QString& localizationRoot) {
  NovelMind::localization::LocaleId locale;
  locale.language = localeCode.toStdString();

  QDir dir(localizationRoot);
  if (!dir.exists()) {
    dir.mkpath(".");
  }

  QStringList candidates = {dir.filePath(localeCode + ".csv"), dir.filePath(localeCode + ".json"),
                            dir.filePath(localeCode + ".po"), dir.filePath(localeCode + ".xliff"),
                            dir.filePath(localeCode + ".xlf")};
  for (const auto& path : candidates) {
    QFileInfo info(path);
    if (!info.exists()) {
      continue;
    }
    const auto format = formatForExtension(info.suffix());
    m_localization.loadStrings(locale, info.absoluteFilePath().toStdString(), format);
    return true;
  }

  return false;
}

bool NMLocalizationIO::saveChanges(const QStringList& availableLocales,
                                   const QString& localizationRoot, QWidget* parentWidget) {
  QDir dir(localizationRoot);
  if (!dir.exists()) {
    dir.mkpath(".");
  }

  // Save each locale to its file
  for (const QString& localeCode : availableLocales) {
    NovelMind::localization::LocaleId locale;
    locale.language = localeCode.toStdString();

    // Default to CSV format
    const QString filePath = dir.filePath(localeCode + ".csv");
    auto result = m_localization.exportStrings(locale, filePath.toStdString(),
                                               ::NovelMind::localization::LocalizationFormat::CSV);

    if (result.isError()) {
      NMMessageDialog::showError(
          parentWidget, QObject::tr("Save Failed"),
          QObject::tr("Failed to save %1: %2")
              .arg(localeCode)
              .arg(QString::fromStdString(result.error())));
      return false;
    }
  }

  return true;
}

void NMLocalizationIO::exportLocale(const QString& currentLocale, const QString& localizationRoot,
                                    QWidget* parentWidget) {
  if (currentLocale.isEmpty()) {
    return;
  }

  const QString filter = QObject::tr("Localization (*.csv *.json *.po *.xliff *.xlf)");
  const QString defaultName = QDir(localizationRoot).filePath(currentLocale + ".csv");
  const QString path = NMFileDialog::getSaveFileName(parentWidget, QObject::tr("Export Localization"),
                                                     defaultName, filter);
  if (path.isEmpty()) {
    return;
  }

  const QFileInfo info(path);
  const auto format = formatForExtension(info.suffix());

  NovelMind::localization::LocaleId locale;
  locale.language = currentLocale.toStdString();
  auto result = m_localization.exportStrings(locale, path.toStdString(), format);
  if (result.isError()) {
    NMMessageDialog::showError(parentWidget, QObject::tr("Export Failed"),
                               QString::fromStdString(result.error()));
  }
}

bool NMLocalizationIO::importLocale(const QString& localizationRoot, QWidget* parentWidget,
                                    QString& outLocaleCode) {
  const QString filter = QObject::tr("Localization (*.csv *.json *.po *.xliff *.xlf)");
  const QString path = NMFileDialog::getOpenFileName(
      parentWidget, QObject::tr("Import Localization"), QDir::homePath(), filter);
  if (path.isEmpty()) {
    return false;
  }

  QFileInfo info(path);
  outLocaleCode = info.baseName();
  const auto format = formatForExtension(info.suffix());

  NovelMind::localization::LocaleId locale;
  locale.language = outLocaleCode.toStdString();
  auto result = m_localization.loadStrings(locale, path.toStdString(), format);
  if (result.isError()) {
    NMMessageDialog::showError(parentWidget, QObject::tr("Import Failed"),
                               QString::fromStdString(result.error()));
    return false;
  }

  return true;
}

void NMLocalizationIO::exportMissingStrings(const QString& currentLocale,
                                            const QString& localizationRoot, QWidget* parentWidget) {
  if (currentLocale.isEmpty()) {
    return;
  }

  const QString filter = QObject::tr("Localization (*.csv *.json *.po *.xliff *.xlf)");
  const QString defaultName = QDir(localizationRoot).filePath(currentLocale + "_missing.csv");
  const QString path = NMFileDialog::getSaveFileName(
      parentWidget, QObject::tr("Export Missing Translations"), defaultName, filter);
  if (path.isEmpty()) {
    return;
  }

  const QFileInfo info(path);
  const auto format = formatForExtension(info.suffix());

  NovelMind::localization::LocaleId locale;
  locale.language = currentLocale.toStdString();
  auto result = m_localization.exportMissingStrings(locale, path.toStdString(), format);
  if (result.isError()) {
    NMMessageDialog::showError(parentWidget, QObject::tr("Export Failed"),
                               QString::fromStdString(result.error()));
  } else {
    NMMessageDialog::showInfo(
        parentWidget, QObject::tr("Export Successful"),
        QObject::tr("Missing translations exported successfully to:\n%1").arg(path));
  }
}

void NMLocalizationIO::exportToCsv(const QString& filePath, const QString& currentLocale) {
  NovelMind::localization::LocaleId locale;
  locale.language = currentLocale.toStdString();
  m_localization.exportStrings(locale, filePath.toStdString(),
                               ::NovelMind::localization::LocalizationFormat::CSV);
}

void NMLocalizationIO::exportToJson(const QString& filePath, const QString& currentLocale) {
  NovelMind::localization::LocaleId locale;
  locale.language = currentLocale.toStdString();
  m_localization.exportStrings(locale, filePath.toStdString(),
                               ::NovelMind::localization::LocalizationFormat::JSON);
}

bool NMLocalizationIO::importFromCsv(const QString& filePath, const QString& currentLocale,
                                     QWidget* parentWidget) {
  Q_UNUSED(parentWidget);

  // Perform the import
  NovelMind::localization::LocaleId locale;
  locale.language = currentLocale.toStdString();
  m_localization.loadStrings(locale, filePath.toStdString(),
                             ::NovelMind::localization::LocalizationFormat::CSV);

  return true;
}

bool NMLocalizationIO::importFromJson(const QString& filePath, const QString& currentLocale,
                                      QWidget* parentWidget) {
  Q_UNUSED(parentWidget);

  // Perform the import
  NovelMind::localization::LocaleId locale;
  locale.language = currentLocale.toStdString();
  m_localization.loadStrings(locale, filePath.toStdString(),
                             ::NovelMind::localization::LocalizationFormat::JSON);

  return true;
}

} // namespace NovelMind::editor::qt
