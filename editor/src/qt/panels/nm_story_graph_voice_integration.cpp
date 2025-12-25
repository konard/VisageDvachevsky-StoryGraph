#include "NovelMind/editor/qt/panels/nm_story_graph_voice_integration.hpp"
#include "NovelMind/audio/audio_manager.hpp"
#include "NovelMind/audio/voice_manifest.hpp"
#include "NovelMind/editor/qt/nm_dialogs.hpp"
#include "NovelMind/editor/qt/panels/nm_story_graph_panel.hpp"
#include "NovelMind/editor/voice_manager.hpp"

#include <QDebug>
#include <QDir>
#include <QFileInfo>

namespace NovelMind::editor::qt {

NMStoryGraphVoiceIntegration::NMStoryGraphVoiceIntegration(
    NMStoryGraphPanel *graphPanel, QObject *parent)
    : QObject(parent), m_graphPanel(graphPanel) {
  // Initialize with user's home directory as default
  m_lastBrowseDirectory = QDir::homePath();
}

NMStoryGraphVoiceIntegration::~NMStoryGraphVoiceIntegration() = default;

void NMStoryGraphVoiceIntegration::setVoiceManager(VoiceManager *voiceManager) {
  m_voiceManager = voiceManager;
}

void NMStoryGraphVoiceIntegration::setVoiceManifest(
    audio::VoiceManifest *manifest) {
  m_manifest = manifest;
}

void NMStoryGraphVoiceIntegration::assignVoiceClip(const QString &nodeIdString,
                                                   const QString &currentPath) {
  if (!m_graphPanel) {
    qWarning() << "[VoiceIntegration] Graph panel is null";
    return;
  }

  // Set initial directory for file dialog
  QString initialDir = m_lastBrowseDirectory;
  if (!currentPath.isEmpty()) {
    QFileInfo fileInfo(currentPath);
    if (fileInfo.exists()) {
      initialDir = fileInfo.absolutePath();
    }
  }

  // Open file dialog for voice file selection
  QString voiceFile = NMFileDialog::getOpenFileName(
      m_graphPanel, tr("Select Voice Clip"), initialDir,
      tr("Audio Files (*.ogg *.wav *.mp3 *.flac *.opus);;All Files (*)"));

  if (voiceFile.isEmpty()) {
    // User cancelled
    return;
  }

  // Update last browse directory
  m_lastBrowseDirectory = QFileInfo(voiceFile).absolutePath();

  // Verify file exists
  if (!QFileInfo::exists(voiceFile)) {
    emit errorOccurred(QString("Voice file not found: %1").arg(voiceFile));
    return;
  }

  // Determine binding status
  int bindingStatus = determineBindingStatus(voiceFile);

  // Update node
  updateNodeVoiceStatus(nodeIdString, voiceFile, bindingStatus);

  // Emit signal
  emit voiceClipChanged(nodeIdString, voiceFile, bindingStatus);

  qDebug() << "[VoiceIntegration] Assigned voice clip:" << voiceFile
           << "to node:" << nodeIdString;
}

void NMStoryGraphVoiceIntegration::autoDetectVoice(
    const QString &nodeIdString, const QString &localizationKey) {
  if (!m_voiceManager) {
    emit errorOccurred("Voice Manager not available");
    qWarning() << "[VoiceIntegration] Voice Manager is null";
    return;
  }

  qDebug() << "[VoiceIntegration] Auto-detecting voice for node:"
           << nodeIdString << "with key:" << localizationKey;

  // First, try to find a voice file already bound to this line ID
  const DialogueLine *line =
      m_voiceManager->getLine(nodeIdString.toStdString());
  if (line && !line->voiceFile.empty()) {
    // Line already has a bound voice file
    QString voicePath = QString::fromStdString(line->voiceFile);
    if (QFileInfo::exists(voicePath)) {
      int bindingStatus = determineBindingStatus(voicePath);
      updateNodeVoiceStatus(nodeIdString, voicePath, bindingStatus);
      emit voiceClipChanged(nodeIdString, voicePath, bindingStatus);
      qDebug() << "[VoiceIntegration] Found existing binding:" << voicePath;
      return;
    }
  }

  // Try auto-mapping: preview what would be mapped and check for this node
  auto previewMap = m_voiceManager->previewAutoMapping();
  auto it = previewMap.find(nodeIdString.toStdString());
  if (it != previewMap.end() && !it->second.empty()) {
    QString voicePath = QString::fromStdString(it->second);
    if (QFileInfo::exists(voicePath)) {
      // Apply the binding
      auto result =
          m_voiceManager->bindVoice(nodeIdString.toStdString(), it->second);
      if (result.isOk()) {
        int bindingStatus = determineBindingStatus(voicePath);
        updateNodeVoiceStatus(nodeIdString, voicePath, bindingStatus);
        emit voiceClipChanged(nodeIdString, voicePath, bindingStatus);
        qDebug() << "[VoiceIntegration] Auto-detected and bound:" << voicePath;
        return;
      }
    }
  }

  // If we have a manifest, try to find a matching voice file there
  if (m_manifest) {
    const audio::VoiceManifestLine *manifestLine =
        m_manifest->getLine(nodeIdString.toStdString());
    if (manifestLine) {
      // Check files for the default locale
      const audio::VoiceLocaleFile *localeFile =
          manifestLine->getFile(m_manifest->getDefaultLocale());
      if (localeFile && !localeFile->filePath.empty()) {
        QString voicePath = QString::fromStdString(localeFile->filePath);
        if (QFileInfo::exists(voicePath)) {
          int bindingStatus = determineBindingStatus(voicePath);
          updateNodeVoiceStatus(nodeIdString, voicePath, bindingStatus);
          emit voiceClipChanged(nodeIdString, voicePath, bindingStatus);
          qDebug() << "[VoiceIntegration] Found in manifest:" << voicePath;
          return;
        }
      }
    }
  }

  // No match found - search by localization key pattern
  // Common pattern: voice files named similar to localization key
  if (!localizationKey.isEmpty()) {
    const auto &voiceFiles = m_voiceManager->getVoiceFiles();
    QString keyBaseName = localizationKey;
    keyBaseName.replace('.', '_').replace('-', '_');

    for (const auto &vf : voiceFiles) {
      if (vf.bound) {
        continue; // Skip already bound files
      }

      QString filename = QString::fromStdString(vf.filename);
      QString baseName = QFileInfo(filename).baseName();

      // Check for partial match (case-insensitive)
      if (baseName.compare(keyBaseName, Qt::CaseInsensitive) == 0 ||
          baseName.contains(keyBaseName, Qt::CaseInsensitive) ||
          keyBaseName.contains(baseName, Qt::CaseInsensitive)) {
        QString voicePath = QString::fromStdString(vf.path);
        if (QFileInfo::exists(voicePath)) {
          // Apply the binding
          auto result =
              m_voiceManager->bindVoice(nodeIdString.toStdString(), vf.path);
          if (result.isOk()) {
            int bindingStatus = determineBindingStatus(voicePath);
            updateNodeVoiceStatus(nodeIdString, voicePath, bindingStatus);
            emit voiceClipChanged(nodeIdString, voicePath, bindingStatus);
            qDebug() << "[VoiceIntegration] Auto-detected by key match:"
                     << voicePath;
            return;
          }
        }
      }
    }
  }

  // No match found
  emit errorOccurred(
      tr("No matching voice file found for: %1").arg(nodeIdString));
  qDebug() << "[VoiceIntegration] No voice file found for:" << nodeIdString;
}

void NMStoryGraphVoiceIntegration::previewVoice(const QString &nodeIdString,
                                                const QString &voicePath) {
  if (voicePath.isEmpty()) {
    emit errorOccurred("No voice clip assigned to preview");
    return;
  }

  if (!QFileInfo::exists(voicePath)) {
    emit errorOccurred(QString("Voice file not found: %1").arg(voicePath));
    return;
  }

  if (m_voiceManager) {
    m_currentPreviewNode = nodeIdString;
    m_voiceManager->previewVoiceFile(voicePath.toStdString());
    qDebug() << "[VoiceIntegration] Previewing voice:" << voicePath;
  } else {
    emit errorOccurred("Voice Manager not available for preview");
  }
}

void NMStoryGraphVoiceIntegration::stopPreview() {
  if (m_voiceManager) {
    m_voiceManager->stopPreview();
    m_currentPreviewNode.clear();
  }
}

void NMStoryGraphVoiceIntegration::updateNodeVoiceStatus(
    const QString &nodeIdString, const QString &voicePath, int bindingStatus) {
  if (!m_graphPanel) {
    return;
  }

  // Find the node and update its voice properties
  auto *node = m_graphPanel->findNodeByIdString(nodeIdString);
  if (node) {
    node->setVoiceClipPath(voicePath);
    node->setVoiceBindingStatus(bindingStatus);
    node->update(); // Trigger repaint
  }
}

int NMStoryGraphVoiceIntegration::determineBindingStatus(
    const QString &voicePath) {
  if (voicePath.isEmpty()) {
    return 0; // Unbound
  }

  if (!QFileInfo::exists(voicePath)) {
    return 2; // MissingFile
  }

  return 1; // Bound
}

} // namespace NovelMind::editor::qt
