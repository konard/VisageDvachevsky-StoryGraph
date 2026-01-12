#pragma once

/**
 * @file voice_studio_effects.hpp
 * @brief Audio effects processing for Voice Studio
 */

#include <cstdint>
#include <vector>

namespace NovelMind::editor::qt {

// Forward declarations
struct VoiceClipEdit;
struct AudioFormat;

/**
 * @brief Processes audio samples with the given edit parameters
 *
 * All processing is done in-memory and returns a new buffer.
 * Used for both preview playback and final export.
 */
class AudioProcessor {
public:
  /**
   * @brief Apply all edit parameters to the source samples
   * @param source Source samples
   * @param edit Edit parameters
   * @param format Audio format
   * @return Processed samples
   */
  static std::vector<float> process(const std::vector<float> &source,
                                    const VoiceClipEdit &edit,
                                    const AudioFormat &format);

  /**
   * @brief Apply trim only
   */
  static std::vector<float> applyTrim(const std::vector<float> &samples,
                                      int64_t trimStart, int64_t trimEnd);

  /**
   * @brief Apply fade in/out
   */
  static void applyFades(std::vector<float> &samples, float fadeInMs,
                         float fadeOutMs, uint32_t sampleRate);

  /**
   * @brief Apply pre-gain
   */
  static void applyGain(std::vector<float> &samples, float gainDb);

  /**
   * @brief Apply normalization
   */
  static void applyNormalize(std::vector<float> &samples, float targetDbFS);

  /**
   * @brief Apply high-pass filter
   */
  static void applyHighPass(std::vector<float> &samples, float cutoffHz,
                            uint32_t sampleRate);

  /**
   * @brief Apply low-pass filter
   */
  static void applyLowPass(std::vector<float> &samples, float cutoffHz,
                           uint32_t sampleRate);

  /**
   * @brief Apply 3-band EQ
   */
  static void applyEQ(std::vector<float> &samples, float lowGainDb,
                      float midGainDb, float highGainDb, float lowFreq,
                      float highFreq, uint32_t sampleRate);

  /**
   * @brief Apply noise gate
   */
  static void applyNoiseGate(std::vector<float> &samples, float thresholdDb,
                             float reductionDb, float attackMs, float releaseMs,
                             uint32_t sampleRate);

  /**
   * @brief Calculate peak level in dB
   */
  static float calculatePeakDb(const std::vector<float> &samples);

  /**
   * @brief Calculate RMS level in dB
   */
  static float calculateRmsDb(const std::vector<float> &samples);
};

} // namespace NovelMind::editor::qt
