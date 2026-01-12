/**
 * @file nm_voice_manager_panel_matching.cpp
 * @brief Voice matching and scanning implementation for Voice Manager Panel
 *
 * This file contains the voice file auto-matching and scanning functionality
 * extracted from nm_voice_manager_panel.cpp for better modularity.
 *
 * Refactored as part of issue #487 to split large monolithic file.
 */

#include "NovelMind/editor/qt/panels/nm_voice_manager_panel.hpp"
#include "NovelMind/editor/project_manager.hpp"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>
#include <filesystem>

namespace fs = std::filesystem;

namespace NovelMind::editor::qt {

// ============================================================================
// Scanning and matching implementation
// ============================================================================

void NMVoiceManagerPanel::scanScriptsForDialogue() {
  auto& pm = ProjectManager::instance();
  if (!pm.hasOpenProject()) {
    return;
  }

  QString scriptsDir = QString::fromStdString(pm.getFolderPath(ProjectFolder::Scripts));
  if (scriptsDir.isEmpty() || !fs::exists(scriptsDir.toStdString())) {
    return;
  }

  // Pattern to match dialogue lines: say Character "Text" or Character: "Text"
  QRegularExpression dialoguePattern(R"((?:say\s+)?(\w+)\s*[:\s]?\s*\"([^\"]+)\")");

  // Track speakers for character filter
  QStringList speakers;

  for (const auto& entry : fs::recursive_directory_iterator(scriptsDir.toStdString())) {
    if (!entry.is_regular_file() || entry.path().extension() != ".nms") {
      continue;
    }

    QString scriptPath = QString::fromStdString(entry.path().string());
    QString relPath = QString::fromStdString(pm.toRelativePath(entry.path().string()));

    // Extract scene name from file path
    QString sceneName = QFileInfo(relPath).baseName();

    QFile file(scriptPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      continue;
    }

    QTextStream in(&file);
    int lineNum = 0;
    while (!in.atEnd()) {
      QString line = in.readLine();
      lineNum++;

      QRegularExpressionMatch match = dialoguePattern.match(line);
      if (match.hasMatch()) {
        QString speaker = match.captured(1);
        QString dialogueText = match.captured(2);
        QString dialogueId = generateDialogueId(relPath, lineNum);

        // Create VoiceManifestLine
        NovelMind::audio::VoiceManifestLine manifestLine;
        manifestLine.id = dialogueId.toStdString();
        manifestLine.textKey = dialogueId.toStdString(); // Use ID as text key for now
        manifestLine.speaker = speaker.toStdString();
        manifestLine.scene = sceneName.toStdString();
        manifestLine.sourceScript = relPath.toStdString();
        manifestLine.sourceLine = static_cast<NovelMind::u32>(lineNum);

        // Add to manifest
        m_manifest->addLine(manifestLine);

        if (!speakers.contains(speaker)) {
          speakers.append(speaker);
        }
      }
    }
    file.close();
  }

  // Update character filter
  if (m_characterFilter) {
    m_characterFilter->clear();
    m_characterFilter->addItem(tr("All Characters"));
    m_characterFilter->addItems(speakers);
  }
}

void NMVoiceManagerPanel::scanVoiceFolder() {
  auto& pm = ProjectManager::instance();
  if (!pm.hasOpenProject()) {
    return;
  }

  QString voiceDir = QString::fromStdString(pm.getProjectPath()) + "/Assets/Voice";
  if (!fs::exists(voiceDir.toStdString())) {
    return;
  }

  for (const auto& entry : fs::recursive_directory_iterator(voiceDir.toStdString())) {
    if (!entry.is_regular_file()) {
      continue;
    }

    QString ext = QString::fromStdString(entry.path().extension().string()).toLower();
    if (ext == ".ogg" || ext == ".wav" || ext == ".mp3" || ext == ".flac") {
      m_voiceFiles.append(QString::fromStdString(entry.path().string()));
    }
  }
}

void NMVoiceManagerPanel::autoMatchVoiceFiles() {
  for (const QString& voiceFile : m_voiceFiles) {
    matchVoiceToDialogue(voiceFile);
  }
}

void NMVoiceManagerPanel::matchVoiceToDialogue(const QString& voiceFile) {
  QString filename = QFileInfo(voiceFile).baseName();

  // Try exact match with dialogue ID
  auto* line = m_manifest->getLineMutable(filename.toStdString());
  if (line) {
    auto& localeFile = line->getOrCreateFile(m_currentLocale.toStdString());
    localeFile.filePath = voiceFile.toStdString();
    localeFile.status = NovelMind::audio::VoiceLineStatus::Imported;
    return;
  }

  // Try pattern matching: character_linenum or scene_linenum
  QRegularExpression pattern(R"((\w+)_(\d+))");
  QRegularExpressionMatch match = pattern.match(filename);
  if (match.hasMatch()) {
    QString prefix = match.captured(1);
    int lineNum = match.captured(2).toInt();

    // Search through all lines for matching speaker or scene
    for (const auto& manifestLine : m_manifest->getLines()) {
      if ((QString::fromStdString(manifestLine.speaker).compare(prefix, Qt::CaseInsensitive) == 0 ||
           QString::fromStdString(manifestLine.scene).compare(prefix, Qt::CaseInsensitive) == 0) &&
          manifestLine.sourceLine == static_cast<NovelMind::u32>(lineNum)) {
        // Check if already has file for this locale
        if (!manifestLine.hasFile(m_currentLocale.toStdString())) {
          auto* mutableLine = m_manifest->getLineMutable(manifestLine.id);
          if (mutableLine) {
            auto& localeFile = mutableLine->getOrCreateFile(m_currentLocale.toStdString());
            localeFile.filePath = voiceFile.toStdString();
            localeFile.status = NovelMind::audio::VoiceLineStatus::Imported;
            return;
          }
        }
      }
    }
  }
}

QString NMVoiceManagerPanel::generateDialogueId(const QString& scriptPath, int lineNumber) const {
  QString baseName = QFileInfo(scriptPath).baseName();
  return QString("%1_%2").arg(baseName).arg(lineNumber);
}

} // namespace NovelMind::editor::qt
