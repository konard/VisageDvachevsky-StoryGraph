#pragma once

/**
 * @file nm_story_graph_voice_integration.hpp
 * @brief Voice-over integration helpers for Story Graph
 *
 * Provides helper functions to integrate voice-over functionality
 * with dialogue nodes in the story graph.
 */

#include <QFileDialog>
#include <QObject>
#include <QString>
#include <memory>

class QWidget;

namespace NovelMind::audio {
class VoiceManifest;
}

namespace NovelMind::editor {
class VoiceManager;
namespace qt {

class NMStoryGraphPanel;
class NMGraphNodeItem;

/**
 * @brief Helper class for voice-over integration in Story Graph
 */
class NMStoryGraphVoiceIntegration : public QObject {
  Q_OBJECT

public:
  explicit NMStoryGraphVoiceIntegration(NMStoryGraphPanel *graphPanel,
                                        QObject *parent = nullptr);
  ~NMStoryGraphVoiceIntegration() override;

  /**
   * @brief Set the voice manager to use for voice operations
   */
  void setVoiceManager(VoiceManager *voiceManager);

  /**
   * @brief Set the voice manifest for voice operations
   */
  void setVoiceManifest(audio::VoiceManifest *manifest);

  /**
   * @brief Open file dialog to assign voice clip to a dialogue node
   */
  void assignVoiceClip(const QString &nodeIdString, const QString &currentPath);

  /**
   * @brief Auto-detect voice file based on localization key
   */
  void autoDetectVoice(const QString &nodeIdString,
                       const QString &localizationKey);

  /**
   * @brief Preview voice clip for a dialogue node
   */
  void previewVoice(const QString &nodeIdString, const QString &voicePath);

  /**
   * @brief Stop current voice preview
   */
  void stopPreview();

  /**
   * @brief Check if voice features are available
   * @return true if both VoiceManager and GraphPanel are available
   */
  [[nodiscard]] bool isVoiceSystemAvailable() const;

  /**
   * @brief Get human-readable message explaining why voice features are unavailable
   * @return Empty string if available, or error message if not available
   */
  [[nodiscard]] QString getUnavailabilityReason() const;

signals:
  /**
   * @brief Emitted when voice clip assignment changes
   */
  void voiceClipChanged(const QString &nodeIdString, const QString &voicePath,
                        int bindingStatus);

  /**
   * @brief Emitted when recording is requested
   */
  void recordingRequested(const QString &nodeIdString,
                          const QString &dialogueText, const QString &speaker);

  /**
   * @brief Emitted when an error occurs
   */
  void errorOccurred(const QString &message);

private:
  void updateNodeVoiceStatus(const QString &nodeIdString,
                             const QString &voicePath, int bindingStatus);
  int determineBindingStatus(const QString &voicePath);

  NMStoryGraphPanel *m_graphPanel = nullptr;
  VoiceManager *m_voiceManager = nullptr;
  audio::VoiceManifest *m_manifest = nullptr;
  QString m_currentPreviewNode;
  QString m_lastBrowseDirectory;
};

} // namespace qt
} // namespace NovelMind::editor
