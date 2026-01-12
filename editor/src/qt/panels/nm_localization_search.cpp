#include "NovelMind/editor/qt/panels/nm_localization_search.hpp"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QTextStream>

namespace NovelMind::editor::qt {

// Key validation: allows alphanumeric, underscore, dot, dash
const QRegularExpression NMLocalizationSearch::s_keyValidationRegex(
    QStringLiteral("^[A-Za-z0-9_.-]+$"));

NMLocalizationSearch::NMLocalizationSearch(NMLocalizationDataModel& dataModel)
    : m_dataModel(dataModel) {}

NMLocalizationSearch::~NMLocalizationSearch() = default;

bool NMLocalizationSearch::isValidKeyName(const QString& key) const {
  return s_keyValidationRegex.match(key).hasMatch();
}

bool NMLocalizationSearch::isKeyUnique(const QString& key) const {
  const auto& entries = m_dataModel.entries();
  return !entries.contains(key) || entries.value(key).isDeleted;
}

QStringList NMLocalizationSearch::findMissingTranslations(const QString& locale) const {
  QStringList missing;

  const auto& entries = m_dataModel.entries();
  for (auto it = entries.constBegin(); it != entries.constEnd(); ++it) {
    const LocalizationEntry& entry = it.value();
    if (entry.isDeleted) {
      continue;
    }

    if (!entry.translations.contains(locale) || entry.translations.value(locale).isEmpty()) {
      missing.append(entry.key);
    }
  }

  return missing;
}

QStringList NMLocalizationSearch::findUnusedKeys() const {
  QStringList unused;

  const auto& entries = m_dataModel.entries();
  for (auto it = entries.constBegin(); it != entries.constEnd(); ++it) {
    const LocalizationEntry& entry = it.value();
    if (entry.isDeleted) {
      continue;
    }

    if (entry.isUnused || entry.usageLocations.isEmpty()) {
      unused.append(entry.key);
    }
  }

  return unused;
}

void NMLocalizationSearch::scanProjectForUsages(const QString& projectPath) {
  // Clear existing usage data
  auto& entries = m_dataModel.entries();
  for (auto it = entries.begin(); it != entries.end(); ++it) {
    it->usageLocations.clear();
    it->isUnused = true;
  }

  // Directories to scan for script files
  QStringList scanDirs = {projectPath + "/Scripts", projectPath + "/Scenes", projectPath + "/Data"};

  // File extensions to scan
  QStringList extensions = {"*.nms", "*.json", "*.yaml", "*.yml", "*.xml"};

  // Regex patterns to find localization key references
  // Matches patterns like: localize("key"), tr("key"), @key, loc:key
  QRegularExpression keyRefPattern(
      R"((?:localize|tr|getText)\s*\(\s*["\']([^"\']+)["\']\)|@([A-Za-z0-9_.-]+)|loc:([A-Za-z0-9_.-]+))");

  for (const QString& scanDir : scanDirs) {
    QDir dir(scanDir);
    if (!dir.exists()) {
      continue;
    }

    QDirIterator it(scanDir, extensions, QDir::Files, QDirIterator::Subdirectories);

    while (it.hasNext()) {
      const QString filePath = it.next();
      QFile file(filePath);

      if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        continue;
      }

      QTextStream in(&file);
      int lineNumber = 0;

      while (!in.atEnd()) {
        const QString line = in.readLine();
        lineNumber++;

        QRegularExpressionMatchIterator matchIt = keyRefPattern.globalMatch(line);

        while (matchIt.hasNext()) {
          QRegularExpressionMatch match = matchIt.next();
          QString foundKey;

          // Check which capture group matched
          if (!match.captured(1).isEmpty()) {
            foundKey = match.captured(1); // localize/tr/getText pattern
          } else if (!match.captured(2).isEmpty()) {
            foundKey = match.captured(2); // @key pattern
          } else if (!match.captured(3).isEmpty()) {
            foundKey = match.captured(3); // loc:key pattern
          }

          if (!foundKey.isEmpty() && entries.contains(foundKey)) {
            LocalizationEntry& entry = entries[foundKey];
            const QString usageLocation = QString("%1:%2").arg(filePath).arg(lineNumber);

            if (!entry.usageLocations.contains(usageLocation)) {
              entry.usageLocations.append(usageLocation);
            }
            entry.isUnused = false;
          }
        }
      }

      file.close();
    }
  }
}

const QRegularExpression& NMLocalizationSearch::keyValidationRegex() {
  return s_keyValidationRegex;
}

} // namespace NovelMind::editor::qt
