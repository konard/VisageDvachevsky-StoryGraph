#pragma once

/**
 * @file voice_studio_wav_io.hpp
 * @brief WAV file I/O operations for Voice Studio
 */

#include <QString>
#include <memory>

namespace NovelMind::editor::qt {

// Forward declarations
struct VoiceClip;

/**
 * @brief WAV file I/O operations
 */
class VoiceStudioWavIO {
public:
  /**
   * @brief Load a WAV file into a VoiceClip
   * @param filePath Path to the WAV file
   * @param clip Output clip (will be created if successful)
   * @return true if successful, false otherwise
   */
  static bool loadWavFile(const QString &filePath,
                          std::unique_ptr<VoiceClip> &clip);

  /**
   * @brief Save a VoiceClip to a WAV file
   * @param filePath Path to save the WAV file
   * @param clip Clip to save
   * @return true if successful, false otherwise
   */
  static bool saveWavFile(const QString &filePath, const VoiceClip *clip);
};

} // namespace NovelMind::editor::qt
