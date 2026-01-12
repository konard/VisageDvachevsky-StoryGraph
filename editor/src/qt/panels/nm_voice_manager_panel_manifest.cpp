/**
 * @file nm_voice_manager_panel_manifest.cpp
 * @brief Manifest adapter implementation for Voice Manager Panel
 *
 * This file contains the VoiceManifest adapter methods (import/export, queries)
 * extracted from nm_voice_manager_panel.cpp for better modularity.
 *
 * Refactored as part of issue #487 to split large monolithic file.
 */

#include "NovelMind/editor/qt/panels/nm_voice_manager_panel.hpp"

#include <QFile>

namespace NovelMind::editor::qt {

// ============================================================================
// Manifest adapter methods
// ============================================================================

std::vector<const NovelMind::audio::VoiceManifestLine*>
NMVoiceManagerPanel::getMissingLines() const {
  if (!m_manifest) {
    return {};
  }
  return m_manifest->getLinesByStatus(NovelMind::audio::VoiceLineStatus::Missing,
                                      m_currentLocale.toStdString());
}

QStringList NMVoiceManagerPanel::getUnmatchedLines() const {
  if (!m_manifest) {
    return QStringList();
  }

  QStringList unmatchedLines;
  const std::string locale = m_currentLocale.toStdString();

  // Iterate through all lines and find those without voice files
  for (const auto& line : m_manifest->getLines()) {
    // Check if this line has a file for the current locale
    auto it = line.files.find(locale);
    if (it == line.files.end() || it->second.filePath.empty()) {
      // No file for this locale - it's unmatched
      unmatchedLines.append(QString::fromStdString(line.id));
    } else {
      // Check if the file actually exists on disk
      const QString filePath = QString::fromStdString(it->second.filePath);
      if (!QFile::exists(filePath)) {
        // File is referenced but doesn't exist - also unmatched
        unmatchedLines.append(QString::fromStdString(line.id));
      }
    }
  }

  return unmatchedLines;
}

bool NMVoiceManagerPanel::exportToCsv(const QString& filePath) {
  if (!m_manifest) {
    return false;
  }

  // Use VoiceManifest's built-in export functionality
  auto result = m_manifest->exportToCsv(filePath.toStdString(), m_currentLocale.toStdString());
  return result.isOk();
}

bool NMVoiceManagerPanel::importFromCsv(const QString& filePath) {
  if (!m_manifest) {
    return false;
  }

  // Use VoiceManifest's built-in import functionality
  auto result = m_manifest->importFromCsv(filePath.toStdString(), m_currentLocale.toStdString());
  if (result.isOk()) {
    updateVoiceList();
    updateStatistics();
    return true;
  }
  return false;
}

} // namespace NovelMind::editor::qt
